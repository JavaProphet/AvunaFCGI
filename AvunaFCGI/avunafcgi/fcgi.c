/*
 * fcgi.c
 *
 *  Created on: Nov 26, 2015
 *      Author: root
 */

#include "fcgi.h"
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include "util.h"
#include <string.h>
#include "avunafcgi.h"
#include <stdint.h>
#include <arpa/inet.h>

int __fcgi_writeFCGIFrame(int fd, struct fcgi_frame* fcgif) {
	unsigned char header[8];
	header[0] = FCGI_VERSION_1;
	header[1] = fcgif->type;
	header[2] = (fcgif->reqID & 0xFF00) >> 8;
	header[3] = fcgif->reqID & 0x00FF;
	header[4] = (fcgif->len & 0xFF00) >> 8;
	header[5] = fcgif->len & 0x00FF;
	header[6] = 0;
	header[7] = 0;
	int w = 0;
	while (w < 8) {
		int x = write(fd, header + w, 8 - w);
		if (x < 0) return -1;
		else if (x == 0) {
			errno = ECONNRESET;
			return -1;
		}
		w += x;
	}
	w = 0;
	while (w < fcgif->len) {
		int x = write(fd, fcgif->data + w, fcgif->len - w);
		if (x < 0) return -1;
		else if (x == 0) {
			errno = ECONNRESET;
			return -1;
		}
		w += x;
	}
	return 0;
}

int __fcgi_writeFCGIParam(int fd, const char* name, const char* value) {
	struct fcgi_frame fcgif;
	fcgif.type = FCGI_PARAMS;
	fcgif.reqID = 0;
	size_t ml = strlen(name);
	size_t vl = strlen(value);
	int enl = ml > 127;
	int evl = vl > 127;
	fcgif.len = (enl ? 4 : 1) + (evl ? 4 : 1) + ml + vl;
	unsigned char data[fcgif.len];
	int i = 0;
	if (enl) {
		data[i++] = (ml & 0xFF000000) >> 24 | 0x80;
		data[i++] = (ml & 0x00FF0000) >> 16;
		data[i++] = (ml & 0x0000FF00) >> 8;
		data[i++] = (ml & 0x000000FF);
	} else {
		data[i++] = ml;
	}
	if (evl) {
		data[i++] = ((vl & 0xFF000000) >> 24) | 0x80;
		data[i++] = (vl & 0x00FF0000) >> 16;
		data[i++] = (vl & 0x0000FF00) >> 8;
		data[i++] = (vl & 0x000000FF);
	} else {
		data[i++] = vl;
	}
	memcpy(data + i, name, ml);
	i += ml;
	memcpy(data + i, value, vl);
	fcgif.data = data;
	return __fcgi_writeFCGIFrame(fd, &fcgif);
}

struct fcgi_params* __fcgi_readFCGIParams(unsigned char* data, size_t size, struct fcgi_params* params) {
	if (params == NULL) {
		params = __fcgi_xmalloc(sizeof(struct fcgi_params));
		params->buf = NULL;
		params->buf_size = 0;
		params->param_count = 0;
		params->params = NULL;
	}
	if (params->buf == NULL) {
		params->buf = __fcgi_xmalloc(size);
		params->buf_size = 0;
	} else {
		params->buf = __fcgi_xrealloc(params->buf, params->buf_size + size);
	}
	memcpy(params->buf + params->buf_size, data, size);
	params->buf_size += size;
	return params;
}

