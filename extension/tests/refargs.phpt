--TEST--
Trace with arguments by-reference #Issue 77
--INI--
trace.dotrace=1
--FILE--
<?php
function recv_arg($arg) {}
function recv_ref_arg(&$arg) {}

$data = 'hello';
recv_arg($data, 'dummy');
recv_ref_arg($data, 'dummy');

$st = null;
parse_str('', $st);
?>
--EXPECTF--
> cli php %s
> {main}() called at [%s:2]
    > recv_arg("hello", "dummy") called at [%s:6]
    < recv_arg("hello", "dummy") = NULL called at [%s:6] ~ %fs %fs
    > recv_ref_arg("hello", "dummy") called at [%s:7]
    < recv_ref_arg("hello", "dummy") = NULL called at [%s:7] ~ %fs %fs
    > parse_str("", NULL) called at [%s:10]
    < parse_str("", NULL) = NULL called at [%s:10] ~ %fs %fs
< {main}() = 1 called at [%s:2] ~ %fs %fs
< cli php %s
