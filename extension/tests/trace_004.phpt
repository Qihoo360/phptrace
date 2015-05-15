--TEST--
Trace various handlers
--INI--
trace.dotrace=1
--FILE--
<?php
// Tick
function handler_for_tick() {}
register_tick_function('handler_for_tick');
declare (ticks = 1) {;}

// Shutdown
function handler_for_shutdown() {}
register_shutdown_function('handler_for_shutdown');

// Error
function handler_for_error($errno, $errstr) {}
set_error_handler('handler_for_error', E_ALL);
trigger_error('test', E_USER_NOTICE);
$a = $b; // Notice: Undefined variable

// Exception
function handler_for_exception(Exception $ex) {}
set_exception_handler('handler_for_exception');
throw new Exception('test');
?>
--EXPECTF--
> {main}() called at [%s:3]
    > register_tick_function("handler_for_tick") called at [%s:4]
    < register_tick_function("handler_for_tick") = true called at [%s:4] wt: %f ct: %f
    > handler_for_tick() called at [%s:5]
    < handler_for_tick() = NULL called at [%s:5] wt: %f ct: %f
    > handler_for_tick() called at [%s:5]
    < handler_for_tick() = NULL called at [%s:5] wt: %f ct: %f
    > register_shutdown_function("handler_for_shutdown") called at [%s:9]
    < register_shutdown_function("handler_for_shutdown") = NULL called at [%s:9] wt: %f ct: %f
    > set_error_handler("handler_for_error", %d) called at [%s:13]
    < set_error_handler("handler_for_error", %d) = NULL called at [%s:13] wt: %f ct: %f
    > trigger_error("test", %d) called at [%s:14]
        > handler_for_error(%d, "test", "%s"..., 14, array(%d)) called at [%s:14]
        < handler_for_error(%d, "test", "%s"..., 14, array(%d)) = NULL called at [%s:14] wt: %f ct: %f
    < trigger_error("test", %d) = true called at [%s:14] wt: %f ct: %f
    > handler_for_error(8, "Undefined variable: b", %s, 15, array(%d)) called at [%s:15]
    < handler_for_error(8, "Undefined variable: b", %s, 15, array(%d)) = NULL called at [%s:15] wt: %f ct: %f
    > set_exception_handler("handler_for_exception") called at [%s:19]
    < set_exception_handler("handler_for_exception") = NULL called at [%s:19] wt: %f ct: %f
    > Exception->__construct("test") called at [%s:20]
    < Exception->__construct("test") = NULL called at [%s:20] wt: %f ct: %f
< {main}() called at [%s:3] wt: %f ct: %f
> handler_for_exception(object(Exception)#1) called at [(null):0]
< handler_for_exception(object(Exception)#1) = NULL called at [(null):0] wt: %f ct: %f
> handler_for_shutdown() called at [(null):0]
< handler_for_shutdown() = NULL called at [(null):0] wt: %f ct: %f
