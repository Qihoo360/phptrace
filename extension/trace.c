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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_trace.h"

#include "zend_extensions.h"
#include "SAPI.h"

#include "trace_time.h"
#include "trace_type.h"
#include "sds/sds.h"


/**
 * Trace Global
 * --------------------
 */
/* Debug output */
#if TRACE_DEBUG_OUTPUT
#define PTD(format, args...) fprintf(stderr, "[PTDebug:%d] " format "\n", __LINE__, __VA_ARGS__)
#else
#define PTD(format, args...)
#endif

/* Ctrl module */
#define CTRL_IS_ACTIVE()    pt_ctrl_is_active(&PTG(ctrl), PTG(pid))
#define CTRL_SET_INACTIVE() pt_ctrl_set_inactive(&PTG(ctrl), PTG(pid))

/* Ping process */
#define PING_UPDATE() PTG(ping) = pt_time_sec()
#define PING_TIMEOUT() (pt_time_sec() > PTG(ping) + PTG(idle_timeout))

/* Profiling */
#define PROFILING_SET(p) do { \
    p.wall_time = pt_time_usec(); \
    p.cpu_time = pt_cputime_usec(); \
    p.mem = zend_memory_usage(0 TSRMLS_CC); \
    p.mempeak = zend_memory_peak_usage(0 TSRMLS_CC); \
} while (0);

/* Flags of dotrace */
#define TRACE_TO_OUTPUT (1 << 0)
#define TRACE_TO_TOOL   (1 << 1)
#define TRACE_TO_NULL   (1 << 2)

/**
 * Compatible with PHP 5.1, zend_memory_usage() is not available in 5.1.
 * AG(allocated_memory) is the value we want, but it available only when
 * MEMORY_LIMIT is ON during PHP compilation, so use zero instead for safe.
 */
#if PHP_VERSION_ID < 50200
#define zend_memory_usage(args...) 0
#define zend_memory_peak_usage(args...) 0
typedef unsigned long zend_uintptr_t;
#endif

#if TRACE_DEBUG
PHP_FUNCTION(trace_start);
PHP_FUNCTION(trace_end);
PHP_FUNCTION(trace_status);
#endif

static void pt_frame_build(pt_frame_t *frame, zend_bool internal, unsigned char type, zend_execute_data *ex, zend_op_array *op_array TSRMLS_DC);
static void pt_frame_destroy(pt_frame_t *frame TSRMLS_DC);
static void pt_frame_display(pt_frame_t *frame TSRMLS_DC, zend_bool indent, const char *format, ...);
static int pt_frame_send(pt_frame_t *frame TSRMLS_DC);
static void pt_frame_set_retval(pt_frame_t *frame, zend_bool internal, zend_execute_data *ex, zend_fcall_info *fci TSRMLS_DC);

static void pt_status_build(pt_status_t *status, zend_bool internal, zend_execute_data *ex TSRMLS_DC);
static void pt_status_destroy(pt_status_t *status TSRMLS_DC);
static void pt_status_display(pt_status_t *status TSRMLS_DC);
static int pt_status_send(pt_status_t *status TSRMLS_DC);

static sds pt_repr_zval(zval *zv, int limit TSRMLS_DC);
static void pt_set_inactive(TSRMLS_D);

#if PHP_VERSION_ID < 50500
static void (*pt_ori_execute)(zend_op_array *op_array TSRMLS_DC);
static void (*pt_ori_execute_internal)(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC);
ZEND_API void pt_execute(zend_op_array *op_array TSRMLS_DC);
ZEND_API void pt_execute_internal(zend_execute_data *execute_data, int return_value_used TSRMLS_DC);
#else
static void (*pt_ori_execute_ex)(zend_execute_data *execute_data TSRMLS_DC);
static void (*pt_ori_execute_internal)(zend_execute_data *execute_data_ptr, zend_fcall_info *fci, int return_value_used TSRMLS_DC);
ZEND_API void pt_execute_ex(zend_execute_data *execute_data TSRMLS_DC);
ZEND_API void pt_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC);
#endif


/**
 * PHP Extension Init
 * --------------------
 */

ZEND_DECLARE_MODULE_GLOBALS(trace)

/* Make sapi_module accessable */
extern sapi_module_struct sapi_module;

/* True global resources - no need for thread safety here */
static int le_trace;

