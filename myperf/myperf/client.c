#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "myperf.h"
#include "client.h"
#include "util.h"

// -----------------------------------------------------

static int send_burst(int sockfd, const struct sockaddr *dest_addr,
	socklen_t dest_addrlen, const void *message, size_t length,
	int burst_size, ulong *seq_number);

#ifdef USE_STATIC_DELAY
static ulong find_tx_time_ns(int sockfd, const struct sockaddr *dest_addr,
	socklen_t dest_addrlen, void *buf, size_t buflen);
#endif

#ifdef USE_ZEROCOPY
	int zerocopy_wait_notification(int sockfd, int count);
#endif
// -----------------------------------------------------

int client_cmdc_stop(struct cmdc *channel) {
	struct client_command command;
	
	command.opcode = CLIENT_CMDC_STOP;
	cmdc_master_write(channel, &command, sizeof command);
	
	return (command.opcode == CLIENT_CMDC_OK);
}

int client_cmdc_report_bitrate(struct cmdc *channel, ulong bitrate) {
	uint8_t buf[1 + sizeof bitrate];
	
	buf[0] = CLIENT_CMDC_REPORT_BITRATE;
	memcpy(buf + 1, &bitrate, sizeof bitrate);
	
	cmdc_master_write(channel, buf, sizeof buf);
	
	return (buf[0] == CLIENT_CMDC_OK);
}

// -----------------------------------------------------

static void generate_traffic(int sockfd, const struct sockaddr *dest_addr,
	socklen_t dest_addrlen, size_t block_size, int block_overhead,
		ulong target_bitrate, struct cmdc *cmd_channel) {
	
	void *buf = malloc(block_size);
	if(!buf) FERROR("Error @ malloc()");
	
	ulong seq_number = 0;
	
	#ifdef USE_STATIC_DELAY
		ulong static_tx_time = find_tx_time_ns(sockfd, dest_addr,
			dest_addrlen, buf, block_size);
	#endif
	
	MYPERF_SET_HEADER(buf, MYPERF_OPCODE_PRIMER, 0);
	send_burst(sockfd, dest_addr, dest_addrlen,
		buf, sizeof(struct myperf_header), 10, &seq_number);
	
	MYPERF_SET_HEADER(buf, MYPERF_OPCODE_DATA, seq_number);
	
	double bitrate_sum = 0;
	ulong bitrate_count = 0;
	
	double bitrate_diff = 0;
	double bitrate_factor = 1;
	
	double wait_time = 0;
	
	int burst_size_cap = 8192;
	ulong burst_size = 1;
	
	while(1) {
		struct timespec start_ts, end_ts;
		uint8_t cmdc_buf[1 + sizeof(ulong)];
		
		int packets = 0;
		ulong burst_time;
		
		if(cmdc_slave_read(cmd_channel, cmdc_buf, sizeof cmdc_buf)) {
			if(cmdc_buf[0] == CLIENT_CMDC_STOP) {
				cmdc_buf[0] = CLIENT_CMDC_OK;
				cmdc_slave_respond(cmd_channel, cmdc_buf, 1);
				
				break;
			}
			
			else if(cmdc_buf[0] == CLIENT_CMDC_REPORT_BITRATE) {
				if(bitrate_count > 0) {
					ulong reported_bitrate = * (ulong *) ((void *) cmdc_buf + 1);
					double bitrate_avg = bitrate_sum / bitrate_count;
					
					bitrate_factor = reported_bitrate / bitrate_avg;
					// bitrate_diff = reported_bitrate - bitrate_avg;
					
					/* printf("Reported = %.3lf Mbits/sec\n", reported_bitrate / 1e06);
					printf("Avg = %.3lf Mbits/sec\n", bitrate_avg / 1e06);
					printf("Factor = %.3lf\n", bitrate_factor);
					printf("Diff = %.3lf Mbits/sec\n", bitrate_diff / 1e06);
					printf("\n"); */
					
					bitrate_count = 0;
					bitrate_sum = 0;
				}
				
				cmdc_buf[0] = CLIENT_CMDC_OK;
				cmdc_slave_respond(cmd_channel, cmdc_buf, 1);
			}
		}
		
		clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
		
		packets = send_burst(sockfd, dest_addr, dest_addrlen,
			buf, block_size, burst_size, &seq_number);
		
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts);
		
		#ifdef USE_STATIC_DELAY
			burst_time = static_tx_time * packets;
		#else
			burst_time = ts_diff_ns(start_ts, end_ts);
		#endif
		
		double cycle_time = wait_time + burst_time;
		ulong bytes_sent = (block_size + block_overhead) * packets;
		
		double bitrate = (double) bytes_sent * 1e09 * 8 / cycle_time;
		
		bitrate_sum += bitrate;
		bitrate_count++;
		
		bitrate = (bitrate + bitrate_diff) * bitrate_factor;
		
		if(wait_time == 0) wait_time = 10;
		wait_time = wait_time * bitrate / target_bitrate;
		
		if(burst_size > packets) {
			burst_size_cap = packets;
			burst_size = packets;
		}
		
		#ifdef DYNAMIC_BURST_SIZE
			if(wait_time < 1000 && seq_number > 8192 && burst_size < burst_size_cap)
				burst_size <<= 1;
		#endif
		
		wait_ns(wait_time);
	}
	
	free(buf);
}

