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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "SAPI.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_phptrace.h"
#include "phptrace_mmap.h"
#include "phptrace_protocol.h"
#include "phptrace_time.h"
#include "sds/sds.h"

#ifndef PHP_FE_END
#define PHP_FE_END {NULL, NULL, NULL, 0, 0}
#endif

#define PHPTRACE_LOG_SIZE 52428800
#define _STR(s) #s
#define PHPTRACE_CONST_STR(s) _STR(s)
#define HEARTBEAT_TIMEDOUT 30 /*30 seconds*/
#define HEARTBEAT_FLAG (1<<7)
#define REOPEN_FLAG (1<<1)
#define STATUS_FLAG (1<<2)

#if PHP_VERSION_ID < 50500
void (*phptrace_old_execute)(zend_op_array *op_array TSRMLS_DC);
void phptrace_execute(zend_op_array *op_array TSRMLS_DC);

void (*phptrace_old_execute_internal)(zend_execute_data *current_execute_data, int return_value_used TSRMLS_DC);
void phptrace_execute_internal(zend_execute_data *current_execute_data, int return_value_used TSRMLS_DC);
#else
void (*phptrace_old_execute_ex)(zend_execute_data *execute_data TSRMLS_DC);
void phptrace_execute_ex(zend_execute_data *execute_data TSRMLS_DC);

void (*phptrace_old_execute_internal)(zend_execute_data *current_execute_data, struct _zend_fcall_info *fci, int return_value_used TSRMLS_DC);
void phptrace_execute_internal(zend_execute_data *current_execute_data, struct _zend_fcall_info *fci, int return_value_used TSRMLS_DC);
#endif

ZEND_DECLARE_MODULE_GLOBALS(phptrace)

/* True global resources - no need for thread safety here */
static int le_phptrace;
extern sapi_module_struct sapi_module;

/* {{{ phptrace_functions[]
 *
 * Every user visible function must have an entry in phptrace_functions[].
 */
