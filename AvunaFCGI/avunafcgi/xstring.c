/*
 * xstring.c
 *
 *  Created on: Nov 17, 2015
 *      Author: root
 */
#include <string.h>
#include <ctype.h>

int __fcgi_streq_nocase(const char* str1, const char* str2) {
	if (str1 == NULL || str2 == NULL) return 0;
	if (str1 == str2) return 1;
	size_t l1 = strlen(str1);
	size_t l2 = strlen(str2);
	if (l1 != l2) return 0;
	for (int i = 0; i < l1; i++) {
		char s1 = str1[i];
		if (s1 >= 'A' && s1 <= 'Z') s1 += ' ';
		char s2 = str2[i];
		if (s2 >= 'A' && s2 <= 'Z') s2 += ' ';
		if (s1 != s2) {
			return 0;
		}
	}
	return 1;
}
