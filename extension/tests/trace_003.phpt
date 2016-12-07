--TEST--
Trace Class and Object
--INI--
trace.dotrace=1
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
    public function __get($name) {}
    public function __set($name, $value) {}
    public function __isset($name) {}
    public function __unset($name) {}
    public function __sleep() { return array(); }
    public function __wakeup() {}
    public function __toString() { return ''; }
    public function __clone() {}
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

// sleep, wakeup
$data = serialize($o);
$o = unserialize($data);

// toString
strlen($o);

// get, set, isset, unset
$o->attr = 'hello';
$o->attr;
isset($o->attr);
unset($o->attr);

// clone
clone($o);
?>
--EXPECTF--
> cli php %s
> {main}() called at [%s:2]
    > TestClass->__construct() called at [%s:33]
    < TestClass->__construct() = NULL called at [%s:33] ~ %fs %fs
    > TestClass->__construct() called at [%s:34]
    < TestClass->__construct() = NULL called at [%s:34] ~ %fs %fs
    > TestClass->__destruct() called at [%s:34]
    < TestClass->__destruct() = NULL called at [%s:34] ~ %fs %fs
    > TestClass->callNormal() called at [%s:37]
    < TestClass->callNormal() = NULL called at [%s:37] ~ %fs %fs
    > TestParent->callParent() called at [%s:38]
    < TestParent->callParent() = NULL called at [%s:38] ~ %fs %fs
    > TestClass->callOverride() called at [%s:39]
    < TestClass->callOverride() = NULL called at [%s:39] ~ %fs %fs
    > TestClass->callInnerMethod() called at [%s:40]
        > TestClass->callNormal() called at [%s:15]
        < TestClass->callNormal() = NULL called at [%s:15] ~ %fs %fs
    < TestClass->callInnerMethod() = NULL called at [%s:40] ~ %fs %fs
    > TestClass::staticNormal() called at [%s:43]
    < TestClass::staticNormal() = NULL called at [%s:43] ~ %fs %fs
    > TestParent::staticParent() called at [%s:44]
    < TestParent::staticParent() = NULL called at [%s:44] ~ %fs %fs
    > TestClass::staticOverride() called at [%s:45]
    < TestClass::staticOverride() = NULL called at [%s:45] ~ %fs %fs
    > TestClass::staticInnerMethod() called at [%s:46]
        > TestClass::staticNormal() called at [%s:16]
        < TestClass::staticNormal() = NULL called at [%s:16] ~ %fs %fs
    < TestClass::staticInnerMethod() = NULL called at [%s:46] ~ %fs %fs
    > TestClass->undefineMethod() called at [%s:49]
        > TestClass->__call("undefineMethod", array(0)) called at [%s:49]
        < TestClass->__call("undefineMethod", array(0)) = NULL called at [%s:49] ~ %fs %fs
    < TestClass->undefineMethod() = NULL called at [%s:49] ~ %fs %fs
    > serialize(object(TestClass)#2) called at [%s:52]
        > TestClass->__sleep() called at [%s:52]
        < TestClass->__sleep() = array(0) called at [%s:52] ~ %fs %fs
    < serialize(object(TestClass)#2) = "O:9:\"TestClass\":0:{}" called at [%s:52] ~ %fs %fs
    > unserialize("O:9:\"TestClass\":0:{}") called at [%s:53]
        > TestClass->__wakeup() called at [%s:53]
        < TestClass->__wakeup() = NULL called at [%s:53] ~ %fs %fs
    < unserialize("O:9:\"TestClass\":0:{}") = object(TestClass)#1 called at [%s:53] ~ %fs %fs
    > TestClass->__destruct() called at [%s:53]
    < TestClass->__destruct() = NULL called at [%s:53] ~ %fs %fs
    > strlen(object(TestClass)#1) called at [%s:56]
        > TestClass->__toString() called at [%s:56]
        < TestClass->__toString() = "" called at [%s:56] ~ %fs %fs
    < strlen(object(TestClass)#1) = 0 called at [%s:56] ~ %fs %fs
    > TestClass->__set("attr", "hello") called at [%s:59]
    < TestClass->__set("attr", "hello") = NULL called at [%s:59] ~ %fs %fs
    > TestClass->__get("attr") called at [%s:60]
    < TestClass->__get("attr") = NULL called at [%s:60] ~ %fs %fs
    > TestClass->__isset("attr") called at [%s:61]
    < TestClass->__isset("attr") = NULL called at [%s:61] ~ %fs %fs
    > TestClass->__unset("attr") called at [%s:62]
    < TestClass->__unset("attr") = NULL called at [%s:62] ~ %fs %fs
    > TestClass->__clone() called at [%s:65]
    < TestClass->__clone() = NULL called at [%s:65] ~ %fs %fs
    > TestClass->__destruct() called at [%s:65]
    < TestClass->__destruct() = NULL called at [%s:65] ~ %fs %fs
< {main}() = 1 called at [%s:2] ~ %fs %fs
> TestClass->__destruct() called at [(null):0]
< TestClass->__destruct() = NULL called at [(null):0] ~ %fs %fs
< cli php %s
