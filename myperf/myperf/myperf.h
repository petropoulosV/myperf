#ifndef MYPERF_H
#define MYPERF_H

#include <stdint.h>
#include <arpa/inet.h>

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

/*get_addr options*/
#define TCP_SERVER 0
#define TCP_CLIENT 1
#define UDP_SERVER 2
#define UDP_CLIENT 3

/*type of msg tcp_header*/
#define MSG_INVALID 0
#define MSG_CONNECTION 1
#define MSG_FEEDBACK 2
#define MSG_STATS 3

#define MODE_THR 0
#define MODE_RTT 1


#define SIZEOF_TCP_HEADER 20

typedef struct tcp_header{
	unsigned short  int type;     	/*type of msg*/

	unsigned short int mode;		/*rtt or all*/
	unsigned short  int start;
	unsigned int udp_pcktsize;
	unsigned short int parallel_streams;

	unsigned int bitrate;
	int thread_no;

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


/*start serler */
int start_receiver(char* ip, char *port);
/*start client */
int start_sender(char* ip, char * port, unsigned int udp_pcktsize, unsigned int bandwidth,
			int p_streams, double duration, int m_mode);

/*write the tcp msg data and header*/
int write_msg(unsigned char *msg_buffer, unsigned char *data_buffer, unsigned short int type, 
				unsigned short int mode, unsigned short int start, unsigned int udp_pcktsize, 
					unsigned short int parallel_streams, unsigned int bitrate, int thread_no);

/*read the tcp msg data and header*/
int read_msg(unsigned char *msg_buffer,unsigned char *data_buffer, tcp_header *header_tcp);

int find_mtu(char * interface);

/*print myperf stats*/
void print_stats(struct myperf_stats *stats);

/*print myperf stats in a string*/
void string_print_stats(struct myperf_stats *stats,char *buf);

/*prind bitrate*/
void print_bitrate(ulong bitrate);

/*add stats to stat_sum*/
void add_stats(struct myperf_stats *stats_sum, struct myperf_stats *stats, int n);

void get_addr(int option, char *ip, char *port, 
			struct sockaddr **server_addr, socklen_t *server_addrlen);

#endif