struct fcgi_params* __fcgi_calcFCGIParams(struct fcgi_params* params) {
	size_t ci = 0;
	while (ci < params->buf_size) {
		uint32_t fnl = 0;
		unsigned char nameLength = params->buf[ci];
		if ((nameLength >> 7) == 1) {
			fnl = htonl(*((uint32_t*) (params->buf + ci)));
			fnl &= 0x7FFFFFFF;
			ci += 4;
		} else {
			fnl = nameLength;
			ci += 1;
		}
		uint32_t fvl = 0;
		unsigned char valueLength = params->buf[ci];
		if ((valueLength >> 7) == 1) {
			fvl = htonl(*((uint32_t*) (params->buf + ci)));
			fvl &= 0x7FFFFFFF;
			ci += 4;
		} else {
			fvl = valueLength;
			ci += 1;
		}
		char* name = __fcgi_xmalloc(fnl + 1);
		char* value = __fcgi_xmalloc(fvl + 1);
		name[fnl] = 0;
		value[fvl] = 0;
		memcpy(name, params->buf + ci, fnl);
		ci += fnl;
		memcpy(value, params->buf + ci, fvl);
		ci += fvl;
		if (params->params == NULL) {
			params->params = __fcgi_xmalloc(sizeof(char**));
			params->param_count = 0;
		} else {
			params->params = __fcgi_xrealloc(params->params, sizeof(char**) * (params->param_count + 1));
		}
		char** pk = __fcgi_xmalloc(sizeof(char*) * 2);
		pk[0] = name;
		pk[1] = value;
		params->params[params->param_count++] = pk;
	}
	if (params->buf != NULL) {
		__fcgi_xfree(params->buf);
		params->buf = NULL;
		params->buf_size = 0;
	}
	return params;
}

int __fcgi_serializeFCGIParams(struct fcgi_params* params, unsigned char** buf, size_t* size) {
	*buf = NULL;
	*size = 0;
	for (size_t i = 0; i < params->param_count; i++) {
		char** param = params->params[i];
		size_t nl = strlen(param[0]);
		size_t vl = strlen(param[1]);
		size_t cs = nl + vl + (nl < 128 ? 1 : 4) + (vl < 128 ? 1 : 4);
		if (*buf == NULL) {
			*buf = __fcgi_xmalloc(cs);
			*size = 0;
		} else {
			*buf = __fcgi_xrealloc(*buf, *size + cs);
		}
		if (nl < 128) {
			*buf[*size++] = nl;
		} else {
			uint32_t rs = nl;
			memcpy(*buf + *size, &rs, sizeof(uint32_t));
			*size += 4;
		}
		if (vl < 128) {
			*buf[*size++] = vl;
		} else {
			uint32_t rs = vl;
			memcpy(*buf + *size, &rs, sizeof(uint32_t));
			*size += 4;
		}
		memcpy(*buf + *size, param[0], nl);
		*size += nl;
		memcpy(*buf + *size, param[1], vl);
		*size += vl;
	}
	return 0;
}

int __fcgi_freeFCGIParams(struct fcgi_params* params) {
	for (size_t i = 0; i < params->param_count; i++) {
		char** param = params->params[i];
		__fcgi_xfree(param[0]);
		__fcgi_xfree(param[1]);
		__fcgi_xfree(param);
	}
	__fcgi_xfree(params);
	return 0;
}

int __fcgi_readFCGIFrame(int fd, struct fcgi_frame* fcgif) {
	unsigned char header[8];
	int r = 0;
	while (r < 8) {
		int x = read(fd, header + r, 8 - r);
		if (x < 0) return -1;
		else if (x == 0) {
			errno = ECONNRESET;
			return -1;
		}
		r += x;
	}
	if (header[0] != FCGI_VERSION_1) {
		return -2;
	}
	fcgif->type = header[1];
	fcgif->reqID = (header[2] << 8) + header[3];
	fcgif->len = (header[4] << 8) + header[5];
	unsigned char padding = header[6];
//7 = reserved
	fcgif->data = __fcgi_xmalloc(fcgif->len);
	r = 0;
	while (r < fcgif->len) {
		int x = read(fd, fcgif->data + r, fcgif->len - r);
		if (x < 0) return -1;
		else if (x == 0) {
			errno = ECONNRESET;
			return -1;
		}
		r += x;
	}
	r = 0;
	unsigned char pbuf[padding];
	while (r < padding) {
		int x = read(fd, pbuf + r, padding - r);
		if (x < 0) return -1;
		else if (x == 0) {
			errno = ECONNRESET;
			return -1;
		}
		r += x;
	}
	return 0;
}
