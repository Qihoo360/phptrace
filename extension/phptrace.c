#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_phptrace.h"
#include "zend_extensions.h"


/**
 * PHP-Trace Global
 * --------------------
 */
/* FIXME remove PTD */
#define PTD(format, args...) fprintf(stderr, "[PTDebug:%d] " format "\n", __LINE__, ## args)

#define CTRL ((int8_t *) (PTG(ctrl).ctrl_seg.shmaddr))[PTG(pid)]
#define CTRL_IS_ACTIVE() (CTRL & PT_CTRL_ACTIVE)
#define CTRL_SET_INACTIVE() (CTRL &= ~PT_CTRL_ACTIVE)

#define PING_UPDATE() PTG(ping) = phptrace_time_sec()
#define PING_TIMEOUT() (phptrace_time_sec() > PTG(ping) + PTG(idle_timeout))

#if PHP_VERSION_ID < 50200
/**
 * AG(allocated_memory) is the value we want, but it available only when
 * MEMORY_LIMIT is ON during PHP compilation, so use zero instead for safe.
 */
#define zend_memory_usage(args...) 0
#define zend_memory_peak_usage(args...) 0
typedef unsigned long zend_uintptr_t;
#endif

#define PROFILING_SET(p) do { \
    p.wall_time = phptrace_time_usec(); \
    p.cpu_time = phptrace_time_usec(); \
    p.mem = zend_memory_usage(0 TSRMLS_CC); \
    p.mempeak = zend_memory_peak_usage(0 TSRMLS_CC); \
} while (0);

static void pt_frame_build(phptrace_frame *frame, zend_execute_data *ex, zend_op_array *op_array, zend_bool internal TSRMLS_DC);
static void pt_frame_destroy(phptrace_frame *frame TSRMLS_DC);
static void pt_frame_set_retval(phptrace_frame *frame, zend_bool internal, zend_execute_data *ex, zend_fcall_info *fci TSRMLS_DC);
static void pt_fname_display(phptrace_frame *frame, zend_bool indent, const char *format, ...);
static sds pt_repr_zval(zval *zv, int limit TSRMLS_DC);
static void pt_display_backtrace(zend_execute_data *ex, zend_bool internal TSRMLS_DC);

#if PHP_VERSION_ID < 50500
static void (*pt_ori_execute)(zend_op_array *op_array TSRMLS_DC);
static void (*pt_ori_execute_internal)(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC);
ZEND_API void phptrace_execute(zend_op_array *op_array TSRMLS_DC);
ZEND_API void phptrace_execute_internal(zend_execute_data *execute_data, int return_value_used TSRMLS_DC);
#else
static void (*pt_ori_execute_ex)(zend_execute_data *execute_data TSRMLS_DC);
static void (*pt_ori_execute_internal)(zend_execute_data *execute_data_ptr, zend_fcall_info *fci, int return_value_used TSRMLS_DC);
ZEND_API void phptrace_execute_ex(zend_execute_data *execute_data TSRMLS_DC);
ZEND_API void phptrace_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC);
#endif


/**
 * PHP Extension Init
 * --------------------
 */

ZEND_DECLARE_MODULE_GLOBALS(phptrace)

/* True global resources - no need for thread safety here */
static int le_phptrace;

/**
 * Every user visible function must have an entry in phptrace_functions[].
 */
const zend_function_entry phptrace_functions[] = {
#ifdef PHP_FE_END
    PHP_FE_END  /* Must be the last line in phptrace_functions[] */
#else
    { NULL, NULL, NULL, 0, 0 }
#endif
};

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

#ifdef COMPILE_DL_PHPTRACE
ZEND_GET_MODULE(phptrace)
#endif

