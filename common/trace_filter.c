/**
 * Copyright 2017 Bing Bai <silkcutbeta@gmail.com>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "trace_filter.h"
#include "trace_type.h"

filter_map_t filter_name_map_type[] = {
    {"url",             PT_FILTER_URL},
    {"function",        PT_FILTER_FUNCTION_NAME},
    {"class",           PT_FILTER_CLASS_NAME}
};

void pt_filter_search_filter_type(char *type_name, uint8_t *filter_type)
{
    if (type_name != NULL && type_name[0] == '\0') {
        return; 
    }
    size_t n = sizeof(filter_name_map_type)/sizeof(filter_map_t);
    int i = 0;
    for (i = 0; i < n; i++) {
        if (strcmp(filter_name_map_type[i].name, type_name) == 0) {
            *filter_type = filter_name_map_type[i].type;
            break;
        }
    }
}

int pt_filter_pack_filter_msg(pt_filter_t *filter, char *buf)
{
    char *org = buf;
    PACK(buf, uint8_t, filter->type);
    PACK_SDS(buf, filter->content);
    return buf - org;
}

int pt_filter_unpack_filter_msg(pt_filter_t *filter, char *buf)
{
    char *org = buf;
    size_t len;
    UNPACK(buf, uint8_t, filter->type);
    UNPACK_SDS(buf, filter->content);
    return buf - org;
}

void pt_filter_ctr(pt_filter_t *filter) 
{
    filter->type = PT_FILTER_EMPTY;
    if (filter->content == NULL) {
        filter->content = sdsempty();   
    }
}

void pt_filter_dtr(pt_filter_t *filter) 
{
    if (filter->content != NULL) {
        sdsfree(filter->content);   
    }
    filter->content = NULL;
    filter->type = PT_FILTER_EMPTY;
}
