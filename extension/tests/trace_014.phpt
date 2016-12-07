--TEST--
Trace tick handlers  < 5.6.12
--SKIPIF--
<?php
require 'skipif.inc';
trace_skipif_no_trace_start();

if (version_compare(PHP_VERSION, '5.6.12', '>=')) {
    echo 'skip this test is for version < 5.6.12';
}
?>
--FILE--
<?php trace_start();

function handler_for_tick() {}

register_tick_function('handler_for_tick');

declare (ticks = 1) { $a = null; }

trace_end(); ?>
--EXPECTF--
> register_tick_function("handler_for_tick") called at [%s:5]
    < register_tick_function("handler_for_tick") = true called at [%s:5] ~ %fs %fs
    > handler_for_tick() called at [%s:7]
    < handler_for_tick() = NULL called at [%s:7] ~ %fs %fs
    > handler_for_tick() called at [%s:7]
    < handler_for_tick() = NULL called at [%s:7] ~ %fs %fs
    > trace_end() called at [%s:9]
