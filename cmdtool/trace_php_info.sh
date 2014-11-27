#!/bin/sh

if test $# -ne 1; then
    #echo "Usage: `basename $0 .sh` <process-id>" 1>&2
    exit 1
fi

if test ! -r /proc/$1; then
    echo "Process $1 not found." 1>&2
    exit 1
fi


GDB=${GDB:-/usr/bin/gdb}

#if $GDB -nx --quiet --batch --readnever > /dev/null 2>&1; then
#    readnever=--readnever
#else
#    readnever=
#fi

sapi_globals="print &(sapi_globals)"
sapi_globals_1="print &(sapi_globals.request_info.path_translated)"
executor_globals="print &(executor_globals)"
executor_globals_1="print &(executor_globals.current_execute_data)"
executor_globals_10="print (executor_globals.current_execute_data)"
executor_globals_11="print &(executor_globals.current_execute_data->function_state.function)"
executor_globals_110="print (executor_globals.current_execute_data->function_state.function)"
executor_globals_111="print &(executor_globals.current_execute_data->function_state.function->common.function_name)"
executor_globals_12="print &(executor_globals.current_execute_data->prev_execute_data)"
executor_globals_13="print &(executor_globals.current_execute_data->op_array)"
executor_globals_130="print (executor_globals.current_execute_data->op_array)"
executor_globals_131="print &(executor_globals.current_execute_data->op_array->filename)"
executor_globals_14="print &(executor_globals.current_execute_data->opline)"
executor_globals_140="print (executor_globals.current_execute_data->opline)"
executor_globals_141="print &(executor_globals.current_execute_data->opline->lineno)"

# Run GDB, strip out unwanted noise.
$GDB --quiet $readnever -nx /proc/$1/exe $1 <<EOF 2>&1 | 
set width 0
set height 0
set pagination no
$sapi_globals
$sapi_globals_1
$executor_globals
$executor_globals_1
$executor_globals_10
$executor_globals_11
$executor_globals_110
$executor_globals_111
$executor_globals_12
$executor_globals_13
$executor_globals_130
$executor_globals_131
$executor_globals_14
$executor_globals_140
$executor_globals_141
EOF
/bin/sed -n \
    -e 's/^\((gdb) \)*//' \
    -e '/^\$/p' \
    -e '/^Thread/p' | awk '{print $NF}'
