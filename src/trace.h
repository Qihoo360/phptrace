/**
 * Copyright 2016 Yuchen Wang <phobosw@gmail.com>
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

#ifndef TRACE_TRACE_H
#define TRACE_TRACE_H

int pt_trace_main(void);
int pt_send_msg_normal(int fd);
int pt_send_msg_filter(int fd);
int pt_trace_no_stop(int msg_type, uint8_t type);
int pt_trace_stop_limit(int msg_type, uint8_t type);
int pt_trace_stop_filter_url(int msg_type, uint8_t type);

#endif