/* Every user visible function must have an entry in trace_functions[]. */
const zend_function_entry trace_functions[] = {
#if TRACE_DEBUG
    PHP_FE(trace_start, NULL)
    PHP_FE(trace_end, NULL)
    PHP_FE(trace_status, NULL)
#endif
#ifdef PHP_FE_END
    PHP_FE_END  /* Must be the last line in trace_functions[] */
#else
    { NULL, NULL, NULL, 0, 0 }
#endif
};

zend_module_entry trace_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    "trace",
    trace_functions,
    PHP_MINIT(trace),
    PHP_MSHUTDOWN(trace),
    PHP_RINIT(trace),
    PHP_RSHUTDOWN(trace),
    PHP_MINFO(trace),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_TRACE_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_TRACE
ZEND_GET_MODULE(trace)
#endif

/* PHP_INI */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("trace.enable",    "1",    PHP_INI_SYSTEM, OnUpdateBool, enable, zend_trace_globals, trace_globals)
    STD_PHP_INI_ENTRY("trace.dotrace",   "0",    PHP_INI_SYSTEM, OnUpdateLong, dotrace, zend_trace_globals, trace_globals)
    STD_PHP_INI_ENTRY("trace.data_dir",  "/tmp", PHP_INI_SYSTEM, OnUpdateString, data_dir, zend_trace_globals, trace_globals)
PHP_INI_END()

/* php_trace_init_globals */
static void php_trace_init_globals(zend_trace_globals *ptg)
{
    ptg->enable = ptg->dotrace = 0;
    ptg->data_dir = NULL;

    memset(&ptg->ctrl, 0, sizeof(ptg->ctrl));
    memset(ptg->ctrl_file, 0, sizeof(ptg->ctrl_file));

    ptg->comm.active = 0;
    memset(ptg->comm_file, 0, sizeof(ptg->comm_file));

    ptg->pid = ptg->level = 0;

    ptg->ping = 0;
    ptg->idle_timeout = 30; /* hardcoded */
}


/**
 * PHP Extension Function
 * --------------------
 */
PHP_MINIT_FUNCTION(trace)
{
    ZEND_INIT_MODULE_GLOBALS(trace, php_trace_init_globals, NULL);
    REGISTER_INI_ENTRIES();

    if (!PTG(enable)) {
        return SUCCESS;
    }

    /* Replace executor */
#if PHP_VERSION_ID < 50500
    pt_ori_execute = zend_execute;
    zend_execute = pt_execute;
#else
    pt_ori_execute_ex = zend_execute_ex;
    zend_execute_ex = pt_execute_ex;
#endif
    pt_ori_execute_internal = zend_execute_internal;
    zend_execute_internal = pt_execute_internal;

    /* Open ctrl module */
    snprintf(PTG(ctrl_file), sizeof(PTG(ctrl_file)), "%s/%s", PTG(data_dir), PT_CTRL_FILENAME);
    PTD("Ctrl module open %s", PTG(ctrl_file));
    if (pt_ctrl_create(&PTG(ctrl), PTG(ctrl_file)) < 0) {
        php_error(E_ERROR, "Trace ctrl file %s open failed", PTG(ctrl_file));
        return FAILURE;
    }

    /* Trace in CLI */
    if (PTG(dotrace) && sapi_module.name[0] == 'c' && sapi_module.name[1] == 'l' && sapi_module.name[2] == 'i') {
        PTG(dotrace) = TRACE_TO_OUTPUT;
    } else {
        PTG(dotrace) = 0;
    }

#if TRACE_DEBUG
    /* always do trace in debug mode */
    PTG(dotrace) |= TRACE_TO_NULL;
#endif

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(trace)
{
    UNREGISTER_INI_ENTRIES();

    if (!PTG(enable)) {
        return SUCCESS;
    }

    /* Restore original executor */
#if PHP_VERSION_ID < 50500
    zend_execute = pt_ori_execute;
#else
    zend_execute_ex = pt_ori_execute_ex;
#endif
    zend_execute_internal = pt_ori_execute_internal;

    /* Close ctrl module */
    PTD("Ctrl module close");
    pt_ctrl_close(&PTG(ctrl));

    return SUCCESS;
}

PHP_RINIT_FUNCTION(trace)
{
    /* Anything needs pid, init here */
    if (PTG(pid) == 0) {
        PTG(pid) = getpid();
        snprintf(PTG(comm_file), sizeof(PTG(comm_file)), "%s/%s.%d", PTG(data_dir), PT_COMM_FILENAME, PTG(pid));
    }
    PTG(level) = 0;

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(trace)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(trace)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "trace support", "enabled");
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}


