#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "myperf.h"

#define DEFAULT_PORT "6000"


void usage(void){
	printf(
			"\n"
			"Usage:\n"
			"    myperf [-s|-c] [options]\n"
			"    myperf -h\n"
	);
	printf(
	 		"\n"
	 		"Options\n\n"
	 		"Server or Client\n"
	 		"  -a    <ip>  		server ip to bind\n"
	 		"  -p     #  		server port to connect\n"
	 		"Server only\n"
	 		"  -s        		run as server\n"
	 		"client only\n"
	 		"  -c          		run as client\n"
	 		"  -l     #[kmg]  	UDP packet size in bytes\n"
	 		"  -b     #[kmg]  	bandwidth in bits/sec\n"
	 		"  -n     #  		number of parallel data streams\n"
	 		"  -t     #  		duration in seconds (else infinite)\n"
	 		"  -d     #  		measure  only delay\n"

	 		"  -h          		show help message\n" 
	);
	exit(EXIT_FAILURE);
}


void check_args(char *ip, char * port, int sc_mode, unsigned int bandwidth, 
			int streams , double duration, unsigned int pack_size, int flag){
	
	int err;

	err = 0;
	if (sc_mode == -1 ) {
		printf("myperf: error select -s for server or -c for client\n");
		err = 1;
	}else if(sc_mode == 1){
		if(flag){
			printf("myperf: error use some client only options\n");
			err = 1;
		}
	}else{
		if(!ip){
			printf("myperf: error provide some valid ip\n");
			err = 1;
		}

		if(streams < 1 || duration < 0 || pack_size < 1 || bandwidth < 1){
			printf("myperf: error provide some valid parametrs\n");
			err = 1;
		}
	}
	
	if (err){
		usage();
	}
}

unsigned int convert_rate_bits(char * bandwidth){
	unsigned int b;
	char c ='\0';

	if(bandwidth[0] == '-')
		return 0;

	sscanf(bandwidth, "%u%c", &b, &c);

	switch	(c){
	case 'g': case 'G':
		b *= 1000000000;
		break;
	case 'm': case 'M':
		b *= 1000000;
		break;
	case 'k': case 'K':
		b *= 1000;
		break;
	default:
		break;
	}

	return b;

}

unsigned int convert_rate_bytes(char * bytes){
	unsigned int b;
	char c ='\0';

	if(bytes[0] == '-')
		return 0;

	sscanf(bytes, "%u%c", &b, &c);

	switch	(c){
	case 'g': case 'G':
		b *= 1024 * 1024 * 1024;
		break;
	case 'm': case 'M':
		b *= 1024 * 1024;
		break;
	case 'k': case 'K':
		b *= 1024;
		break;
	default:
		b = b;
		break;
	}

	return b;

}

int main(int argc, char *argv[]){

	
	char *srv_ip;			/* server IP */
	int udp_pcktsize;		/*UDP packet size in bytes*/
	unsigned int bandwidth;			/*bandwidth in bits/sec*/
	unsigned int p_streams;			/*number of parallel data streams*/	
	double duration;			/*experiment duration in seconds*/
	int sc_mode;			/*1 for server 0 for client*/
	int m_mode;				/*1 for delay 0 for all*/
	int c_flag;				/*flag for client only options*/
	char *port;				/* server port*/
	int opt;


	/* init*/
	sc_mode = -1;
	m_mode = 0;
	c_flag = 0;
	srv_ip = NULL;
	udp_pcktsize = MYPERF_BLOCK_SIZE_DEFAULT;
	bandwidth = 10000000;
	port = DEFAULT_PORT;
	duration = 0.0;
	p_streams = 1;



	/* get options */
	while ((opt = getopt(argc, argv, "a:p:l:b:n:t:scdh")) != -1) {
		switch (opt) {
		case 'a':
			srv_ip = strdup(optarg);
			break;
		case 'p':
			port = strdup(optarg);
			break;
		case 'l':
			udp_pcktsize = convert_rate_bytes(optarg);
			c_flag = 1;
			break;
		case 'b':
			bandwidth = convert_rate_bits(optarg);
			c_flag = 1;
			break;
		case 'n':
			p_streams = atoi(optarg);
			c_flag = 1;
			break;
		case 't':
			duration = atof(optarg);
			c_flag = 1;
			break;
		case 's':
			sc_mode = 1;
			break;
		case 'c':
			sc_mode = 0;
			c_flag = 1;
			break;
		case 'd':
			m_mode = 1;
			c_flag = 1;
			break;	
		case 'h':
		default:
			usage();
		}
	}

	check_args(srv_ip, port, sc_mode, bandwidth, p_streams,
				 duration, udp_pcktsize, c_flag);


	if(sc_mode == 1){
		start_receiver(srv_ip, port);
	}else{
		start_sender(srv_ip, port, udp_pcktsize, bandwidth,
					p_streams, duration, m_mode);
	}

	return 0;
}