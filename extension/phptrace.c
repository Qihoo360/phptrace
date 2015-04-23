#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_phptrace.h"


/**
 * PHP-Trace Global
 * --------------------
 */
#define get_us_delta(begin, end) (((end.tv_sec - begin.tv_sec) * 1000000) + \
     (end.tv_usec - begin.tv_usec))

#define FP(element) frame.profile.element
#define PROFILING_BEGIN() do { \
    gettimeofday(&PTG(pb_tv), 0); \
    getrusage(RUSAGE_SELF, &PTG(pb_ru)); \
    PTG(pb_mem) = zend_memory_usage(0 TSRMLS_CC); \
    PTG(pb_mempeak) = zend_memory_peak_usage(0 TSRMLS_CC); \
} while (0);
#define PROFILING_END(wall_time, cpu_time, mem, mempeak) do { \
    gettimeofday(&PTG(pe_tv), 0); \
    wall_time = get_us_delta(PTG(pb_tv), PTG(pe_tv)); \
    getrusage(RUSAGE_SELF, &PTG(pe_ru)); \
    cpu_time = get_us_delta(PTG(pb_ru).ru_utime, PTG(pe_ru).ru_utime) + \
        get_us_delta(PTG(pb_ru).ru_stime, PTG(pe_ru).ru_stime); \
    mem = zend_memory_usage(0 TSRMLS_CC) - PTG(pb_mem); \
    mempeak = zend_memory_peak_usage(0 TSRMLS_CC) - PTG(pb_mempeak); \
} while (0);

#define PT_FUNC_UNKNOWN         0x00
#define PT_FUNC_NORMAL          0x01
#define PT_FUNC_MEMBER          0x02
#define PT_FUNC_STATIC          0x03

#define PT_FUNC_INCLUDES        0x10
#define PT_FUNC_INCLUDE         0x10
#define PT_FUNC_INCLUDE_ONCE    0x11
#define PT_FUNC_REQUIRE         0x12
#define PT_FUNC_REQUIRE_ONCE    0x13
#define PT_FUNC_EVAL            0x14

typedef struct _phptrace_frame {
    zend_bool internal;         /* is ZEND_INTERNAL_FUNCTION */
    int type;                   /* flags of PT_FUNC_xxx */
    uint32_t lineno;            /* entry lineno */
    char *filename;             /* entry filename */
    char *class;                /* class name */
    char *function;             /* function name */
    uint32_t level;             /* nesting level during executing */

    int arg_count;              /* arguments number */
    char **args;                /* arguments represent string */
    char *retval;               /* return value represent string */

    struct {
        uint64_t    wall_time;  /* wall time cost */
        uint64_t    cpu_time;   /* cpu time cost */
        int64_t     mem;        /* delta memory usage */
        int64_t     mempeak;    /* delta memory peak */
    } profile;
} phptrace_frame;

static void (*phptrace_ori_execute_ex)(zend_execute_data *execute_data TSRMLS_DC);
static void (*phptrace_ori_execute_internal)(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC);
ZEND_API void phptrace_execute_ex(zend_execute_data *execute_data TSRMLS_DC);
ZEND_API void phptrace_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC);


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
    PHP_FE(phptrace_stack,  NULL)
    PHP_FE(phptrace_begin,  NULL)
    PHP_FE(phptrace_end,    NULL)
    PHP_FE_END  /* Must be the last line in phptrace_functions[] */
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
    STD_PHP_INI_ENTRY("phptrace.enable",    "0", PHP_INI_ALL, OnUpdateLong, enable, zend_phptrace_globals, phptrace_globals)
PHP_INI_END()

/* php_phptrace_init_globals */
static void php_phptrace_init_globals(zend_phptrace_globals *ptg)
{
    ptg->enable = 0;

    ptg->do_trace = 0;
    ptg->do_stack = 0;

    ptg->level = 0;
}


/**
 * PHP Extension Function
 * --------------------
 */

