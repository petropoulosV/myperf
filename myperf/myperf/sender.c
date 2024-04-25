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



int start_sender(char* ip, char * port, unsigned int udp_pcktsize, unsigned int bandwidth,
					int p_streams, double duration, int m_mode){

	int tcp_client_socket;
	unsigned char buffer_tcp[BUFLEN_TCP], msg_data[BUFLEN_TCP - SIZEOF_TCP_HEADER];
	int bytes_send, bytes_rcvd, status, i, udp_port_int, mtu, last_msg;
	struct timespec start, current_time; 			/*need to calculate total duration*/
	double total_time;					 			/*need to calculate total duration*/
	tcp_header header_tcp;
	unsigned  block_size;
	struct client_params *params;
	pthread_t *client_thread;
	struct cmdc *cmd_channel;
	char udp_port_str[20];

	
	/*init*/
	udp_port_int = atoi(port);
	last_msg = 0;


	if(duration == 0){
		duration = 10;
	}

	block_size = udp_pcktsize;

	struct sockaddr *tcp_client_addr ;
	socklen_t tcp_client_addrlen;
	memset(&tcp_client_addr, 0, sizeof(struct sockaddr*));

	get_addr(TCP_CLIENT, ip, port, &tcp_client_addr, &tcp_client_addrlen);

	if( ( tcp_client_socket = socket(tcp_client_addr->sa_family, SOCK_STREAM, IPPROTO_TCP) ) < 0 ) {
		perror("ERROR on socket(tcp): ");
		exit(1);
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1000;
	if (setsockopt(tcp_client_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv) ) < 0) {
		perror("Error setsockopt: ");
	}

	if (connect(tcp_client_socket, (struct sockaddr *) tcp_client_addr,tcp_client_addrlen) < 0) {
		perror("ERROR on connect: ");
		exit(1);
	}

	/*mtu = find_mtu2((struct sockaddr *)tcp_addr->ai_addr, tcp_addr->ai_addrlen);*/
	mtu = find_mtu(INTERFACE);
	if(mtu != 0 && block_size > mtu){
		block_size = mtu;
	}

	printf("Connecting to host %s, port %s\n", ip, port);

	/*tcp chanel send info from client tcp_pctsize and mode*/
	write_msg(buffer_tcp, NULL, MSG_CONNECTION, m_mode, 1, block_size, p_streams, 0, 0);

	bytes_send = 0;
	while(bytes_send < BUFLEN_TCP){
		if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
			perror("ERROR on send(tcp): ");
			exit(1);
		}
	}


	if(m_mode == MODE_THR){

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

		bytes_rcvd = 0;
		while(bytes_rcvd < BUFLEN_TCP){
			if( (bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0)) < 0){
				if(errno != EAGAIN && errno != EWOULDBLOCK){
					perror( "ERROR on msg recv: recvfrom() failed");
					exit(1);
				}else if(bytes_rcvd == -1){
					bytes_rcvd = 0;
					continue;/*time out*/
				}	
			}
			if(bytes_rcvd == 0){
				printf("Connection Clossed\n");
				close(tcp_client_socket);		
				return 0;
				break;
			}
		}

		/*read header*/
		read_msg(buffer_tcp, msg_data, &header_tcp);

		if(header_tcp.start == 2){
			printf("Start\n");
		}else{
			printf("Connection closed\n");
			close(tcp_client_socket);		
			return 0;
		}

		for(i = 0; i < p_streams; i++ ){

			struct sockaddr *udp_client_addr = NULL;
			socklen_t udp_client_addrlen;
			memset(&udp_client_addr, 0, sizeof(struct sockaddr*));

			sprintf(udp_port_str,"%d",udp_port_int++);
			get_addr(UDP_SERVER, ip, udp_port_str, &udp_client_addr, &udp_client_addrlen);

			params = (struct client_params*) malloc( sizeof(struct client_params) );
			if(params == NULL){
				perror("Malloc error: ");
				exit(1);
			}

			cmd_channel[i] =  CMDC_INITIALIZER;

			params->server_addr = udp_client_addr;
			params->server_addrlen = udp_client_addrlen;
			params->target_bitrate = bandwidth;
			params-> block_size = block_size;
			params->cmd_channel = &cmd_channel[i];

			status = pthread_create(&client_thread[i], NULL, client_run, params);
			if(status != 0){
				perror("client thread creation error: ");
				exit(EXIT_FAILURE);
			}
		}

		
		clock_gettime(CLOCK_MONOTONIC_RAW, &start);	/*get start time*/

		while( (total_time  <= duration) || !duration ){
			clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
			total_time = current_time.tv_sec - start.tv_sec;
			total_time += (current_time.tv_nsec - start.tv_nsec) / 1000000000.0;


			/*get feedbac about bitrate from server */
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
				printf("Connection Clossed\n");
				break;
			}

			read_msg(buffer_tcp, msg_data, &header_tcp);

			if(header_tcp.type == MSG_STATS){
				last_msg = 1;
			}

			printf("Thread <%d>  -> ", header_tcp.thread_no);
			print_bitrate(header_tcp.bitrate);
			client_cmdc_report_bitrate(&cmd_channel[header_tcp.thread_no], header_tcp.bitrate);

		}

		for(i = 0; i < p_streams; i++ ){

			client_cmdc_stop(&cmd_channel[i]);
			pthread_join(client_thread[i], NULL);
		}
		
		free(client_thread);

	}
	else{  												/*rtt measurment*/

		struct cmdc cmd_channel = CMDC_INITIALIZER;
		pthread_t client_thread;

		struct sockaddr *udp_client_addr = NULL;
		socklen_t udp_client_addrlen;
		memset(&udp_client_addr, 0, sizeof(struct sockaddr*));

		sprintf(udp_port_str,"%d",udp_port_int++);
		get_addr(UDP_SERVER, ip, udp_port_str, &udp_client_addr, &udp_client_addrlen);
	
		params = (struct client_params*) malloc( sizeof(struct client_params) );
		if(params == NULL){
			perror("Malloc error: ");
			exit(1);
		}


		params->server_addr = udp_client_addr;
		params->server_addrlen = udp_client_addrlen;
		params->target_bitrate = bandwidth;
		params-> block_size = block_size;
		params->cmd_channel = &cmd_channel;

		/*block-waiting for server to open udp ports*/
		bytes_rcvd = 0;
		while(bytes_rcvd < BUFLEN_TCP){
			if( (bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0)) < 0){
				if(errno != EAGAIN && errno != EWOULDBLOCK){
					perror( "ERROR on msg recv: recvfrom() failed");
					exit(1);
				}else if(bytes_rcvd == -1){
					bytes_rcvd = 0;
					continue;/*time out*/
				}	
			}
			if(bytes_rcvd == 0){
				printf("Connection Clossed\n");
				close(tcp_client_socket);		
				return 0;
				break;
			}
		}

		/*read header*/
		read_msg(buffer_tcp, msg_data, &header_tcp);

		if(header_tcp.start == 2){
			printf("RTT MODE\n");
		}else{
			printf("Connection closed\n");
			return 0;
		}

		status = pthread_create(&client_thread, NULL, client_measure_oneway, params);
		if(status != 0){
			perror("client thread creation error: ");
			exit(EXIT_FAILURE);
		}

	
		/*client_cmdc_stop(&cmd_channel);*/
		pthread_join(client_thread, NULL);
		close(tcp_client_socket);
		return 0;
				
	}


	write_msg(buffer_tcp, NULL, MSG_CONNECTION, 0, 0, 0, 0, 0, 0);	/*termination signal*/

	bytes_send = 0;
	while(bytes_send < BUFLEN_TCP){
		if( (bytes_send += send(tcp_client_socket, &buffer_tcp[bytes_send], (BUFLEN_TCP - bytes_send), 0)) < 0){
			perror("ERROR on send(tcp): ");
			exit(1);
		}
	}

	while(last_msg == 0){
	/*receive data*/
		bytes_rcvd = 0;
		while(bytes_rcvd < BUFLEN_TCP){
			if( (bytes_rcvd += recv(tcp_client_socket, &buffer_tcp[bytes_rcvd], (BUFLEN_TCP - bytes_rcvd), 0)) < 0){
				if(errno != EAGAIN && errno != EWOULDBLOCK){
					perror( "ERROR on msg recv: recvfrom() failed");
					exit(1);
				}else if(bytes_rcvd == -1){
					bytes_rcvd = 0;
					continue;/*time out*/
				}	
			}
			if(bytes_rcvd == 0){
				printf("Connection Clossed\n");
				close(tcp_client_socket);		
				return 0;
				break;
			}
		}

		read_msg(buffer_tcp, msg_data, &header_tcp);
		if(header_tcp.type == MSG_STATS){
			last_msg = 1;
		}else{
			printf("Thread <%d>  -> ", header_tcp.thread_no);
			print_bitrate(header_tcp.bitrate);
		}
	}

	/*print results*/
	printf("Throughtput\t  Goodput\t     Jilter     Jilter stdev  Packet loss\n");
	printf("%s\n",msg_data );


	printf("\nConnection Clossed\n");
	close(tcp_client_socket);		
	return 0;
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


int find_mtu2(struct sockaddr *addr, socklen_t addr_len){
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
}


