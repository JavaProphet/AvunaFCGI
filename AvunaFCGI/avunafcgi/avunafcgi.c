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
#include "xstring.h"

char* fcgi_escapehtml(const char* orig) {
	size_t len = strlen(orig);
	size_t clen = len + 1;
	size_t ioff = 0;
	char* ns = __fcgi_xmalloc(clen);
	for (int i = 0; i < len; i++) {
		if (orig[i] == '&') {
			clen += 4;
			ns = __fcgi_xrealloc(ns, clen);
			ns[i + ioff] = '&';
			ns[i + ioff++] = 'a';
			ns[i + ioff++] = 'm';
			ns[i + ioff++] = 'p';
			ns[i + ioff++] = ';';
		} else if (orig[i] == '\"') {
			clen += 5;
			ns = __fcgi_xrealloc(ns, clen);
			ns[i + ioff] = '&';
			ns[i + ioff++] = 'q';
			ns[i + ioff++] = 'u';
			ns[i + ioff++] = 'o';
			ns[i + ioff++] = 't';
			ns[i + ioff++] = ';';
		} else if (orig[i] == '\'') {
			clen += 5;
			ns = __fcgi_xrealloc(ns, clen);
			ns[i + ioff] = '&';
			ns[i + ioff++] = '#';
			ns[i + ioff++] = '0';
			ns[i + ioff++] = '3';
			ns[i + ioff++] = '9';
			ns[i + ioff++] = ';';
		} else if (orig[i] == '<') {
			clen += 3;
			ns = __fcgi_xrealloc(ns, clen);
			ns[i + ioff] = '&';
			ns[i + ioff++] = 'l';
			ns[i + ioff++] = 't';
			ns[i + ioff++] = ';';
		} else if (orig[i] == '>') {
			clen += 3;
			ns = __fcgi_xrealloc(ns, clen);
			ns[i + ioff] = '&';
			ns[i + ioff++] = 'g';
			ns[i + ioff++] = 't';
			ns[i + ioff++] = ';';
		} else {
			ns[i + ioff] = orig[i];
		}
	}
	ns[clen - 1] = 0;
	return ns;
}

void __fcgi_freeHeaders(struct fcgi_headers* headers) {
	if (headers->count > 0) for (int i = 0; i < headers->count; i++) {
		__fcgi_xfree(headers->names[i]);
		__fcgi_xfree(headers->values[i]);
	}
	if (headers->names != NULL) __fcgi_xfree(headers->names);
	if (headers->values != NULL) __fcgi_xfree(headers->values);
	__fcgi_xfree(headers);
}

const char* fcgi_header_get(const struct fcgi_headers* headers, const char* name) {
	if (headers->count == 0) return NULL;
	for (int i = 0; i < headers->count; i++) {
		if (__fcgi_streq_nocase(headers->names[i], name)) {
			return headers->values[i];
		}
	}
	return NULL;
}

int fcgi_header_set(struct fcgi_headers* headers, const char* name, const char* value) {
	if (headers->count == 0) return -1;
	for (int i = 0; i < headers->count; i++) {
		if (__fcgi_streq_nocase(headers->names[i], name)) {
			size_t vl = strlen(value) + 1;
			//if (streq_nocase(name, "Content-Type")) {
			//	printf("ct cur = %s, new = %s, racto = %i\n", headers->values[i], value, vl);
			//}
			headers->values[i] = __fcgi_xrealloc(headers->values[i], vl);
			memcpy(headers->values[i], value, vl);
			return 1;
		}
	}
	return 0;
}

int fcgi_header_add(struct fcgi_headers* headers, const char* name, const char* value) {
	headers->count++;
	if (headers->names == NULL) {
		headers->names = __fcgi_xmalloc(sizeof(char*));
		headers->values = __fcgi_xmalloc(sizeof(char*));
	} else {
		headers->values = __fcgi_xrealloc(headers->values, sizeof(char*) * headers->count);
		headers->names = __fcgi_xrealloc(headers->names, sizeof(char*) * headers->count);
	}
	int cdl = strlen(name) + 1;
	int vl = strlen(value) + 1;
	headers->names[headers->count - 1] = __fcgi_xmalloc(cdl);
	headers->values[headers->count - 1] = __fcgi_xmalloc(vl);
	memcpy(headers->names[headers->count - 1], name, cdl);
	memcpy(headers->values[headers->count - 1], value, vl);
	return 0;
}

