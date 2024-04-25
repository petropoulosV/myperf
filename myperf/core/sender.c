#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
 #include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <linux/if_link.h>

#include <time.h>

#include "myperf.h"
#include "client.h"
#include "util.h"



int start_sender(char* ip, char * port, int udp_pcktsize, int bandwidth,
					int p_streams, double duration, int m_mode){

	struct addrinfo hints, *udp_addr, *tcp_addr;		/*hint for getaddrinfo */
	int tcp_client_socket;
	unsigned char buffer_tcp[BUFLEN_TCP];
	int bytes_send, s, status, i, udp_port_int, mtu, block_size;
	struct timespec start, current_time; 			/*need to calculate total duration*/
	double total_time , round;					 			/*need to calculate total duration*/
	tcp_header header_tcp;

	struct client_params *params;
	pthread_t *client_thread;
	struct cmdc *cmd_channel;
	ulong bitrate;
	ulong bitrate_total;
	ulong bitrate_sum;

	char udp_port_str[10];
	
	/*init*/
	udp_port_int = atoi(port);
	round = 1;
	bitrate_sum = 0;
	bitrate_total = 0;

	if(duration == 0){
		duration = 10;
	}

	block_size = udp_pcktsize;


	/*set hint for getaddrinfo for tcp socket*/
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = 0;
	

	if ((s = getaddrinfo(ip, port, &hints, &tcp_addr)) != 0 ) {
		perror("ERROR on getaddrinfo(tcp): ");
		printf("%s\n",gai_strerror(s) );
		exit(1);
	}

	if( ( tcp_client_socket = socket(tcp_addr->ai_family, tcp_addr->ai_socktype,
								tcp_addr->ai_protocol) ) < 0 ) {
		perror("ERROR on socket(tcp): ");
		exit(1);
	}


	if (connect(tcp_client_socket, (struct sockaddr *) tcp_addr->ai_addr,
					 tcp_addr->ai_addrlen) < 0) {
		
		perror("ERROR on connect: ");
		exit(1);
	}

	/*mtu = find_mtu((struct sockaddr *)tcp_addr->ai_addr, tcp_addr->ai_addrlen);*/
	mtu = find_mtu(INTERFACE);
	if(mtu != 0 && block_size > mtu){
		block_size = mtu;
	}

	printf("Connecting to host %s, port %s\n", ip, port);

	/*tcp chanel send info from client tcp_pctsize and mode*/

	/*fill the tcp header*/
	memset(buffer_tcp, 0, BUFLEN_TCP);
	header_tcp.udp_pcktsize = htonl(block_size);
	header_tcp.parallel_streams = htons(p_streams);
	header_tcp.mode = htons(m_mode);
	header_tcp.start = htons(1);							/*start signal*/
	memcpy(buffer_tcp, &header_tcp, sizeof(header_tcp));

	bytes_send = 0;
	while(bytes_send < BUFLEN_TCP){
		if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
			perror("ERROR on send: sent  different number of bytes than expected");
			exit(1);
		}
	}
