#ifndef PHP_PHPTRACE_H
#define PHP_PHPTRACE_H

extern zend_module_entry phptrace_module_entry;
#define phpext_phptrace_ptr &phptrace_module_entry

#define PHP_PHPTRACE_VERSION "0.3.0-beta"

#ifdef PHP_WIN32
#   define PHP_PHPTRACE_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#   define PHP_PHPTRACE_API __attribute__ ((visibility("default")))
#else
#   define PHP_PHPTRACE_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#include "phptrace_comm.h"
#include "phptrace_ctrl.h"

PHP_MINIT_FUNCTION(phptrace);
PHP_MSHUTDOWN_FUNCTION(phptrace);
PHP_RINIT_FUNCTION(phptrace);
PHP_RSHUTDOWN_FUNCTION(phptrace);
PHP_MINFO_FUNCTION(phptrace);

/**
 * Declare any global variables you may need between the BEGIN and END macros
 * here:
 */
ZEND_BEGIN_MODULE_GLOBALS(phptrace)
    zend_bool               enable;
    long                    dotrace;        /* flags of trace */

    char                    *data_dir;      /* data path, should be writable */

    phptrace_ctrl_t         ctrl;           /* ctrl module */
    char                    ctrl_file[256]; /* ctrl filename */

    phptrace_comm_socket    comm;           /* comm module */
    char                    comm_file[256]; /* comm filename */
    long                    send_size;      /* send handler size */
    long                    recv_size;      /* recv handler size */

    pid_t                   pid;            /* process id */
    uint32_t                level;          /* nesting level */

    uint32_t                ping;           /* last ping time (second) */
    uint32_t                idle_timeout;   /* idle timeout, for current - last ping */
ZEND_END_MODULE_GLOBALS(phptrace)


/**
 * In every utility function you add that needs to use variables in
 * php_phptrace_globals, call TSRMLS_FETCH(); after declaring other variables
 * used by that function, or better yet, pass in TSRMLS_CC after the last
 * function argument and declare your utility function with TSRMLS_DC after the
 * last declared argument.  Always refer to the globals in your function as
 * PHPTRACE_G(variable).  You are encouraged to rename these macros something
 * shorter, see examples in any other php module directory.
 */

#ifdef ZTS
#define PTG(v) TSRMG(phptrace_globals_id, zend_phptrace_globals *, v)
#else
#define PTG(v) (phptrace_globals.v)
#endif

#endif  /* PHP_PHPTRACE_H */
