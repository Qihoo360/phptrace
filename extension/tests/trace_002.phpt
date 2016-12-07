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
> cli php %s
> {main}() called at [%s:2]
    > require_once("%s_require.inc") called at [%s:5]
        > basename("%s"...) called at [%s_require.inc:2]
        < basename("%s"...) = "trace_002_require.inc" called at [%s_require.inc:2] ~ %fs %fs
        > strlen("in trace_002_require.inc") called at [%s_require.inc:2]
        < strlen("in trace_002_require.inc") = 24 called at [%s_require.inc:2] ~ %fs %fs
    < require_once("%s_require.inc") = 1 called at [%s:5] ~ %fs %fs
    > include_once("%s_include.inc") called at [%s:6]
        > basename("%s"...) called at [%s_include.inc:2]
        < basename("%s"...) = "trace_002_include.inc" called at [%s_include.inc:2] ~ %fs %fs
        > strlen("in trace_002_include.inc") called at [%s_include.inc:2]
        < strlen("in trace_002_include.inc") = 24 called at [%s_include.inc:2] ~ %fs %fs
    < include_once("%s_include.inc") = 1 called at [%s:6] ~ %fs %fs
    > require("%s_require.inc") called at [%s:7]
        > basename("%s"...) called at [%s_require.inc:2]
        < basename("%s"...) = "trace_002_require.inc" called at [%s_require.inc:2] ~ %fs %fs
        > strlen("in trace_002_require.inc") called at [%s_require.inc:2]
        < strlen("in trace_002_require.inc") = 24 called at [%s_require.inc:2] ~ %fs %fs
    < require("%s_require.inc") = 1 called at [%s:7] ~ %fs %fs
    > include("%s_include.inc") called at [%s:8]
        > basename("%s"...) called at [%s_include.inc:2]
        < basename("%s"...) = "trace_002_include.inc" called at [%s_include.inc:2] ~ %fs %fs
        > strlen("in trace_002_include.inc") called at [%s_include.inc:2]
        < strlen("in trace_002_include.inc") = 24 called at [%s_include.inc:2] ~ %fs %fs
    < include("%s_include.inc") = 1 called at [%s:8] ~ %fs %fs
    > call_user_func("call_normal") called at [%s:11]
        > call_normal() called at [%s:11]
        < call_normal() = NULL called at [%s:11] ~ %fs %fs
    < call_user_func("call_normal") = NULL called at [%s:11] ~ %fs %fs
    > create_function("", "return 0;") called at [%s:14]
        > %r({main}|include|create_function)\(.*\)%r called at [%s:14]
        < %r({main}|include|create_function)\(.*\)%r = NULL called at [%s:14] ~ %fs %fs
    < create_function("", "return 0;") = "\x00lambda_1" called at [%s:14] ~ %fs %fs
    > {lambda:%s(14) : runtime-created function}() called at [%s:15]
    < {lambda:%s(14) : runtime-created function}() = 0 called at [%s:15] ~ %fs %fs
    > create_function("", "return 1;") called at [%s:16]
        > %r({main}|include|create_function)\(.*\)%r called at [%s:16]
        < %r({main}|include|create_function)\(.*\)%r = NULL called at [%s:16] ~ %fs %fs
    < create_function("", "return 1;") = "\x00lambda_2" called at [%s:16] ~ %fs %fs
    > {lambda:%s(16) : runtime-created function}() called at [%s:17]
    < {lambda:%s(16) : runtime-created function}() = 1 called at [%s:17] ~ %fs %fs
    > call_normal() called at [%s:21]
    < call_normal() = NULL called at [%s:21] ~ %fs %fs
    > {eval} called at [%s:24]
        > strlen("shit") called at [%s(24) : eval()'d code:1]
        < strlen("shit") = 4 called at [%s(24) : eval()'d code:1] ~ %fs %fs
        > call_normal() called at [%s(24) : eval()'d code:1]
        < call_normal() = NULL called at [%s(24) : eval()'d code:1] ~ %fs %fs
    < {eval} = NULL called at [%s:24] ~ %fs %fs
< {main}() = 1 called at [%s:2] ~ %fs %fs
< cli php %s
