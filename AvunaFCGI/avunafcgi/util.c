/*
 * util.c
 *
 *  Created on: Nov 17, 2015
 *      Author: root
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdio.h>

void* __fcgi_xmalloc(size_t size) {
	if (size > 10485760) {
		printf("Big malloc %u!\n", size);
	}
	void* m = malloc(size);
	if (m == NULL) {
		printf("Out of Memory! @ malloc size %u\n", size);
		exit(1);
	}
	return m;
}

void __fcgi_xfree(void* ptr) {
	free(ptr);
}

void* __fcgi_xcalloc(size_t size) {
	if (size > 10485760) {
		printf("Big calloc %u!\n", size);
	}
	void* m = calloc(1, size);
	if (m == NULL) {
		printf("Out of Memory! @ calloc size %u\n", size);
		exit(1);
	}
	return m;
}

void* __fcgi_xrealloc(void* ptr, size_t size) {
	if (size == 0) {
		__fcgi_xfree(ptr);
		return NULL;
	}
	if (size > 10485760) {
		printf("Big realloc %u!\n", size);
	}
	void* m = realloc(ptr, size);
	if (m == NULL) {
		printf("Out of Memory! @ realloc size %u\n", size);
		exit(1);
	}
	return m;
}
