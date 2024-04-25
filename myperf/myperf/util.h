#ifndef UTIL_H
#define UTIL_H

#include <string.h>
#include <errno.h>
#include <time.h>

#define FERROR(msg, ...) ({ \
    if(errno != 0) \
		fprintf(stderr, "%s:%d: " msg " [%s]" "\n", \
			__FILE__, __LINE__, ##__VA_ARGS__, strerror(errno)); \
    else \
		fprintf(stderr, "%s:%d: " msg "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
    \
    fflush(stderr); \
    exit(EXIT_FAILURE); \
})

#define THREAD_ERROR(msg, ...) ({ \
    if (errno != 0) fprintf(stderr, msg " [%s]" "\n", ##__VA_ARGS__, strerror(errno)); \
    else fprintf(stderr, msg "\n", ##__VA_ARGS__); \
    \
    fflush(stderr); \
    pthread_exit(NULL); \
})

#define DEFINEDX(NAME) ((#NAME)[0] == 0)
#define DEFINED(NAME) DEFINEDX(NAME)

// All members implicitly initialzed to zero
#define CMDC_INITIALIZER ((struct cmdc) {})

struct cmdc {
	void *msg_buffer;
	
	size_t msg_buflen;
	size_t response_len;
	
	// 0 = no data, 1 = master sent data,
	// 2 = slave responded
	int state; 
};

// Time measurement methods
// ------------------------

ulong ts_diff_ns(struct timespec start, struct timespec end);
double ts_diff_us(struct timespec start, struct timespec end);
double ts_diff_ms(struct timespec start, struct timespec end);

void wait_ns(ulong ns);
void sleep_ms(ulong ms);

// Socket management methods
// -------------------------

int zerocopy_wait_notification(int sockfd, int count);
int empty_recv_buffer(int sockfd);

// Command Channel management methods
// ----------------------------------

ssize_t cmdc_master_write(struct cmdc *channel, void *buf, size_t count);
ssize_t cmdc_slave_read(struct cmdc *channel, void *buf, size_t count);
ssize_t cmdc_slave_respond(struct cmdc *channel, void *buf, size_t count);

// Statistics helper methods
// ------------------------

/* Calculates the population standard deviation, given the 
 * number of samples, their sum and the sum of their squares */
double stdev(int count, size_t sum, size_t sqsum);


#endif
