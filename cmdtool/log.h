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

#ifndef PHPTRACE_LOG_H
#define PHPTRACE_LOG_H

#define MAX_LOGMSG_LEN 1024

#define LL_DEBUG   0
#define LL_INFO    1
#define LL_NOTICE  2
#define LL_ERROR   3

void log_level_set(int level);
int log_level_get();
void log_msg(int level, const char *msg);
void log_printf(int level, const char *fmt, ...);

#endif // PHPTRACE_LOG_H
