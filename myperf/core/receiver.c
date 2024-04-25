#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/poll.h>
#include <pthread.h>
#include <errno.h>

#include <time.h>

#include "myperf.h"
#include "server.h"
#include "util.h"




int start_receiver(char* ip, char *port){

	struct addrinfo hints, *udp_addr, *tcp_addr;				/*hint for getaddrinfo */
	int  tcp_srv_socket, tcp_client_socket;
	struct sockaddr_storage  tcp_client_addr;
	socklen_t tcp_client_address_len;
	
	unsigned char buffer_tcp[BUFLEN_TCP];
	int optval, bytes_rcvd, s, status, i, mode;
	int udp_pcktsize, p_streams, udp_port_int;
	char client_ip[NI_MAXHOST], client_port[NI_MAXSERV];
	struct timeval tv;
	struct timespec start, current_time; 			/*need to calculate total duration*/
	double total_time , round;
	
	tcp_header header_tcp, *header_tcp_ptr;
	char udp_port_str[10];

	struct server_params *params;
	pthread_t *server_thread;
	struct cmdc *cmd_channel;
	struct myperf_stats stats;



	/*init*/
	optval = 1;
	udp_port_int = atoi(port);
	tcp_client_address_len = sizeof(struct sockaddr_storage);
	round = 1;


	/*set hint for getaddrinfo for tcp socket*/
	memset(&hints, 0, sizeof(struct addrinfo) );

	hints.ai_socktype = SOCK_STREAM; 
	hints.ai_family = AF_UNSPEC;		/* IPv4 or IPv6 */
	hints.ai_flags = AI_PASSIVE;		/* INADDR_ANY INADDR6_ANY */
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_next = NULL;
	hints.ai_addr = NULL;
	hints.ai_canonname = NULL;

	
	if ((s = getaddrinfo(ip, port, &hints, &tcp_addr)) != 0 ) {
		perror("ERROR on getaddrinfo(tcp): ");
		printf("%s\n",gai_strerror(s) );
		exit(1);
	}

	if( ( tcp_srv_socket = socket(tcp_addr->ai_family, tcp_addr->ai_socktype,
								tcp_addr->ai_protocol) ) < 0 ) {
		perror("ERROR on socket(tcp): ");
		exit(1);
	}

	if (setsockopt(tcp_srv_socket,SOL_SOCKET,SO_REUSEADDR /*| SO_REUSEPORT*/, &optval, sizeof(int))) {
		perror("ERROR on socket(tcp): setsockopt() failed");
		exit(1);
	}

	if( bind(tcp_srv_socket, (struct sockaddr *) tcp_addr->ai_addr, tcp_addr->ai_addrlen ) < 0 ) {
		perror( "ERROR on binding(tcp): ");
		exit(1);
	}
	if( listen(tcp_srv_socket, optval) < 0 ) {
		perror( "ERROR on listen: ");
		exit(1);
	}



	while(1){

		printf("-----------------------------------------------------------\n");
		printf("Server listening on port %s\n", port);
		printf("-----------------------------------------------------------\n");

		if ((tcp_client_socket = accept(tcp_srv_socket, (struct sockaddr *)&tcp_client_addr,
										 &tcp_client_address_len)) < 0) {
			perror( "ERROR on accept: ");
			exit(1);
		}

		tv.tv_sec = 0;
		tv.tv_usec = 100;
		if (setsockopt(tcp_client_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv) ) < 0) {
    		perror("Error setsockopt: ");
		}

		if((s = getnameinfo((struct sockaddr *)&tcp_client_addr, tcp_client_address_len, client_ip,
		 				NI_MAXHOST,client_port, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV)) != 0 ) {
			perror("ERROR on getaddrinfo: ");
			printf("%s\n",gai_strerror(s) );
			exit(1);
		}
		printf("Accepted connection from %s, port %s \n",client_ip, client_port);


		

		/*tcp chanel recv info from client udp_pctsize and mode*/
		bytes_rcvd = 0;
		while(bytes_rcvd < BUFLEN_TCP){
			if( (bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0)) < 0){
				perror("ERROR on msg recv: ");
				exit(1);
			}
		}

		/*read header*/
		header_tcp_ptr = (tcp_header *)buffer_tcp;
		header_tcp.udp_pcktsize = ntohl(header_tcp_ptr->udp_pcktsize);
		header_tcp.parallel_streams = ntohs(header_tcp_ptr->parallel_streams);
		header_tcp.mode = ntohs(header_tcp_ptr->mode);
		header_tcp.start = ntohs(header_tcp_ptr->start);

		udp_pcktsize = header_tcp.udp_pcktsize;
		p_streams = header_tcp.parallel_streams;
		mode = header_tcp.mode;


		if(mode == 0){

			printf("Thread   Throughtput\t   Goodput\t   Jilter    Jilter stdev  Packet loss\n");

			server_thread =  malloc(p_streams* sizeof(pthread_t));
			if(server_thread == NULL){
				perror("Malloc error: ");
				exit(EXIT_FAILURE);
			}

			cmd_channel =  malloc(p_streams* sizeof(struct cmdc));
			if(cmd_channel == NULL){
				perror("Malloc error: ");
				exit(1);
			}


			for(i = 0; i < p_streams; i++ ){

				/*set hint for getaddrinfo for udp socket*/
				memset(&hints, 0, sizeof(struct addrinfo) );

				hints.ai_socktype = SOCK_DGRAM; 
				hints.ai_family = AF_UNSPEC;		/* IPv4 or IPv6 */
				hints.ai_flags = AI_PASSIVE;		/* INADDR_ANY INADDR6_ANY */
				hints.ai_protocol = IPPROTO_UDP;
				hints.ai_next = NULL;
				hints.ai_addr = NULL;
				hints.ai_canonname = NULL;

				sprintf(udp_port_str,"%d",udp_port_int++);
				if ((s = getaddrinfo(ip, udp_port_str, &hints, &udp_addr)) != 0 ) {
					perror("ERROR on getaddrinfo(udp): ");
					printf("%s\n",gai_strerror(s) );
					exit(1);
				}

				params = (struct server_params*) malloc( sizeof(struct server_params) );
				if(params == NULL){
					perror("Malloc error: ");
					exit(1);
				}

				cmd_channel[i] =  CMDC_INITIALIZER;

				params->bind_addr = udp_addr->ai_addr;
				params->bind_addrlen = udp_addr->ai_addrlen;
				params-> block_size = udp_pcktsize;
				params->cmd_channel = &cmd_channel[i];

				status = pthread_create(&server_thread[i], NULL, server_run, params);
				if(status != 0){
					perror("client thread creation error: ");
					exit(EXIT_FAILURE);
				}
				/*server_cmdc_stats_start(&cmd_channel[i]);*/


			}
			udp_port_int = atoi(port);

			round = 1;
			total_time = 0;
			clock_gettime(CLOCK_MONOTONIC_RAW, &start);	
			stats = (struct myperf_stats){
				.throughput = 23452354,
				.goodput = 23452300,
				.packet_loss = 0.4,
				.jitter = 0.00023,
				.jitter_stdev = 0.00067
			};
			struct myperf_stats  stats_sum =(struct myperf_stats){};
			while(1){
				clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);	
				total_time = current_time.tv_sec - start.tv_sec;
				total_time += (current_time.tv_nsec - start.tv_nsec) / 1000000000.0;

				if(round < total_time){
					printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");
				
					for(i = 0; i < p_streams; i++ ){
						/*server_cmdc_stats_read(&cmd_channel[i], &stats);*/
						printf("%2d     ",(i+1));
						print_stats(&stats);
						add_stats(&stats_sum, &stats ,p_streams);
					}
					if(p_streams > 1){
						printf("sum    ");
						print_stats(&stats_sum);
						stats_sum =(struct myperf_stats){};
					}

					round ++;
				}


				bytes_rcvd = 0;
				bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0);
				if( bytes_rcvd < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
					perror( "ERROR on msg recv: recvfrom() failed");
					exit(1);
				}	
				if(bytes_rcvd == -1){
						continue;
				}

				header_tcp_ptr = (tcp_header *)buffer_tcp;
				header_tcp.udp_pcktsize = ntohl(header_tcp_ptr->udp_pcktsize);
				header_tcp.parallel_streams = ntohs(header_tcp_ptr->parallel_streams);
				header_tcp.mode = ntohs(header_tcp_ptr->mode);
				header_tcp.start = ntohs(header_tcp_ptr->start);
				
				if(header_tcp.start == 0)
					break;
			}
			
			printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n"
					"- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -\n");

			/*waiting threads*/
			for(i = 0; i < p_streams; i++ ){
/*				server_cmdc_stats_read_average(&cmd_channel[i], &stats);
				server_cmdc_stop(&cmd_channel[i]);*/
				printf("%2d     ",(i+1));
				print_stats(&stats);
				add_stats(&stats_sum, &stats ,p_streams);
				pthread_join(server_thread[i], NULL);
			}	
			if(p_streams > 1){
				printf("sum    ");
				print_stats(&stats_sum);
				stats_sum =(struct myperf_stats){};
			}


			memset(buffer_tcp, 0, BUFLEN_TCP);
			
			stats.throughput = htonl(stats.throughput); 
			stats.goodput = htonl(stats.goodput);
			stats.packet_loss = htonl(stats.packet_loss);
			stats.jitter = htonl(stats.jitter);
			stats.jitter_stdev = htonl(stats.jitter_stdev);
			memcpy(buffer_tcp, &stats, sizeof(struct myperf_stats));


			int bytes_send = 0;
			while(bytes_send < BUFLEN_TCP){
				if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
					perror("ERROR on send: sent  different number of bytes than expected");
					exit(1);
				}
			}

			free(server_thread);
		}else{

			/*hear to do the rtt measure*/
		}


		/*tcp chanel recv info from client udp_pctsize and mode*/
	/*	bytes_rcvd = 0;
		while(bytes_rcvd < BUFLEN_TCP){
			if( (bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0)) < 0){
				perror("ERROR on msg recv: ");
				exit(1);
			}
		}
		
		header_tcp_ptr = (tcp_header *)buffer_tcp;
		header_tcp.udp_pcktsize = ntohl(header_tcp_ptr->udp_pcktsize);
		header_tcp.parallel_streams = ntohs(header_tcp_ptr->parallel_streams);
		header_tcp.mode = ntohs(header_tcp_ptr->mode);
		header_tcp.start = ntohs(header_tcp_ptr->start);*/


		close(tcp_client_socket);
		printf("Connection with %s at  port %s terminated\n\n",client_ip, client_port);

	}


	return 0;
}


