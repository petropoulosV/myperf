#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "myperf.h"
#include "server.h"
#include "util.h"

struct server_stats {
	ulong packets;
	ulong packets_lost;
	ulong packets_duplicate;
	ulong packets_out_of_order;
	
	ulong last_seq;
	int has_last_seq;
	
	ulong bytes;
	ulong bytes_bnf;
	
	ulong jitter_sum;
	ulong jitter_sqsum;
	
	struct timespec start_ts;
	struct timespec last_ts;
};

void stats_generate(struct server_stats *s, struct myperf_stats *r) {
	ulong time_ns = ts_diff_ns(s->start_ts, s->last_ts);
	
	if(s->packets == 0) {
		*r = (struct myperf_stats) {};
		return;
	}
	
	r->throughput = (double) s->bytes * 1e09 * 8 / time_ns;
	r->goodput = (double) s->bytes_bnf * 1e09 * 8 / time_ns;
	
	r->packet_loss = (double) s->packets_lost / (s->packets + s->packets_lost);
	
	r->jitter = (double) s->jitter_sum / s->packets / 1e09;
	r->jitter_stdev = stdev(s->packets, s->jitter_sum, s->jitter_sqsum) / 1e09;
}

void stats_reset(struct server_stats *stats) {
	struct timespec ts;
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	
	*stats = (struct server_stats) {
		.start_ts = ts,
		.last_ts = ts
	};
}

void stats_new_packet(struct server_stats *stats, struct timespec ts,
		size_t bytes, int block_overhead, ulong seq_number) {
	
	// First packet in measurement
	if(!stats->has_last_seq) {
		stats->last_seq = seq_number - 1;
		stats->has_last_seq = 1;
	}
	
	// In-order packet
	if(seq_number == stats->last_seq + 1) {
		ulong jitter_ns = ts_diff_ns(stats->last_ts, ts);
		
		stats->jitter_sum += jitter_ns;
		stats->jitter_sqsum += jitter_ns*jitter_ns;
	}
	
	// Packet loss
	if(seq_number > stats->last_seq + 1)
		stats->packets_lost += seq_number - stats->last_seq;
	
	// Out-of-order packet
	if(seq_number < stats->last_seq) {
		stats->packets_out_of_order++;
		stats->packets_lost--;
	}
	
	// Duplicate packet
	if(seq_number == stats->last_seq)
		stats->packets_duplicate++;
	
	stats->packets++;
	stats->bytes += bytes + block_overhead;
	stats->bytes_bnf += bytes;
	stats->last_ts = ts;
	
	stats->last_seq = seq_number;
}

int server_cmdc_stop(struct cmdc *channel) {
	struct server_command command =
		(struct server_command) {SERVER_CMDC_STOP};
	
	cmdc_master_write(channel, &command, sizeof command);
	
	return (command.opcode == SERVER_CMDC_OK);
}

int server_cmdc_stats_read(struct cmdc *channel, struct myperf_stats *stats) {
	uint8_t buf[1 + sizeof(struct myperf_stats)] = {SERVER_CMDC_STATS_READ};
	
	cmdc_master_write(channel, buf, sizeof buf);
	
	if(buf[0] == SERVER_CMDC_OK)
		*stats = * (struct myperf_stats *) ((void *) buf + 1);
	
	return (buf[0] == SERVER_CMDC_OK);
}

int server_cmdc_stats_read_average(struct cmdc *channel, struct myperf_stats *stats) {
	uint8_t buf[1 + sizeof(struct myperf_stats)] = {SERVER_CMDC_STATS_READ_AVG};
	
	cmdc_master_write(channel, buf, sizeof buf);
	
	if(buf[0] == SERVER_CMDC_OK)
		*stats = * (struct myperf_stats *) ((void *) buf + 1);
	
	return (buf[0] == SERVER_CMDC_OK);
}

int server_cmdc_stats_reset(struct cmdc *channel) {
	struct server_command command =
		(struct server_command) {SERVER_CMDC_STATS_RESET};
	
	cmdc_master_write(channel, &command, sizeof command);
	
	return (command.opcode == SERVER_CMDC_OK);
}