/*
	sleep(1);*/
	if(m_mode == 0){

		/*start cliend threads*/

		client_thread =  malloc(p_streams* sizeof(pthread_t));
		if(client_thread == NULL){
			perror("Malloc error: ");
			exit(1);
		}

		cmd_channel =  malloc(p_streams* sizeof(struct cmdc));
		if(cmd_channel == NULL){
			perror("Malloc error: ");
			exit(1);
		}

		for(i = 0; i < p_streams; i++ ){

			/*set hint for getaddrinfo for udp socket*/
			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
			hints.ai_socktype = SOCK_DGRAM; 
			hints.ai_protocol = IPPROTO_UDP;
			hints.ai_flags = 0;
			
			sprintf(udp_port_str,"%d",udp_port_int++);
			if ((s = getaddrinfo(ip, udp_port_str, &hints, &udp_addr)) != 0 ) {
				perror("ERROR on getaddrinfo(udp): ");
				printf("%s\n",gai_strerror(s) );
				exit(1);
			}

			params = (struct client_params*) malloc( sizeof(struct client_params) );
			if(params == NULL){
				perror("Malloc error: ");
				exit(1);
			}

			cmd_channel[i] =  CMDC_INITIALIZER;

			params->server_addr = udp_addr->ai_addr;
			params->server_addrlen = udp_addr->ai_addrlen;
			params->target_bitrate = bandwidth;
			params-> block_size = block_size;
			params->cmd_channel = &cmd_channel[i];

			status = pthread_create(&client_thread[i], NULL, client_run, params);
			if(status != 0){
				perror("client thread creation error: ");
				exit(EXIT_FAILURE);
			}
		}

		/*wait for some time*/
		ulong *bitrate_total_per_thread ;
		bitrate_total_per_thread = (ulong*)malloc(p_streams* sizeof(ulong));
		if(bitrate_total_per_thread == NULL){
			perror("Malloc error: ");
			exit(1);
		}
		memset(bitrate_total_per_thread, 0, p_streams* sizeof(ulong));
		bitrate = 12356456;
		clock_gettime(CLOCK_MONOTONIC_RAW, &start);	/*get start time*/

		while( (total_time  <= duration) || !duration ){

			clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);	/*calculate total time*/
			total_time = current_time.tv_sec - start.tv_sec;
			total_time += (current_time.tv_nsec - start.tv_nsec) / 1000000000.0;
			bitrate = 12356456;
			if(round < total_time){
				printf("- - - - - - - - - - - - - - - - - - - - - - - - - \n"
						"Thread   Throughput       Time: %.1fsec\n",total_time);
			
				for(i = 0; i < p_streams; i++ ){
					/*client_cmdc_report_bitrate(&cmd_channel[i], bitrate);*/
					printf("%3d    ",(i+1));
					print_bitrate(bitrate);
					bitrate_sum += bitrate;
					bitrate_total_per_thread[i] += bitrate;
				}
				if(p_streams > 1){
					printf("sum    ");
					print_bitrate(bitrate_sum);
				}	
				bitrate_total += bitrate_sum;
				bitrate_sum = 0;
				
				round ++;
			}

		}



		/*Waiting for threads to finish*/
		printf("- - - - - - - - - - - - - - - - - - - - - - - - - \n"
				"- - - - - - - - - - - - - - - - - - - - - - - - - \n"
						"Thread   Average Throughput\n");
		for(i = 0; i < p_streams; i++ ){
			printf("%3d    ",(i+1));
			print_bitrate(bitrate_total_per_thread[i]/duration);
			
			/*client_cmdc_stop(&cmd_channel[i]);*/
			pthread_join(client_thread[i], NULL);
			free(bitrate_total_per_thread);
		}
		if(p_streams > 1){
			printf("sum    ");
			print_bitrate(bitrate_total/duration);
		}


		free(client_thread);

	}else
	{  /*rtt measurment*/

		client_thread =  malloc(sizeof(pthread_t));
		if(client_thread == NULL){
			perror("Malloc error: ");
			exit(1);
		}

		cmd_channel =  malloc(sizeof(struct cmdc));
		if(cmd_channel == NULL){
			perror("Malloc error: ");
			exit(1);
		}



		/*set hint for getaddrinfo for udp socket*/
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_UNSPEC;	/* Allow IPv4 or IPv6 */
		hints.ai_socktype = SOCK_DGRAM; 
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = 0;
		
		sprintf(udp_port_str,"%d",udp_port_int++);
		if ((s = getaddrinfo(ip, udp_port_str, &hints, &udp_addr)) != 0 ) {
			perror("ERROR on getaddrinfo(udp): ");
			printf("%s\n",gai_strerror(s) );
			exit(1);
		}

		params = (struct client_params*) malloc( sizeof(struct client_params) );
		if(params == NULL){
			perror("Malloc error: ");
			exit(1);
		}

		cmd_channel[0] =  CMDC_INITIALIZER;

		params->server_addr = udp_addr->ai_addr;
		params->server_addrlen = udp_addr->ai_addrlen;
		params->target_bitrate = bandwidth;
		params-> block_size = block_size;
		params->cmd_channel = &cmd_channel[0];

		status = pthread_create(&client_thread[0], NULL, client_run, params);
		if(status != 0){
			perror("client thread creation error: ");
			exit(EXIT_FAILURE);
		}
				
	}


	/*tcp chanel send results from client and fin msg*/

	/*fill the tcp header*/
	memset(buffer_tcp, 0, BUFLEN_TCP);
	header_tcp.udp_pcktsize = htonl(udp_pcktsize);
	header_tcp.parallel_streams = htons(p_streams);
	header_tcp.mode = htons(m_mode);
	header_tcp.start = htons(0);							/*termination signal*/
	memcpy(buffer_tcp, &header_tcp, sizeof(header_tcp));

	bytes_send = 0;
	while(bytes_send < BUFLEN_TCP){
		if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
			perror("ERROR on send: sent  different number of bytes than expected");
			exit(1);
		}
	}

	/*receive data*/

	int bytes_rcvd = 0;
	while(bytes_rcvd < BUFLEN_TCP){
		if( (bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0)) < 0){
			perror("ERROR on msg recv: ");
			exit(1);
		}
	}

	struct myperf_stats * stats_p, stats;
	stats_p = (struct myperf_stats *) buffer_tcp;
	stats.throughput = ntohl(stats_p->throughput); 
	stats.goodput = ntohl(stats_p->goodput);
	stats.packet_loss = ntohl(stats_p->packet_loss);
	stats.jitter = ntohl(stats_p->jitter);
	stats.jitter_stdev = ntohl(stats_p->jitter_stdev);
	printf("Server Stats"
			"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n"
			"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n"
			"Thread   Throughtput\t   Goodput\t   Jilter    Jilter stdev  Packet loss\n");
	print_stats(&stats);


	/*print results*/

	close(tcp_client_socket);
		
	return 0;
}



