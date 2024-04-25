#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "server.h"
#include "util.h"

#define SERVER_PORT 6070


static void myperf_stats_show(struct myperf_stats *perf_stats) {
	printf("Throughput: %.3lf Mbits/sec\n", perf_stats->throughput / 1e06);
	// printf("Throughput: %.3lf Gbits/sec\n", perf_stats->throughput / 1e09);
	// printf("Goodput: %.3lf Mbits/sec\n", perf_stats->goodput / 1e06);
	// printf("Packet loss: %.3lf %%\n", perf_stats->packet_loss * 100);
	// printf("\n");
	// printf("Jitter: %.3lf us\n", perf_stats->jitter * 1e06);
	// printf("Jitter stdev: %.3lf us\n", perf_stats->jitter_stdev * 1e06);
	// printf("\n");
}

/* void *slave(void *param) {
	struct cmdc *channel = (struct cmdc *) param;
	int count;
	
	while(1) {
		if(cmdc_slave_read(channel, &count, sizeof count) == 4) {
			count++;
			cmdc_slave_respond(channel, &count, sizeof count);
		}
	}
} */

int main(int argc, char **argv) {
	struct cmdc cmd_channel = CMDC_INITIALIZER;
	pthread_t thread_id;
	
	/* pthread_create(&thread_id, NULL, slave, (void *) &cmd_channel);
	
	for(int i = 0; i < 1000; i++) {
		int count = i;
		
		cmdc_master_write(&cmd_channel, &count, sizeof count);
		
		printf("%d --> %d\n", i, count);
		assert(count == i + 1);
	} */
	
	struct server_params params;
	
	struct sockaddr_in bind_addr = (struct sockaddr_in) {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(SERVER_PORT)
	};
	
	params = (struct server_params) {
		.bind_addr = (void *) &bind_addr,
		.bind_addrlen = sizeof(bind_addr),
		.block_size = 1460,
		.cmd_channel = &cmd_channel
	};
	
	pthread_create(&thread_id, NULL, server_run, &params);
	
	while(1) {
		struct myperf_stats stats;
		
		// sleep_ms(1000);
		sleep_ms(100);
		// sleep_ms(1);
		// wait_ns(100000);
		
		server_cmdc_stats_read(&cmd_channel, &stats);
		
		if(stats.throughput != 0)
			myperf_stats_show(&stats);
	}
	
	server_cmdc_stop(&cmd_channel);
	pthread_join(thread_id, NULL);
	
	return 0;
}
