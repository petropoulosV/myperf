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

int client_cmdc_stop(struct cmdc *channel) {
	struct client_command command;
	
	command.opcode = CLIENT_CMDC_STOP;
	cmdc_master_write(channel, &command, sizeof command);
	
	return (command.opcode == CLIENT_CMDC_OK);
}

int client_cmdc_report_bitrate(struct cmdc *channel, ulong bitrate) {
	uint8_t buf[1 + sizeof bitrate];
	
	buf[0] = CLIENT_CMDC_STOP;
	memcpy(buf + 1, &bitrate, sizeof bitrate);
	
	cmdc_master_write(channel, buf, sizeof buf);
	
	return (buf[0] == CLIENT_CMDC_OK);
}

static void send_burst(int sockfd, const struct sockaddr *dest_addr,
	socklen_t dest_addrlen, const void *message, size_t length,
		int burst_size, ulong *seq_number) {
	
	ssize_t r;
	
	for(int i = 0; i < burst_size; i++) {
		((struct myperf_header *) message)->seq_number = (*seq_number)++;
		
		#ifdef USE_ZEROCOPY
			int acked = 0
			
			// Zerocopy TODO
			// Check return values when msg queue is full
			// Do not increase burst size above msg queue size?
			
			#error zero copy is 95% implemented (5% short of being)
			
			r = sendto(sockfd, message, length, MSG_ZEROCOPY,
				dest_addr, dest_addrlen);
			
			if(r == -1 && errno == ) {
				if(i == 0)
					FERROR("Error @ sendto()");
				
				r = zerocopy_wait_notification(sockfd, i - acked);
				if(r != 0) FERROR("Error @ wait_notification()");
				
				acked = i;
				i--;
			}
		
			r = zerocopy_wait_notification(sockfd, burst_size - acked);
			if(r != 0) FERROR("Error @ wait_notification()");
		#else
			r = sendto(sockfd, message, length, 0, dest_addr, dest_addrlen);
			if(r != length) FERROR("Error @ sendto()");
		#endif
	}
}

static void generate_traffic(int sockfd, const struct sockaddr *dest_addr,
	socklen_t dest_addrlen, size_t block_size, int block_overhead,
		ulong target_bitrate, struct cmdc *cmd_channel) {
	
	void *buf = malloc(block_size);
	if(!buf) FERROR("Error @ malloc()");
	
	ulong seq_number = 0;
	
	MYPERF_SET_HEADER(buf, MYPERF_OPCODE_PRIMER, 0);
	send_burst(sockfd, dest_addr, dest_addrlen,
		buf, sizeof(struct myperf_header), 1, &seq_number);
	
	MYPERF_SET_HEADER(buf, MYPERF_OPCODE_DATA, seq_number);
	
	double bitrate_factor = 1;
	double bitrate_sum = 0;
	ulong bitrate_count = 0;
	
	double wait_time = 0;
	// ulong burst_size = 1;
	ulong burst_size = 100;
	
	while(1) {
		struct timespec start_ts, end_ts;
		uint8_t cmdc_buf[1 + sizeof(ulong)];
		
		if(cmdc_slave_read(cmd_channel, cmdc_buf, sizeof cmdc_buf)) {
			if(cmdc_buf[0] == CLIENT_CMDC_STOP) {
				cmdc_buf[0] = CLIENT_CMDC_OK;
				cmdc_slave_respond(cmd_channel, cmdc_buf, 1);
				
				break;
			}
			
			else if(cmdc_buf[0] == CLIENT_CMDC_REPORT_BITRATE) {
				ulong reported_bitrate = * (ulong *) ((void *) cmdc_buf + 1);
				double bitrate_avg = bitrate_sum / bitrate_count;
				
				bitrate_factor = reported_bitrate / bitrate_avg;
				
				bitrate_sum = 0;
				bitrate_count = 0;
				
				cmdc_buf[0] = CLIENT_CMDC_OK;
				cmdc_slave_respond(cmd_channel, cmdc_buf, 1);
			}
		}
		
		clock_gettime(CLOCK_MONOTONIC_RAW, &start_ts);
		
		send_burst(sockfd, dest_addr, dest_addrlen,
			buf, block_size, burst_size, &seq_number);
		
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_ts);
		
		ulong burst_time = ts_diff_ns(start_ts, end_ts);
		double cycle_time = wait_time + burst_time;
		ulong bytes_sent = (block_size + block_overhead) * burst_size;
		
		double bitrate = (double) bytes_sent * 1e09 * 8 / cycle_time;
		bitrate = bitrate * bitrate_factor;
		
		bitrate_sum += bitrate;
		bitrate_count++;
		
		if(wait_time == 0) wait_time = 10;
		wait_time = wait_time * bitrate / target_bitrate;
		
		#ifdef DYNAMIC_BURST_SIZE
			if(wait_time < 1000 && seq_number > 8192 && burst_size < 8192)
				burst_size <<= 1;
		#endif
		
		wait_ns(wait_time);
	}
	
	free(buf);
}

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
	
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd == -1) FERROR("Error @ socket()");
	
	#if defined(USE_ZEROCOPY)
		int sockopt = 1;
		int r = setsockopt(sockfd, SOL_SOCKET, SO_ZEROCOPY, &sockopt, sizeof sockopt);
		if(r != 0) FERROR("Error @ setsockopt()");
	#elif defined(SNDBUF_SIZE)
		int sockopt = 0;
		int r = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sockopt, sizeof sockopt);
		if(r != 0) FERROR("Error @ setsockopt()");
	#endif
	
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
	
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
