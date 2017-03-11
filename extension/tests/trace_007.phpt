--TEST--
Trace Closure
--SKIPIF--
<?php
require 'skipif.inc';
require_debug_mode();
for_verion_gte('5.3');
?>
--FILE--
<?php trace_start();

function call_normal($arg = null) {}

function call_closure($c) { $c(); }

// Closure as argument
call_normal(function () {});

// Call closure
call_closure(function () {
    call_normal();
});

trace_end(); ?>
--EXPECTF--
> call_normal(object(Closure)#1) called at [%s:8]
    < call_normal(object(Closure)#1) = NULL called at [%s:8] ~ %fs %fs
    > call_closure(object(Closure)#1) called at [%s:13]
        > {closure:%s:11-13}() called at [%s:5]
            > call_normal() called at [%s:12]
            < call_normal() = NULL called at [%s:12] ~ %fs %fs
        < {closure:%s:11-13}() = NULL called at [%s:5] ~ %fs %fs
    < call_closure(object(Closure)#1) = NULL called at [%s:13] ~ %fs %fs
    > trace_end() called at [%s:15]