/**
 * Trace Interface
 * --------------------
 */
#if TRACE_DEBUG
PHP_FUNCTION(trace_start)
{
    PTG(dotrace) |= TRACE_TO_OUTPUT;
}

PHP_FUNCTION(trace_end)
{
    PTG(dotrace) &= ~TRACE_TO_OUTPUT;
}

PHP_FUNCTION(trace_status)
{
    pt_status_t status;
    pt_status_build(&status, 1, EG(current_execute_data) TSRMLS_CC);
    pt_status_display(&status TSRMLS_CC);
    pt_status_destroy(&status TSRMLS_CC);
}
#endif


/**
 * Trace Manipulation of Frame
 * --------------------
 */
static void pt_frame_build(pt_frame_t *frame, zend_bool internal, unsigned char type, zend_execute_data *ex, zend_op_array *op_array TSRMLS_DC)
{
    int i;
    zval **args;
    zend_function *zf;

    /* init */
    memset(frame, 0, sizeof(pt_frame_t));

#if PHP_VERSION_ID < 50500
    if (internal || ex) {
        op_array = ex->op_array;
        zf = ex->function_state.function;
    } else {
        zf = (zend_function *) op_array;
    }
#else
    zf = ex->function_state.function;
#endif

    /* types, level */
    frame->type = type;
    frame->functype = internal ? PT_FUNC_INTERNAL : 0x00;
    frame->level = PTG(level);

    /* args init */
    args = NULL;
    frame->arg_count = 0;
    frame->args = NULL;

    /* names */
    if (zf->common.function_name) {
        /* functype, class name */
        if (ex && ex->object) {
            frame->functype |= PT_FUNC_MEMBER;
            /**
             * User care about which method is called exactly, so use
             * zf->common.scope->name instead of ex->object->name.
             */
            if (zf->common.scope) {
                frame->class = sdsnew(zf->common.scope->name);
            } else {
                /* TODO zend uses zend_get_object_classname() in
                 * debug_print_backtrace() */
                php_error(E_WARNING, "Trace catch a entry with ex->object but without zf->common.scope");
            }
        } else if (zf->common.scope) {
            frame->functype |= PT_FUNC_STATIC;
            frame->class = sdsnew(zf->common.scope->name);
        } else {
            frame->functype |= PT_FUNC_NORMAL;
        }

        /* function name */
        if (strcmp(zf->common.function_name, "{closure}") == 0) {
            frame->function = sdscatprintf(sdsempty(), "{closure:%s:%d-%d}", zf->op_array.filename, zf->op_array.line_start, zf->op_array.line_end);
        } else if (strcmp(zf->common.function_name, "__lambda_func") == 0) {
            frame->function = sdscatprintf(sdsempty(), "{lambda:%s}", zf->op_array.filename);
#if PHP_VERSION_ID >= 50414
        } else if (zf->common.scope && zf->common.scope->trait_aliases) {
            /* Use trait alias instead real function name.
             * There is also a bug "#64239 Debug backtrace changed behavior
             * since 5.4.10 or 5.4.11" about this
             * https://bugs.php.net/bug.php?id=64239.*/
            frame->function = sdsnew(zend_resolve_method_name(ex->object ? Z_OBJCE_P(ex->object) : zf->common.scope, zf));
#endif
        } else {
            frame->function = sdsnew(zf->common.function_name);
        }

        /* args */
#if PHP_VERSION_ID < 50300
        /* TODO support fetching arguments in backtrace */
        if (EG(argument_stack).top >= 2) {
            frame->arg_count = (int)(zend_uintptr_t) *(EG(argument_stack).top_element - 2);
            args = (zval **)(EG(argument_stack).top_element - 2 - frame->arg_count);
        }
#else
        if (ex && ex->function_state.arguments) {
            frame->arg_count = (int)(zend_uintptr_t) *(ex->function_state.arguments);
            args = (zval **)(ex->function_state.arguments - frame->arg_count);
        }
#endif
        if (frame->arg_count > 0) {
            frame->args = calloc(frame->arg_count, sizeof(sds));
            for (i = 0; i < frame->arg_count; i++) {
                frame->args[i] = pt_repr_zval(args[i], 32 TSRMLS_CC);
            }
        }
    } else {
        int add_filename = 1;
        long ev = 0;

#if ZEND_EXTENSION_API_NO < 220100525
        if (ex) {
            ev = ex->opline->op2.u.constant.value.lval;
        } else if (op_array && op_array->opcodes) {
            ev = op_array->opcodes->op2.u.constant.value.lval;
        }
#else
        if (ex) {
            ev = ex->opline->extended_value;
        } else if (op_array && op_array->opcodes) {
            ev = op_array->opcodes->extended_value;
        }
#endif

        /* special user function */
        switch (ev) {
            case ZEND_INCLUDE_ONCE:
                frame->functype |= PT_FUNC_INCLUDE_ONCE;
                frame->function = "include_once";
                break;
            case ZEND_REQUIRE_ONCE:
                frame->functype |= PT_FUNC_REQUIRE_ONCE;
                frame->function = "require_once";
                break;
            case ZEND_INCLUDE:
                frame->functype |= PT_FUNC_INCLUDE;
                frame->function = "include";
                break;
            case ZEND_REQUIRE:
                frame->functype |= PT_FUNC_REQUIRE;
                frame->function = "require";
                break;
            case ZEND_EVAL:
                frame->functype |= PT_FUNC_EVAL;
                frame->function = "{eval}"; /* TODO add eval code */
                add_filename = 0;
                break;
            default:
                /* should be function main */
                frame->functype |= PT_FUNC_NORMAL;
                frame->function = "{main}";
                add_filename = 0;
                break;
        }
        frame->function = sdsnew(frame->function);
        if (add_filename) {
            frame->arg_count = 1;
            frame->args = calloc(frame->arg_count, sizeof(sds));
            frame->args[0] = sdscatrepr(sdsempty(), zf->op_array.filename, strlen(zf->op_array.filename));
        }
    }

    /* lineno */
    if (ex && ex->opline) {
        frame->lineno = ex->opline->lineno;
    } else if (ex && ex->prev_execute_data && ex->prev_execute_data->opline) {
        frame->lineno = ex->prev_execute_data->opline->lineno; /* try using prev */
    } else if (op_array && op_array->opcodes) {
        frame->lineno = op_array->opcodes->lineno;
    /**
     * TODO Shall we use definition lineno if entry lineno is null
     * } else if (ex != EG(current_execute_data) && EG(current_execute_data)->opline) {
     *     frame->lineno = EG(current_execute_data)->opline->lineno; [> try using current <]
     */
    } else {
        frame->lineno = 0;
    }

    /* filename */
    if (ex && ex->op_array) {
        frame->filename = sdsnew(ex->op_array->filename);
    } else if (ex && ex->prev_execute_data && ex->prev_execute_data->op_array) {
        frame->filename = sdsnew(ex->prev_execute_data->op_array->filename); /* try using prev */
    } else if (op_array) {
        frame->filename = sdsnew(op_array->filename);
    /**
     * } else if (ex != EG(current_execute_data) && EG(current_execute_data)->op_array) {
     *     frame->filename = sdsnew(EG(current_execute_data)->op_array->filename); [> try using current <]
     */
    } else {
        frame->filename = NULL;
    }
}

