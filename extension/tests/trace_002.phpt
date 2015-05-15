--TEST--
Trace include, require, lambda, eval
--INI--
trace.dotrace=1
--FILE--
<?php
function call_normal($arg = null) {}

// Includes
require_once 'trace_002_require.inc';
include_once 'trace_002_include.inc';
require      'trace_002_require.inc';
include      'trace_002_include.inc';

// Call in internal
call_user_func('call_normal');

// Lambda
$f = create_function('', 'return 0;');
$f();
$g = create_function('', 'return 1;');
$g();

// Variable function
$f = 'call_normal';
$f();

// Eval
eval('strlen("shit");call_normal();');

// TODO generator, yield
?>
--EXPECTF--
> {main}() called at [%s:2]
    > require_once("%s_require.inc") called at [%s:5]
        > basename("%s"...) called at [%s_require.inc:2]
        < basename("%s"...) = "trace_002_require.inc" called at [%s_require.inc:2] wt: %f ct: %f
        > strlen("in trace_002_require.inc") called at [%s_require.inc:2]
        < strlen("in trace_002_require.inc") = 24 called at [%s_require.inc:2] wt: %f ct: %f
    < require_once("%s_require.inc") = 1 called at [%s:5] wt: %f ct: %f
    > include_once("%s_include.inc") called at [%s:6]
        > basename("%s"...) called at [%s_include.inc:2]
        < basename("%s"...) = "trace_002_include.inc" called at [%s_include.inc:2] wt: %f ct: %f
        > strlen("in trace_002_include.inc") called at [%s_include.inc:2]
        < strlen("in trace_002_include.inc") = 24 called at [%s_include.inc:2] wt: %f ct: %f
    < include_once("%s_include.inc") = 1 called at [%s:6] wt: %f ct: %f
    > require("%s_require.inc") called at [%s:7]
        > basename("%s"...) called at [%s_require.inc:2]
        < basename("%s"...) = "trace_002_require.inc" called at [%s_require.inc:2] wt: %f ct: %f
        > strlen("in trace_002_require.inc") called at [%s_require.inc:2]
        < strlen("in trace_002_require.inc") = 24 called at [%s_require.inc:2] wt: %f ct: %f
    < require("%s_require.inc") = 1 called at [%s:7] wt: %f ct: %f
    > include("%s_include.inc") called at [%s:8]
        > basename("%s"...) called at [%s_include.inc:2]
        < basename("%s"...) = "trace_002_include.inc" called at [%s_include.inc:2] wt: %f ct: %f
        > strlen("in trace_002_include.inc") called at [%s_include.inc:2]
        < strlen("in trace_002_include.inc") = 24 called at [%s_include.inc:2] wt: %f ct: %f
    < include("%s_include.inc") = 1 called at [%s:8] wt: %f ct: %f
    > call_user_func("call_normal") called at [%s:11]
        > call_normal() called at [%s:11]
        < call_normal() = NULL called at [%s:11] wt: %f ct: %f
    < call_user_func("call_normal") = NULL called at [%s:11] wt: %f ct: %f
    > create_function("", "return 0;") called at [%s:14]
        > create_function("", "return 0;") called at [%s:14]
        < create_function("", "return 0;") = NULL called at [%s:14] wt: %f ct: %f
    < create_function("", "return 0;") = "\x00lambda_1" called at [%s:14] wt: %f ct: %f
    > {lambda:%s(14) : runtime-created function}() called at [%s:15]
    < {lambda:%s(14) : runtime-created function}() = 0 called at [%s:15] wt: %f ct: %f
    > create_function("", "return 1;") called at [%s:16]
        > create_function("", "return 1;") called at [%s:16]
        < create_function("", "return 1;") = NULL called at [%s:16] wt: %f ct: %f
    < create_function("", "return 1;") = "\x00lambda_2" called at [%s:16] wt: %f ct: %f
    > {lambda:%s(16) : runtime-created function}() called at [%s:17]
    < {lambda:%s(16) : runtime-created function}() = 1 called at [%s:17] wt: %f ct: %f
    > call_normal() called at [%s:21]
    < call_normal() = NULL called at [%s:21] wt: %f ct: %f
    > {eval} called at [%s:24]
        > strlen("shit") called at [%s(24) : eval()'d code:1]
        < strlen("shit") = 4 called at [%s(24) : eval()'d code:1] wt: %f ct: %f
        > call_normal() called at [%s(24) : eval()'d code:1]
        < call_normal() = NULL called at [%s(24) : eval()'d code:1] wt: %f ct: %f
    < {eval} = NULL called at [%s:24] wt: %f ct: %f
< {main}() = 1 called at [%s:2] wt: %f ct: %f
