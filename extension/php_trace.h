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

#ifndef PHP_TRACE_H
#define PHP_TRACE_H

extern zend_module_entry trace_module_entry;
#define phpext_trace_ptr &trace_module_entry

#define PHP_TRACE_VERSION "0.3.0"

#ifdef PHP_WIN32
#   define PHP_TRACE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#   define PHP_TRACE_API __attribute__ ((visibility("default")))
#else
#   define PHP_TRACE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#include "trace_comm.h"
#include "trace_ctrl.h"

PHP_MINIT_FUNCTION(trace);
PHP_MSHUTDOWN_FUNCTION(trace);
PHP_RINIT_FUNCTION(trace);
PHP_RSHUTDOWN_FUNCTION(trace);
PHP_MINFO_FUNCTION(trace);

/**
 * Declare any global variables you may need between the BEGIN and END macros
 * here:
 */
ZEND_BEGIN_MODULE_GLOBALS(trace)
    zend_bool               enable;
    long                    dotrace;        /* flags of trace */

    char                    *data_dir;      /* data path, should be writable */

    pt_ctrl_t               ctrl;           /* ctrl module */
    char                    ctrl_file[256]; /* ctrl filename */

    pt_comm_socket_t        comm;           /* comm module */
    char                    comm_file[256]; /* comm filename */

    pid_t                   pid;            /* process id */
    long                    level;          /* nesting level */

    long                    ping;           /* last ping time (second) */
    long                    idle_timeout;   /* idle timeout, for current - last ping */
ZEND_END_MODULE_GLOBALS(trace)


/**
 * In every utility function you add that needs to use variables in
 * php_trace_globals, call TSRMLS_FETCH(); after declaring other variables
 * used by that function, or better yet, pass in TSRMLS_CC after the last
 * function argument and declare your utility function with TSRMLS_DC after the
 * last declared argument.  Always refer to the globals in your function as
 * TRACE_G(variable).  You are encouraged to rename these macros something
 * shorter, see examples in any other php module directory.
 */

#ifdef ZTS
#define PTG(v) TSRMG(trace_globals_id, zend_trace_globals *, v)
#else
#define PTG(v) (trace_globals.v)
#endif

#endif  /* PHP_TRACE_H */