static void pt_frame_destroy(pt_frame_t *frame TSRMLS_DC)
{
    int i;

    sdsfree(frame->filename);
    sdsfree(frame->class);
    sdsfree(frame->function);
    if (frame->args && frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            sdsfree(frame->args[i]);
        }
        free(frame->args);
    }
    sdsfree(frame->retval);
}

static void pt_frame_set_retval(pt_frame_t *frame, zend_bool internal, zend_execute_data *ex, zend_fcall_info *fci TSRMLS_DC)
{
    zval *retval = NULL;

    if (internal) {
        /* Ensure there is no exception occurs before fetching return value.
         * opline would be replaced by the Exception's opline if exception was
         * thrown which processed in function zend_throw_exception_internal().
         * It may cause a SEGMENTATION FAULT if we get the return value from a
         * exception opline. */
#if PHP_VERSION_ID >= 50500
        if (fci != NULL) {
            retval = *fci->retval_ptr_ptr;
        } else if (ex->opline && !EG(exception)) {
            retval = EX_TMP_VAR(ex, ex->opline->result.var)->var.ptr;
        }
#else
        if (ex->opline && !EG(exception)) {
#if PHP_VERSION_ID < 50400
            retval = ((temp_variable *)((char *) ex->Ts + ex->opline->result.u.var))->var.ptr;
#else
            retval = ((temp_variable *)((char *) ex->Ts + ex->opline->result.var))->var.ptr;
#endif
        }
#endif
    } else if (*EG(return_value_ptr_ptr)) {
        retval = *EG(return_value_ptr_ptr);
    }

    if (retval) {
        frame->retval = pt_repr_zval(retval, 32 TSRMLS_CC);
    }
}

