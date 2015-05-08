--TEST--
Trace call under namespace
--INI--
phptrace.dotrace=on
--FILE--
<?php
require 'trace_006_include.php';

\Trace\Test\call_normal();
(new \Trace\Test\TestClass())->callNormal();

Trace\Test\call_normal();
(new Trace\Test\TestClass())->callNormal();

use Trace\Test;
Test\call_normal();
(new Test\TestClass())->callNormal();

use Trace\Test\TestClass;
(new TestClass())->callNormal();

use Trace\Test\TestClass as TTT;
(new TTT())->callNormal();
?>
--EXPECTF--
> require("%s") called at [%s:2]
    > require("%s") called at [%s:2]
    < require("%s") = 1 called at [%s:2] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:4]
    < Trace\Test\call_normal() = NULL called at [%s:4] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:5]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:5] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:7]
    < Trace\Test\call_normal() = NULL called at [%s:7] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:8]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:8] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:11]
    < Trace\Test\call_normal() = NULL called at [%s:11] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:12]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:12] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:15]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:15] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:18]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:18] wt: %f ct: %f
< require("%s") = 1 called at [%s:2] wt: %f ct: %f