void print_stats(struct myperf_stats *stats){


	if(stats->throughput >= 1000000000){
		printf("%6.2lf Gbits/sec  ", stats->throughput/1000000000.0);
	}
	else if(stats->throughput >= 1000000){
		printf("%6.2lf Mbits/sec  ", stats->throughput/1000000.0);
	}
	else if(stats->throughput >= 1000){
		printf("%6.2lf Kbits/sec  ", stats->throughput/1000.0);
	}
	else{
		printf("%6ld bits/sec   ", stats->throughput);
	}

	if(stats->goodput >= 1000000000){
		printf("%6.2lf Gbits/sec  ", stats->goodput/1000000000.0);
	}
	else if(stats->goodput >= 1000000){
		printf("%6.2lf Mbits/sec  ", stats->goodput/1000000.0);
	}
	else if(stats->goodput >= 1000){
		printf("%6.2lf Kbits/sec  ", stats->goodput/1000.0);
	}
	else{
		printf("%6ld bits/sec   ", stats->goodput);
	}

	printf("%6.3lf ms  ", stats->jitter * 1000 );
	printf("%6.3lf ms   ", stats->jitter_stdev * 1000 );
	printf("%6.2lf%%", stats->packet_loss * 100);
	printf("\n");

}

void add_stats(struct myperf_stats *stats_sum, struct myperf_stats *stats, int n){

	stats_sum->throughput += stats->throughput;
	stats_sum->goodput += stats->goodput;
	stats_sum->packet_loss += stats->packet_loss/n;
	stats_sum->jitter += stats->jitter/n;
	stats_sum->jitter_stdev += stats->jitter_stdev/n;

}

