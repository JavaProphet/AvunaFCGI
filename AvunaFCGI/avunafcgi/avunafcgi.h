/*
 * avunafcgi.h
 *
 *  Created on: Feb 14, 2016
 *      Author: root
 */

#ifndef AVUNAFCGI_H_
#define AVUNAFCGI_H_

#include <sys/socket.h>

char* fcgi_escapehtml(const char* orig);

struct fcgi_headers {
		int count;
		char** names;
		char** values;
};

const char* fcgi_header_get(const struct fcgi_headers* headers, const char* name);

int fcgi_header_set(struct fcgi_headers* headers, const char* name, const char* value);

int fcgi_header_add(struct fcgi_headers* headers, const char* name, const char* value);

int fcgi_header_tryadd(struct fcgi_headers* headers, const char* name, const char* value);

int fcgi_header_setoradd(struct fcgi_headers* headers, const char* name, const char* value);

struct fcgi_params {
		unsigned char* buf;
		size_t buf_size;
		char*** params;
		size_t param_count;
};

struct fcgi_conn {
		int fd;
		struct sockaddr* addr;
		socklen_t addrlen;
		struct fcgi_server* server;
};

struct fcgi_request {
		struct fcgi_params* params;
		unsigned char* stdin;
		size_t stdin_size;
		int stdout;
		int stderr;
};

struct fcgi_server {
		int fd;
		size_t maxConns;
		int (*callback)(struct fcgi_conn*, struct fcgi_request*, struct fcgi_headers*);
		pthread_t accept;
};

const char* fcgi_getparam(struct fcgi_params* params, const char* name);

struct fcgi_server* fcgi_start(struct sockaddr* addr, socklen_t len, size_t maxConns);

/*
 * Callback is a pointer to a function which returns an int, -2 on connection failure, -1 on request failure, and 0 on success. It takes a struct fcgi_conn* as it's first argument, a struct fcgi_request* as it's second argument, and a struct fcgi_headers* as it's third.
 *
 */
int fcgi_sethandler(struct fcgi_server* server, int (*callback)(struct fcgi_conn*, struct fcgi_request*, struct fcgi_headers*));

int fcgi_stop(struct fcgi_server* server);

#endif /* AVUNAFCGI_H_ */
