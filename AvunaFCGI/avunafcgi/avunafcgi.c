/*
 * avunafcgi.c
 *
 *  Created on: Feb 14, 2016
 *      Author: root
 */

#define _GNU_SOURCE

#include "avunafcgi.h"
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include "util.h"
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include "fcgi.h"
#include <fcntl.h>
#include "xstring.h"
#include <sys/types.h>

void __fcgi_runWork(struct fcgiconn* fc) {
	struct fcgiframe frame;
	struct fcgirequest creq;
	creq.params = NULL;
	creq.stdin = NULL;
	creq.stdin_size = 0;
	creq.stdout = -1;
	creq.stderr = -1;
	int state = 0;
	while (!__fcgi_readFCGIFrame(fc->fd, &frame)) {
		if (frame.type == FCGI_GET_VALUES) {
			struct fcgiparams* gparams = __fcgi_readFCGIParams(frame.data, frame.len, NULL);
			gparams = __fcgi_calcFCGIParams(gparams);
			for (size_t i = 0; i < gparams->param_count; i++) {
				char** param = gparams->params[i];
				if (__fcgi_streq_nocase(param[0], "FCGI_MAX_CONNS") || __fcgi_streq_nocase(param[0], "FCGI_MAX_REQS")) {
					char nv[32];
					snprintf(nv, 32, "%i", fc->server->maxConns);
					size_t nvl = strlen(nv);
					param[1] = __fcgi_xrealloc(param[1], nvl + 1);
					memcpy(param[1], nv, nvl + 1);
				} else if (__fcgi_streq_nocase(param[0], "FCGI_MPXS_CONNS")) {
					param[1] = __fcgi_xrealloc(param[1], 2);
					param[1][0] = '0';
					param[1][1] = 0;
				}
			}
			unsigned char* buf = NULL;
			size_t buflen = 0;
			__fcgi_serializeFCGIParams(gparams, &buf, &buflen);
			frame.type = FCGI_GET_VALUES_RESULT;
			if (frame.data != NULL) __fcgi_xfree(frame.data);
			frame.data = buf;
			frame.len = buflen;
			if (__fcgi_writeFCGIFrame(fc->fd, &frame)) {
				__fcgi_xfree(frame.data);
				__fcgi_freeFCGIParams(gparams);
				goto ret;
			}
		}
		if (state == 0 && frame.type == FCGI_BEGIN_REQUEST) {
			state = 1;
		}
		if (state == 1 && frame.type == FCGI_PARAMS) {
			if (frame.len == 0) {
				creq.params = __fcgi_calcFCGIParams(creq.params);
				state = 2;
			} else creq.params = __fcgi_readFCGIParams(frame.data, frame.len, creq.params);
		}
		if (state == 2 && frame.type == FCGI_STDIN) {
			if (frame.len == 0) {
				int stdoutp[2];
				int stderrp[2];
				if (pipe(stdoutp) || pipe(stderrp)) {
					goto ret;
				}
				creq.stdout = stdoutp[1];
				creq.stderr = stderrp[1];
				(*fc->server->callback)(fc, &creq);
				fcntl(stdoutp[0], F_SETFL, fcntl(stdoutp[0], F_GETFL, 0) | O_NONBLOCK);
				fcntl(stderrp[0], F_SETFL, fcntl(stderrp[0], F_GETFL, 0) | O_NONBLOCK);
				ssize_t x = 0;
				unsigned char buf[8192];
				int re = 0;
				while ((x = read(stderrp[0], buf, 8192)) > 0) {
					re = 1;
					frame.data = buf;
					frame.len = x;
					frame.type = FCGI_STDERR;
					__fcgi_writeFCGIFrame(fc->fd, &frame);
				}
				if (re) {
					frame.data = NULL;
					frame.len = 0;
					frame.type = FCGI_STDERR;
					__fcgi_writeFCGIFrame(fc->fd, &frame);
				}
				while ((x = read(stdoutp[0], buf, 8192)) > 0) {
					frame.data = buf;
					frame.len = x;
					frame.type = FCGI_STDOUT;
					__fcgi_writeFCGIFrame(fc->fd, &frame);
				}
				frame.data = NULL;
				frame.len = 0;
				frame.type = FCGI_STDOUT;
				__fcgi_writeFCGIFrame(fc->fd, &frame);
				close(stdoutp[0]);
				close(stdoutp[1]);
				close(stderrp[0]);
				close(stderrp[1]);
				frame.type = FCGI_END_REQUEST;
				frame.len = 8;
				unsigned char dtt[8];
				frame.data = dtt;
				memset(dtt, 0, 8);
				__fcgi_writeFCGIFrame(fc->fd, &frame);
				if (creq.params != NULL) __fcgi_freeFCGIParams(creq.params);
				creq.params = NULL;
				creq.stderr = -1;
				creq.stdout = -1;
				creq.stdin_size = 0;
				if (creq.stdin != NULL) {
					__fcgi_xfree(creq.stdin);
					creq.stdin = NULL;
				}
				state = 0;
			} else {
				if (creq.stdin == NULL) {
					creq.stdin = __fcgi_xmalloc(frame.len);
					creq.stdin_size = 0;
				} else {
					creq.stdin = __fcgi_xrealloc(creq.stdin, creq.stdin_size + frame.len);
				}
				memcpy(creq.stdin + creq.stdin_size, frame.data, frame.len);
				creq.stdin_size += frame.len;
			}
		}
	}
	ret: ;
	if (creq.params != NULL) __fcgi_freeFCGIParams(creq.params);
	creq.params = NULL;
	creq.stderr = -1;
	creq.stdout = -1;
	creq.stdin_size = 0;
	if (creq.stdin != NULL) {
		__fcgi_xfree(creq.stdin);
		creq.stdin = NULL;
	}
	close(fc->fd);
	__fcgi_xfree(fc->addr);
	__fcgi_xfree(fc);
	pthread_cancel (pthread_self());;
}