static int pt_frame_send(pt_frame_t *frame TSRMLS_DC)
{
    size_t len;
    pt_comm_message_t *msg;

    len = pt_type_len_frame(frame);
    if ((msg = pt_comm_swrite_begin(&PTG(comm), len)) == NULL) {
        php_error(E_WARNING, "Trace comm-module write begin failed, tried to allocate %ld bytes", len);
        return -1;
    }
    pt_type_pack_frame(frame, msg->data);
    pt_comm_swrite_end(&PTG(comm), PT_MSG_RET, msg);

    return 0;
}

static void pt_frame_display(pt_frame_t *frame TSRMLS_DC, zend_bool indent, const char *format, ...)
{
    int i, has_bracket = 1;
    size_t len;
    char *buf;
    va_list ap;

    /* indent */
    if (indent) {
        zend_printf("%*s", (frame->level - 1) * 4, "");
    }

    /* format */
    if (format) {
        va_start(ap, format);
        len = vspprintf(&buf, 0, format, ap); /* implementation of zend_vspprintf() */
        zend_write(buf, len);
        efree(buf);
        va_end(ap);
    }

    /* frame */
    if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_NORMAL ||
            frame->functype & PT_FUNC_TYPES & PT_FUNC_INCLUDES) {
        zend_printf("%s(", frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_MEMBER) {
        zend_printf("%s->%s(", frame->class, frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_STATIC) {
        zend_printf("%s::%s(", frame->class, frame->function);
    } else if ((frame->functype & PT_FUNC_TYPES) == PT_FUNC_EVAL) {
        zend_printf("%s", frame->function);
        has_bracket = 0;
    } else {
        zend_printf("unknown");
        has_bracket = 0;
    }

    /* arguments */
    if (frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            zend_printf("%s", frame->args[i]);
            if (i < frame->arg_count - 1) {
                zend_printf(", ");
            }
        }
    }
    if (has_bracket) {
        zend_printf(")");
    }

    /* return value */
    if (frame->type == PT_FRAME_EXIT && frame->retval) {
        zend_printf(" = %s", frame->retval);
    }

    /* TODO output relative filepath */
    zend_printf(" called at [%s:%d]", frame->filename, frame->lineno);
    if (frame->type == PT_FRAME_EXIT) {
        zend_printf(" wt: %.3f ct: %.3f\n",
                ((frame->exit.wall_time - frame->entry.wall_time) / 1000000.0),
                ((frame->exit.cpu_time - frame->entry.cpu_time) / 1000000.0));
    } else {
        zend_printf("\n");
    }
}


/**
 * Trace Manipulation of Status
 * --------------------
 */
static void pt_status_build(pt_status_t *status, zend_bool internal, zend_execute_data *ex TSRMLS_DC)
{
    int i;
    zend_execute_data *ex_ori = ex;

    /* init */
    memset(status, 0, sizeof(pt_status_t));

    /* common */
    status->php_version = PHP_VERSION;
    status->sapi_name = sapi_module.name;

    /* profile */
    status->mem = zend_memory_usage(0 TSRMLS_CC);
    status->mempeak = zend_memory_peak_usage(0 TSRMLS_CC);
    status->mem_real = zend_memory_usage(1 TSRMLS_CC);
    status->mempeak_real = zend_memory_peak_usage(1 TSRMLS_CC);

    /* request */
    status->request_method = (char *) SG(request_info).request_method;
    status->request_uri = SG(request_info).request_uri;
    status->request_query = SG(request_info).query_string;
    status->request_time = SG(global_request_time);
    status->request_script = SG(request_info).path_translated;
    status->proto_num = SG(request_info).proto_num;

    /* arguments */
    status->argc = SG(request_info).argc;
    status->argv = SG(request_info).argv;

    /* frame */
    for (i = 0; ex; ex = ex->prev_execute_data, i++) ; /* calculate stack depth */
    status->frame_count = i;
    if (status->frame_count) {
        status->frames = calloc(status->frame_count, sizeof(pt_frame_t));
        for (i = 0, ex = ex_ori; i < status->frame_count && ex; i++, ex = ex->prev_execute_data) {
            pt_frame_build(status->frames + i, internal, PT_FRAME_STACK, ex, NULL TSRMLS_CC);
            status->frames[i].level = 1;
        }
    } else {
        status->frames = NULL;
    }
}

