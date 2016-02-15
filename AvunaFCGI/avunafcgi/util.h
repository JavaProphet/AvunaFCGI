/*
 * util.h
 *
 *  Created on: Nov 17, 2015
 *      Author: root
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <stdlib.h>

void* __fcgi_xmalloc(size_t size);

void __fcgi_xfree(void* ptr);

void* __fcgi_xcalloc(size_t size);

void* __fcgi_xrealloc(void* ptr, size_t size);

#endif /* UTIL_H_ */
