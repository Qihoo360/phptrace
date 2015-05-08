--TEST--
Trace handlers
--INI--
phptrace.dotrace=on
--FILE--
<?php
// Autoload
function handler_for_autoload($name) { eval('class '.$name.' {}'); }
spl_autoload_register('handler_for_autoload');
new UndefinedClass;

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
    > spl_autoload_register("handler_for_autoload") called at [%s:4]
    < spl_autoload_register("handler_for_autoload") = true called at [%s:4] wt: %f ct: %f
    > spl_autoload_call("UndefinedClass") called at [%s:5]
        > handler_for_autoload("UndefinedClass") called at [(null):0]
            > {eval} called at [%s:3]
            < {eval} = NULL called at [%s:3] wt: %f ct: %f
        < handler_for_autoload("UndefinedClass") = NULL called at [(null):0] wt: %f ct: %f
    < spl_autoload_call("UndefinedClass") = NULL called at [%s:5] wt: %f ct: %f
    > register_tick_function("handler_for_tick") called at [%s:9]
    < register_tick_function("handler_for_tick") = true called at [%s:9] wt: %f ct: %f
    > handler_for_tick() called at [%s:10]
    < handler_for_tick() = NULL called at [%s:10] wt: %f ct: %f
    > handler_for_tick() called at [%s:10]
    < handler_for_tick() = NULL called at [%s:10] wt: %f ct: %f
    > register_shutdown_function("handler_for_shutdown") called at [%s:14]
    < register_shutdown_function("handler_for_shutdown") = NULL called at [%s:14] wt: %f ct: %f
    > set_error_handler("handler_for_error", 32767) called at [%s:18]
    < set_error_handler("handler_for_error", 32767) = NULL called at [%s:18] wt: %f ct: %f
    > trigger_error("test", 1024) called at [%s:19]
        > handler_for_error(1024, "test", "%s"..., 19, array(7)) called at [%s:19]
        < handler_for_error(1024, "test", "%s"..., 19, array(7)) = NULL called at [%s:19] wt: %f ct: %f
    < trigger_error("test", 1024) = true called at [%s:19] wt: %f ct: %f
    > handler_for_error(8, "Undefined variable: b", "%s"..., 20, array(7)) called at [%s:20]
    < handler_for_error(8, "Undefined variable: b", "%s"..., 20, array(7)) = NULL called at [%s:20] wt: %f ct: %f
    > set_exception_handler("handler_for_exception") called at [%s:24]
    < set_exception_handler("handler_for_exception") = NULL called at [%s:24] wt: %f ct: %f
    > Exception->__construct("test") called at [%s:25]
    < Exception->__construct("test") = NULL called at [%s:25] wt: %f ct: %f
< {main}() called at [%s:3] wt: %f ct: %f
> handler_for_exception(object(Exception)#1) called at [(null):0]
< handler_for_exception(object(Exception)#1) = NULL called at [(null):0] wt: %f ct: %f
> handler_for_shutdown() called at [(null):0]
< handler_for_shutdown() = NULL called at [(null):0] wt: %f ct: %f
