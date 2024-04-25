#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "client.h"
#include "util.h"

// #define SERVER_IP "127.0.0.1"
// #define SERVER_IP "192.168.1.245"
#define SERVER_IP "192.168.8.11"
#define SERVER_PORT 6070

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
	
	struct client_params params;
	
	struct sockaddr_in server_addr = (struct sockaddr_in) {
		.sin_family = AF_INET,
		.sin_port = htons(SERVER_PORT)
	};
	
	int r = inet_aton(SERVER_IP, &server_addr.sin_addr);
	if(r == 0) FERROR("Error @ inet_aton()\n");
	
	params = (struct client_params) {
		.server_addr = (void *) &server_addr,
		.server_addrlen = sizeof(server_addr),
		.block_size = 1460,
		.target_bitrate = 80e06,
		.cmd_channel = &cmd_channel
	};
	
	// pthread_create(&thread_id, NULL, client_measure_oneway, &params);
	// pthread_join(thread_id, NULL);
	// return 0;
	
	pthread_create(&thread_id, NULL, client_run, &params);
	
	sleep(5);
	
	client_cmdc_stop(&cmd_channel);
	pthread_join(thread_id, NULL);
	
	return 0;
}
