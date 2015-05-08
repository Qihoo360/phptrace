--TEST--
Trace call under namespace
--INI--
phptrace.dotrace=on
--FILE--
<?php
require 'trace_006_include.inc';

\Trace\Test\call_normal();
$o = new \Trace\Test\TestClass();
$o->callNormal();

Trace\Test\call_normal();
$o = new Trace\Test\TestClass();
$o->callNormal();

use Trace\Test;
Test\call_normal();
$o = new Test\TestClass();
$o->callNormal();

use Trace\Test\TestClass;
$o = new TestClass();
$o->callNormal();

use Trace\Test\TestClass as TTT;
$o = new TTT();
$o->callNormal();
?>
--EXPECTF--
> require("%s") called at [%s:2]
    > require("%s") called at [%s:2]
    < require("%s") = 1 called at [%s:2] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:4]
    < Trace\Test\call_normal() = NULL called at [%s:4] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:6]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:6] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:8]
    < Trace\Test\call_normal() = NULL called at [%s:8] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:10]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:10] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:13]
    < Trace\Test\call_normal() = NULL called at [%s:13] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:15]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:15] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:19]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:19] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:23]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:23] wt: %f ct: %f
< require("%s") = 1 called at [%s:2] wt: %f ct: %f
