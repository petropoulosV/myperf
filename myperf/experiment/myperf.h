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

#endif
