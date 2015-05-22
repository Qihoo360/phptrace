--TEST--
Trace call under namespace
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

trace_end(); ?>
--EXPECTF--
> require(%s) called at [%s:3]
    < require(%s) = 1 called at [%s:3] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:5]
    < Trace\Test\call_normal() = NULL called at [%s:5] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:7]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:7] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:9]
    < Trace\Test\call_normal() = NULL called at [%s:9] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:11]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:11] wt: %f ct: %f
    > Trace\Test\call_normal() called at [%s:14]
    < Trace\Test\call_normal() = NULL called at [%s:14] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:16]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:16] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:20]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:20] wt: %f ct: %f
    > Trace\Test\TestClass->callNormal() called at [%s:24]
    < Trace\Test\TestClass->callNormal() = NULL called at [%s:24] wt: %f ct: %f
    > trace_end() called at [%s:26]
