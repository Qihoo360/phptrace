/**
 * Copyright 2017 Yuchen Wang <phobosw@gmail.com>
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

#ifndef TRACE_COLOR_H
#define TRACE_COLOR_H

/* http://ascii-table.com/ansi-escape-sequences.php */

#define PTC_RESET       "\x1b[0m"

#define PTC_BLACK       "\x1b[30m"
#define PTC_RED         "\x1b[31m"
#define PTC_GREEN       "\x1b[32m"
#define PTC_YELLOW      "\x1b[33m"
#define PTC_BLUE        "\x1b[34m"
#define PTC_MAGENTA     "\x1b[35m"
#define PTC_CYAN        "\x1b[36m"
#define PTC_WHITE       "\x1b[37m"

#define PTC_BBLACK      "\x1b[1;30m"
#define PTC_BRED        "\x1b[1;31m"
#define PTC_BGREEN      "\x1b[1;32m"
#define PTC_BYELLOW     "\x1b[1;33m"
#define PTC_BBLUE       "\x1b[1;34m"
#define PTC_BMAGENTA    "\x1b[1;35m"
#define PTC_BCYAN       "\x1b[1;36m"
#define PTC_BWHITE      "\x1b[1;37m"

#endif