void __fcgi_runServer(struct fcgiserver* server) {
	while (1) {
		struct sockaddr* addr = __fcgi_xmalloc(sizeof(struct sockaddr_in6));
		socklen_t len = sizeof(struct sockaddr_in6);
		int fd = accept4(server->fd, addr, &len, SOCK_CLOEXEC);
		if (fd < 0) {
			__fcgi_xfree(addr);
			continue;
		}
		struct fcgiconn* fc = __fcgi_xmalloc(sizeof(struct fcgiconn));
		fc->fd = fd;
		fc->addr = addr;
		fc->addrlen = len;
		fc->server = server;
		pthread_t pt;
		pthread_create(&pt, NULL, (void*) __fcgi_runWork, fc);
	}
}

const char* fcgi_getparam(struct fcgiparams* params, const char* name) {
	for (size_t i = 0; i < params->param_count; i++) {
		char** param = params->params[i];
		if (__fcgi_streq_nocase(param[0], name)) {
			return param[1];
		}
	}
	return NULL;
}

struct fcgiserver* fcgi_start(struct sockaddr* addr, socklen_t len, size_t maxConns) {
	int namespace;
	if (addr->sa_family == AF_INET) namespace = PF_INET;
	else if (addr->sa_family == AF_INET6) namespace = PF_INET6;
	else if (addr->sa_family == AF_LOCAL) namespace = PF_LOCAL;
	else {
		errno = EINVAL;
		return NULL;
	}
	struct fcgiserver* server = __fcgi_xmalloc(sizeof(struct fcgiserver));
	server->callback = NULL;
	server->fd = socket(namespace, SOCK_STREAM, 0);
	if (server->fd < 0) {
		__fcgi_xfree(server);
		return NULL;
	}
	server->maxConns = maxConns;
	if (bind(server->fd, addr, len) != 0 || listen(server->fd, 50) != 0) {
		close(server->fd);
		__fcgi_xfree(server);
		return NULL;
	}
	if (pthread_create(&server->accept, NULL, (void*) __fcgi_runServer, server)) {
		close(server->fd);
		__fcgi_xfree(server);
		return NULL;
	}
	return server;
}

int fcgi_sethandler(struct fcgiserver* server, int (*callback)(struct fcgiconn*, struct fcgirequest*)) {
	server->callback = callback;
	return 0;
}

int fcgi_stop(struct fcgiserver* server) {
	close(server->fd);
	pthread_cancel(server->accept);
	__fcgi_xfree(server);
	return 0;
}
