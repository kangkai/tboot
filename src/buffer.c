#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "buffer.h"

/*
 * free a buffer
 */
void buffer_free(struct buffer *buf)
{
	if (!buf)
		return;
	if (buf->data)
		free(buf->data);
	pthread_mutex_destroy(&buf->mutex);
	free(buf);
}

/*
 * create a buffer
 */
struct buffer *buffer_init(int size)
{
	int i;
	struct buffer *buf;

	if (size <= 0) {
		printf("invalid buffer_init\n");
		return NULL;
	}

	buf = malloc(sizeof(*buf));
	if (!buf) {
		printf("out of memory.\n");
		return NULL;
	}

	buf->data = malloc(size);
	if (!buf->data) {
		printf("out of memory.\n");
		buffer_free(buf);
		return NULL;
	}

	buf->size = size;
	buf->len = 0;
	if (pthread_mutex_init(&buf->mutex, NULL)) {
		printf("init mutex of buffer failed.\n");
		buffer_free(buf);
		return NULL;
	}

	return buf;
}
