/**
 * Copyright 2015 Qihoo 360
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PHPTRACE_STRING_H
#define PHPTRACE_STRING_H
#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_PRINT_LENGTH 32

/* var string */
typedef struct phptrace_str_s{
    uint32_t len;
    char data[];
}phptrace_str_t;


#define phptrace_string(str) phptrace_str_new((str), strlen(str))
phptrace_str_t *phptrace_str_new(const char *str, int len);
phptrace_str_t *phptrace_str_empty();
void phptrace_str_free(phptrace_str_t *s);
int phptrace_str_print(phptrace_str_t *s, int max_len);

int phptrace_str_equal(phptrace_str_t *s1, phptrace_str_t *s2);
phptrace_str_t *phptrace_str_nconcat(phptrace_str_t *s, const char *t, size_t len);


#endif
