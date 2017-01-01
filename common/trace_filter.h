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
#ifndef TRACE_FILTER_H
#define TRACE_FILTER_H


#include <stdint.h>
#include "sds/sds.h"

#define MAX_FILTER_LENGTH       256
#define PT_FILTER_SIZE          sizeof(pt_filter_t)

/* Type codes of filter*/
#define PT_FILTER_EMPTY             (0x01<<0)
#define PT_FILTER_URL               (0x01<<1)
#define PT_FILTER_FUNCTION_NAME     (0x01<<2)
#define PT_FILTER_CLASS_NAME        (0x01<<3)

/* Filter name map filter type */
typedef struct  {
    char name[20];
    uint8_t type;
}filter_map_t;

/* Filter struct */
typedef struct {
    uint8_t type;                          /* filter type */
    sds content;                           /* filter content */
}pt_filter_t;

/* Filter function */
int pt_filter_pack_filter_msg(pt_filter_t *filter, char *buf);
int pt_filter_unpack_filter_msg(pt_filter_t *filter, char *buf);
void pt_filter_search_filter_type(char *type_name, uint8_t *filter_type);
void pt_filter_ctr(pt_filter_t *filter);
void pt_filter_dtr(pt_filter_t *filter); 
#endif
