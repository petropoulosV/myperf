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
	
	time_t sec = ms / 1000;
	ms = ms % 1000;
	
	ts = (struct timespec) {.tv_sec = sec, .tv_nsec = ms * 1e06};
	
	nanosleep(&ts, NULL);
}

// Socket management methods
// -------------------------

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
