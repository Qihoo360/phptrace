--TEST--
Trace special function  < 7.0 && >= 5.3
--INI--
trace.dotrace=1
--SKIPIF--
<?php
require 'skipif.inc';

if (version_compare(PHP_VERSION, '7.0', '>=') ||
        version_compare(PHP_VERSION, '5.3', '<')) {
    skip('Required version < 7.0 && >= 5.3');
}
?>
--FILE--
<?php
function call_normal($arg = null) {}

// Call in internal
call_user_func('call_normal');

// strlen
strlen('hello');

// Closure
call_user_func(function () {
    call_normal();
});
?>
--EXPECTF--
> cli php %s
> {main}() called at [%s:2]
    > call_user_func("call_normal") called at [%s:5]
        > call_normal() called at [%s:5]
        < call_normal() = NULL called at [%s:5] ~ %fs %fs
    < call_user_func("call_normal") = NULL called at [%s:5] ~ %fs %fs
    > strlen("hello") called at [%s:8]
    < strlen("hello") = 5 called at [%s:8] ~ %fs %fs
    > call_user_func(object(Closure)#1) called at [%s:13]
        > {closure:%s:11-13}() called at [%s:13]
            > call_normal() called at [%s:12]
            < call_normal() = NULL called at [%s:12] ~ %fs %fs
        < {closure:%s:11-13}() = NULL called at [%s:13] ~ %fs %fs
    < call_user_func(object(Closure)#1) = NULL called at [%s:13] ~ %fs %fs
< {main}() = 1 called at [%s:2] ~ %fs %fs
< cli php %s