int find_mtu(char * interface){
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	struct ifreq ifr;

	strcpy(ifr.ifr_name, interface);
	if(ioctl(sock, SIOCGIFMTU, &ifr) != -1) {
		return ifr.ifr_mtu;;
	}else{
		return 0;
	}
	
}

void print_bitrate(ulong bitrate){
	if(bitrate >= 1000000000){
		printf("  %6.2lf Gbits/sec\n", bitrate/1000000000.0);
	}
	else if(bitrate >= 1000000){
		printf("  %6.2lf Mbits/sec\n", bitrate/1000000.0);
	}
	else if(bitrate >= 1000){
		printf("  %6.2lf Kbits/sec\n", bitrate/1000.0);
	}
	else{
		printf("  %6ld bits/sec\n", bitrate);
	}
}


/*int find_mtu(struct sockaddr *addr, socklen_t addr_len){
	struct ifreq ifr;
	struct ifaddrs *ifaddr, *ifa;
	int i_family, host_family, s, n, mtu;
	char intrface[NI_MAXHOST], host[NI_MAXHOST];
	int sock ;

	if( ( sock = socket(addr->sa_family, SOCK_DGRAM, 0) ) < 0 ) {
		perror("ERROR on socket(tcp): ");
		exit(1);
	}

	if (getifaddrs(&ifaddr) == -1) {
	   perror("getifaddrs");
	   exit(EXIT_FAILURE);
	}

	host_family = addr->sa_family;
	if((s = getnameinfo(addr, addr_len, host, NI_MAXHOST,NULL, 0, NI_NUMERICHOST) )!= 0 ) {
		perror("ERROR on getaddrinfo: ");
		printf("%s\n",gai_strerror(s) );
		exit(1);
	}

	printf("\t\taddress: <%s>\n", host);

	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
		if (ifa->ifa_addr == NULL)
			continue;

		i_family = ifa->ifa_addr->sa_family;

		if (i_family == host_family){
			
			if((s = getnameinfo(ifa->ifa_addr, addr_len, intrface,
		 				NI_MAXHOST,NULL, 0, NI_NUMERICHOST) )!= 0 ) {
						perror("ERROR on getaddrinfo: ");
						printf("%s\n",gai_strerror(s) );
						exit(1);
			}
			
			if(strcmp(intrface,host))
				continue;

			printf("%s \n", ifa->ifa_name);
			printf("\t\taddress: <%s>\n", intrface);
			strcpy(ifr.ifr_name, ifa->ifa_name);


			if(!ioctl(sock, SIOCGIFMTU, &ifr)) {
				printf("mtu->%d\n", ifr.ifr_mtu);
				mtu = ifr.ifr_mtu;
				freeifaddrs(ifaddr);
				return mtu;
			}else{
				perror("ERROR on ioctl: ");
				exit(1);
			}
		}

	}

	freeifaddrs(ifaddr);
	return 0;
}*/