/* PHP_INI */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("phptrace.enable",    "1",    PHP_INI_SYSTEM, OnUpdateBool, enable, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.do_trace",  "0",    PHP_INI_SYSTEM, OnUpdateBool, do_trace, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.data_dir",  "/tmp", PHP_INI_SYSTEM, OnUpdateString, data_dir, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.recv_size", "4m",   PHP_INI_SYSTEM, OnUpdateLong, recv_size, zend_phptrace_globals, phptrace_globals)
    STD_PHP_INI_ENTRY("phptrace.send_size", "64m",  PHP_INI_SYSTEM, OnUpdateLong, send_size, zend_phptrace_globals, phptrace_globals)
PHP_INI_END()

/* php_phptrace_init_globals */
static void php_phptrace_init_globals(zend_phptrace_globals *ptg)
{
    ptg->enable = 0;
    ptg->data_dir = NULL;

    memset(ptg->ctrl_file, 0, sizeof(ptg->ctrl_file));
    memset(ptg->comm_file, 0, sizeof(ptg->comm_file));
    ptg->ctrl.ctrl_seg.shmaddr = ptg->comm.seg.shmaddr = MAP_FAILED; /* TODO using more intuitive element */
    ptg->recv_size = ptg->send_size = 0;

    ptg->do_trace = ptg->pid = ptg->level = 0;

    ptg->ping = 0;
    ptg->idle_timeout = 30;
}


/**
 * PHP Extension Function
 * --------------------
 */

PHP_MINIT_FUNCTION(phptrace)
{
    ZEND_INIT_MODULE_GLOBALS(phptrace, php_phptrace_init_globals, NULL);
    REGISTER_INI_ENTRIES();

    if (!PTG(enable)) {
        return SUCCESS;
    }

    /* Replace executor */
    PTD("Replace executor");
#if PHP_VERSION_ID < 50500
    pt_ori_execute = zend_execute;
    zend_execute = phptrace_execute;
#else
    pt_ori_execute_ex = zend_execute_ex;
    zend_execute_ex = phptrace_execute_ex;
#endif
    pt_ori_execute_internal = zend_execute_internal;
    zend_execute_internal = phptrace_execute_internal;

    /* Open ctrl module */
    snprintf(PTG(ctrl_file), sizeof(PTG(ctrl_file)), "%s/%s", PTG(data_dir), PHPTRACE_CTRL_FILENAME);
    if (phptrace_ctrl_needcreate(PTG(ctrl_file), PID_MAX)) {
        PTD("Ctrl module create %s", PTG(ctrl_file));
        phptrace_ctrl_create(&PTG(ctrl), PTG(ctrl_file), PID_MAX);
    } else {
        PTD("Ctrl module open %s", PTG(ctrl_file));
        phptrace_ctrl_open(&PTG(ctrl), PTG(ctrl_file));
    }
    if (PTG(ctrl).ctrl_seg.shmaddr == MAP_FAILED) {
        PTD("Ctrl module %s failed", PTG(ctrl_file));
        return FAILURE;
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phptrace)
{
    UNREGISTER_INI_ENTRIES();

    if (!PTG(enable)) {
        return SUCCESS;
    }

    /* Restore original executor */
    PTD("Restore executor");
#if PHP_VERSION_ID < 50500
    zend_execute = pt_ori_execute;
#else
    zend_execute_ex = pt_ori_execute_ex;
#endif
    zend_execute_internal = pt_ori_execute_internal;

    /* Close ctrl module */
    PTD("Ctrl module close");
    phptrace_ctrl_close(&PTG(ctrl));

    return SUCCESS;
}

PHP_RINIT_FUNCTION(phptrace)
{
    PTG(pid) = getpid();
    PTG(level) = 0;

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(phptrace)
{
    return SUCCESS;
}

PHP_MINFO_FUNCTION(phptrace)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "phptrace support", "enabled");
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}


/**
 * PHP-Trace Interface
 * --------------------
 */


/**
 * PHP-Trace Function
 * --------------------
 */
void pt_frame_build(phptrace_frame *frame, zend_execute_data *ex, zend_op_array *op_array, zend_bool internal TSRMLS_DC)
{
    int i;
    zval **args;
    zend_function *zf;

    /* init */
    memset(frame, 0, sizeof(phptrace_frame));

#if PHP_VERSION_ID < 50500
    if (internal || ex) {
        op_array = ex->op_array;
        zf = ex->function_state.function;
    } else {
        zf = (zend_function *) op_array;
    }
#else
    /**
     * Current execute_data is the data going to be executed not the entry
     * point, so we switch to previous data. The internal function is a
     * exception because it's no need to execute by op_array.
     */
    if (!internal && ex->prev_execute_data) {
        ex = ex->prev_execute_data;
    }
    zf = ex->function_state.function;
#endif

    /* internal */
    frame->internal = internal;
    frame->level = PTG(level);

    /* names */
    if (zf->common.function_name) {
        /* type, class name */
        if (ex && ex->object) {
            frame->type = PT_FUNC_MEMBER;
            /**
             * User care about which method is called exactly, so use
             * zf->common.scope->name instead of ex->object->name.
             */
            if (zf->common.scope) {
                frame->class = sdsnew(zf->common.scope->name);
            } else {
                /* FIXME zend use zend_get_object_classname() */
                PTD("Catch a entry with ex->object but without zf->common.scope");
            }
        } else if (zf->common.scope) {
            frame->type = PT_FUNC_STATIC;
            frame->class = sdsnew(zf->common.scope->name);
        } else {
            frame->type = PT_FUNC_NORMAL;
        }

        /* function name */
        if (strcmp(zf->common.function_name, "{closure}") == 0) {
            frame->function = sdscatprintf(sdsempty(), "{closure:%s:%d-%d}", zf->op_array.filename, zf->op_array.line_start, zf->op_array.line_end);
        } else if (strcmp(zf->common.function_name, "__lambda_func") == 0) {
            frame->function = sdscatprintf(sdsempty(), "{lambda:%s}", zf->op_array.filename);
        } else {
            /**
             * TODO deal trait alias better, zend has a function below:
             * zend_resolve_method_name(ex->object ?  Z_OBJCE_P(ex->object) : zf->common.scope, zf)
             */
            frame->function = sdsnew(zf->common.function_name);
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
                frame->type = PT_FUNC_INCLUDE_ONCE;
                frame->function = "include_once";
                break;
            case ZEND_REQUIRE_ONCE:
                frame->type = PT_FUNC_REQUIRE_ONCE;
                frame->function = "require_once";
                break;
            case ZEND_INCLUDE:
                frame->type = PT_FUNC_INCLUDE;
                frame->function = "include";
                break;
            case ZEND_REQUIRE:
                frame->type = PT_FUNC_REQUIRE;
                frame->function = "require";
                break;
            case ZEND_EVAL:
                frame->type = PT_FUNC_EVAL;
                frame->function = sdsnew("{eval}");
                add_filename = 0;
                break;
            default:
                /* should be function main */
                frame->type = PT_FUNC_NORMAL;
                frame->function = sdsnew("{main}");
                add_filename = 0;
                break;
        }
        if (add_filename) {
            frame->function = sdscatprintf(sdsempty(), "{%s:%s}", frame->function, zf->op_array.filename);
        }
    }

    /* args */
    frame->arg_count = 0;
    frame->args = NULL;
#if PHP_VERSION_ID < 50300
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
            frame->args[i] = pt_repr_zval(args[i], 0 TSRMLS_CC);
        }
    }

    /* lineno */
    if (ex && ex->opline) {
        frame->lineno = ex->opline->lineno;
    } else if (ex && ex->prev_execute_data && ex->prev_execute_data->opline) {
        /* FIXME try it in loop ? */
        frame->lineno = ex->prev_execute_data->opline->lineno; /* try using prev */
    } else if (op_array && op_array->opcodes) {
        frame->lineno = op_array->opcodes->lineno;
    /**
     * TODO use definition lineno when entry lineno is null ?
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

void pt_frame_destroy(phptrace_frame *frame TSRMLS_DC)
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

void pt_frame_set_retval(phptrace_frame *frame, zend_bool internal, zend_execute_data *ex, zend_fcall_info *fci TSRMLS_DC)
{
    zval *retval;

    if (internal) {
#if PHP_VERSION_ID < 50400
        retval = ((temp_variable *)((char *) ex->Ts + ex->opline->result.u.var))->var.ptr;
#elif PHP_VERSION_ID < 50500
        retval = ((temp_variable *)((char *) ex->Ts + ex->opline->result.var))->var.ptr;
#else
        if (fci != NULL) {
            retval = *fci->retval_ptr_ptr;
        } else {
            retval = EX_TMP_VAR(ex, ex->opline->result.var)->var.ptr;
        }
#endif
    } else if (*EG(return_value_ptr_ptr)) {
        retval = *EG(return_value_ptr_ptr);
    }

    if (retval) {
        frame->retval = pt_repr_zval(retval, 0 TSRMLS_CC);
    }
}

sds pt_repr_zval(zval *zv, int limit TSRMLS_DC)
{
    int tmp_len = 0;
    char buf[128], *tmp_str = NULL;
    sds result;

    /* php_var_export_ex is a good example */
    /* TODO limit string length */
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
            sprintf(buf, "%ld", Z_LVAL_P(zv));
            return sdsnew(buf);
        case IS_DOUBLE:
            sprintf(buf, "%f", Z_DVAL_P(zv));
            return sdsnew(buf);
        case IS_STRING:
            /* TODO addslashes and deal special chars, make it binary safe */
            result = sdsnewlen("'", Z_STRLEN_P(zv) + 2);
            memcpy(result + 1, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
            result[Z_STRLEN_P(zv) + 1] = '\'';
            return result;
        case IS_ARRAY:
            return sdscatprintf(sdsempty(), "array(%d)", zend_hash_num_elements(Z_ARRVAL_P(zv)));
        case IS_OBJECT:
            if (Z_OBJ_HANDLER(*zv, get_class_name)) {
                Z_OBJ_HANDLER(*zv, get_class_name)(zv, (const char **) &tmp_str, (zend_uint *) &tmp_len, 0 TSRMLS_CC);
                result = sdscatprintf(sdsempty(), "object(%s)#%d", tmp_str, Z_OBJ_HANDLE_P(zv));
                efree(tmp_str);
            } else {
                result = sdscatprintf(sdsempty(), "object(unknown)#%d", Z_OBJ_HANDLE_P(zv));
            }
            return result;
        case IS_RESOURCE:
            tmp_str = (char *) zend_rsrc_list_get_rsrc_type(Z_LVAL_P(zv) TSRMLS_CC);
            return sdscatprintf(sdsempty(), "resource(%s)#%ld", tmp_str ? tmp_str : "Unknown", Z_LVAL_P(zv));
        default:
            return sdsnew("{unknown}");
    }
}

