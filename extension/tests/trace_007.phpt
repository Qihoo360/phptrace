--TEST--
Trace Closure
--SKIPIF--
<?php
require 'skipif.inc';
trace_skipif_no_trace_start();

if (version_compare(PHP_VERSION, '5.3', '<')) {
    echo 'skip this test is for version >= 5.3';
}
?>
--FILE--
<?php trace_start();

function call_normal($arg = null) {}

call_normal(function () {});

call_user_func(function () {
    call_normal();
});

trace_end(); ?>
--EXPECTF--
> call_normal(object(Closure)#1) called at [%s:5]
    < call_normal(object(Closure)#1) = NULL called at [%s:5] wt: %f ct: %f
    > call_user_func(object(Closure)#1) called at [%s:9]
        > {closure:%s:7-9}() called at [%s:9]
            > call_normal() called at [%s:8]
            < call_normal() = NULL called at [%s:8] wt: %f ct: %f
        < {closure:%s:7-9}() = NULL called at [%s:9] wt: %f ct: %f
    < call_user_func(object(Closure)#1) = NULL called at [%s:9] wt: %f ct: %f
    > trace_end() called at [%s:11]
