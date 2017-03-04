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

// Lambda
$f = create_function('', 'return 0;');
$f();
$g = create_function('', 'return 1;');
$g();

// Variable function
$f = 'call_normal';
$f();

// Eval
eval('substr("shit", 0, 1);call_normal();');

// TODO generator, yield
?>
--EXPECTF--
> cli php %s
> {main}() called at [%s:2]
    > require_once("%s_require.inc") called at [%s:5]
        > basename("%s"...) called at [%s_require.inc:2]
        < basename("%s"...) = "trace_002_require.inc" called at [%s_require.inc:2] ~ %fs %fs
        > substr("in trace_002_require.inc", 0, 1) called at [%s_require.inc:2]
        < substr("in trace_002_require.inc", 0, 1) = "i" called at [%s_require.inc:2] ~ %fs %fs
    < require_once("%s_require.inc") = 1 called at [%s:5] ~ %fs %fs
    > include_once("%s_include.inc") called at [%s:6]
        > basename("%s"...) called at [%s_include.inc:2]
        < basename("%s"...) = "trace_002_include.inc" called at [%s_include.inc:2] ~ %fs %fs
        > substr("in trace_002_include.inc", 0, 1) called at [%s_include.inc:2]
        < substr("in trace_002_include.inc", 0, 1) = "i" called at [%s_include.inc:2] ~ %fs %fs
    < include_once("%s_include.inc") = 1 called at [%s:6] ~ %fs %fs
    > require("%s_require.inc") called at [%s:7]
        > basename("%s"...) called at [%s_require.inc:2]
        < basename("%s"...) = "trace_002_require.inc" called at [%s_require.inc:2] ~ %fs %fs
        > substr("in trace_002_require.inc", 0, 1) called at [%s_require.inc:2]
        < substr("in trace_002_require.inc", 0, 1) = "i" called at [%s_require.inc:2] ~ %fs %fs
    < require("%s_require.inc") = 1 called at [%s:7] ~ %fs %fs
    > include("%s_include.inc") called at [%s:8]
        > basename("%s"...) called at [%s_include.inc:2]
        < basename("%s"...) = "trace_002_include.inc" called at [%s_include.inc:2] ~ %fs %fs
        > substr("in trace_002_include.inc", 0, 1) called at [%s_include.inc:2]
        < substr("in trace_002_include.inc", 0, 1) = "i" called at [%s_include.inc:2] ~ %fs %fs
    < include("%s_include.inc") = 1 called at [%s:8] ~ %fs %fs
    > create_function("", "return 0;") called at [%s:11]
        > %r({main}|include|create_function)\(.*\)%r called at [%s:11]
        < %r({main}|include|create_function)\(.*\)%r = NULL called at [%s:11] ~ %fs %fs
    < create_function("", "return 0;") = "\x00lambda_1" called at [%s:11] ~ %fs %fs
    > {lambda:%s(11) : runtime-created function}() called at [%s:12]
    < {lambda:%s(11) : runtime-created function}() = 0 called at [%s:12] ~ %fs %fs
    > create_function("", "return 1;") called at [%s:13]
        > %r({main}|include|create_function)\(.*\)%r called at [%s:13]
        < %r({main}|include|create_function)\(.*\)%r = NULL called at [%s:13] ~ %fs %fs
    < create_function("", "return 1;") = "\x00lambda_2" called at [%s:13] ~ %fs %fs
    > {lambda:%s(13) : runtime-created function}() called at [%s:14]
    < {lambda:%s(13) : runtime-created function}() = 1 called at [%s:14] ~ %fs %fs
    > call_normal() called at [%s:18]
    < call_normal() = NULL called at [%s:18] ~ %fs %fs
    > {eval} called at [%s:21]
        > substr("shit", 0, 1) called at [%s(21) : eval()'d code:1]
        < substr("shit", 0, 1) = "s" called at [%s(21) : eval()'d code:1] ~ %fs %fs
        > call_normal() called at [%s(21) : eval()'d code:1]
        < call_normal() = NULL called at [%s(21) : eval()'d code:1] ~ %fs %fs
    < {eval} = NULL called at [%s:21] ~ %fs %fs
< {main}() = 1 called at [%s:2] ~ %fs %fs
< cli php %s