void pt_fname_display(phptrace_frame *frame, zend_bool indent, const char *format, ...)
{
    int i;
    va_list ap;

    /* indent */
    if (indent) {
        fprintf(stderr, "%*s", (frame->level - 1) * 4, "");
    }

    /* format */
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    /* frame */
    if (frame->type == PT_FUNC_NORMAL) {
        fprintf(stderr, "%s(", frame->function);
    } else if (frame->type == PT_FUNC_MEMBER) {
        fprintf(stderr, "%s->%s(", frame->class, frame->function);
    } else if (frame->type == PT_FUNC_STATIC) {
        fprintf(stderr, "%s::%s(", frame->class, frame->function);
    } else if (frame->type & PT_FUNC_INCLUDES) {
        fprintf(stderr, "%s", frame->function);
        goto done;
    } else {
        fprintf(stderr, "unknown");
        goto done;
    }

    /* arguments */
    if (frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            fprintf(stderr, "%s", frame->args[i]);
            if (i < frame->arg_count - 1) {
                fprintf(stderr, ", ");
            }
        }
    }
    fprintf(stderr, ")");

    /* return value */
    if (frame->retval) {
        fprintf(stderr, " = %s", frame->retval);
    }

done:
    /* TODO output relative filepath */
    fprintf(stderr, " called at [%s:%d]", frame->filename, frame->lineno);
    fprintf(stderr, " wt: %.3f ct: %.3f\n",
            ((frame->exit.wall_time - frame->entry.wall_time) / 1000000.0),
            ((frame->exit.cpu_time - frame->entry.cpu_time) / 1000000.0));
}

