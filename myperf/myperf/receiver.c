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

	int  tcp_srv_socket, tcp_client_socket;
	struct sockaddr_storage  tcp_client_addr;
	socklen_t tcp_client_address_len;
	
	unsigned char buffer_tcp[BUFLEN_TCP], msg_data[BUFLEN_TCP - SIZEOF_TCP_HEADER];;
	int optval, bytes_rcvd, bytes_send, s, status, i, mode;
	int  p_streams, udp_port_int;
	unsigned int udp_pcktsize;
	char client_ip[NI_MAXHOST], client_port[NI_MAXSERV];
	struct timeval tv;
	struct timespec start, current_time; 			/*need to calculate total duration*/
	double total_time , round;
	tcp_header header_tcp;
	char udp_port_str[10];

	struct server_params *params;
	pthread_t *server_thread;
	struct cmdc *cmd_channel;
	struct myperf_stats stats;
	struct myperf_stats  stats_sum =(struct myperf_stats){};


	/*init*/
	optval = 1;
	udp_port_int = atoi(port);
	tcp_client_address_len = sizeof(struct sockaddr_storage);
	round = 1;


	struct sockaddr *tcp_server_addr ;
	socklen_t tcp_server_addrlen;
	memset(&tcp_server_addr, 0, sizeof(struct sockaddr*));

	get_addr(TCP_SERVER, ip, port, &tcp_server_addr, &tcp_server_addrlen);

	if( ( tcp_srv_socket = socket(tcp_server_addr->sa_family, SOCK_STREAM, IPPROTO_TCP) ) < 0 ) {
		perror("ERROR on socket(tcp): ");
		exit(1);
	}printf("%s\n",((tcp_server_addr->sa_family ==AF_INET)?"IPv4":"IPv6") );

	if (setsockopt(tcp_srv_socket,SOL_SOCKET,SO_REUSEADDR /*| SO_REUSEPORT*/, &optval, sizeof(int))) {
		perror("ERROR on socket(tcp): setsockopt() failed");
		exit(1);
	}

	if( bind(tcp_srv_socket, tcp_server_addr, tcp_server_addrlen ) < 0 ) {
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
		read_msg(buffer_tcp, msg_data, &header_tcp);

		udp_pcktsize = header_tcp.udp_pcktsize;
		p_streams = header_tcp.parallel_streams;
		mode = header_tcp.mode;


		if(mode == MODE_THR){
			/*Throughtput  Goodput  Jilter Jilterdtdev  Packet loss mode*/

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
				struct sockaddr *udp_server_addr = NULL;
				socklen_t udp_server_addrlen;
				memset(&udp_server_addr, 0, sizeof(struct sockaddr*));

				sprintf(udp_port_str,"%d",udp_port_int++);
				get_addr(UDP_SERVER, ip, udp_port_str, &udp_server_addr, &udp_server_addrlen);

				params = (struct server_params*) malloc( sizeof(struct server_params) );
				if(params == NULL){
					perror("Malloc error: ");
					exit(1);
				}

				cmd_channel[i] =  CMDC_INITIALIZER;

				params->bind_addr = udp_server_addr;
				params->bind_addrlen = udp_server_addrlen;
				//get_addr(UDP_SERVER, ip, udp_port_str, &params->bind_addr, &params->bind_addrlen);
				params-> block_size = udp_pcktsize;
				params->cmd_channel = &cmd_channel[i];

				status = pthread_create(&server_thread[i], NULL, server_run, params);
				if(status != 0){
					perror("client thread creation error: ");
					exit(EXIT_FAILURE);
				}

			}
			udp_port_int = atoi(port);

			write_msg(buffer_tcp, NULL, MSG_CONNECTION, 0, 2, 0, 0, 0, 0);

			bytes_send = 0;
			while(bytes_send < BUFLEN_TCP){
				if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
					perror("ERROR on send: sent  different number of bytes than expected");
					exit(1);
				}
			}

			round = 1;
			total_time = 0;
			
			printf("Thread   Throughtput\t   Goodput\t     Jilter      Jilter stdev   Packet loss\n");

			clock_gettime(CLOCK_MONOTONIC_RAW, &start);	
			

			while(1){
				clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);	
				total_time = current_time.tv_sec - start.tv_sec;
				total_time += (current_time.tv_nsec - start.tv_nsec) / 1000000000.0;


				/*sleep_ms(500);*/
				if(round < total_time){
					printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - - - - \n");
					stats_sum =(struct myperf_stats){};
					for(i = 0; i < p_streams; i++ ){
						/*print stats for every thread*/
						server_cmdc_stats_read(&cmd_channel[i], &stats);
						printf("%2d     ",(i+1));
						print_stats(&stats);
						add_stats(&stats_sum, &stats ,p_streams);

						/*inform client thread[i] for bitrate at server*/
						write_msg(buffer_tcp, NULL, MSG_FEEDBACK, 0, 0, 0, 0, (unsigned int)stats.throughput, i);

						bytes_send = 0;
						while(bytes_send < BUFLEN_TCP){
							if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
								perror("ERROR on send: sent  different number of bytes than expected");
								exit(1);
							}
						}

					}
					if(p_streams > 1){
						printf("sum    ");
						print_stats(&stats_sum);
						stats_sum =(struct myperf_stats){};
					}

					round ++;
				}


				bytes_rcvd = 0;
				while(bytes_rcvd < BUFLEN_TCP){
					if( (bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0)) < 0){
						if(errno != EAGAIN && errno != EWOULDBLOCK){
							perror( "ERROR on msg recv: recvfrom() failed");
							exit(1);
						}else if(bytes_rcvd == -1){
							break;/*time out*/
						}	
					}
					if(bytes_rcvd == 0){
						break;
					}
				}

				if(bytes_rcvd == -1){
						continue;
				}

				if(bytes_rcvd == 0){
					printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - - - - \n");
					printf("Connection Clossed\n");
					break;
				}

				read_msg(buffer_tcp, msg_data, &header_tcp);
				
				if(header_tcp.start == 0){
					break;
				}
			}

			/*print*/
			/*if()*/
			/*printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - - - - \n");
			stats_sum =(struct myperf_stats){};
			for(i = 0; i < p_streams; i++ ){
				server_cmdc_stats_read(&cmd_channel[i], &stats);
				printf("%2d     ",(i+1));
				print_stats(&stats);
				add_stats(&stats_sum, &stats ,p_streams);
			}
			if(p_streams > 1){
				printf("sum    ");
				print_stats(&stats_sum);
				stats_sum =(struct myperf_stats){};
			}	*/

			printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - - - - \n"
					"- - - - - - - - - - - - - - - - - - - Total - - - - - - - - - - -- - - - - - - - - \n");
			
			stats_sum =(struct myperf_stats){};
			/*waiting threads*/
			struct myperf_stats stats_p1 = (struct myperf_stats){};
			for(i = 0; i < p_streams; i++ ){

				server_cmdc_stats_read_average(&cmd_channel[i], &stats_p1);
				printf("%2d     ",(i+1));
				print_stats(&stats_p1);
				add_stats(&stats_sum, &stats_p1 ,p_streams);
				server_cmdc_stop(&cmd_channel[i]);
				pthread_join(server_thread[i], NULL);
			}	
			if(p_streams > 1){
				printf("sum    ");
				print_stats(&stats_sum);
			}

			char buf[220];
			string_print_stats(&stats_sum, buf);

			write_msg(buffer_tcp, (unsigned char*)buf, MSG_STATS, 0, 0, 0, 0, 0, 0);

			bytes_send = 0;
			while(bytes_send < BUFLEN_TCP){
				if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
					perror("ERROR on send(): ");
					exit(1);
				}
			}

			free(server_thread);
		}
		else{


			struct cmdc cmd_channel = CMDC_INITIALIZER;
			pthread_t server_thread;

			struct sockaddr *udp_server_addr = NULL;
			socklen_t udp_server_addrlen;
			memset(&udp_server_addr, 0, sizeof(struct sockaddr*));

			sprintf(udp_port_str,"%d",udp_port_int);
			get_addr(UDP_SERVER, ip, udp_port_str, &udp_server_addr, &udp_server_addrlen);


			params = (struct server_params*) malloc( sizeof(struct server_params) );
			if(params == NULL){
				perror("Malloc error: ");
				exit(1);
			}

			params->bind_addr = udp_server_addr;
			params->bind_addrlen = udp_server_addrlen;
			//get_addr(UDP_SERVER, ip, udp_port_str, &params->bind_addr, &params->bind_addrlen);
			params-> block_size = udp_pcktsize;
			params->cmd_channel = &cmd_channel;

	
			status = pthread_create(&server_thread, NULL, server_run, params);
			if(status != 0){
				perror("client thread creation error: ");
				exit(EXIT_FAILURE);
			}

			memset(buffer_tcp, 0, BUFLEN_TCP);
			header_tcp.start = htons(2);							/*start rtt signal*/
			memcpy(buffer_tcp, &header_tcp, sizeof(header_tcp));

			write_msg(buffer_tcp, NULL, MSG_CONNECTION, 0, 2, 0, 0, 0, 0);

			bytes_send = 0;
			while(bytes_send < BUFLEN_TCP){
				if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
					perror("ERROR on send: sent  different number of bytes than expected");
					exit(1);
				}
			}

			sleep(2);

			server_cmdc_stop(&cmd_channel);
			pthread_join(server_thread, NULL);

		}


		close(tcp_client_socket);
		printf("- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -- - - - - - - - - \n");
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

	printf("%8.3lf us  ", stats->jitter * 1e06 );
	printf("%8.3lf us   ", stats->jitter_stdev * 1e06 );
	printf("%6.2lf%%", stats->packet_loss * 100);
	printf("\n");

}

