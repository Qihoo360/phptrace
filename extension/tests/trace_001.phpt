--TEST--
Trace simple call with various types of argument
--INI--
phptrace.dotrace=on
--FILE--
<?php
define('DUMMYCONST', 'this is a const string');
class DummyClass {}
function call_normal($arg = null) {}

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
call_normal(function () {});
@call_normal($undefine_variable);

// Call recursive
function call_recursive($r = 0) {
    if ($r > 0) {
        call_recursive($r - 1);
    }
}

call_recursive(36);
?>
--EXPECTF--
> {main}() called at [%s:2]
    > define("DUMMYCONST", "this is a const string") called at [%s:2]
    < define("DUMMYCONST", "this is a const string") = true called at [%s:2] wt: %f ct: %f
    > call_normal(NULL) called at [%s:7]
    < call_normal(NULL) = NULL called at [%s:7] wt: %f ct: %f
    > call_normal(1234) called at [%s:8]
    < call_normal(1234) = NULL called at [%s:8] wt: %f ct: %f
    > call_normal(1.234000) called at [%s:9]
    < call_normal(1.234000) = NULL called at [%s:9] wt: %f ct: %f
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
    < tmpfile() = resource(stream)#4 called at [%s:15] wt: %f ct: %f
    > call_normal(resource(stream)#4) called at [%s:15]
    < call_normal(resource(stream)#4) = NULL called at [%s:15] wt: %f ct: %f
    > call_normal("this is a const string") called at [%s:16]
    < call_normal("this is a const string") = NULL called at [%s:16] wt: %f ct: %f
    > call_normal(object(DummyClass)#1) called at [%s:17]
    < call_normal(object(DummyClass)#1) = NULL called at [%s:17] wt: %f ct: %f
    > call_normal(object(Closure)#1) called at [%s:18]
    < call_normal(object(Closure)#1) = NULL called at [%s:18] wt: %f ct: %f
    > call_normal(NULL) called at [%s:19]
    < call_normal(NULL) = NULL called at [%s:19] wt: %f ct: %f
    > call_recursive(36) called at [%s:28]
        > call_recursive(35) called at [%s:24]
            > call_recursive(34) called at [%s:24]
                > call_recursive(33) called at [%s:24]
                    > call_recursive(32) called at [%s:24]
                        > call_recursive(31) called at [%s:24]
                            > call_recursive(30) called at [%s:24]
                                > call_recursive(29) called at [%s:24]
                                    > call_recursive(28) called at [%s:24]
                                        > call_recursive(27) called at [%s:24]
                                            > call_recursive(26) called at [%s:24]
                                                > call_recursive(25) called at [%s:24]
                                                    > call_recursive(24) called at [%s:24]
                                                        > call_recursive(23) called at [%s:24]
                                                            > call_recursive(22) called at [%s:24]
                                                                > call_recursive(21) called at [%s:24]
                                                                    > call_recursive(20) called at [%s:24]
                                                                        > call_recursive(19) called at [%s:24]
                                                                            > call_recursive(18) called at [%s:24]
                                                                                > call_recursive(17) called at [%s:24]
                                                                                    > call_recursive(16) called at [%s:24]
                                                                                        > call_recursive(15) called at [%s:24]
                                                                                            > call_recursive(14) called at [%s:24]
                                                                                                > call_recursive(13) called at [%s:24]
                                                                                                    > call_recursive(12) called at [%s:24]
                                                                                                        > call_recursive(11) called at [%s:24]
                                                                                                            > call_recursive(10) called at [%s:24]
                                                                                                                > call_recursive(9) called at [%s:24]
                                                                                                                    > call_recursive(8) called at [%s:24]
                                                                                                                        > call_recursive(7) called at [%s:24]
                                                                                                                            > call_recursive(6) called at [%s:24]
                                                                                                                                > call_recursive(5) called at [%s:24]
                                                                                                                                    > call_recursive(4) called at [%s:24]
                                                                                                                                        > call_recursive(3) called at [%s:24]
                                                                                                                                            > call_recursive(2) called at [%s:24]
                                                                                                                                                > call_recursive(1) called at [%s:24]
                                                                                                                                                    > call_recursive(0) called at [%s:24]
                                                                                                                                                    < call_recursive(0) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                                                < call_recursive(1) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                                            < call_recursive(2) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                                        < call_recursive(3) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                                    < call_recursive(4) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                                < call_recursive(5) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                            < call_recursive(6) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                        < call_recursive(7) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                    < call_recursive(8) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                                < call_recursive(9) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                            < call_recursive(10) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                        < call_recursive(11) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                    < call_recursive(12) = NULL called at [%s:24] wt: %f ct: %f
                                                                                                < call_recursive(13) = NULL called at [%s:24] wt: %f ct: %f
                                                                                            < call_recursive(14) = NULL called at [%s:24] wt: %f ct: %f
                                                                                        < call_recursive(15) = NULL called at [%s:24] wt: %f ct: %f
                                                                                    < call_recursive(16) = NULL called at [%s:24] wt: %f ct: %f
                                                                                < call_recursive(17) = NULL called at [%s:24] wt: %f ct: %f
                                                                            < call_recursive(18) = NULL called at [%s:24] wt: %f ct: %f
                                                                        < call_recursive(19) = NULL called at [%s:24] wt: %f ct: %f
                                                                    < call_recursive(20) = NULL called at [%s:24] wt: %f ct: %f
                                                                < call_recursive(21) = NULL called at [%s:24] wt: %f ct: %f
                                                            < call_recursive(22) = NULL called at [%s:24] wt: %f ct: %f
                                                        < call_recursive(23) = NULL called at [%s:24] wt: %f ct: %f
                                                    < call_recursive(24) = NULL called at [%s:24] wt: %f ct: %f
                                                < call_recursive(25) = NULL called at [%s:24] wt: %f ct: %f
                                            < call_recursive(26) = NULL called at [%s:24] wt: %f ct: %f
                                        < call_recursive(27) = NULL called at [%s:24] wt: %f ct: %f
                                    < call_recursive(28) = NULL called at [%s:24] wt: %f ct: %f
                                < call_recursive(29) = NULL called at [%s:24] wt: %f ct: %f
                            < call_recursive(30) = NULL called at [%s:24] wt: %f ct: %f
                        < call_recursive(31) = NULL called at [%s:24] wt: %f ct: %f
                    < call_recursive(32) = NULL called at [%s:24] wt: %f ct: %f
                < call_recursive(33) = NULL called at [%s:24] wt: %f ct: %f
            < call_recursive(34) = NULL called at [%s:24] wt: %f ct: %f
        < call_recursive(35) = NULL called at [%s:24] wt: %f ct: %f
    < call_recursive(36) = NULL called at [%s:28] wt: %f ct: %f
< {main}() = 1 called at [%s:2] wt: %f ct: %f