const zend_function_entry phptrace_functions[] = {
    PHP_FE(phptrace_info,    NULL)
#if PHPTRACE_UNIT_TEST
    PHP_FE(phptrace_mmap_test,   NULL)
#endif // PHPTRACE_UNIT_TEST
    PHP_FE_END    /* Must be the last line in phptrace_functions[] */
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
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("phptrace.enabled",      "0", PHP_INI_ALL, OnUpdateLong, enabled, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.dotrace",      "0", PHP_INI_ALL, OnUpdateLong, dotrace, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.logsize",      PHPTRACE_CONST_STR(PHPTRACE_LOG_SIZE), PHP_INI_ALL, OnUpdateLong, logsize, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.pid_max",      PHPTRACE_CONST_STR(PID_MAX), PHP_INI_ALL, OnUpdateLong, pid_max, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.logdir",      NULL, PHP_INI_ALL, OnUpdateString, logdir, zend_phptrace_globals, phptrace_globals)
PHP_INI_END()
/* }}} */

/* {{{ php_phptrace_init_globals
 */
static void php_phptrace_init_globals(zend_phptrace_globals *phptrace_globals)
{
    phptrace_globals->enabled = 0;
    phptrace_globals->dotrace = 0;
    phptrace_globals->logsize = PHPTRACE_LOG_SIZE;
    phptrace_globals->logdir = NULL;
    phptrace_globals->pid_max = PID_MAX;
    memset(&phptrace_globals->ctx, 0, sizeof(phptrace_context_t));
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(phptrace)
{
    char filename[256];
    struct stat st;
    ZEND_INIT_MODULE_GLOBALS(phptrace, php_phptrace_init_globals, NULL);
    REGISTER_INI_ENTRIES();
    if (!PHPTRACE_G(enabled)) {
        return SUCCESS;
    }
    if (!PHPTRACE_G(logdir)) {
        PHPTRACE_G(logdir) = PHPTRACE_LOG_DIR;
    }
    if (PHPTRACE_G(logsize) < PHPTRACE_LOG_SIZE) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "phptrace.logsize is too small"
                " to run trace, expected >= %d , actually is %ld",
                PHPTRACE_LOG_SIZE, PHPTRACE_G(logsize));
    }
    /*phptrace enabled*/
#if PHP_VERSION_ID < 50500
    phptrace_old_execute = zend_execute;
    zend_execute = phptrace_execute;
#else
    phptrace_old_execute_ex = zend_execute_ex;
    zend_execute_ex = phptrace_execute_ex;
#endif
    phptrace_old_execute_internal = zend_execute_internal;
    zend_execute_internal = phptrace_execute_internal;
    /* Create contrl mmap file
     * */
    phptrace_context_t *ctx;
    ctx = &PHPTRACE_G(ctx);
    snprintf(filename, sizeof(filename), "%s/%s", PHPTRACE_G(logdir), PHPTRACE_CTRL_FILENAME);
    if (stat(filename, &st) == -1) {
        if (errno == ENOENT) {
            ctx->ctrl = phptrace_mmap_create(filename, PHPTRACE_G(pid_max)+1);
            memset(ctx->ctrl.shmaddr, 0, PHPTRACE_G(pid_max)+1);
        } else {
            php_error_docref(NULL TSRMLS_CC, E_WARNING, "phptrace_mmap_create %s failed: %s", filename, strerror(errno));
            return FAILURE;
        }
    } else if (st.st_size < PHPTRACE_G(pid_max)+1) {
        /*Reopen phptrace.ctrl and truncate to a correct size
         *This is useful when upgrade phpext and increment the size of phptrace.ctrl
         * */
        ctx->ctrl = phptrace_mmap_create(filename, PHPTRACE_G(pid_max)+1);
    } else {
        ctx->ctrl = phptrace_mmap_write(filename);
    }
    if (ctx->ctrl.shmaddr == MAP_FAILED) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "phptrace_mmap_write %s failed: %s", filename, strerror(errno));
        return FAILURE;
    }
    if (sapi_module.name[0]=='c' &&
        sapi_module.name[1]=='l' &&
        sapi_module.name[2]=='i') {
        ctx->cli = 1;
    }
    ctx->heartbeat_timedout = HEARTBEAT_TIMEDOUT;
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(phptrace)
{
    char filename[256];
    phptrace_context_t *ctx;

    if (!PHPTRACE_G(enabled)) {
        UNREGISTER_INI_ENTRIES();
        return SUCCESS;
    }

#if PHP_VERSION_ID < 50500
    zend_execute = phptrace_old_execute;
#else
    zend_execute_ex = phptrace_old_execute_ex;
#endif
    zend_execute_internal = phptrace_old_execute_internal;

    ctx = &PHPTRACE_G(ctx);
    snprintf(filename, sizeof(filename), "%s/%s", PHPTRACE_G(logdir), PHPTRACE_CTRL_FILENAME);
    phptrace_unmap(&ctx->ctrl);
    if (ctx->tracelog.shmaddr) {
        phptrace_unmap(&ctx->ctrl);
        snprintf(filename, sizeof(filename), "%s/%s.%d", PHPTRACE_G(logdir), PHPTRACE_TRACE_FILENAME, ctx->pid);
        unlink(filename);
    }
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(phptrace)
{
    return SUCCESS;
}
/* }}} */

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

    DISPLAY_INI_ENTRIES();
}
/* }}} */


/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string phptrace_info()
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(phptrace_info)
{
    int len;
    char *strg;

    len = spprintf(&strg, 0, "Congratulations! Module %.78s is now compiled into PHP, and the version is %s .", "phptrace", PHP_PHPTRACE_VERSION);
    RETURN_STRINGL(strg, len, 0);
}
/* }}} */

typedef struct phptrace_execute_data_s{
    zend_op_array *op_array;
    zend_execute_data *current_execute_data;
    int return_value_used;
    struct _zend_fcall_info *fci;

    int internal;
}phptrace_execute_data;

sds phptrace_repr_zval(zval *val TSRMLS_DC)
{
    char value[128], *type_name;
    switch (Z_TYPE_P(val)) {
        case IS_NULL:
            sprintf(value, "nil");
            break;
        case IS_LONG:
            sprintf(value, "%ld", Z_LVAL_P(val));
            break;
        case IS_DOUBLE:
            sprintf(value, "%f", Z_DVAL_P(val));
            break;
        case IS_BOOL:
            sprintf(value, "%s", Z_LVAL_P(val)?"true":"false");
            break;
        case IS_STRING:
            return sdsnewlen(Z_STRVAL_P(val), Z_STRLEN_P(val));
        case IS_ARRAY:
            sprintf(value, "array<%p>", Z_ARRVAL_P(val));
            break;
        case IS_OBJECT:
            sprintf(value, "object<%p>", &Z_OBJVAL_P(val));
            break;
        case IS_RESOURCE:
            type_name = (char *) zend_rsrc_list_get_rsrc_type(Z_LVAL_P(val) TSRMLS_CC);
            sprintf(value, "%s(%ld)", type_name, Z_LVAL_P(val));
            break;
        default:
            sprintf(value, "<unknown>");
    }
    return sdsnewlen(value, strlen(value));
}

