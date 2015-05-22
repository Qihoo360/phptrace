--TEST--
Trace Trait
--SKIPIF--
<?php
require 'skipif.inc';
trace_skipif_no_trace_start();

if (version_compare(PHP_VERSION, '5.4', '<')) {
    echo 'skip this test is for version >= 5.4';
}
?>
--FILE--
<?php trace_start();

trait TestTrait
{
    public function callNormal() {}
}

class TestClassA
{
    use TestTrait;
}

class TestClassB
{
    use TestTrait;
}

class TestClassC
{
    use TestTrait {
        TestTrait::callNormal as callAlias;
    }
}

$a = new TestClassA();
$b = new TestClassB();
$c = new TestClassC();

$a->callNormal();
$b->callNormal();
$c->callAlias();

trace_end(); ?>
--EXPECTF--
> TestClassA->callNormal() called at [%s:29]
    < TestClassA->callNormal() = NULL called at [%s:29] wt: %f ct: %f
    > TestClassB->callNormal() called at [%s:30]
    < TestClassB->callNormal() = NULL called at [%s:30] wt: %f ct: %f
    > TestClassC->callAlias() called at [%s:31]
    < TestClassC->callAlias() = NULL called at [%s:31] wt: %f ct: %f
    > trace_end() called at [%s:33]