static void pt_display_backtrace(zend_execute_data *ex, zend_bool internal TSRMLS_DC)
{
#if 0
    int num = 0;
    phptrace_frame frame;

    while (ex) {
        if (!internal && !ex->prev_execute_data) {
            break;
        }

        pt_frame_build(&frame, ex, internal TSRMLS_CC);
        frame.level = 1;
        pt_fname_display(&frame, 0, "#%-3d", num++);
        pt_frame_destroy(&frame TSRMLS_CC);

        ex = ex->prev_execute_data;
    }
#endif
}


/**
 * PHP-Trace Executor Replacement
 * --------------------
 */
#if PHP_VERSION_ID < 50500
ZEND_API void phptrace_execute_core(int internal, zend_execute_data *execute_data, zend_op_array *op_array, int rvu TSRMLS_DC)
{
    zend_fcall_info *fci = NULL;
#else
ZEND_API void phptrace_execute_core(int internal, zend_execute_data *execute_data, zend_fcall_info *fci, int rvu TSRMLS_DC)
{
    zend_op_array *op_array = NULL;
#endif
    zend_bool do_trace;
    zval *retval = NULL;
    phptrace_comm_message *msg;
    phptrace_frame frame;

    /* Check ctrl module */
    if (CTRL_IS_ACTIVE()) {
        /* Open comm socket */
        if (PTG(comm).seg.shmaddr == MAP_FAILED) {
            snprintf(PTG(comm_file), sizeof(PTG(comm_file)), "%s/%s.%d", PTG(data_dir), PHPTRACE_COMM_FILENAME, PTG(pid));
            PTD("Comm socket %s create", PTG(comm_file));
            if (phptrace_comm_screate(&PTG(comm), PTG(comm_file), 0, PTG(send_size), PTG(recv_size)) == -1) {
                PTD("Comm socket create failed");
                CTRL_SET_INACTIVE();
                goto exec;
            }

            /* Clear */
            phptrace_comm_clear(&PTG(comm).send_handler);
            phptrace_comm_clear(&PTG(comm).recv_handler);

            PING_UPDATE();
        }

        /* Handle message */
        while (1) {
            msg = phptrace_comm_sread(&PTG(comm));
            PTD("Msg type: 0x%08x", msg->type);

            if (msg == NULL || (msg->type == PT_MSG_EMPTY && PING_TIMEOUT())) {
                PTD("Msg is NULL or ping timeout");
                CTRL_SET_INACTIVE();
                break;
            } else if (msg->type != PT_MSG_EMPTY) {
                PING_UPDATE();

                switch (msg->type) { /* TODO beautiful handler */
                    case 0x10000001:
                        PTG(do_trace) = 1;
                        break;
                    case 0x10000002:
                        pt_display_backtrace(execute_data, 0 TSRMLS_CC);
                        break;
                    case 0x10001001:
                        printf("set zvallen: %d\n", *(int *) msg->data);
                        break;
                    default:
                        break;
                }
            }
        }
    } else {
        /* Close comm socket */
        if (PTG(comm).seg.shmaddr != MAP_FAILED) {
            PTD("Comm socket close");
            phptrace_comm_sclose(&PTG(comm));
        }
    }

exec:
    /* assign to a local variable to prevent changing during executing */
    do_trace = PTG(do_trace);

    PTG(level)++;

    if (do_trace) {
        pt_frame_build(&frame, execute_data, op_array, internal TSRMLS_CC);

        /* Send frame message */
        if (0) {
            msg = phptrace_comm_swrite_begin(&PTG(comm), phptrace_type_len_frame(&frame));
            phptrace_type_pack_frame(&frame, msg->data);
            phptrace_comm_swrite_end(&PTG(comm), 0x10000101, msg); /* FIXME type */
        }
        pt_fname_display(&frame, 1, "> ");

        /* Register reture value ptr */
        if (!internal && EG(return_value_ptr_ptr) == NULL) {
            EG(return_value_ptr_ptr) = &retval;
        }

        PROFILING_SET(frame.entry);
    }

    /* call original */
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

    if (do_trace) {
        PROFILING_SET(frame.exit);

        pt_frame_set_retval(&frame, internal, execute_data, fci TSRMLS_CC);

        /* Send frame message */
        if (0) {
            msg = phptrace_comm_swrite_begin(&PTG(comm), phptrace_type_len_frame(&frame));
            phptrace_type_pack_frame(&frame, msg->data);
            phptrace_comm_swrite_end(&PTG(comm), 0x10000101, msg); /* FIXME type */
        }
        pt_fname_display(&frame, 1, "< ");

        /* Free reture value */
        if (!internal && retval != NULL) {
            zval_ptr_dtor(&retval);
        }

        pt_frame_destroy(&frame TSRMLS_CC);
    }

    PTG(level)--;
}

#if PHP_VERSION_ID < 50500
ZEND_API void phptrace_execute(zend_op_array *op_array TSRMLS_DC)
{
    phptrace_execute_core(0, EG(current_execute_data), op_array, 0 TSRMLS_CC);
#else
ZEND_API void phptrace_execute_ex(zend_execute_data *execute_data TSRMLS_DC)
{
    phptrace_execute_core(0, execute_data, NULL, 0 TSRMLS_CC);
#endif
}

#if PHP_VERSION_ID < 50500
ZEND_API void phptrace_execute_internal(zend_execute_data *execute_data, int return_value_used TSRMLS_DC)
{
    phptrace_execute_core(1, execute_data, NULL, return_value_used TSRMLS_CC);
#else
ZEND_API void phptrace_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC)
{
    phptrace_execute_core(1, execute_data, fci, return_value_used TSRMLS_CC);
#endif
}