#define EXF_COMMON(e) ex->function_state.function->common.e
#define EXF(e) ex->function_state.function->e
sds phptrace_get_funcname(zend_execute_data *ex TSRMLS_DC)
{
    sds funcname;
    const char *scope;
    const char *name;

    /*Just return if there is no function name*/
    if (ex == NULL || 
            ex->function_state.function == NULL || 
            EXF_COMMON(function_name) == NULL) {
        return NULL;
    }

    if (ex->object && EXF_COMMON(scope)) {
        /*This is an object, so the function should be a member of a class.
         *Get the scope which is the class's name
         * */
        scope = EXF_COMMON(scope->name);
    }
    else if (EG(scope) &&
             EXF_COMMON(scope) &&
             EXF_COMMON(scope->name)) {
        /*static member of class*/
        scope = EXF_COMMON(scope->name);
    }else{
        scope = NULL;
    }
    if (strncmp(EXF_COMMON(function_name), "{closure}", sizeof("closure")-1) == 0) {
        /*a closure may be a chunk of code, get the filename and line range*/
        /*TODO process a closure*/
        /*EXF(op_array.filename);
        EXF(op_array.line_start);
        EXF(op_array.line_end);*/
    }
    name = EXF_COMMON(function_name);
    if (scope) {
        funcname = sdsnewlen(scope, strlen(scope));
        if(ex->object){
            funcname = sdscat(funcname, "->");
        }else{
            funcname = sdscat(funcname, "::");
        }
        funcname = sdscat(funcname, name);
    } else {
        funcname = sdsnew(name);
    }
    return funcname;
}
sds phptrace_get_parameters(zend_execute_data *ex TSRMLS_DC)
{
    sds parameters, param;
    zend_arg_info *arg_info;
    zval **args;
    long n, i, narg_info;

    if (ex == NULL || 
            ex->function_state.function == NULL || 
            EXF_COMMON(num_args) == 0) {
        return NULL;
    }
    n = 0;
#if PHP_VERSION_ID >=50300
    n = (long)(zend_uintptr_t)*(ex->function_state.arguments);
    args = (zval **)(ex->function_state.arguments) - n;
#else
    if (EG(argument_stack).top >= 2) { 
        void **p;
        p = EG(argument_stack).top_element - 2; 
        n = (long) *p;
        args = p - n;
    } 
#endif
    arg_info = EXF_COMMON(arg_info);
    narg_info  = EXF_COMMON(num_args);

    parameters = sdsempty();
    for (i = 0; i < n; ++i) {
        /*The format $p = "hello", $b = "world"*/
        if (i < narg_info) {
            parameters = sdscat(parameters, "$");
            parameters = sdscatlen(parameters, arg_info[i].name, arg_info[i].name_len);
        } else {
            parameters = sdscat(parameters, "optional");
        }
        parameters = sdscat(parameters, " = ");
        param = phptrace_repr_zval(args[i] TSRMLS_CC);
        parameters = sdscatsds(parameters, param);
        sdsfree(param);
        if(i != n-1){
            parameters = sdscat(parameters, ", ");
        }
    }
    return parameters;
}
sds phptrace_get_return_value(zval *return_value TSRMLS_DC)
{
    if (return_value) {
        return phptrace_repr_zval(return_value TSRMLS_CC); 
    } else {
        return NULL;
    }
}

