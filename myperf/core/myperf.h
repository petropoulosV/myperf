#ifndef MYPERF_H
#define MYPERF_H

#include <stdint.h>

#define MYPERF_BLOCK_SIZE_DEFAULT 1460

// UDP: 8, IPv4: 20, MAC: 14, Ethernet: 24
#define MYPERF_BLOCK_OVERHEAD_IPv4 (8 + 20 + 14 + 24)

// UDP: 8, IPv6: 40, MAC: 14, Ethernet: 24
#define MYPERF_BLOCK_OVERHEAD_IPv6 (8 + 40 + 14 + 24)

#define MYPERF_OPCODE_DATA 0
#define MYPERF_OPCODE_PRIMER 1
#define MYPERF_OPCODE_RTT 2

#define MYPERF_HEADER(op, seq) ((struct myperf_header) {(op), (seq)})

// Beware of buffer overflows
#define MYPERF_SET_HEADER(dest, op, seq) ({ \
	*(struct myperf_header *) (dest) = MYPERF_HEADER(op, seq); \
})

#define BUFLEN_TCP 256
#define BUFLEN_UDP 2048
#define BLOCK_TIME 100 /*milliseconds to block*/
#define INTERFACE "enp5s0"

typedef struct udp_header{
	unsigned int seq_number;
}udp_header;

typedef struct tcp_header{
	unsigned int udp_pcktsize;
	unsigned short  int parallel_streams;
	unsigned short  int mode;
	unsigned short  int start;
}tcp_header;




struct myperf_stats {
	ulong throughput;
	ulong goodput;
	double packet_loss;
	
	double jitter;
	double jitter_stdev;
};

struct myperf_header {
	uint8_t opcode;
	ulong seq_number;
};

int start_receiver(char* ip, char *port);
int start_sender(char* ip, char * port, int udp_pcktsize, int bandwidth,
			int p_streams, double duration, int m_mode);

int find_mtu(char * interface);
void print_stats(struct myperf_stats *stats);
void print_bitrate(ulong bitrate);
void add_stats(struct myperf_stats *stats_sum, struct myperf_stats *stats, int n);

#endif
