/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2015 The PHP Group                                |
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_phptrace.h"

/* If you declare any globals in php_phptrace.h uncomment this: */
ZEND_DECLARE_MODULE_GLOBALS(phptrace)

/* True global resources - no need for thread safety here */
static int le_phptrace;

/* {{{ phptrace_functions[]
 *
 * Every user visible function must have an entry in phptrace_functions[].
 */
const zend_function_entry phptrace_functions[] = {
    PHP_FE_END  /* Must be the last line in phptrace_functions[] */
};
/* }}} */

/* {{{ phptrace_module_entry
 */
zend_module_entry phptrace_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "phptrace",
    phptrace_functions,
    PHP_MINIT(phptrace),
    PHP_MSHUTDOWN(phptrace),
    PHP_RINIT(phptrace),        /* Replace with NULL if there's nothing to do at request start */
    PHP_RSHUTDOWN(phptrace),    /* Replace with NULL if there's nothing to do at request end */
    PHP_MINFO(phptrace),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_PHPTRACE_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PHPTRACE
ZEND_GET_MODULE(phptrace)
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("phptrace.enabled",      "0", PHP_INI_ALL, OnUpdateLong, enabled, zend_phptrace_globals, phptrace_globals)
PHP_INI_END()
/* }}} */

/* {{{ php_phptrace_init_globals
 */
/* Uncomment this function if you have INI entries */
static void php_phptrace_init_globals(zend_phptrace_globals *phptrace_globals)
{
    phptrace_globals->enabled = 0;
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(phptrace)
{
    /* If you have INI entries, uncomment these lines */
    REGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(phptrace)
{
    /* uncomment this line if you have INI entries */
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(phptrace)
{
    return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(phptrace)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(phptrace)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "phptrace support", "enabled");
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini */
    DISPLAY_INI_ENTRIES();
}
/* }}} */
