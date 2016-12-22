--TEST--
Trace filter function name 
--INI--
trace.enable=1
--FILE--
<?php
trace_set_filter(PT_FILTER_FUNCTION_NAME, 'simple');
trace_start();

function simple() 
{
	$a = 1;
	$b = 3;
}

function simple2()
{
	$a = 1;
	$b = 3;
}

function simpl3()
{

	$a = 2;
	$b = 3;
}

simple();
simple2();
simpl3();
trace_end();
?>
--EXPECTF--
> simple() called at [%s:24]
    < simple() = NULL called at [%s:24] ~ %fs %fs
    > simple2() called at [%s:25]
    < simple2() = NULL called at [%s:25] ~ %fs %fs