void string_print_stats(struct myperf_stats *stats,char *buf){
	if(buf == NULL)
		return;
	buf[0] = '\0';
	if(stats->throughput >= 1000000000){
		sprintf(buf + strlen(buf),"%6.2lf Gbits/sec  ", stats->throughput/1000000000.0);
	}
	else if(stats->throughput >= 1000000){
		sprintf(buf + strlen(buf),"%6.2lf Mbits/sec  ", stats->throughput/1000000.0);
	}
	else if(stats->throughput >= 1000){
		sprintf(buf + strlen(buf),"%6.2lf Kbits/sec  ", stats->throughput/1000.0);
	}
	else{
		sprintf(buf + strlen(buf),"%6ld bits/sec   ", stats->throughput);
	}

	if(stats->goodput >= 1000000000){
		sprintf(buf + strlen(buf),"%6.2lf Gbits/sec  ", stats->goodput/1000000000.0);
	}
	else if(stats->goodput >= 1000000){
		sprintf(buf + strlen(buf),"%6.2lf Mbits/sec  ", stats->goodput/1000000.0);
	}
	else if(stats->goodput >= 1000){
		sprintf(buf + strlen(buf),"%6.2lf Kbits/sec  ", stats->goodput/1000.0);
	}
	else{
		sprintf(buf + strlen(buf),"%6ld bits/sec   ", stats->goodput);
	}

	sprintf(buf + strlen(buf),"%8.3lf us  ", stats->jitter * 1e06 );
	sprintf(buf + strlen(buf),"%8.3lf us   ", stats->jitter_stdev * 1e06 );
	sprintf(buf + strlen(buf),"%6.2lf%%", stats->packet_loss * 100);
	sprintf(buf + strlen(buf),"\n");

}