// -----------------------------------------------------

void *client_run(void *params) {
	struct client_params *p;
	int sockfd;
	int block_overhead;
	
	p = (struct client_params *) params;
	
	if(p->server_addr->sa_family == AF_INET)
		block_overhead = MYPERF_BLOCK_OVERHEAD_IPv4;
	else if(p->server_addr->sa_family == AF_INET6)
		block_overhead = MYPERF_BLOCK_OVERHEAD_IPv6;
	else
		FERROR("Invalid address family");
	
	if(p->block_size < sizeof(struct myperf_header))
		FERROR("The block size cannot be less than %zu bytes",
			sizeof(struct myperf_header));
	
	sockfd = socket(p->server_addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd == -1) FERROR("Error @ socket()");
	
	// if(DEFINED(USE_ZEROCOPY)) {
	#ifdef USE_ZEROCOPY
		int r, sockopt = 1;
		r = setsockopt(sockfd, SOL_SOCKET, SO_ZEROCOPY, &sockopt, sizeof sockopt);
		if(r != 0) FERROR("Error @ setsockopt()");
	#endif
	// }
	
	generate_traffic(sockfd, p->server_addr, p->server_addrlen,
		p->block_size, block_overhead, p->target_bitrate, p->cmd_channel);
	
	close(sockfd);
	
	return 0;
}

void *client_measure_oneway(void *params) {
	struct client_params *p;
	int sockfd;
	ssize_t r;
	
	p = (struct client_params *) params;
	
	if(p->block_size < sizeof(struct myperf_header))
		FERROR("The block size cannot be less than %zu bytes",
			sizeof(struct myperf_header));
	
	sockfd = socket(p->server_addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd == -1) FERROR("Error @ socket()");
	
	struct timeval tv = (struct timeval) {.tv_sec = 2, .tv_usec = 0};
	r = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	if(r < 0) FERROR("Error @ setsockopt()");
	
	uint8_t *buf = malloc(p->block_size);
	if(!buf) FERROR("Error @ malloc()");
	
	buf[0] = MYPERF_OPCODE_RTT;
	
	struct timespec start, end;
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	
	r = sendto(sockfd, buf, p->block_size, 0, p->server_addr, p->server_addrlen);
	if(r != p->block_size) FERROR("Error @ sendto()");
	
	r = recvfrom(sockfd, buf, p->block_size, 0, NULL, NULL);
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &end);
	
	if(r == -1) {
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			printf("Timeout in %.3lf s\n", ts_diff_ms(start, end) / 1e03);
		else
			FERROR("Error @ recvfrom()");
	} else {
		printf("One way delay: %.3lf ms\n", ts_diff_ms(start, end) / 2);
	}
	
	close(sockfd);
	
	return 0;
}

// -----------------------------------------------------

static int send_burst(int sockfd, const struct sockaddr *dest_addr,
	socklen_t dest_addrlen, const void *message, size_t length,
		int burst_size, ulong *seq_number) {
	
	ssize_t r;
	int flags = 0;
	
	#ifdef USE_ZEROCOPY
		flags = MSG_ZEROCOPY;
	#endif
	
	int sent;
	
	for(sent = 0; sent < burst_size; sent++) {
		((struct myperf_header *) message)->seq_number = (*seq_number)++;
		
		r = sendto(sockfd, message, length, flags,
			dest_addr, dest_addrlen);
		
		if(r == -1) {
			#ifdef USE_ZEROCOPY
				if(errno == ENOBUFS)
					break;
			#endif
			
			FERROR("Error @ sendto()");
		}
	}
	
	#ifdef USE_ZEROCOPY
		r = zerocopy_wait_notification(sockfd, sent);
		if(r != 0) FERROR("Error @ wait_notification()");
	#endif
	
	return sent;
}

#ifdef USE_STATIC_DELAY

// Does changing SO_SNDBUF affect this process ??
static ulong find_tx_time_ns(int sockfd, const struct sockaddr *dest_addr,
		socklen_t dest_addrlen, void *buf, size_t buflen) {
	
	struct timespec start_ts, end_ts;
	ssize_t r;
	
	int sndbuf;
	socklen_t optsize;
	
	// Save SO_SNDBUF
	optsize = sizeof sndbuf;
	r = getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optsize);
	if(r != 0 || optsize != sizeof sndbuf) FERROR("Error @ setsockopt()");
	
	// Minimize SO_SNDBUF
	int sockopt = 0;
	r = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sockopt, sizeof sockopt);
	if(r != 0) FERROR("Error @ setsockopt()");
	
	MYPERF_SET_HEADER(buf, MYPERF_OPCODE_PRIMER, 0);
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
	
	for(int i = 0; i < 30; i++) {
		if(i == 20)
			clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
		
		r = sendto(sockfd, buf, buflen, 0,
			dest_addr, dest_addrlen);
		
		if(r != buflen)
			FERROR("Error @ sendto()");
	}
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts);
	
	// Revert SO_SNDBUF
	r = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof sndbuf);
	if(r != 0) FERROR("Error @ setsockopt()");
	
	return ts_diff_ns(start_ts, end_ts) / 10;
}

#ifdef USE_ZEROCOPY

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

#endif

#endif