static void pt_status_destroy(pt_status_t *status TSRMLS_DC)
{
    int i;

    if (status->frames && status->frame_count) {
        for (i = 0; i < status->frame_count; i++) {
            pt_frame_destroy(status->frames + i TSRMLS_CC);
        }
        free(status->frames);
    }
}

static void pt_status_display(pt_status_t *status TSRMLS_DC)
{
    int i;

    zend_printf("------------------------------- Status --------------------------------\n");
    zend_printf("PHP Version:       %s\n", status->php_version);
    zend_printf("SAPI:              %s\n", status->sapi_name);

    zend_printf("memory:            %.2fm\n", status->mem / 1048576.0);
    zend_printf("memory peak:       %.2fm\n", status->mempeak / 1048576.0);
    zend_printf("real-memory:       %.2fm\n", status->mem_real / 1048576.0);
    zend_printf("real-memory peak   %.2fm\n", status->mempeak_real / 1048576.0);

    zend_printf("------------------------------- Request -------------------------------\n");
    if (status->request_method) {
    zend_printf("request method:    %s\n", status->request_method);
    }
    zend_printf("request time:      %f\n", status->request_time);
    if (status->request_uri) {
    zend_printf("request uri:       %s\n", status->request_uri);
    }
    if (status->request_query) {
    zend_printf("request query:     %s\n", status->request_query);
    }
    if (status->request_script) {
    zend_printf("request script:    %s\n", status->request_script);
    }
    zend_printf("proto_num:         %d\n", status->proto_num);

    if (status->argc) {
        zend_printf("------------------------------ Arguments ------------------------------\n");
        for (i = 0; i < status->argc; i++) {
            zend_printf("$%-10d        %s\n", i, status->argv[i]);
        }
    }

    if (status->frame_count) {
        zend_printf("------------------------------ Backtrace ------------------------------\n");
        for (i = 0; i < status->frame_count; i++) {
            pt_frame_display(status->frames + i TSRMLS_CC, 0, "#%-3d", i);
        }
    }
}

static int pt_status_send(pt_status_t *status TSRMLS_DC)
{
    size_t len;
    pt_comm_message_t *msg;

    len = pt_type_len_status(status);
    if ((msg = pt_comm_swrite_begin(&PTG(comm), len)) == NULL) {
        php_error(E_WARNING, "Trace comm-module write begin failed, tried to allocate %ld bytes", len);
        return -1;
    }
    pt_type_pack_status(status, msg->data);
    pt_comm_swrite_end(&PTG(comm), PT_MSG_RET, msg);

    return 0;
}


/**
 * Trace Misc Function
 * --------------------
 */