void add_stats(struct myperf_stats *stats_sum, struct myperf_stats *stats, int n){
	if(n==0)
		n=1;
	stats_sum->throughput += stats->throughput;
	stats_sum->goodput += stats->goodput;
	stats_sum->packet_loss += stats->packet_loss/(double)n;
	stats_sum->jitter += stats->jitter/(double)n;
	stats_sum->jitter_stdev += stats->jitter_stdev/(double)n;

}

void get_addr(int option, char *ip, char *port, 
			struct sockaddr **server_addr, socklen_t *server_addrlen){

	struct addrinfo hints, *addr;
	int s;

	/*set hint for getaddrinfo for tcp socket*/
	memset(&hints, 0, sizeof(struct addrinfo) );

	hints.ai_family = AF_UNSPEC;		/* IPv4 or IPv6 */
	
	if(option == TCP_SERVER || option == TCP_CLIENT ){
		hints.ai_socktype = SOCK_STREAM; 
		hints.ai_protocol = IPPROTO_TCP;
	}else{
		hints.ai_socktype = SOCK_DGRAM; 
		hints.ai_protocol = IPPROTO_UDP;
	}

	if(option == TCP_SERVER || option == UDP_SERVER ){
		hints.ai_flags = AI_PASSIVE;		/* INADDR_ANY INADDR6_ANY */
		hints.ai_next = NULL;
		hints.ai_addr = NULL;
		hints.ai_canonname = NULL;
	}
	else{
		hints.ai_flags = 0;
	}
	
	if ((s = getaddrinfo(ip, port, &hints, &addr)) != 0 ) {
		perror("ERROR on getaddrinfo(get_addr): ");
		printf("%s\n",gai_strerror(s) );
		exit(1);
	}

	*server_addr = addr->ai_addr;
	*server_addrlen = addr->ai_addrlen;
	
	if(addr->ai_next != NULL)
		freeaddrinfo(addr->ai_next);

}

