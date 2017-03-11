--TEST--
Trace signal handler  < 7.1
--SKIPIF--
<?php
require 'skipif.inc';
require_debug_mode();
for_verion_lt('7.1');

require_function('pcntl_signal');
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
    < pcntl_signal(1, "handler_for_signal") = true called at [%s:4] ~ %fs %fs
    > getmypid() called at [%s:5]
    < getmypid() = %d called at [%s:5] ~ %fs %fs
    > system("kill -HUP %d") called at [%s:7]
    < system("kill -HUP %d") = "" called at [%s:7] ~ %fs %fs
    > handler_for_signal(1) called at [%s:7]
    < handler_for_signal(1) = NULL called at [%s:7] ~ %fs %fs
    > trace_end() called at [%s:10]