int fcgi_header_tryadd(struct fcgi_headers* headers, const char* name, const char* value) {
	if (fcgi_header_get(headers, name) != NULL) return 1;
	return fcgi_header_add(headers, name, value);
}

int fcgi_header_setoradd(struct fcgi_headers* headers, const char* name, const char* value) {
	int r = 0;
	if (!(r = fcgi_header_set(headers, name, value))) r = fcgi_header_add(headers, name, value);
	return r;
}

char* __fcgi_serializeHeaders(struct fcgi_headers* headers, size_t* len) {
	*len = 0;
	if (headers->count == 0) {
		return NULL;
	}
	for (int i = 0; i < headers->count; i++) {
		*len += strlen(headers->names[i]) + strlen(headers->values[i]) + 4;
	}
	(*len) += 2;
	char* ret = __fcgi_xmalloc(*len);
	int ri = 0;
	for (int i = 0; i < headers->count; i++) {
		int nl = strlen(headers->names[i]);
		int vl = strlen(headers->values[i]);
		memcpy(ret + ri, headers->names[i], nl);
		ri += nl;
		ret[ri++] = ':';
		ret[ri++] = ' ';
		memcpy(ret + ri, headers->values[i], vl);
		ri += vl;
		ret[ri++] = '\r';
		ret[ri++] = '\n';
	}
	ret[ri++] = '\r';
	ret[ri++] = '\n';
	return ret;
}

void __fcgi_runWork(struct fcgi_conn* fc) {
	struct fcgi_frame frame;
	struct fcgi_request creq;
	creq.params = NULL;
	creq.stdin = NULL;
	creq.stdin_size = 0;
	creq.stdout = -1;
	creq.stderr = -1;
	int state = 0;
	while (!__fcgi_readFCGIFrame(fc->fd, &frame)) {
		if (frame.type == FCGI_GET_VALUES) {
			struct fcgi_params* gparams = __fcgi_readFCGIParams(frame.data, frame.len, NULL);
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
				struct fcgi_headers hdrs;
				(*fc->server->callback)(fc, &creq, &hdrs);
				frame.type = FCGI_STDOUT;
				frame.data = __fcgi_serializeHeaders(&hdrs, &frame.len);
				__fcgi_writeFCGIFrame(fc->fd, &frame);
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

void __fcgi_runServer(struct fcgi_server* server) {
	while (1) {
		struct sockaddr* addr = __fcgi_xmalloc(sizeof(struct sockaddr_in6));
		socklen_t len = sizeof(struct sockaddr_in6);
		int fd = accept4(server->fd, addr, &len, SOCK_CLOEXEC);
		if (fd < 0) {
			__fcgi_xfree(addr);
			continue;
		}
		struct fcgi_conn* fc = __fcgi_xmalloc(sizeof(struct fcgi_conn));
		fc->fd = fd;
		fc->addr = addr;
		fc->addrlen = len;
		fc->server = server;
		pthread_t pt;
		pthread_create(&pt, NULL, (void*) __fcgi_runWork, fc);
	}
}

const char* fcgi_getparam(struct fcgi_params* params, const char* name) {
	for (size_t i = 0; i < params->param_count; i++) {
		char** param = params->params[i];
		if (__fcgi_streq_nocase(param[0], name)) {
			return param[1];
		}
	}
	return NULL;
}

struct fcgi_server* fcgi_start(struct sockaddr* addr, socklen_t len, size_t maxConns) {
	int namespace;
	if (addr->sa_family == AF_INET) namespace = PF_INET;
	else if (addr->sa_family == AF_INET6) namespace = PF_INET6;
	else if (addr->sa_family == AF_LOCAL) namespace = PF_LOCAL;
	else {
		errno = EINVAL;
		return NULL;
	}
	struct fcgi_server* server = __fcgi_xmalloc(sizeof(struct fcgi_server));
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

int fcgi_sethandler(struct fcgi_server* server, int (*callback)(struct fcgi_conn*, struct fcgi_request*, struct fcgi_headers*)) {
	server->callback = callback;
	return 0;
}

int fcgi_stop(struct fcgi_server* server) {
	close(server->fd);
	pthread_cancel(server->accept);
	__fcgi_xfree(server);
	return 0;
}
