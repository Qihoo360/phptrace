--TEST--
Trace various handlers
--INI--
trace.dotrace=1
--FILE--
<?php
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
> cli php %s
> {main}() called at [%s:3]
    > register_shutdown_function("handler_for_shutdown") called at [%s:4]
    < register_shutdown_function("handler_for_shutdown") = NULL called at [%s:4] ~ %fs %fs
    > set_error_handler("handler_for_error", %d) called at [%s:8]
    < set_error_handler("handler_for_error", %d) = NULL called at [%s:8] ~ %fs %fs
    > trigger_error("test", %d) called at [%s:9]
        > handler_for_error(%d, "test", "%s"..., 9, array(%d)) called at [%s:9]
        < handler_for_error(%d, "test", "%s"..., 9, array(%d)) = NULL called at [%s:9] ~ %fs %fs
    < trigger_error("test", %d) = true called at [%s:9] ~ %fs %fs
    > handler_for_error(8, "Undefined variable: b", %s, 10, array(%d)) called at [%s:10]
    < handler_for_error(8, "Undefined variable: b", %s, 10, array(%d)) = NULL called at [%s:10] ~ %fs %fs
    > set_exception_handler("handler_for_exception") called at [%s:14]
    < set_exception_handler("handler_for_exception") = NULL called at [%s:14] ~ %fs %fs
    > Exception->__construct("test") called at [%s:15]
    < Exception->__construct("test") = NULL called at [%s:15] ~ %fs %fs
< {main}() %r(= {undef} )?%rcalled at [%s:3] ~ %fs %fs
> handler_for_exception(object(Exception)#1) called at [(null):0]
< handler_for_exception(object(Exception)#1) = NULL called at [(null):0] ~ %fs %fs
> handler_for_shutdown() called at [(null):0]
< handler_for_shutdown() = NULL called at [(null):0] ~ %fs %fs
< cli php %s