static sds pt_repr_zval(zval *zv, int limit TSRMLS_DC)
{
    int tlen = 0;
    char buf[256], *tstr = NULL;
    sds result;

    /* php_var_export_ex is a good example */
    switch (Z_TYPE_P(zv)) {
        case IS_BOOL:
            if (Z_LVAL_P(zv)) {
                return sdsnew("true");
            } else {
                return sdsnew("false");
            }
        case IS_NULL:
            return sdsnew("NULL");
        case IS_LONG:
            snprintf(buf, sizeof(buf), "%ld", Z_LVAL_P(zv));
            return sdsnew(buf);
        case IS_DOUBLE:
            snprintf(buf, sizeof(buf), "%.*G", (int) EG(precision), Z_DVAL_P(zv));
            return sdsnew(buf);
        case IS_STRING:
            tlen = (limit <= 0 || Z_STRLEN_P(zv) < limit) ? Z_STRLEN_P(zv) : limit;
            result = sdscatrepr(sdsempty(), Z_STRVAL_P(zv), tlen);
            if (limit > 0 && Z_STRLEN_P(zv) > limit) {
                result = sdscat(result, "...");
            }
            return result;
        case IS_ARRAY:
            /* TODO more info */
            return sdscatprintf(sdsempty(), "array(%d)", zend_hash_num_elements(Z_ARRVAL_P(zv)));
        case IS_OBJECT:
            if (Z_OBJ_HANDLER(*zv, get_class_name)) {
                Z_OBJ_HANDLER(*zv, get_class_name)(zv, (const char **) &tstr, (zend_uint *) &tlen, 0 TSRMLS_CC);
                result = sdscatprintf(sdsempty(), "object(%s)#%d", tstr, Z_OBJ_HANDLE_P(zv));
                efree(tstr);
            } else {
                result = sdscatprintf(sdsempty(), "object(unknown)#%d", Z_OBJ_HANDLE_P(zv));
            }
            return result;
        case IS_RESOURCE:
            tstr = (char *) zend_rsrc_list_get_rsrc_type(Z_LVAL_P(zv) TSRMLS_CC);
            return sdscatprintf(sdsempty(), "resource(%s)#%ld", tstr ? tstr : "Unknown", Z_LVAL_P(zv));
        default:
            return sdsnew("{unknown}");
    }
}

static void pt_set_inactive(TSRMLS_D)
{
    PTD("Ctrl set inactive");
    CTRL_SET_INACTIVE();

    /* toggle trace off, close comm-module */
    PTG(dotrace) &= ~TRACE_TO_TOOL;
    if (PTG(comm).active) {
        PTD("Comm socket close");
        pt_comm_sclose(&PTG(comm), 1);
    }
}


/**
 * Trace Executor Replacement
 * --------------------
 */
