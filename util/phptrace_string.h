#ifndef PHPTRACE_STRING_H
#define PHPTRACE_STRING_H
#include <stdlib.h>
#include <stdint.h>
/* var string */
typedef struct phptrace_str_s{
    uint32_t len;
    char data[];
}phptrace_str_t;


#define phptrace_string(str) phptrace_str_new((str), strlen(str))
phptrace_str_t *phptrace_str_new(const char *str, int len);
phptrace_str_t *phptrace_str_empty();
void phptrace_str_free(phptrace_str_t *s);
int phptrace_str_print(phptrace_str_t *s);

int phptrace_str_equal(phptrace_str_t *s1, phptrace_str_t *s2);
phptrace_str_t *phptrace_str_nconcat(phptrace_str_t *s, const char *t, size_t len);


#endif
