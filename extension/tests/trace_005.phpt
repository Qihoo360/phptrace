--TEST--
Trace signal handler
--SKIPIF--
<?php
require 'skipif.inc';
trace_skipif_no_trace_start();

if (!function_exists('pcntl_signal')) {
    echo "skip this test is for pcntl_signal() only";
}
?>
--FILE--
<?php trace_start();

function handler_for_signal($signo) {}
pcntl_signal(SIGHUP, 'handler_for_signal');
$cmd = 'kill -HUP '.getmypid();
declare (ticks = 1) {
    system($cmd);
}

trace_end(); ?>
--EXPECTF--
> pcntl_signal(1, "handler_for_signal") called at [%s:4]
    < pcntl_signal(1, "handler_for_signal") = true called at [%s:4] wt: %f ct: %f
    > getmypid() called at [%s:5]
    < getmypid() = %d called at [%s:5] wt: %f ct: %f
    > system("kill -HUP %d") called at [%s:7]
    < system("kill -HUP %d") = "" called at [%s:7] wt: %f ct: %f
    > handler_for_signal(1) called at [%s:7]
    < handler_for_signal(1) = NULL called at [%s:7] wt: %f ct: %f
    > trace_end() called at [%s:10]
