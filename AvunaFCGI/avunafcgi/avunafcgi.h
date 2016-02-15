/*
 * avunafcgi.h
 *
 *  Created on: Feb 14, 2016
 *      Author: root
 */

#ifndef AVUNAFCGI_H_
#define AVUNAFCGI_H_

#include <sys/socket.h>

struct fcgiparams {
		unsigned char* buf;
		size_t buf_size;
		char*** params;
		size_t param_count;
};

struct fcgiconn {
		int fd;
		struct sockaddr* addr;
		socklen_t addrlen;
		struct fcgiserver* server;
};

struct fcgirequest {
		struct fcgiparams* params;
		unsigned char* stdin;
		size_t stdin_size;
		int stdout;
		int stderr;
};

struct fcgiserver {
		int fd;
		size_t maxConns;
		int (*callback)(struct fcgiconn*, struct fcgirequest*);
		pthread_t accept;
};

const char* fcgi_getparam(struct fcgiparams* params, const char* name);

struct fcgiserver* fcgi_start(struct sockaddr* addr, socklen_t len, size_t maxConns);

/*
 * Callback is a pointer to a function which returns an int, -2 on connection failure, -1 on request failure, and 0 on success. It takes a struct fcgiconn* as it's first argument, and a struct fcgirequest* as it's second argument.
 *
 */
int fcgi_sethandler(struct fcgiserver* server, int (*callback)(struct fcgiconn*, struct fcgirequest*));

int fcgi_stop(struct fcgiserver* server);

#endif /* AVUNAFCGI_H_ */
