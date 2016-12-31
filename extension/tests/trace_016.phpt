--TEST--
Trace filter class name 
--INI--
trace.enable=1
--FILE--
<?php
trace_set_filter(PT_FILTER_CLASS_NAME, 'simple');
trace_start();

class simple 
{
	public function __construct()
	{
	}
}

class simple2
{
	public function __construct()
	{
	}
}

class simpl
{
	public function __construct()
	{
	}
}

$s1 = new simple();
$s2 = new simple2();
$s3 = new simpl();
trace_end();
?>
--EXPECTF--
> simple->__construct() called at [%s:26]
    < simple->__construct() = NULL called at [%s:26] ~ %fs %fs
    > simple2->__construct() called at [%s:27]
    < simple2->__construct() = NULL called at [%s:27] ~ %fs %fs