int write_msg(unsigned char *msg_buffer, unsigned char *data_buffer, unsigned short int type, 
				unsigned short int mode, unsigned short int start, unsigned int udp_pcktsize, 
					unsigned short int parallel_streams, unsigned int bitrate, int thread_no){
	
	tcp_header header_tcp = {};

	memset(msg_buffer, 0, BUFLEN_TCP);

	header_tcp.type = htons(type);

	header_tcp.udp_pcktsize = htonl(udp_pcktsize);
	header_tcp.parallel_streams = htons(parallel_streams);
	header_tcp.mode = htons(mode);
	header_tcp.start = htons(start);

	header_tcp.bitrate = htonl(bitrate);
	header_tcp.thread_no = htonl(thread_no);

	memcpy(msg_buffer, &header_tcp, sizeof(header_tcp));
	if(data_buffer != NULL && BUFLEN_TCP > (strlen((char*)data_buffer) + sizeof(header_tcp)) ){
		memcpy(msg_buffer + sizeof(header_tcp), data_buffer, strlen((char*)data_buffer));
	}

	return 0;

}
int read_msg(unsigned char *msg_buffer,unsigned char *data_buffer, tcp_header *header_tcp){

	tcp_header *header_tcp_ptr;

	if(!header_tcp || !msg_buffer){
		printf("Error on read_msg()\n");
		return -1;
	}
	memset(data_buffer, '\0', BUFLEN_TCP - SIZEOF_TCP_HEADER - 1);

	/*data_buffer = msg_buffer + sizeof(tcp_header);*/
	strcpy((char*)data_buffer,(char*) (msg_buffer + sizeof(tcp_header)));

	header_tcp_ptr = (tcp_header *)msg_buffer;

	header_tcp->type = ntohs(header_tcp_ptr->type);

	header_tcp->udp_pcktsize = ntohl(header_tcp_ptr->udp_pcktsize);
	header_tcp->parallel_streams = ntohs(header_tcp_ptr->parallel_streams);
	header_tcp->mode = ntohs(header_tcp_ptr->mode);
	header_tcp->start = ntohs(header_tcp_ptr->start);

	header_tcp->bitrate = ntohl(header_tcp_ptr->bitrate);
	header_tcp->thread_no = ntohl(header_tcp_ptr->thread_no);


	return 0;
}
