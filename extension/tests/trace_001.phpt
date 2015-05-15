--TEST--
Trace simple call with various types of argument
--INI--
trace.dotrace=1
--FILE--
<?php
class DummyClass {}
function call_normal($arg = null) {}
define('DUMMYCONST', 'this is a const string');

// Call normal
call_normal(null);
call_normal(1234);
call_normal(1.234);
call_normal(true);
call_normal(false);
call_normal(array(1, 'two' => 2));
call_normal('hello');
call_normal(str_repeat('hello', 1024)); /* long string */
call_normal(tmpfile());
call_normal(DUMMYCONST);
call_normal(new DummyClass());
@call_normal($undefine_variable);
call_normal(null, 1234, 1.234, true, false, array(), 'hello', tmpfile(), DUMMYCONST, new DummyClass());
?>
--EXPECTF--
> {main}() called at [%s:2]
    > define("DUMMYCONST", "this is a const string") called at [%s:4]
    < define("DUMMYCONST", "this is a const string") = true called at [%s:4] wt: %f ct: %f
    > call_normal(NULL) called at [%s:7]
    < call_normal(NULL) = NULL called at [%s:7] wt: %f ct: %f
    > call_normal(1234) called at [%s:8]
    < call_normal(1234) = NULL called at [%s:8] wt: %f ct: %f
    > call_normal(1.234) called at [%s:9]
    < call_normal(1.234) = NULL called at [%s:9] wt: %f ct: %f
    > call_normal(true) called at [%s:10]
    < call_normal(true) = NULL called at [%s:10] wt: %f ct: %f
    > call_normal(false) called at [%s:11]
    < call_normal(false) = NULL called at [%s:11] wt: %f ct: %f
    > call_normal(array(2)) called at [%s:12]
    < call_normal(array(2)) = NULL called at [%s:12] wt: %f ct: %f
    > call_normal("hello") called at [%s:13]
    < call_normal("hello") = NULL called at [%s:13] wt: %f ct: %f
    > str_repeat("hello", 1024) called at [%s:14]
    < str_repeat("hello", 1024) = "hellohellohellohellohellohellohe"... called at [%s:14] wt: %f ct: %f
    > call_normal("hellohellohellohellohellohellohe"...) called at [%s:14]
    < call_normal("hellohellohellohellohellohellohe"...) = NULL called at [%s:14] wt: %f ct: %f
    > tmpfile() called at [%s:15]
    < tmpfile() = resource(stream)#%d called at [%s:15] wt: %f ct: %f
    > call_normal(resource(stream)#%d) called at [%s:15]
    < call_normal(resource(stream)#%d) = NULL called at [%s:15] wt: %f ct: %f
    > call_normal("this is a const string") called at [%s:16]
    < call_normal("this is a const string") = NULL called at [%s:16] wt: %f ct: %f
    > call_normal(object(DummyClass)#1) called at [%s:17]
    < call_normal(object(DummyClass)#1) = NULL called at [%s:17] wt: %f ct: %f
    > call_normal(NULL) called at [%s:18]
    < call_normal(NULL) = NULL called at [%s:18] wt: %f ct: %f
    > tmpfile() called at [%s:19]
    < tmpfile() = resource(stream)#%d called at [%s:19] wt: %f ct: %f
    > call_normal(NULL, 1234, 1.234, true, false, array(0), "hello", resource(stream)#%d, "this is a const string", object(DummyClass)#1) called at [%s:19]
    < call_normal(NULL, 1234, 1.234, true, false, array(0), "hello", resource(stream)#%d, "this is a const string", object(DummyClass)#1) = NULL called at [%s:19] wt: %f ct: %f
< {main}() = 1 called at [%s:2] wt: %f ct: %f
