--TEST--
Trace magic method __callStatic, __invoke
--SKIPIF--
<?php
require 'skipif.inc';
trace_skipif_no_trace_start();

if (version_compare(PHP_VERSION, '5.3', '<')) {
    echo 'skip this test is for version >= 5.3';
}
?>
--FILE--
<?php trace_start();

class TestClass
{
    public static function __callStatic($name, $args) {}
    public function __invoke() {}
}

TestClass::undefineMethod();
$o = new TestClass();
$o();

trace_end(); ?>
--EXPECTF--
> TestClass::undefineMethod() called at [%s:9]
        > TestClass::__callStatic("undefineMethod", array(0)) called at [%s:9]
        < TestClass::__callStatic("undefineMethod", array(0)) = NULL called at [%s:9] wt: %f ct: %f
    < TestClass::undefineMethod() = NULL called at [%s:9] wt: %f ct: %f
    > TestClass->__invoke() called at [%s:11]
    < TestClass->__invoke() = NULL called at [%s:11] wt: %f ct: %f
    > trace_end() called at [%s:13]
