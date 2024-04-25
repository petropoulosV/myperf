#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <poll.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <linux/types.h>
#include <linux/errqueue.h>

#include "util.h"

// Time measurement methods
// ------------------------

ulong ts_diff_ns(struct timespec start, struct timespec end) {
	return (end.tv_sec - start.tv_sec) * 1000000000
		+ (end.tv_nsec - start.tv_nsec);
}

double ts_diff_us(struct timespec start, struct timespec end) {
	return (double) (end.tv_sec - start.tv_sec) * 1e06
		+ (double) (end.tv_nsec - start.tv_nsec) / 1e03;
}

double ts_diff_ms(struct timespec start, struct timespec end) {
	return (double) (end.tv_sec - start.tv_sec) * 1e03
		+ (double) (end.tv_nsec - start.tv_nsec) / 1e06;
}

void wait_ns(ulong ns) {
	
	struct timespec start, current;
	
	if(ns == 0)
		return;
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);

	do {
		clock_gettime(CLOCK_MONOTONIC_RAW, &current);	
	} while(ts_diff_ns(start, current) < ns);
}

void sleep_ms(ulong ms) {
	struct timespec ts;
	
	ts = (struct timespec) {.tv_sec = 0, .tv_nsec = ms * 1e06};
	
	nanosleep(&ts, NULL);
}

// Socket management methods
// -------------------------

int zerocopy_wait_notification(int sockfd, int count) {
	int confirmed_packets = 0;
	
	do {
		uint8_t cbuf[CMSG_SPACE(sizeof(struct sock_extended_err))];
		
		// 1. Block until an event is available
		
		struct pollfd pfd = (struct pollfd) {
			.fd = sockfd,
			.events = 0
		};
		
		if(poll(&pfd, 1, -1) != 1 || (pfd.revents & POLLERR) == 0)
			return -1;
		
		// 2. Read the message from the error queue
		
		struct msghdr msg = (struct msghdr) {
			.msg_control = cbuf,
			.msg_controllen = sizeof cbuf
		};
		
		if(recvmsg(sockfd, &msg, MSG_ERRQUEUE) == -1)
			return -2;
		
		// 3. Parse the "error" message
		
		struct cmsghdr *cm = CMSG_FIRSTHDR(&msg);
		struct sock_extended_err *serr = (void *) CMSG_DATA(cm);
		
		if(cm->cmsg_level != SOL_IP && cm->cmsg_type != IP_RECVERR)
			return -3;
		
		if(serr->ee_origin != SO_EE_ORIGIN_ZEROCOPY || serr->ee_errno != 0)
			return -4;
		
		/* printf("Received confirmation for [%u, %u]\n",
			serr->ee_info, serr->ee_data); */
		
		confirmed_packets += serr->ee_data - serr->ee_info + 1;
	} while(count >= 0 && confirmed_packets < count);
	
	return 0;
}

int empty_recv_buffer(int sockfd) {
	while(1) {
		int bytes;
		
		if(ioctl(sockfd, FIONREAD, &bytes) == -1)
			return -1;
		
		if(bytes == 0)
			break;
		
		if(recvfrom(sockfd, NULL, 0, 0, NULL, NULL) != 0)
			return -2;
	}
	
	return 0;
}

// Command Channel management operations
// -------------------------------------

ssize_t cmdc_master_write(struct cmdc *channel, void *buf, size_t count) {
	if(channel->state != 0) return -1;
	
	channel->msg_buffer = buf;
	channel->msg_buflen = count;
	channel->state = 1;
	
	while(channel->state != 2);
	channel->state = 0;
	
	return channel->response_len;
}

ssize_t cmdc_slave_read(struct cmdc *channel, void *buf, size_t count) {
	if(channel->state != 1) return 0;
	
	if(channel->msg_buflen > count)
		return -1;
	
	memcpy(buf, channel->msg_buffer, channel->msg_buflen);
	
	return channel->msg_buflen;
}

ssize_t cmdc_slave_respond(struct cmdc *channel, void *buf, size_t count) {
	if(channel->state != 1) return 0;
	
	if(count > channel->msg_buflen)
		return -1;
	
	memcpy(channel->msg_buffer, buf, count);
	
	channel->response_len = count;
	channel->state = 2;
	
	return count;
}

// Statistics helper methods
// ------------------------

/* Calculates the population standard deviation, given the 
 * number of samples, their sum and the sum of their squares */
double stdev(int count, size_t sum, size_t sqsum) {
	return count ? sqrt((double) (count*sqsum - sum*sum) / (count * count)) : 0;
}