PHP_MINIT_FUNCTION(phptrace)
{
    REGISTER_INI_ENTRIES();

    /* Save original executor */
    phptrace_ori_execute_ex = zend_execute_ex;
    phptrace_ori_execute_internal = zend_execute_internal;

    if (PTG(enable)) {
        /* Replace executor */
        zend_execute_ex = phptrace_execute_ex;
        zend_execute_internal = phptrace_execute_internal;
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phptrace)
{
    UNREGISTER_INI_ENTRIES();

    if (PTG(enable)) {
        /* TODO ini_set during runtime ? */
        /* Restore original executor */
        zend_execute_ex = phptrace_ori_execute_ex;
        zend_execute_internal = phptrace_ori_execute_internal;
    }

    return SUCCESS;
}

PHP_RINIT_FUNCTION(phptrace)
{
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

    /* Remove comments if you have entries in php.ini */
    DISPLAY_INI_ENTRIES();
}


/**
 * PHP-Trace Interface
 * --------------------
 */
PHP_FUNCTION(phptrace_stack)
{
    PTG(do_stack) = 1;
}

PHP_FUNCTION(phptrace_begin)
{
    PTG(do_trace) = 1;
}

PHP_FUNCTION(phptrace_end)
{
    PTG(do_trace) = 0;
}


/**
 * PHP-Trace Function
 * --------------------
 */
void phptrace_build_frame(phptrace_frame *frame, zend_execute_data *execute_data, zend_bool internal TSRMLS_DC)
{
    int i;
    zval **args;
    zend_execute_data *ex = execute_data;
    zend_function *zf;

    /* init */
    memset(frame, 0, sizeof(phptrace_frame));

    /* internal */
    frame->internal = internal;
    frame->level = PTG(level);

    /**
     * Current execute_data is the data going to be executed not the entry
     * point, so we switch to previous data. The internal function is a
     * exception because it's no need to execute by op_array.
     */
    if (!internal && ex->prev_execute_data) {
        ex = ex->prev_execute_data;
    }
    zf = ex->function_state.function;

    /* names */
    if (zf->common.function_name) {
        /* type, class name */
        if (ex->object) {
            frame->type = PT_FUNC_MEMBER;
            /**
             * User care about which method is called exactly, so use
             * zf->common.scope->name instead of ex->object->name.
             */
            if (zf->common.scope) {
                frame->class = strdup(zf->common.scope->name);
            } else {
                /* FIXME zend use zend_get_object_classname() */
                fprintf(stderr, "why here %d\n", __LINE__);
            }
        } else if (zf->common.scope) {
            frame->type = PT_FUNC_STATIC;
            frame->class = strdup(zf->common.scope->name);
        } else {
            frame->type = PT_FUNC_NORMAL;
        }

        /* function name */
        if (strcmp(zf->common.function_name, "{closure}") == 0) {
            asprintf(&frame->function, "{closure:%s:%d-%d}", zf->op_array.filename, zf->op_array.line_start, zf->op_array.line_end);
        } else if (strcmp(zf->common.function_name, "__lambda_func") == 0) {
            asprintf(&frame->function, "{lambda:%s}", zf->op_array.filename);
        } else {
            /**
             * TODO deal trait alias better, zend has a function below:
             * zend_resolve_method_name(ex->object ?  Z_OBJCE_P(ex->object) : zf->common.scope, zf)
             */
            frame->function = strdup(zf->common.function_name);
        }
    } else {
        int add_filename = 1;

        /* special user function */
        switch (ex->opline->extended_value) {
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
                frame->function = strdup("{eval}");
                add_filename = 0;
                break;
            default:
                /* should be function main */
                frame->type = PT_FUNC_NORMAL;
                frame->function = strdup("{main}");
                add_filename = 0;
                break;
        }
        if (add_filename) {
            asprintf(&frame->function, "{%s:%s}", frame->function, zf->op_array.filename);
        }
    }

    /* args */
    if (ex->function_state.arguments) {
        frame->arg_count = (int)(zend_uintptr_t) *(ex->function_state.arguments);
        args = (zval **)(ex->function_state.arguments - frame->arg_count);
        frame->args = calloc(frame->arg_count, sizeof(char *));
        for (i = 0; i < frame->arg_count; i++) {
            phptrace_repr_zval(&frame->args[i], args[i], 0 TSRMLS_CC);
        }
    } else {
        frame->arg_count = 0;
        frame->args = NULL;
    }

    /* lineno */
    if (ex->opline) {
        frame->lineno = ex->opline->lineno;
    } else if (ex->prev_execute_data && ex->prev_execute_data->opline) {
        /* FIXME try it in loop ? */
        frame->lineno = ex->prev_execute_data->opline->lineno; /* try using prev */
    } else if (ex != execute_data && execute_data->opline) {
        frame->lineno = execute_data->opline->lineno; /* try using current */
    } else {
        frame->lineno = 0;
    }

    /* filename */
    if (ex->op_array) {
        frame->filename = strdup(ex->op_array->filename);
    } else if (ex->prev_execute_data && ex->prev_execute_data->op_array) {
        frame->filename = strdup(ex->prev_execute_data->op_array->filename); /* try using prev */
    } else if (ex != execute_data && execute_data->op_array) {
        frame->filename = strdup(execute_data->op_array->filename); /* try using current */
    } else {
        frame->filename = NULL;
    }
}

void phptrace_destroy_frame(phptrace_frame *frame TSRMLS_DC)
{
    int i;

    if (frame->class)
        free(frame->class);
    if (frame->function)
        free(frame->function);
    if (frame->filename)
        free(frame->filename);
    if (frame->args && frame->arg_count) {
        for (i = 0; i < frame->arg_count; i++) {
            free(frame->args[i]);
        }
    }
    if (frame->retval)
        free(frame->retval);
}

void phptrace_frame_set_retval(phptrace_frame *frame, zend_bool internal, zend_execute_data *ex, zend_fcall_info *fci TSRMLS_DC)
{
    zval *retval;

    if (internal) {
        if (fci != NULL) {
            retval = *fci->retval_ptr_ptr;
        } else {
            retval = EX_TMP_VAR(ex, ex->opline->result.var)->var.ptr;
        }
    } else if (*EG(return_value_ptr_ptr)) {
        retval = *EG(return_value_ptr_ptr);
    }

    if (retval) {
        phptrace_repr_zval(&frame->retval, retval TSRMLS_CC);
    }
}

void phptrace_repr_zval(char **repr, zval *zv, int limit TSRMLS_DC)
{
    char *buf, *tmp_str = NULL;
    int tmp_len = 0;

    /* php_var_export_ex is a good example */
    /* TODO limit string length */
    switch (Z_TYPE_P(zv)) {
        case IS_BOOL:
            if (Z_LVAL_P(zv)) {
                buf = strdup("true");
            } else {
                buf = strdup("false");
            }
            break;
        case IS_NULL:
            buf = strdup("NULL");
            break;
        case IS_LONG:
            asprintf(&buf, "%ld", Z_LVAL_P(zv));
            break;
        case IS_DOUBLE:
            asprintf(&buf, "%f", Z_DVAL_P(zv));
            break;
        case IS_STRING:
            /* TODO addslashes and deal special chars, make it binary safe */
            buf = malloc(Z_STRLEN_P(zv) + 3);
            buf[0] = '\'';
            memcpy(buf + 1, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
            buf[Z_STRLEN_P(zv) + 1] = '\'';
            buf[Z_STRLEN_P(zv) + 2] = '\0';
            break;
        case IS_ARRAY:
            asprintf(&buf, "array(%d)", zend_hash_num_elements(Z_ARRVAL_P(zv)));
            break;
        case IS_OBJECT:
            if (Z_OBJ_HANDLER(*zv, get_class_name)) {
                Z_OBJ_HANDLER(*zv, get_class_name)(zv, &tmp_str, &tmp_len, 0 TSRMLS_CC);
                asprintf(&buf, "object(%s)#%d", tmp_str, Z_OBJ_HANDLE_P(zv));
            } else {
                asprintf(&buf, "object(unknown)#%d", Z_OBJ_HANDLE_P(zv));
            }

            break;
        case IS_RESOURCE:
            tmp_str = zend_rsrc_list_get_rsrc_type(Z_LVAL_P(zv) TSRMLS_CC);
            asprintf(&buf, "resource(%s)#%ld", tmp_str ? tmp_str : "Unknown", Z_LVAL_P(zv));
            break;
        default:
            buf = strdup("{unknown}");
            break;
    }

    /* FIXME efree or free */
    if (tmp_len) {
        efree(tmp_str);
    }

    *repr = buf;
}

static void pt_display_frame(phptrace_frame *frame, zend_bool indent, const char *format, ...)
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
    fprintf(stderr, " called at [%s:%d]\n", frame->filename, frame->lineno);
}

static void pt_display_backtrace(zend_execute_data *ex, zend_bool internal TSRMLS_DC)
{
    int num = 0;
    phptrace_frame frame;

    while (ex) {
        if (!internal && !ex->prev_execute_data) {
            break;
        }

        phptrace_build_frame(&frame, ex, internal TSRMLS_CC);
        frame.level = 1;
        pt_display_frame(&frame, 0, "#%-3d", num++);
        phptrace_destroy_frame(&frame TSRMLS_CC);

        ex = ex->prev_execute_data;
    }
}


/**
 * PHP-Trace Executor Replacement
 * --------------------
 */
ZEND_API void phptrace_execute_ex(zend_execute_data *execute_data TSRMLS_DC)
{
    zend_bool do_trace = PTG(do_trace);
    phptrace_frame frame;
    zval *retval = NULL;

    PTG(level)++;

    if (PTG(do_stack)) {
        pt_display_backtrace(execute_data, 0 TSRMLS_CC);
        PTG(do_stack) = 0;
    }

    if (do_trace) {
        phptrace_build_frame(&frame, execute_data, 0 TSRMLS_CC);

        /* Register reture value ptr */
        if (EG(return_value_ptr_ptr) == NULL) {
            EG(return_value_ptr_ptr) = &retval;
        }

        /* send entry */
        pt_display_frame(&frame, 1, "> ");
        PROFILING_BEGIN();
    }

    /* call original */
    phptrace_ori_execute_ex(execute_data TSRMLS_CC);

    if (do_trace) {
        PROFILING_END(FP(wall_time), FP(cpu_time), FP(mem), FP(mempeak));

        phptrace_frame_set_retval(&frame, 0, NULL, NULL);

        /* Free reture value */
        if (retval != NULL) {
            zval_ptr_dtor(&retval);
        }

        /* send return */
        /* fprintf(stderr, "wt: %.4f ct: %.4f mem: %ld mempeak: %ld\n", FP(wall_time) / 1000000.0, FP(cpu_time) / 1000000.0, FP(mem), FP(mempeak)); */
        pt_display_frame(&frame, 1, "< ");
        phptrace_destroy_frame(&frame TSRMLS_CC);
    }

    PTG(level)--;
}

ZEND_API void phptrace_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC)
{
    zend_bool do_trace = PTG(do_trace);
    phptrace_frame frame;

    PTG(level)++;

    if (PTG(do_stack)) {
        pt_display_backtrace(execute_data, 1 TSRMLS_CC);
        PTG(do_stack) = 0;
    }

    if (do_trace) {
        phptrace_build_frame(&frame, execute_data, 1 TSRMLS_CC);
        /* send entry */
        pt_display_frame(&frame, 1, "> ");
        PROFILING_BEGIN();
    }

    /* call original */
    if (phptrace_ori_execute_internal) {
        phptrace_ori_execute_internal(execute_data, fci, return_value_used TSRMLS_CC);
    } else {
        execute_internal(execute_data, fci, return_value_used TSRMLS_CC);
    }

    if (do_trace) {
        PROFILING_END(FP(wall_time), FP(cpu_time), FP(mem), FP(mempeak));
        phptrace_frame_set_retval(&frame, 1, execute_data, fci);
        /* send return */
        /* fprintf(stderr, "wt: %.4f ct: %.4f mem: %ld mempeak: %ld\n", FP(wall_time) / 1000000.0, FP(cpu_time) / 1000000.0, FP(mem), FP(mempeak)); */
        pt_display_frame(&frame, 1, "< ");
        phptrace_destroy_frame(&frame TSRMLS_CC);
    }

    PTG(level)--;
}
