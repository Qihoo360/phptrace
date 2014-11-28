#include <string.h>
#include <stdio.h>
#include "phptrace_string.h"

phptrace_str_t *phptrace_str_new(const char *str, int len){
	phptrace_str_t *ss;

	ss = (phptrace_str_t *)malloc(sizeof(phptrace_str_t) + sizeof(char) * len + 1);
    if(ss == NULL){
        return NULL;
    }

	ss->len = len;
    if(str){
        memcpy(ss->data, str, len);
    }
	ss->data[len] = '\0';

	return ss;
}

/* note: only free the malloc string, not mmap ones*/
void phptrace_str_free(phptrace_str_t *s){
	if (s){
        free(s);
    }
}

int phptrace_str_print(phptrace_str_t *s) {
	int i;
	if (s) {
		if (s->len == 0) {
			printf ("<Null>");
		}
		for (i = 0; i < s->len; i++)
			putchar(s->data[i]);
		return s->len;
	}
	return -1;
}
phptrace_str_t *phptrace_str_empty(){
	phptrace_str_t *ss;

	ss = (phptrace_str_t *)malloc(sizeof(phptrace_str_t) + 1);
    if(ss == NULL){
        return NULL;
    }
	ss->len = 0;
	ss->data[0] = '\0';
    return ss;
}
int phptrace_str_equal(phptrace_str_t *s1, phptrace_str_t *s2){
	if (!s1 || !s2)
		return s1 == s2;
	return s1->len == s2->len && strncmp(s1->data, s2->data, s1->len) == 0;
}

int phptrace_str_cmp(phptrace_str_t *s1, phptrace_str_t *s2){
    if(s1->len != s2->len){
        return s1->len - s2->len;
    }
    return strncmp(s1->data, s2->data, s1->len);
}

phptrace_str_t *phptrace_str_nconcat(phptrace_str_t *s, const char *t, size_t len){
    size_t tlen, slen;

    slen = s->len;
    tlen = len;

    len += slen + 1 + sizeof(phptrace_str_t);
    s = realloc(s, len);
    memcpy(s->data+slen, t, tlen);
    s->len = slen + tlen;
    s->data[s->len] = '\0';

    return s;
}
