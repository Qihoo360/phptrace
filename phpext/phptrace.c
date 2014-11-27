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
#include "../util/phptrace_mmap.h"
#include "../util/phptrace_protocol.h"
#include "../util/phptrace_string.h"

#define PID_MAX 0x8000 /*32768*/
#define PHPTRACE_LOG_DIR "/tmp"

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
	PHP_FE(phptrace_info,	NULL)
#if PHPTRACE_UNIT_TEST
    PHP_FE(phptrace_mmap_test,   NULL)
#endif // PHPTRACE_UNIT_TEST
	PHP_FE_END	/* Must be the last line in phptrace_functions[] */
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
	PHP_RINIT(phptrace),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(phptrace),	/* Replace with NULL if there's nothing to do at request end */
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
    STD_PHP_INI_ENTRY("phptrace.logsize",      "52428800", PHP_INI_ALL, OnUpdateLong, logsize, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.logdir",      NULL, PHP_INI_ALL, OnUpdateString, logdir, zend_phptrace_globals, phptrace_globals)
PHP_INI_END()
/* }}} */

/* {{{ php_phptrace_init_globals
 */
static void php_phptrace_init_globals(zend_phptrace_globals *phptrace_globals)
{
    phptrace_globals->enabled = 0;
    phptrace_globals->logsize = 50*1024*1024;
    phptrace_globals->logdir = NULL;
    memset(&phptrace_globals->ctx, 0, sizeof(phptrace_context_t));
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(phptrace)
{
    char filename[256];
    ZEND_INIT_MODULE_GLOBALS(phptrace, php_phptrace_init_globals, NULL);
    REGISTER_INI_ENTRIES();
    if(!PHPTRACE_G(enabled)){
        return SUCCESS;
    }
    if(!PHPTRACE_G(logdir)){
        PHPTRACE_G(logdir) = PHPTRACE_LOG_DIR;
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
    snprintf(filename, sizeof(filename), "%s/%s", PHPTRACE_G(logdir), "phptrace.ctrl");
    ctx->ctrl = phptrace_mmap_write(filename, PID_MAX);
    if(ctx->ctrl.shmaddr == MAP_FAILED){
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "phptrace_mmap_write %s failed: %s", filename, strerror(errno));
        return FAILURE;
    }
    memset(ctx->ctrl.shmaddr, 0, PID_MAX);
    if(sapi_module.name[0]=='c' &&
       sapi_module.name[1]=='l' &&
       sapi_module.name[2]=='i'){
        ctx->cli = 1;
    }
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(phptrace)
{
    char filename[256];
    phptrace_context_t *ctx;

    if(!PHPTRACE_G(enabled)){
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
    snprintf(filename, sizeof(filename), "%s/%s", PHPTRACE_G(logdir), "phptrace.ctrl");
    phptrace_unmap(&ctx->ctrl);
    //unlink(filename); /*TODO uncomment this*/
    if(ctx->tracelog.shmaddr){
        phptrace_unmap(&ctx->ctrl);
        snprintf(filename, sizeof(filename), "%s/%s.%d", PHPTRACE_G(logdir), "phptrace.log", ctx->pid);
        //unlink(filename); /*TODO uncomment this*/
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

uint64_t phptrace_time_usec(){
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
phptrace_str_t *phptrace_repr_zval(zval *val){
    phptrace_str_t *s;
    char value[128];
    switch(Z_TYPE_P(val)){
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
            return phptrace_str_new(Z_STRVAL_P(val), Z_STRLEN_P(val));
        case IS_ARRAY:
            sprintf(value, "array<%p>", Z_ARRVAL_P(val));
            break;
        case IS_OBJECT:
            sprintf(value, "object<%p>", Z_OBJVAL_P(val));
            break;
        default:
            sprintf(value, "unknown");
    }
    return phptrace_str_new(value, strlen(value));
}

#define EXF_COMMON(e) ex->function_state.function->common.e
#define EXF(e) ex->function_state.function->e
phptrace_str_t *phptrace_get_funcname(zend_execute_data *ex){
    phptrace_str_t *funcname;
    const char *scope;
    const char *name;

    /*Just return if there is no function name*/
    if(ex == NULL || 
            ex->function_state.function == NULL || 
            EXF_COMMON(function_name) == NULL){
        return NULL;
    }

    if(ex->object && EXF_COMMON(scope)){
        /*This is an object, so the function should be a member of a class.
         *Get the scope which is the class's name
         * */
        scope = EXF_COMMON(scope->name);
    }
    else if(EG(scope) &&
            EXF_COMMON(scope) &&
            EXF_COMMON(scope->name)){
        /*static member of class*/
        scope = EXF_COMMON(scope->name);
    }else{
        scope = NULL;
    }
    if(strncmp(EXF_COMMON(function_name), "{closure}", sizeof("closure")-1) == 0){
        /*a closure may be a chunk of code, get the filename and line range*/
        /*TODO process a closure*/
        EXF(op_array.filename);
        EXF(op_array.line_start);
        EXF(op_array.line_end);
    }
    name = EXF_COMMON(function_name);
    if(scope){
        funcname = phptrace_str_new(scope, strlen(scope));
        if(ex->object){
            funcname = phptrace_str_nconcat(funcname, "->", 2);
        }else{
            funcname = phptrace_str_nconcat(funcname, "::", 2);
        }
        funcname = phptrace_str_nconcat(funcname, name, strlen(name));
    }else{
        funcname = phptrace_str_new(name, strlen(name));
    }
    return funcname;
}
phptrace_str_t *phptrace_get_parameters(zend_execute_data *ex){
    phptrace_str_t *parameters, *param;
    zend_arg_info *arg_info;
    zval **args, *arg;
    long n, i, got;
    size_t size;

    if(ex == NULL || 
            ex->function_state.function == NULL || 
            EXF_COMMON(num_args) == 0){
        return NULL;
    }
    n = (long)(zend_uintptr_t)*(ex->function_state.arguments);
    args = (zval **)(ex->function_state.arguments) - n;
    arg_info = EXF_COMMON(arg_info);

    parameters = phptrace_str_new(NULL, 0);
    for(i = 0; i < n; ++i){
        /*The format $p = "hello", $b = "world"*/
        /*FIXME: It is a terriable idea to realloc memory frequently*/
        parameters = phptrace_str_nconcat(parameters, "$", 1);
        parameters = phptrace_str_nconcat(parameters, arg_info[i].name, arg_info[i].name_len);
        param = phptrace_repr_zval(args[i]);
        parameters = phptrace_str_nconcat(parameters, " = \"", 4);
        parameters = phptrace_str_nconcat(parameters, param->data, param->len);
        parameters = phptrace_str_nconcat(parameters, "\"", 1);
        phptrace_str_free(param);
        if(i != n-1){
            parameters = phptrace_str_nconcat(parameters, ", ", 2);
        }
    }
    return parameters;
}
phptrace_str_t *phptrace_get_return_value(zval *return_value){
    if(return_value){
        return phptrace_repr_zval(return_value); 
    }else{
        return NULL;
    }
}

void phptrace_reset_tracelog(phptrace_context_t *ctx){
    char filename[256];

    snprintf(filename, sizeof(filename), "%s/%s.%d", PHPTRACE_G(logdir),"phptrace.log", ctx->pid);
    phptrace_unmap(&ctx->tracelog);
    ctx->tracelog.shmaddr = NULL;
    ctx->tracelog.size = 0;
    unlink(filename);
    ctx->seq = 0;
    ctx->level = 0;
    ctx->rotate = 0;
    ctx->shmoffset = NULL;
}
void phptrace_get_callinfo(phptrace_file_record_t *record, zend_execute_data *ex){
    phptrace_str_t *funcname, *parameters;
    funcname = phptrace_get_funcname(ex);
    if(funcname){
        record->func_name = funcname;
    }

    parameters = phptrace_get_parameters(ex);
    if(parameters){
        record->params = parameters;
    }
}
void phptrace_register_return_value(zval **return_value){
    if (EG(return_value_ptr_ptr) == NULL){
        EG(return_value_ptr_ptr) = return_value;
    }
}
void phptrace_get_execute_return_value(phptrace_file_record_t *record, zval *return_value){
    if(*EG(return_value_ptr_ptr)){
        record->ret_values = phptrace_get_return_value(return_value);
        zval_ptr_dtor(EG(return_value_ptr_ptr));
        EG(return_value_ptr_ptr) = NULL;
    }

}
void phptrace_get_execute_internal_return_value(phptrace_file_record_t *record, zend_execute_data *ex){
    zend_op *opline;
    zval *return_value;
    zend_free_op should_free;

    if(*EG(opline_ptr)){
        opline = *EG(opline_ptr);
        if(opline && opline->result_type & 0x0F){
            return_value = zend_get_zval_ptr((opline->result_type&0x0F), &opline->result, ex->Ts, &should_free, 0);
            if(return_value){
                record->ret_values = phptrace_get_return_value(return_value);
            }
            if(should_free.var){
                zval_dtor(should_free.var);
            }
        }
    }
}

void phptrace_print_callinfo(phptrace_file_record_t *record){
    printf("%d %d %f ",record->seq, record->level, record->start_time/1000000.0);
    if(record->func_name){
        printf("%s ", record->func_name->data);
    }else{
        printf("- ");
    }
    if(record->params){
        printf("(%s) ", record->params->data);
    }else{
        printf("() ");
    }
}

#if PHP_VERSION_ID < 50500
void phptrace_execute(zend_op_array *op_array TSRMLS_DC)
{
    zend_execute_data    *ex = EG(current_execute_data);
#else
void phptrace_execute_ex(zend_execute_data *execute_data TSRMLS_DC)
{
    zend_op_array        *op_array = execute_data->op_array;
    zend_execute_data    *ex = execute_data->prev_execute_data;
#endif 

    uint64_t now;
    uint8_t *ctrl;
    void * retoffset;
    char filename[256];
    zval *return_value;
    phptrace_context_t *ctx;
    phptrace_str_t *retval;
    phptrace_str_t *funcname;
    phptrace_str_t *parameters;

    phptrace_file_record_t record;
    phptrace_file_tailer_t tailer = {MAGIC_NUMBER_TAILER, 0};
    phptrace_file_header_t header = {MAGIC_NUMBER_HEADER, 1, 0};

    if(ex == NULL){
        goto exec;
    }


    ctx = &PHPTRACE_G(ctx);
    ctrl = ctx->ctrl.shmaddr;
    /*TODO move to request init ?*/
    if(ctx->pid == 0){
        ctx->pid = getpid();
    }

    snprintf(filename, sizeof(filename), "%s/%s.%d", PHPTRACE_G(logdir),"phptrace.log", ctx->pid);

    if(ctrl[ctx->pid] == 0 && !ctx->cli){
        /*unmap tracelog if phptrace was shutdown*/
        if(ctx->tracelog.shmaddr){
            phptrace_reset_tracelog(ctx);
        }
        goto exec;
    
    }

    /*do trace*/
    /*first startup*/
    if(ctx->tracelog.shmaddr == NULL){
        ctx->tracelog = phptrace_mmap_write(filename, PHPTRACE_G(logsize));
        if(ctx->tracelog.shmaddr == MAP_FAILED){
            ctx->tracelog.shmaddr = NULL;
            /*TODO write log here*/
            goto exec;
        }
        ctx->shmoffset = ctx->tracelog.shmaddr;
        /*write into waitflag immediately after mmapping*/
        phptrace_mem_write_waitflag(ctx->shmoffset);
        ctx->rotate = 1;
    }
    /*should do rotate*/
    if((ctx->shmoffset - ctx->tracelog.shmaddr) > PHPTRACE_G(logsize)-10*1024){ /*FIXME Use a more safty condition to rotate*/
        ctx->rotate = 1;
        tailer.filename = phptrace_str_new(filename, strlen(filename));
        ctx->shmoffset = phptrace_mem_write_tailer(&tailer, ctx->shmoffset);
        phptrace_str_free(tailer.filename);
    }

    /*ex MUST NOT be NULL*/
    if(ctx->rotate){
        ctx->rotate = 0;
        ctx->shmoffset = ctx->tracelog.shmaddr;
        /*TODO write header & waitflag at once*/
        ctx->shmoffset = phptrace_mem_write_header(&header, ctx->shmoffset);
        phptrace_mem_write_waitflag(ctx->shmoffset);
    }

    record.params = NULL;
    record.func_name = NULL;
    record.ret_values = NULL;

    phptrace_get_callinfo(&record, ex);

    phptrace_register_return_value(&return_value);

    record.seq = ctx->seq ++;
    record.level = ctx->level ++;
    record.start_time = phptrace_time_usec();

    if(ctx->cli){
        phptrace_print_callinfo(&record);
    }

    ctx->shmoffset = phptrace_mem_write_record(&record, ctx->shmoffset);

    phptrace_str_free(record.func_name);
    phptrace_str_free(record.params);

    retoffset = ctx->shmoffset - sizeof(uint64_t) - RET_VALUE_SIZE;
    phptrace_mem_write_waitflag(ctx->shmoffset);

#if PHP_VERSION_ID < 50500
    phptrace_old_execute(op_array TSRMLS_CC);
#else
    phptrace_old_execute_ex(execute_data TSRMLS_CC);
#endif

    -- ctx->level;
    now = phptrace_time_usec();

    if(return_value){
        phptrace_get_execute_return_value(&record, return_value);
    }

    record.time_cost = now - record.start_time;
    /*write the return value and cost time*/
    if(record.ret_values == NULL){
        record.ret_values = phptrace_str_empty();
    }

    phptrace_mem_fix_record(&record, retoffset);

    if(ctx->cli){
        printf(" = %s ", record.ret_values->data);
        printf("%f\n", record.time_cost/1000000.0);
    }
    phptrace_str_free(record.ret_values);
    return;

exec:
#if PHP_VERSION_ID < 50500
    return phptrace_old_execute(op_array TSRMLS_CC);
#else
    return phptrace_old_execute_ex(execute_data TSRMLS_CC);
#endif
}

#if PHP_VERSION_ID < 50500
void phptrace_execute_internal(zend_execute_data *current_execute_data, int return_value_used TSRMLS_DC)
#else
void phptrace_execute_internal(zend_execute_data *current_execute_data, struct _zend_fcall_info *fci, int return_value_used TSRMLS_DC)
#endif
{
    zend_execute_data    *ex = current_execute_data;

    uint64_t now;
    uint8_t *ctrl;
    void * retoffset;
    char filename[256];
    phptrace_context_t *ctx;

    zend_op *opline;
    zval *return_value;
    zend_free_op should_free;

    phptrace_str_t *retval;
    phptrace_str_t *funcname;
    phptrace_str_t *parameters;


    phptrace_file_record_t record;
    phptrace_file_tailer_t tailer = {MAGIC_NUMBER_TAILER, 0};
    phptrace_file_header_t header = {MAGIC_NUMBER_HEADER, 1, 0};

    if(ex == NULL){
        goto exec;
    }


    ctx = &PHPTRACE_G(ctx);
    ctrl = ctx->ctrl.shmaddr;
    if(ctx->pid == 0){
        ctx->pid = getpid();
    }

    snprintf(filename, sizeof(filename), "%s/%s.%d", PHPTRACE_G(logdir),"phptrace.log", ctx->pid);

    if(ctrl[ctx->pid] == 0 && !ctx->cli){
        /*unmap tracelog if phptrace was shutdown*/
        if(ctx->tracelog.shmaddr){
            phptrace_reset_tracelog(ctx);
        }
        goto exec;
    
    }

    /*do trace*/
    /*first startup*/
    if(ctx->tracelog.shmaddr == NULL){
        ctx->tracelog = phptrace_mmap_write(filename, PHPTRACE_G(logsize));
        if(ctx->tracelog.shmaddr == MAP_FAILED){
            ctx->tracelog.shmaddr = NULL;
            /*TODO write log here*/
            goto exec;
        }
        ctx->shmoffset = ctx->tracelog.shmaddr;
        ctx->rotate = 1;
    }
    /*should do rotate*/
    if((ctx->shmoffset - ctx->tracelog.shmaddr) > PHPTRACE_G(logsize)-10*1024){ /*FIXME Use a more safty condition to rotate*/
        ctx->rotate = 1;
        tailer.filename = phptrace_str_new(filename, strlen(filename));
        ctx->shmoffset = phptrace_mem_write_tailer(&tailer, ctx->shmoffset);
        phptrace_str_free(tailer.filename);
    }

    /*ex MUST NOT be NULL*/
    if(ctx->rotate){
        ctx->rotate = 0;
        ctx->shmoffset = ctx->tracelog.shmaddr;
        /*TODO write header & waitflag at once*/
        ctx->shmoffset = phptrace_mem_write_header(&header, ctx->shmoffset);
        phptrace_mem_write_waitflag(ctx->shmoffset);
    }

    record.params = NULL;
    record.func_name = NULL;
    record.ret_values = NULL;

    phptrace_get_callinfo(&record, ex);

    record.seq = ctx->seq ++;
    record.level = ctx->level ++;
    record.start_time = phptrace_time_usec();

    if(ctx->cli){
        phptrace_print_callinfo(&record);
    }

    ctx->shmoffset = phptrace_mem_write_record(&record, ctx->shmoffset);

    phptrace_str_free(record.func_name);
    phptrace_str_free(record.params);

    retoffset = ctx->shmoffset - sizeof(uint64_t) - RET_VALUE_SIZE;
    phptrace_mem_write_waitflag(ctx->shmoffset);

#if PHP_VERSION_ID < 50500
    if(phptrace_old_execute_internal){
        phptrace_old_execute_internal(current_execute_data, return_value_used TSRMLS_DC);
    }else{
        execute_internal(current_execute_data, return_value_used TSRMLS_DC);
    }
#else
    if(phptrace_old_execute_internal){
        phptrace_old_execute_internal(current_execute_data, fci, return_value_used TSRMLS_DC);
    }else{
        execute_internal(current_execute_data, fci, return_value_used TSRMLS_DC);
    }
#endif
    -- ctx->level;
    now = phptrace_time_usec();

    phptrace_get_execute_internal_return_value(&record, ex);
    /*write the return value and cost time*/
    record.time_cost = now - record.start_time;
    if(record.ret_values == NULL){
        record.ret_values = phptrace_str_empty();
    }
    phptrace_mem_fix_record(&record, retoffset);
    if(ctx->cli){
        printf(" = %s ", record.ret_values->data);
        printf("%f\n", record.time_cost/1000000.0);
    }
    phptrace_str_free(record.ret_values);

exec:
#if PHP_VERSION_ID < 50500
            if(phptrace_old_execute_internal){
                return phptrace_old_execute_internal(current_execute_data, return_value_used TSRMLS_DC);
            }else{
                return execute_internal(current_execute_data, return_value_used TSRMLS_DC);
            }
#else
            if(phptrace_old_execute_internal){
                return phptrace_old_execute_internal(current_execute_data, fci, return_value_used TSRMLS_DC);
            }else{
                return execute_internal(current_execute_data, fci, return_value_used TSRMLS_DC);
            }
#endif
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
 * vim600: noet fdm=marker
 * vim<600: noet
 */
