#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <arpa/inet.h>

#include "util.h"

// To be used for opcodes when issuing commands
#define CLIENT_CMDC_OK 0
#define CLIENT_CMDC_STOP 1
#define CLIENT_CMDC_REPORT_BITRATE 2

/* USE_ZEROCOPY may provide more accurate measurements by avoiding
 * buffering in the kernel (and in the network card ?). Spoiler alert:
 * No easily-measurable advantage. Linux 5.0 is required. */

// #define USE_ZEROCOPY
// #define USE_STATIC_DELAY

#define DYNAMIC_BURST_SIZE

struct client_params {
	struct sockaddr *server_addr;
	socklen_t server_addrlen;
	
	size_t block_size;
	ulong target_bitrate;
	
	struct cmdc *cmd_channel;
};

struct client_command {
	uint8_t opcode;
};

void *client_run(void *params);
void *client_measure_oneway(void *params);

// Stops the client. Returns 1 on success.
int client_cmdc_stop(struct cmdc *channel);

/* Informs the client of the reported bitrate so that he may
 * adjust his transmission parameters. Returns 1 on success. */
int client_cmdc_report_bitrate(struct cmdc *channel, ulong bitrate);

#endif
