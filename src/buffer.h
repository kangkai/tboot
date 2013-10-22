#ifndef __BUFFER_H
#define __BUFFER_H

struct buffer {
	unsigned char *data;	// data address
	int size;	// buffer size
	int len;	// data length
	pthread_mutex_t mutex;
};

void buffer_free(struct buffer *buf);
struct buffer *buffer_init(int size);

#endif
