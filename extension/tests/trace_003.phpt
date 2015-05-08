--TEST--
Trace class and object call
--INI--
phptrace.dotrace=on
--FILE--
<?php
class TestParent {
    public function callParent() {}
    public function callOverride() {}
    public static function staticParent() {}
    public static function staticOverride() {}
}

class TestClass extends TestParent {
    public function callNormal() {}
    public function callOverride() {}
    public static function staticNormal() {}
    public static function staticOverride() {}

    public function callInnerMethod() { $this->callNormal(); }
    public static function staticInnerMethod() { self::staticNormal(); }

    // Magic method
    public function __construct() {}
    public function __destruct() {}
    public function __call($name, $args) {}
    public static function __callStatic($name, $args) {}
    public function __get($name) {}
    public function __set($name, $value) {}
    public function __isset($name) {}
    public function __unset($name) {}
    public function __sleep() { return array(); }
    public function __wakeup() {}
    public function __toString() { return ''; }
    public function __invoke() {}
    /* public static function __set_state($props) {} */
    public function __clone() {}
    public function __debugInfo()  {}
}

// construct AND destruct
$o = new TestClass();
$o = new TestClass(); // DO NOT remove this line, it will invoke __destruct

// object
$o->callNormal();
$o->callParent();
$o->callOverride();
$o->callInnerMethod();

// static
TestClass::staticNormal();
TestClass::staticParent();
TestClass::staticOverride();
TestClass::staticInnerMethod();

// call, callStatic
$o->undefineMethod();
TestClass::undefineMethod();

// sleep, wakeup
$data = serialize($o);
$o = unserialize($data);

// toString
strlen($o);

// invode
$o();

// debugInfo, added in PHP 5.6.0
// var_dump($o);

// get, set, isset, unset
$o->attr = 'hello';
$o->attr;
isset($o->attr);
unset($o->attr);

// clone
clone($o);

// TODO trait
?>
--EXPECTF--
> {main}() called at [%s:2]
    > TestClass->__construct() called at [%s:37]
    < TestClass->__construct() = NULL called at [%s:37] wt: %f ct: %f
    > TestClass->__construct() called at [%s:38]
    < TestClass->__construct() = NULL called at [%s:38] wt: %f ct: %f
    > TestClass->__destruct() called at [%s:38]
    < TestClass->__destruct() = NULL called at [%s:38] wt: %f ct: %f
    > TestClass->callNormal() called at [%s:41]
    < TestClass->callNormal() = NULL called at [%s:41] wt: %f ct: %f
    > TestParent->callParent() called at [%s:42]
    < TestParent->callParent() = NULL called at [%s:42] wt: %f ct: %f
    > TestClass->callOverride() called at [%s:43]
    < TestClass->callOverride() = NULL called at [%s:43] wt: %f ct: %f
    > TestClass->callInnerMethod() called at [%s:44]
        > TestClass->callNormal() called at [%s:15]
        < TestClass->callNormal() = NULL called at [%s:15] wt: %f ct: %f
    < TestClass->callInnerMethod() = NULL called at [%s:44] wt: %f ct: %f
    > TestClass::staticNormal() called at [%s:47]
    < TestClass::staticNormal() = NULL called at [%s:47] wt: %f ct: %f
    > TestParent::staticParent() called at [%s:48]
    < TestParent::staticParent() = NULL called at [%s:48] wt: %f ct: %f
    > TestClass::staticOverride() called at [%s:49]
    < TestClass::staticOverride() = NULL called at [%s:49] wt: %f ct: %f
    > TestClass::staticInnerMethod() called at [%s:50]
        > TestClass::staticNormal() called at [%s:16]
        < TestClass::staticNormal() = NULL called at [%s:16] wt: %f ct: %f
    < TestClass::staticInnerMethod() = NULL called at [%s:50] wt: %f ct: %f
    > TestClass->undefineMethod() called at [%s:53]
        > TestClass->__call("undefineMethod", array(0)) called at [%s:53]
        < TestClass->__call("undefineMethod", array(0)) = NULL called at [%s:53] wt: %f ct: %f
    < TestClass->undefineMethod() = NULL called at [%s:53] wt: %f ct: %f
    > TestClass::undefineMethod() called at [%s:54]
        > TestClass::__callStatic("undefineMethod", array(0)) called at [%s:54]
        < TestClass::__callStatic("undefineMethod", array(0)) = NULL called at [%s:54] wt: %f ct: %f
    < TestClass::undefineMethod() = NULL called at [%s:54] wt: %f ct: %f
    > serialize(object(TestClass)#2) called at [%s:57]
        > TestClass->__sleep() called at [%s:57]
        < TestClass->__sleep() = array(0) called at [%s:57] wt: %f ct: %f
    < serialize(object(TestClass)#2) = "O:9:\"TestClass\":0:{}" called at [%s:57] wt: %f ct: %f
    > unserialize("O:9:\"TestClass\":0:{}") called at [%s:58]
        > TestClass->__wakeup() called at [%s:58]
        < TestClass->__wakeup() = NULL called at [%s:58] wt: %f ct: %f
    < unserialize("O:9:\"TestClass\":0:{}") = object(TestClass)#1 called at [%s:58] wt: %f ct: %f
    > TestClass->__destruct() called at [%s:58]
    < TestClass->__destruct() = NULL called at [%s:58] wt: %f ct: %f
    > strlen(object(TestClass)#1) called at [%s:61]
        > TestClass->__toString() called at [%s:61]
        < TestClass->__toString() = "" called at [%s:61] wt: %f ct: %f
    < strlen(object(TestClass)#1) = 0 called at [%s:61] wt: %f ct: %f
    > TestClass->__invoke() called at [%s:64]
    < TestClass->__invoke() = NULL called at [%s:64] wt: %f ct: %f
    > TestClass->__set("attr", "hello") called at [%s:70]
    < TestClass->__set("attr", "hello") = NULL called at [%s:70] wt: %f ct: %f
    > TestClass->__get("attr") called at [%s:71]
    < TestClass->__get("attr") = NULL called at [%s:71] wt: %f ct: %f
    > TestClass->__isset("attr") called at [%s:72]
    < TestClass->__isset("attr") = NULL called at [%s:72] wt: %f ct: %f
    > TestClass->__unset("attr") called at [%s:73]
    < TestClass->__unset("attr") = NULL called at [%s:73] wt: %f ct: %f
    > TestClass->__clone() called at [%s:76]
    < TestClass->__clone() = NULL called at [%s:76] wt: %f ct: %f
    > TestClass->__destruct() called at [%s:76]
    < TestClass->__destruct() = NULL called at [%s:76] wt: %f ct: %f
< {main}() = 1 called at [%s:2] wt: %f ct: %f
> TestClass->__destruct() called at [(null):0]
< TestClass->__destruct() = NULL called at [(null):0] wt: %f ct: %f