void receive_traffic(int sockfd, size_t block_size,
		int block_overhead, struct cmdc *cmd_channel) {
	
	size_t buflen;
	int flags;
	
	#ifdef IGNORE_CONTENTS
		buflen = sizeof(struct myperf_header);
		flags = MSG_TRUNC;
	#else
		buflen = block_size;
		flags = 0;
	#endif
	
	void *buf = malloc(buflen);
	if(!buf) FERROR("Error @ malloc()");
	
	struct myperf_header *header = (struct myperf_header *) buf;
	
	struct server_stats stats_average = (struct server_stats) {};
	struct server_stats stats = (struct server_stats) {};
	
	while(1) {
		struct sockaddr_in6 source_addr;
		socklen_t source_addrlen = sizeof source_addr;
		
		struct timespec ts;
		ssize_t r;
		
		uint8_t cmdc_buf[1 + sizeof(struct myperf_stats)];
		
		if(cmdc_slave_read(cmd_channel, cmdc_buf, sizeof cmdc_buf)) {
			if(cmdc_buf[0] == SERVER_CMDC_STOP) {
				cmdc_buf[0] = SERVER_CMDC_OK;
				cmdc_slave_respond(cmd_channel, cmdc_buf, 1);
				
				break;
			}
			
			else if(cmdc_buf[0] == SERVER_CMDC_STATS_READ) {
				stats_generate(&stats, (void *) cmdc_buf + 1);
				cmdc_buf[0] = SERVER_CMDC_OK;
				
				cmdc_slave_respond(cmd_channel, cmdc_buf, sizeof cmdc_buf);
				
				stats_reset(&stats);
			}
			
			else if(cmdc_buf[0] == SERVER_CMDC_STATS_READ_AVG) {
				stats_generate(&stats_average, (void *) cmdc_buf + 1);
				cmdc_buf[0] = SERVER_CMDC_OK;
				
				cmdc_slave_respond(cmd_channel, cmdc_buf, sizeof cmdc_buf);
			}
			
			else if(cmdc_buf[0] == SERVER_CMDC_STATS_RESET) {
				stats_reset(&stats_average);
				stats_reset(&stats);
				
				cmdc_buf[0] = SERVER_CMDC_OK;
				cmdc_slave_respond(cmd_channel, cmdc_buf, 1);
				
				break;
			}
		}
		
		r = recvfrom(sockfd, buf, buflen, flags,
			(void *) &source_addr, &source_addrlen);
		
		clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
		
		if(r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
			FERROR("Error @ recvfrom()");
		
		#ifndef IGNORE_ORIGIN
			#error Origin check not implemented
		#endif
		
		if(r == -1 || r < sizeof(struct myperf_header))
			continue;
		
		switch(header->opcode) {
			case MYPERF_OPCODE_PRIMER:
				stats_reset(&stats_average);
				stats_reset(&stats);
				
				break;
			
			case MYPERF_OPCODE_RTT:
				r = sendto(sockfd, buf, buflen, 0,
					(void *) &source_addr, source_addrlen);
				
				if(r != buflen)
					FERROR("Error @ sendto()");
				
				break;
			
			case MYPERF_OPCODE_DATA:
				stats_new_packet(&stats_average, ts, r, block_overhead, header->seq_number);
				stats_new_packet(&stats, ts, r, block_overhead, header->seq_number);
				
				break;
		}
	}
	
	free(buf);
}

void *server_run(void *params) {
	struct server_params *p;
	int block_overhead;
	int sockfd, r;
	
	p = (struct server_params *) params;
	
	if(p->bind_addr->sa_family == AF_INET)
		block_overhead = MYPERF_BLOCK_OVERHEAD_IPv4;
	else if(p->bind_addr->sa_family == AF_INET6)
		block_overhead = MYPERF_BLOCK_OVERHEAD_IPv6;
	else
		FERROR("Invalid address family");
	
	if(p->block_size < sizeof(struct myperf_header))
		FERROR("The block size cannot be less than %zu bytes",
			sizeof(struct myperf_header));
	
	sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockfd == -1) FERROR("Error @ socket()");
	
	struct timeval tv = (struct timeval) {.tv_sec = 0, .tv_usec = 1000};
	r = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
	if(r < 0) FERROR("Error @ setsockopt()");
	
	r = bind(sockfd, p->bind_addr, p->bind_addrlen);
	if(r == -1) FERROR("Error @ bind()");
	
	receive_traffic(sockfd, p->block_size,
		block_overhead, p->cmd_channel);
	
	close(sockfd);
	
	return 0;
}