#if PHP_VERSION_ID < 50500
ZEND_API void pt_execute_core(int internal, zend_execute_data *execute_data, zend_op_array *op_array, int rvu TSRMLS_DC)
{
    zend_fcall_info *fci = NULL;
#else
ZEND_API void pt_execute_core(int internal, zend_execute_data *execute_data, zend_fcall_info *fci, int rvu TSRMLS_DC)
{
    zend_op_array *op_array = NULL;
#endif
    long dotrace;
    zend_bool dobailout = 0;
    zend_execute_data *ex_entry = execute_data;
    zval *retval = NULL;
    pt_comm_message_t *msg;
    pt_frame_t frame;

#if PHP_VERSION_ID >= 50500
    /* Why has a ex_entry ?
     * In PHP 5.5 and after, execute_data is the data going to be executed, not
     * the entry point, so we switch to previous data. The internal function is
     * a exception because it's no need to execute by op_array. */
    if (!internal && execute_data->prev_execute_data) {
        ex_entry = execute_data->prev_execute_data;
    }
#endif

    /* Check ctrl module */
    if (CTRL_IS_ACTIVE()) {
        /* Open comm socket */
        if (!PTG(comm).active) {
            PTD("Comm socket %s open", PTG(comm_file));
            if (pt_comm_sopen(&PTG(comm), PTG(comm_file), 0) == -1) {
                php_error(E_WARNING, "Trace comm-module %s open failed", PTG(comm_file));
                pt_set_inactive(TSRMLS_C);
                goto exec;
            }
            PING_UPDATE();
        }

        /* Handle message */
        while (1) {
            switch (pt_comm_sread(&PTG(comm), &msg, 1)) {
                case PT_MSG_INVALID:
                    PTD("msg type: invalid");
                    pt_set_inactive(TSRMLS_C);
                    goto exec;

                case PT_MSG_EMPTY:
                    PTD("msg type: empty");
                    if (PING_TIMEOUT()) {
                        PTD("Ping timeout");
                        pt_set_inactive(TSRMLS_C);
                    }
                    goto exec;

                case PT_MSG_DO_TRACE:
                    PTD("msg type: do_trace");
                    PTG(dotrace) |= TRACE_TO_TOOL;
                    break;

                case PT_MSG_DO_STATUS:
                    PTD("msg type: do_status");
                    pt_status_t status;
                    pt_status_build(&status, internal, ex_entry TSRMLS_CC);
                    pt_status_send(&status TSRMLS_CC);
                    pt_status_destroy(&status TSRMLS_CC);
                    break;

                case PT_MSG_DO_PING:
                    PTD("msg type: do_ping");
                    PING_UPDATE();
                    break;

                default:
                    php_error(E_NOTICE, "Trace unknown message received with type 0x%x", msg->type);
                    break;
            }
        }
    } else if (PTG(comm).active) { /* comm-module still open */
        pt_set_inactive(TSRMLS_C);
    }

exec:
    /* Assigning to a LOCAL VARIABLE at begining to prevent value changed
     * during executing. And whether send frame mesage back is controlled by
     * GLOBAL VALUE and LOCAL VALUE both because comm-module may be closed in
     * recursion and sending on exit point will be affected. */
    dotrace = PTG(dotrace);

    PTG(level)++;

    if (dotrace) {
        pt_frame_build(&frame, internal, PT_FRAME_ENTRY, ex_entry, op_array TSRMLS_CC);

        /* Register reture value ptr */
        if (!internal && EG(return_value_ptr_ptr) == NULL) {
            EG(return_value_ptr_ptr) = &retval;
        }

        PROFILING_SET(frame.entry);

        /* Send frame message */
        if (dotrace & TRACE_TO_TOOL) {
            pt_frame_send(&frame TSRMLS_CC);
        }
        if (dotrace & TRACE_TO_OUTPUT) {
            pt_frame_display(&frame TSRMLS_CC, 1, "> ");
        }
    }

    /* Call original under zend_try. baitout will be called when exit(), error
     * occurs, exception thrown and etc, so we have to catch it and free our
     * resources. */
    zend_try {
#if PHP_VERSION_ID < 50500
        if (internal) {
            if (pt_ori_execute_internal) {
                pt_ori_execute_internal(execute_data, rvu TSRMLS_CC);
            } else {
                execute_internal(execute_data, rvu TSRMLS_CC);
            }
        } else {
            pt_ori_execute(op_array TSRMLS_CC);
        }
#else
        if (internal) {
            if (pt_ori_execute_internal) {
                pt_ori_execute_internal(execute_data, fci, rvu TSRMLS_CC);
            } else {
                execute_internal(execute_data, fci, rvu TSRMLS_CC);
            }
        } else {
            pt_ori_execute_ex(execute_data TSRMLS_CC);
        }
#endif
    } zend_catch {
        dobailout = 1;
        /* call zend_bailout() at the end of this function, we still want to
         * send message. */
    } zend_end_try();

    if (dotrace) {
        PROFILING_SET(frame.exit);

        if (!dobailout) {
            pt_frame_set_retval(&frame, internal, execute_data, fci TSRMLS_CC);
        }
        frame.type = PT_FRAME_EXIT;

        /* Send frame message */
        if (PTG(dotrace) & TRACE_TO_TOOL & dotrace) {
            pt_frame_send(&frame TSRMLS_CC);
        }
        if (PTG(dotrace) & TRACE_TO_OUTPUT & dotrace) {
            pt_frame_display(&frame TSRMLS_CC, 1, "< ");
        }

        /* Free reture value */
        if (!internal && retval != NULL) {
            zval_ptr_dtor(&retval);
            EG(return_value_ptr_ptr) = NULL;
        }

        pt_frame_destroy(&frame TSRMLS_CC);
    }

    PTG(level)--;

    if (dobailout) {
        zend_bailout();
    }
}

#if PHP_VERSION_ID < 50500
ZEND_API void pt_execute(zend_op_array *op_array TSRMLS_DC)
{
    pt_execute_core(0, EG(current_execute_data), op_array, 0 TSRMLS_CC);
}

ZEND_API void pt_execute_internal(zend_execute_data *execute_data, int return_value_used TSRMLS_DC)
{
    pt_execute_core(1, execute_data, NULL, return_value_used TSRMLS_CC);
}
#else
ZEND_API void pt_execute_ex(zend_execute_data *execute_data TSRMLS_DC)
{
    pt_execute_core(0, execute_data, NULL, 0 TSRMLS_CC);
}

ZEND_API void pt_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC)
{
    pt_execute_core(1, execute_data, fci, return_value_used TSRMLS_CC);
}
#endif
