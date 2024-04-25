#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <arpa/inet.h>

#include "myperf.h"
#include "util.h"

// To be used for opcodes when issuing commands
#define SERVER_CMDC_OK 0
#define SERVER_CMDC_STOP 1
#define SERVER_CMDC_STATS_RESET 2
#define SERVER_CMDC_STATS_READ 3
#define SERVER_CMDC_STATS_READ_AVG 4

/* When this flag is set, the server won't
 * copy the message from the kernel's buffer. */
#define IGNORE_CONTENTS

/* When this flag is set, the server won't
 * check the source address of the packets. */
#define IGNORE_ORIGIN

struct server_params {
	struct sockaddr *bind_addr;
	socklen_t bind_addrlen;
	
	size_t block_size;
	
	struct cmdc *cmd_channel;
};

struct server_command {
	uint8_t opcode;
};

void *server_run(void *params);

// Stops the server. Returns 1 on success.
int server_cmdc_stop(struct cmdc *channel);

// Reads the "temporary" stats and resets them. Returns 1 on success.
int server_cmdc_stats_read(struct cmdc *channel, struct myperf_stats *stats);

// Reads the "all-time average" stats. Returns 1 on success.
int server_cmdc_stats_read_average(struct cmdc *channel, struct myperf_stats *stats);

/* Resets all types of stats. Keep in mind the stats are also automatically
 * reset for each new traffic measurement. Returns 1 on success. */
int server_cmdc_stats_reset(struct cmdc *channel);

#endif
