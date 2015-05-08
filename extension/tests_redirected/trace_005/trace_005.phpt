--TEST--
(redirected)
--INI--
phptrace.dotrace=on
--FILE--
<?php
function handler_for_signal($signo) {}
pcntl_signal(SIGHUP, 'handler_for_signal');
$cmd = 'kill -HUP '.getmypid();
declare (ticks = 1) {
    system($cmd);
}
?>
--EXPECTF--
> {main}() called at [%s:28]
    > pcntl_signal(1, "handler_for_signal") called at [%s:29]
    < pcntl_signal(1, "handler_for_signal") = true called at [%s:29] wt: %f ct: %f
    > getmypid() called at [%s:30]
    < getmypid() = %d called at [%s:30] wt: %f ct: %f
    > system("kill -HUP %d") called at [%s:32]
    < system("kill -HUP %d") = "" called at [%s:32] wt: %f ct: %f
    > handler_for_signal(1) called at [%s:32]
    < handler_for_signal(1) = NULL called at [%s:32] wt: %f ct: %f
< {main}() = 1 called at [%s:28] wt: %f ct: %f
