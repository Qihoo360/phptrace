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
    unsigned char internal;
    int type;
    char *class;
    char *function;
    char *filename;
    uint lineno;
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
static void php_phptrace_init_globals(zend_phptrace_globals *phptrace_globals)
{
    phptrace_globals->enable = 0;
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

    if (PHPTRACE_G(enable)) {
        /* Replace executor */
        zend_execute_ex = phptrace_execute_ex;
        zend_execute_internal = phptrace_execute_internal;
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phptrace)
{
    UNREGISTER_INI_ENTRIES();

    if (PHPTRACE_G(enable)) {
        /* TODO ini_set during runtime ? */
        /* Restore original executor */
        zend_execute_ex = phptrace_ori_execute_ex;
        zend_execute_internal = phptrace_ori_execute_internal;
    }

    return SUCCESS;
}

PHP_RINIT_FUNCTION(phptrace)
{
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
 * PHP-Trace Function
 * --------------------
 */
int phptrace_build_frame(phptrace_frame *frame, zend_execute_data *execute_data, unsigned char internal TSRMLS_DC)
{
    zend_execute_data *ex = execute_data;
    zend_function *zf;

    /* init */
    memset(frame, 0, sizeof(phptrace_frame));

    /* internal */
    frame->internal = internal;

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

    return 0;
}

void phptrace_destroy_frame(phptrace_frame *frame TSRMLS_DC)
{
    if (frame->class)
        free(frame->class);
    if (frame->function)
        free(frame->function);
    if (frame->filename)
        free(frame->filename);
}

static void pt_display_frame(phptrace_frame *frame, char *type)
{
    fprintf(stderr, "%s ", type);
    if (frame->type == PT_FUNC_NORMAL) {
        fprintf(stderr, "%s()", frame->function);
    } else if (frame->type == PT_FUNC_MEMBER) {
        fprintf(stderr, "%s->%s()", frame->class, frame->function);
    } else if (frame->type == PT_FUNC_STATIC) {
        fprintf(stderr, "%s::%s()", frame->class, frame->function);
    } else if (frame->type & PT_FUNC_INCLUDES) {
        fprintf(stderr, "%s", frame->function);
    } else {
        fprintf(stderr, "unknown");
    }
    fprintf(stderr, " called at [%s:%d]\n", frame->filename, frame->lineno);
}

static void pt_display_backtrace(zend_execute_data *ex, unsigned char internal TSRMLS_DC)
{
    phptrace_frame frame;
    while (ex) {
        if (!internal && !ex->prev_execute_data) {
            break;
        }

        phptrace_build_frame(&frame, ex, internal TSRMLS_CC);
        pt_display_frame(&frame, "#bt");
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
    phptrace_frame frame;
    phptrace_build_frame(&frame, execute_data, 0 TSRMLS_CC);

    /* pt_display_frame(&frame, "frame"); */

    if (strcmp(frame.function, "pt_backtrace") == 0) {
        pt_display_backtrace(execute_data, 0 TSRMLS_CC);
    }

    /* call original */
    phptrace_ori_execute_ex(execute_data TSRMLS_CC);

    /* pt_display_frame(&frame, "return"); */

    phptrace_destroy_frame(&frame TSRMLS_CC);
}

ZEND_API void phptrace_execute_internal(zend_execute_data *execute_data, zend_fcall_info *fci, int return_value_used TSRMLS_DC)
{
    phptrace_frame frame;
    phptrace_build_frame(&frame, execute_data, 1 TSRMLS_CC);

    /* pt_display_frame(&frame, "frame"); */

    if (strcmp(frame.function, "pt_backtrace") == 0) {
        pt_display_backtrace(execute_data, 1 TSRMLS_CC);
    }

    /* call original */
    if (phptrace_ori_execute_internal) {
        phptrace_ori_execute_internal(execute_data, fci, return_value_used TSRMLS_CC);
    } else {
        execute_internal(execute_data, fci, return_value_used TSRMLS_CC);
    }

    /* pt_display_frame(&frame, "return"); */

    phptrace_destroy_frame(&frame TSRMLS_CC);
}