/*void *server_run( void *args){

	struct client_params *params =args;
	int udp_srv_socket;
	unsigned char buffer[BUFLEN_UDP];
	udp_header header_udp, *header_udp_ptr;
	unsigned int total_pack_rcvd = 0;
	struct timeval tv;
	int bytes_rcvd;


	if( ( udp_srv_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) ) < 0 ) {
		perror("ERROR on socket(udp): ");
		exit(1);
	}

	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	if (setsockopt(udp_srv_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv) ) < 0) {
    	perror("Error setsockopt: ");
	}

	if(bind(udp_srv_socket, (struct sockaddr *) params->bind_addr, params->bind_addrlen) < 0 ) {
	   	perror( "ERROR on binding(udp): ");
	   	exit(1);
	}

	while(1){
		bytes_rcvd = 0;
		if( (bytes_rcvd = recvfrom(udp_srv_socket, buffer, params->block_size, 0, NULL, NULL) ) < 0){
			if(errno == EAGAIN && errno == EWOULDBLOCK){
				break;
			}else{
				perror( "ERROR on msg recv: recvfrom() failed");
				exit(1);
			}
			
		}
		total_pack_rcvd ++;
	}

	header_udp_ptr = (udp_header *)buffer;


	header_udp.seq_number = ntohl(header_udp_ptr->seq_number);
	
	printf("%d\n", total_pack_rcvd);


	close(udp_srv_socket);
	return args;

}
*/