void phptrace_reset_tracelog(phptrace_context_t *ctx TSRMLS_DC)
{
    char filename[256];

    snprintf(filename, sizeof(filename), "%s/%s.%d", PHPTRACE_G(logdir),PHPTRACE_TRACE_FILENAME, ctx->pid);
    phptrace_unmap(&ctx->tracelog);
    ctx->tracelog.shmaddr = NULL;
    ctx->tracelog.size = 0;
    unlink(filename);
    ctx->seq = 0;
    ctx->rotate = 0;
    ctx->shmoffset = NULL;
    ctx->heartbeat = 0;
}
void phptrace_get_callinfo(phptrace_file_record_t *record, zend_execute_data *ex TSRMLS_DC)
{
    sds funcname, parameters;
    funcname = phptrace_get_funcname(ex TSRMLS_CC);
    if (funcname) {
        record->function_name = funcname;
    }

    parameters = phptrace_get_parameters(ex TSRMLS_CC);
    if (parameters) {
        RECORD_ENTRY(record, params) = parameters;
    }
}
void phptrace_register_return_value(zval **return_value TSRMLS_DC)
{
    if (EG(return_value_ptr_ptr) == NULL) {
        EG(return_value_ptr_ptr) = return_value;
    }
}
void phptrace_get_execute_return_value(phptrace_file_record_t *record, zval *return_value TSRMLS_DC)
{
    if (return_value) {
        /*registered by phptrace*/
        RECORD_EXIT(record, return_value) = phptrace_get_return_value(return_value TSRMLS_CC);
        zval_ptr_dtor(EG(return_value_ptr_ptr));
        EG(return_value_ptr_ptr) = NULL;
    } else if (*EG(return_value_ptr_ptr)) {
        /*registered by someone else, just fetch it*/
        RECORD_EXIT(record, return_value) = phptrace_get_return_value(*EG(return_value_ptr_ptr) TSRMLS_CC);
    }

}
void phptrace_get_execute_internal_return_value(phptrace_file_record_t *record, zend_execute_data *ex TSRMLS_DC)
{
    zend_op *opline;
    zval *return_value;

    /* Ensure that there is no exception occurs
     * When some exception was thrown, opline would
     * be replaced by the Exception opline. It may
     * cause a segfault if we get the return value
     * from a exception opline.
     * */
    if (*EG(opline_ptr) && !EG(exception)) {
        opline = *EG(opline_ptr);
#if PHP_VERSION_ID >= 50500
        if (opline && opline->result_type & 0x0F) {
            return_value = EX_TMP_VAR(ex, ex->opline->result.var)->var.ptr;
#elif PHP_VERSION_ID >= 50400
        if (opline && opline->result_type & 0x0F) {
            return_value = (*(temp_variable *)((char *) ex->Ts + ex->opline->result.var)).var.ptr;
#else
        if (opline && opline->result.op_type & 0x0F) {
            return_value = (*(temp_variable *)((char *) ex->Ts + ex->opline->result.u.var)).var.ptr;
#endif
            if (return_value) {
                RECORD_EXIT(record, return_value) = phptrace_get_return_value(return_value TSRMLS_CC);
            }
        }
    }
}

void phptrace_print_callinfo(phptrace_file_record_t *record)
{
    printf("%lu %d %f ",record->seq, record->level, record->start_time/1000000.0);
    if (record->function_name) {
        printf("%s ", record->function_name);
    } else {
        printf("- ");
    }
    if (RECORD_ENTRY(record, params)) {
        printf("(%s) ", RECORD_ENTRY(record, params));
    } else {
        printf("() ");
    }
}
void phptrace_print_call_result(phptrace_file_record_t *record)
{
    printf(" = %s ", RECORD_EXIT(record, return_value));
    printf("%f\n", RECORD_EXIT(record, cost_time)/1000000.0);
}

void phptrace_get_php_status(phptrace_status_t *status TSRMLS_DC)
{
    int  last_error_type;
    char *last_error_message;
    char *last_error_file;
    int  last_error_lineno;
    double request_time;

    zend_execute_data *ex;

    size_t memory_usage;
    size_t memory_peak_usage;
    size_t real_memory_usage;
    size_t real_memory_peak_usage;

    sapi_request_info request_info;

    last_error_type = PG(last_error_type);
    last_error_message = PG(last_error_message);
    last_error_file = PG(last_error_file);
    last_error_lineno = PG(last_error_lineno);

#if PHP_VERSION_ID < 50500
    ex = EG(current_execute_data);
#else
    ex = EG(current_execute_data)->prev_execute_data;
#endif

    memory_usage = zend_memory_usage(0 TSRMLS_CC);
    memory_peak_usage = zend_memory_peak_usage(0 TSRMLS_CC);
    real_memory_usage = zend_memory_usage(1 TSRMLS_CC);
    real_memory_peak_usage = zend_memory_peak_usage(1 TSRMLS_CC);

    request_info = SG(request_info);
    request_time = SG(global_request_time);

    if (last_error_message) {
        status->core_last_error = sdscatprintf(sdsempty(), "[%d] %s %s:%d",
                last_error_type, last_error_message, last_error_file, last_error_lineno);
    }

    if (request_info.request_method) {
        status->request_line = sdscatprintf(sdsempty(), "%s %s%s%s HTTP/%d.%d", request_info.request_method,
                request_info.request_uri,
                request_info.query_string && *request_info.query_string ? "?":"",
                request_info.query_string ? request_info.query_string:"",
                request_info.proto_num/1000,
                request_info.proto_num - 1000);
    }

    status->request_time = request_time;

    status->stack = sdsempty();
    while(ex) {
        status->stack = sdscatprintf(status->stack, "[%p] %s(%s) %s:%d\n", ex,
               phptrace_get_funcname(ex TSRMLS_CC),
               phptrace_get_parameters(ex TSRMLS_CC),
               zend_get_executed_filename(TSRMLS_C),
               zend_get_executed_lineno(TSRMLS_C));
        ex = ex->prev_execute_data;
    }

    status->memory_usage = memory_usage;
    status->memory_peak_usage = memory_peak_usage;
    status->real_memory_usage = real_memory_usage;
    status->real_memory_peak_usage = real_memory_peak_usage;
}
void phptrace_init_php_status(phptrace_status_t *status)
{
    memset(status, 0, sizeof(phptrace_status_t));
}
void phptrace_destroy_php_status(phptrace_status_t *status)
{
    if (status->core_last_error) {
        sdsfree(status->core_last_error);
    }
    if (status->request_line) {
        sdsfree(status->request_line);
    }
    if (status->stack) {
        sdsfree(status->stack);
    }
}
void phptrace_dump_php_status(phptrace_status_t *status)
{
    FILE *fp;
    char filename[256], tmp[256];
    phptrace_context_t *ctx;
    ctx = &PHPTRACE_G(ctx);

    sprintf(filename, "%s/%s.%d", PHPTRACE_LOG_DIR, PHPTRACE_STATUS_FILENAME, ctx->pid);
    sprintf(tmp, "%s/%s.%d.tmp", PHPTRACE_LOG_DIR, PHPTRACE_STATUS_FILENAME, ctx->pid);
    fp = fopen(tmp, "w");
    if (fp == NULL) {
        return;
    }
    if (status->core_last_error) {
        fprintf(fp, "Last error\n");
        fprintf(fp, "%s\n\n", status->core_last_error);
    }
    if (status->memory_usage) {
        fprintf(fp, "Memory\n");
        fprintf(fp, "usage: %d\npeak_usage:%d\n", status->memory_usage, status->memory_peak_usage);
        fprintf(fp, "real_usage: %d\nreal_peak_usage:%d\n\n", status->real_memory_usage,
                status->real_memory_peak_usage);
    }
    if (status->request_line) {
        fprintf(fp, "Request\n");
        fprintf(fp, "%s\n", status->request_line);
    }
    if (status->request_time) {
        fprintf(fp, "request_time:%f\n\n", status->request_time);
    }
    if (status->stack) {
        fprintf(fp, "Stack\n");
        fprintf(fp, "%s\n", status->stack);
    }
    fclose(fp);
    rename(tmp, filename);
}
void phptrace_execute_core(zend_execute_data *ex, phptrace_execute_data *px TSRMLS_DC)
{
    uint64_t now, cpu_time;
    int64_t memory_usage, memory_peak_usage;
    uint8_t *ctrl;
    char filename[256];
    const char *p;
    zval *return_value;
    phptrace_context_t *ctx;
    phptrace_status_t status;

    phptrace_file_record_t record;
    phptrace_file_tailer_t tailer = {MAGIC_NUMBER_TAILER, 0};
    phptrace_file_header_t header = {MAGIC_NUMBER_HEADER, 1, 0};

    if (ex == NULL) {
        goto exec;
    }


    ctx = &PHPTRACE_G(ctx);
    ++ctx->level;
    ctrl = ctx->ctrl.shmaddr;
    if (ctx->pid == 0) {
        ctx->pid = getpid();
    }

    if (ctrl[ctx->pid] & STATUS_FLAG) {
        phptrace_init_php_status(&status);
        phptrace_get_php_status(&status TSRMLS_CC);
        phptrace_dump_php_status(&status);
        phptrace_destroy_php_status(&status);
        ctrl[ctx->pid] &= ~STATUS_FLAG & 0x00FF;
    }

    if ((ctrl[ctx->pid] & 0x01) == 0 && !PHPTRACE_G(dotrace)) {
        /*unmap tracelog if phptrace was shutdown*/
        if (ctx->tracelog.shmaddr) {
            phptrace_reset_tracelog(ctx TSRMLS_CC);
        }
        goto exec;
    
    }

    if (ctrl[ctx->pid] & HEARTBEAT_FLAG) {
        ctrl[ctx->pid] &= ~HEARTBEAT_FLAG & 0x00FF;
        ctx->heartbeat = phptrace_time_usec();
    }
    if (((ctrl[ctx->pid] & HEARTBEAT_FLAG) == 0) && ctx->heartbeat) {
        now = phptrace_time_usec();
        if (now - ctx->heartbeat > ctx->heartbeat_timedout * 1000000) {
            ctrl[ctx->pid] = 0;
            phptrace_reset_tracelog(ctx TSRMLS_CC);
            goto exec;
        }
    }

    if (ctrl[ctx->pid] & REOPEN_FLAG) {
        ctrl[ctx->pid] &= ~REOPEN_FLAG & 0x00FF;
        phptrace_reset_tracelog(ctx TSRMLS_CC);
    }

    snprintf(filename, sizeof(filename), "%s/%s.%d", PHPTRACE_G(logdir), PHPTRACE_TRACE_FILENAME, ctx->pid);
    /*do trace*/
    /*first startup*/
    if (ctx->tracelog.shmaddr == NULL) {
        ctx->tracelog = phptrace_mmap_create(filename, PHPTRACE_G(logsize));
        if (ctx->tracelog.shmaddr == MAP_FAILED) {
            ctx->tracelog.shmaddr = NULL;
            goto exec;
        }
        ctx->shmoffset = ctx->tracelog.shmaddr;
        /*write into waitflag immediately after mmapping*/
        phptrace_mem_write_waitflag(ctx->shmoffset);
        ctx->rotate = 1;
        ctx->rotate_count = 0;
    }
    /*should do rotate*/
    if ((ctx->shmoffset - ctx->tracelog.shmaddr) > PHPTRACE_G(logsize)-1024*1024) { /*FIXME Use a more safty condition to rotate*/
        ctx->rotate = 1;
    }

    /*ex MUST NOT be NULL*/
    if (ctx->rotate) {
        ctx->rotate = 0;
        ++ctx->rotate_count;

        /*set waitflag at header before rotate*/
        phptrace_mem_write_waitflag(ctx->tracelog.shmaddr);

        tailer.filename = sdsnew(filename);
        ctx->shmoffset = phptrace_mem_write_tailer(&tailer, ctx->shmoffset);
        sdsfree(tailer.filename);

        ctx->shmoffset = ctx->tracelog.shmaddr;
        ctx->shmoffset = phptrace_mem_write_header(&header, ctx->shmoffset);
        phptrace_mem_write_waitflag(ctx->shmoffset);
    }

    RECORD_ENTRY(&record, params) = NULL;
    record.function_name = NULL;
    p = zend_get_executed_filename(TSRMLS_C);
    RECORD_ENTRY(&record, filename) = sdsnew(p);
    RECORD_ENTRY(&record, lineno) = zend_get_executed_lineno(TSRMLS_C);

    phptrace_get_callinfo(&record, ex TSRMLS_CC);

    if (!px->internal) {
        /* register return_values's address
         * it will be setting if needed after execute
         * */
        return_value = NULL;
        phptrace_register_return_value(&return_value TSRMLS_CC);
    }

    memory_usage = zend_memory_usage(0 TSRMLS_CC);
    memory_peak_usage = zend_memory_peak_usage(0 TSRMLS_CC);

    record.seq = ctx->seq ++;
    record.level = ctx->level;
    record.start_time = phptrace_time_usec();

    if (ctx->cli && PHPTRACE_G(dotrace)) {
        phptrace_print_callinfo(&record);
    }

    record.flag = RECORD_FLAG_ENTRY;
    ctx->shmoffset = phptrace_mem_write_record(&record, ctx->shmoffset);
    phptrace_mem_write_waitflag(ctx->shmoffset);

    /* params and filename MUST be freed before we
     * assign cost_time and return_value.
     * It maybe hear strange to you; but this is true !
     * These all because the member entry and exit of
     * structure phptrace_record_t are UNION. The entry member
     * will be overwritten when set some value to exit.
     * TODO : use a more appropriate design
     * */
    sdsfree(RECORD_ENTRY(&record, params));
    sdsfree(RECORD_ENTRY(&record, filename));

    RECORD_EXIT(&record, cost_time) = 0;
    RECORD_EXIT(&record, return_value) = NULL;

    cpu_time = phptrace_cputime_usec();
    zend_try {
        if (!px->internal) {
#if PHP_VERSION_ID < 50500
            phptrace_old_execute(px->op_array TSRMLS_CC);
#else
            phptrace_old_execute_ex(px->current_execute_data TSRMLS_CC);
#endif
        } else {
#if PHP_VERSION_ID < 50500
            if (phptrace_old_execute_internal) {
                phptrace_old_execute_internal(px->current_execute_data, px->return_value_used TSRMLS_CC);
            } else {
                execute_internal(px->current_execute_data, px->return_value_used TSRMLS_CC);
            }
#else
            if (phptrace_old_execute_internal) {
                phptrace_old_execute_internal(px->current_execute_data, px->fci, px->return_value_used TSRMLS_CC);
            } else {
                execute_internal(px->current_execute_data, px->fci, px->return_value_used TSRMLS_CC);
            }
#endif
        }
    } zend_catch {
        /* do some cleanup when catch an exception
         * remeber to free the memory we allocated whether explicitly or implicitly
         * */
        --ctx->level;
        sdsfree(record.function_name);
        if (!px->internal && return_value) {
            /*registered by phptrace, dtor the return value other causes a memleak*/
            zval_ptr_dtor(EG(return_value_ptr_ptr));
            EG(return_value_ptr_ptr) = NULL;
        }
        zend_bailout();
    }zend_end_try();

    RECORD_EXIT(&record, cpu_time) = phptrace_cputime_usec() - cpu_time;
    -- ctx->level;
    now = phptrace_time_usec();

    if (!px->internal) {
        /* return_value was setted
         * now get the value
         * */
        phptrace_get_execute_return_value(&record, return_value TSRMLS_CC);
    } else {
#if PHP_VERSION_ID >= 50500
        /* if px->fci is set, return value will be stored in *px->fci->retval_ptr_ptr
         * see what execute_internal dose in Zend/zend_execute.c
         * */
        if (px->fci && px->fci->retval_ptr_ptr && *px->fci->retval_ptr_ptr) {
            RECORD_EXIT(&record, return_value) = phptrace_repr_zval(*px->fci->retval_ptr_ptr TSRMLS_CC);
        } else {
            phptrace_get_execute_internal_return_value(&record, ex TSRMLS_CC);
        }
#else
            phptrace_get_execute_internal_return_value(&record, ex TSRMLS_CC);
#endif
    }

    RECORD_EXIT(&record, cost_time) = now - record.start_time;
    /*write the return value and cost time*/
    if (RECORD_EXIT(&record, return_value) == NULL) {
        RECORD_EXIT(&record, return_value) = sdsempty();
    }
    RECORD_EXIT(&record, memory_usage) = zend_memory_usage(0 TSRMLS_CC) - memory_usage;
    RECORD_EXIT(&record, memory_peak_usage) = zend_memory_peak_usage(0 TSRMLS_CC) - memory_peak_usage;
    
    /*ctx->shmoffset may have been reset in the inner level 
     * of the phptrace_execute_core, so all the return info
     * will be dropped
     * */
    if(ctx->shmoffset){
        record.seq = ctx->seq ++;
        record.flag = RECORD_FLAG_EXIT;
        ctx->shmoffset = phptrace_mem_write_record(&record, ctx->shmoffset);
        phptrace_mem_write_waitflag(ctx->shmoffset);
    }

    if (ctx->cli && PHPTRACE_G(dotrace)) {
        phptrace_print_call_result(&record);
    }
    sdsfree(record.function_name);
    sdsfree(RECORD_EXIT(&record, return_value));
    return;

exec:
    zend_try {
        if (!px->internal) {
#if PHP_VERSION_ID < 50500
            phptrace_old_execute(px->op_array TSRMLS_CC);
#else
            phptrace_old_execute_ex(px->current_execute_data TSRMLS_CC);
#endif
        } else {
#if PHP_VERSION_ID < 50500
            if (phptrace_old_execute_internal) {
                phptrace_old_execute_internal(px->current_execute_data, px->return_value_used TSRMLS_CC);
            } else {
                execute_internal(px->current_execute_data, px->return_value_used TSRMLS_CC);
            }
#else
            if (phptrace_old_execute_internal) {
                phptrace_old_execute_internal(px->current_execute_data, px->fci, px->return_value_used TSRMLS_CC);
            } else {
                execute_internal(px->current_execute_data, px->fci, px->return_value_used TSRMLS_CC);
            }
#endif
        }
        if (ex) {
            -- ctx->level;
        }
    } zend_catch {
        if (ex) {
            -- ctx->level;
        }
        zend_bailout();
    } zend_end_try();
}

#if PHP_VERSION_ID < 50500
void phptrace_execute(zend_op_array *op_array TSRMLS_DC)
{
    zend_execute_data    *ex = EG(current_execute_data);
    phptrace_execute_data px;
    px.op_array = op_array;
#else
void phptrace_execute_ex(zend_execute_data *execute_data TSRMLS_DC)
{
    zend_op_array        *op_array = execute_data->op_array;
    zend_execute_data    *ex = execute_data->prev_execute_data;
    phptrace_execute_data px;

    px.current_execute_data = execute_data;
#endif

    px.internal = 0;

    return phptrace_execute_core(ex, &px TSRMLS_CC);
}

#if PHP_VERSION_ID < 50500
void phptrace_execute_internal(zend_execute_data *current_execute_data, int return_value_used TSRMLS_DC)
{
    phptrace_execute_data px;
#else
void phptrace_execute_internal(zend_execute_data *current_execute_data, struct _zend_fcall_info *fci, int return_value_used TSRMLS_DC)
{
    phptrace_execute_data px;
    px.fci = fci;
#endif
    zend_execute_data    *ex = current_execute_data;

    px.current_execute_data = current_execute_data;
    px.return_value_used = return_value_used;
    px.internal = 1;

    return phptrace_execute_core(ex, &px TSRMLS_CC);
}

#if PHPTRACE_UNIT_TEST
/* {{{ proto void phptrace_mmap_test()
 */
PHP_FUNCTION(phptrace_mmap_test)
{
    phptrace_segment_t seg;
    char buf[32] = {0};

    seg = phptrace_mmap_write(NULL, 1024 * 1024);
    if (seg.shmaddr == MAP_FAILED) {
        php_printf("phptrace_mmap NULL failed!\n");
    } else {
        php_printf("phptrace_mmap NULL successfully!\n");
    }
    phptrace_unmap(&seg);

    seg = phptrace_mmap("/dev/zero", 1024 * 1024);
    if (seg.shmaddr == MAP_FAILED) {
        php_printf("phptrace_mmap '/dev/zero' failed!\n");
    } else {
        php_printf("phptrace_mmap '/dev/zero' successfully!\n");
    }
    phptrace_unmap(&seg);

#if 0
    snprintf(buf, 32, "/tmp/a.shm.XXXXXX");
    seg = phptrace_mmap(buf, 1024 * 1024);
    if (seg.shmaddr == -1) {
        php_printf("phptrace_mmap '%s' failed!\n", buf);
    } else {
        php_printf("phptrace_mmap '%s' successfully!\n", buf);
    }
    phptrace_unmap(&seg);
#endif

    snprintf(buf, 32, "/tmp/abc.XXXXXX");
    seg = phptrace_mmap(buf, 1024 * 1024);
    if (seg.shmaddr == MAP_FAILED) {
        php_printf("phptrace_mmap '%s' failed!\n", buf);
    } else {
        php_printf("phptrace_mmap '%s' successfully!\n", buf);
    }
    phptrace_unmap(&seg);
}
/* }}} */
#endif // PHPTRACE_UNIT_TEST

/* The previous line is meant for vim and emacs, so it can correctly fold and 
   unfold functions in source code. See the corresponding marks just before 
   function definition, where the functions purpose is also documented. Please 
   follow this convention for the convenience of others editing your code.
*/

/*
 * Local variables:
 * End:
 * vim600: et fdm=marker
 * vim<600: et
 */
