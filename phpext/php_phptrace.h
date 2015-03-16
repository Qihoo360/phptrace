/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2014 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifndef PHP_PHPTRACE_H
#define PHP_PHPTRACE_H
#include "phptrace_mmap.h"
#include "sds/sds.h"

extern zend_module_entry phptrace_module_entry;
#define phpext_phptrace_ptr &phptrace_module_entry

#define PHP_PHPTRACE_VERSION "0.2.3" /* Replace with version number for your extension */
#define PHPTRACE_UNIT_TEST 0

#ifdef PHP_WIN32
#    define PHP_PHPTRACE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#    define PHP_PHPTRACE_API __attribute__ ((visibility("default")))
#else
#    define PHP_PHPTRACE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(phptrace);
PHP_MSHUTDOWN_FUNCTION(phptrace);
PHP_RINIT_FUNCTION(phptrace);
PHP_RSHUTDOWN_FUNCTION(phptrace);
PHP_MINFO_FUNCTION(phptrace);

PHP_FUNCTION(phptrace_info);
#if PHPTRACE_UNIT_TEST
PHP_FUNCTION(phptrace_mmap_test);
#endif // PHPTRACE_UNIT_TEST

typedef struct phptrace_status_s {
    /*core global info*/
    sds core_last_error;
    sds user_ini_filename;
    /*request info*/
    sds request_line;
    sds request_headers;
    double request_time;
    /*memory usage*/
    unsigned long long memory_usage;
    unsigned long long memory_peak_usage;
    unsigned long long real_memory_usage;
    unsigned long long real_memory_peak_usage;
    /*executor info*/
    sds stack;
} phptrace_status_t;
typedef struct phptrace_context_s{
    pid_t pid;
    int cli;
    unsigned long long heartbeat;
    unsigned int heartbeat_timedout;
    phptrace_segment_t ctrl;   
    phptrace_segment_t tracelog;   
    void *shmoffset;
    int rotate;
    int rotate_count;
    unsigned long long seq;
    int level;

    struct phptrace_status_s status;
}phptrace_context_t;
ZEND_BEGIN_MODULE_GLOBALS(phptrace)
    long  enabled;
    long  dotrace;
    long  logsize;
    long  pid_max;
    char *logdir;
    phptrace_context_t ctx;
ZEND_END_MODULE_GLOBALS(phptrace)

/* In every utility function you add that needs to use variables 
   in php_phptrace_globals, call TSRMLS_FETCH(); after declaring other 
   variables used by that function, or better yet, pass in TSRMLS_CC
   after the last function argument and declare your utility function
   with TSRMLS_DC after the last declared argument.  Always refer to
   the globals in your function as PHPTRACE_G(variable).  You are 
   encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/

#ifdef ZTS
#define PHPTRACE_G(v) TSRMG(phptrace_globals_id, zend_phptrace_globals *, v)
#else
#define PHPTRACE_G(v) (phptrace_globals.v)
#endif

#endif    /* PHP_PHPTRACE_H */
