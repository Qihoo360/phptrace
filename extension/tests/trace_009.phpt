--TEST--
Trace call recursive
--INI--
trace.dotrace=1
--FILE--
<?php
function call_recursive($r = 0) {
    if ($r > 0) {
        call_recursive($r - 1);
    }
}

call_recursive(36);
?>
--EXPECTF--
> {main}() called at [%s:2]
    > call_recursive(36) called at [%s:8]
        > call_recursive(35) called at [%s:4]
            > call_recursive(34) called at [%s:4]
                > call_recursive(33) called at [%s:4]
                    > call_recursive(32) called at [%s:4]
                        > call_recursive(31) called at [%s:4]
                            > call_recursive(30) called at [%s:4]
                                > call_recursive(29) called at [%s:4]
                                    > call_recursive(28) called at [%s:4]
                                        > call_recursive(27) called at [%s:4]
                                            > call_recursive(26) called at [%s:4]
                                                > call_recursive(25) called at [%s:4]
                                                    > call_recursive(24) called at [%s:4]
                                                        > call_recursive(23) called at [%s:4]
                                                            > call_recursive(22) called at [%s:4]
                                                                > call_recursive(21) called at [%s:4]
                                                                    > call_recursive(20) called at [%s:4]
                                                                        > call_recursive(19) called at [%s:4]
                                                                            > call_recursive(18) called at [%s:4]
                                                                                > call_recursive(17) called at [%s:4]
                                                                                    > call_recursive(16) called at [%s:4]
                                                                                        > call_recursive(15) called at [%s:4]
                                                                                            > call_recursive(14) called at [%s:4]
                                                                                                > call_recursive(13) called at [%s:4]
                                                                                                    > call_recursive(12) called at [%s:4]
                                                                                                        > call_recursive(11) called at [%s:4]
                                                                                                            > call_recursive(10) called at [%s:4]
                                                                                                                > call_recursive(9) called at [%s:4]
                                                                                                                    > call_recursive(8) called at [%s:4]
                                                                                                                        > call_recursive(7) called at [%s:4]
                                                                                                                            > call_recursive(6) called at [%s:4]
                                                                                                                                > call_recursive(5) called at [%s:4]
                                                                                                                                    > call_recursive(4) called at [%s:4]
                                                                                                                                        > call_recursive(3) called at [%s:4]
                                                                                                                                            > call_recursive(2) called at [%s:4]
                                                                                                                                                > call_recursive(1) called at [%s:4]
                                                                                                                                                    > call_recursive(0) called at [%s:4]
                                                                                                                                                    < call_recursive(0) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                                                < call_recursive(1) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                                            < call_recursive(2) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                                        < call_recursive(3) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                                    < call_recursive(4) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                                < call_recursive(5) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                            < call_recursive(6) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                        < call_recursive(7) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                    < call_recursive(8) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                                < call_recursive(9) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                            < call_recursive(10) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                        < call_recursive(11) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                    < call_recursive(12) = NULL called at [%s:4] wt: %f ct: %f
                                                                                                < call_recursive(13) = NULL called at [%s:4] wt: %f ct: %f
                                                                                            < call_recursive(14) = NULL called at [%s:4] wt: %f ct: %f
                                                                                        < call_recursive(15) = NULL called at [%s:4] wt: %f ct: %f
                                                                                    < call_recursive(16) = NULL called at [%s:4] wt: %f ct: %f
                                                                                < call_recursive(17) = NULL called at [%s:4] wt: %f ct: %f
                                                                            < call_recursive(18) = NULL called at [%s:4] wt: %f ct: %f
                                                                        < call_recursive(19) = NULL called at [%s:4] wt: %f ct: %f
                                                                    < call_recursive(20) = NULL called at [%s:4] wt: %f ct: %f
                                                                < call_recursive(21) = NULL called at [%s:4] wt: %f ct: %f
                                                            < call_recursive(22) = NULL called at [%s:4] wt: %f ct: %f
                                                        < call_recursive(23) = NULL called at [%s:4] wt: %f ct: %f
                                                    < call_recursive(24) = NULL called at [%s:4] wt: %f ct: %f
                                                < call_recursive(25) = NULL called at [%s:4] wt: %f ct: %f
                                            < call_recursive(26) = NULL called at [%s:4] wt: %f ct: %f
                                        < call_recursive(27) = NULL called at [%s:4] wt: %f ct: %f
                                    < call_recursive(28) = NULL called at [%s:4] wt: %f ct: %f
                                < call_recursive(29) = NULL called at [%s:4] wt: %f ct: %f
                            < call_recursive(30) = NULL called at [%s:4] wt: %f ct: %f
                        < call_recursive(31) = NULL called at [%s:4] wt: %f ct: %f
                    < call_recursive(32) = NULL called at [%s:4] wt: %f ct: %f
                < call_recursive(33) = NULL called at [%s:4] wt: %f ct: %f
            < call_recursive(34) = NULL called at [%s:4] wt: %f ct: %f
        < call_recursive(35) = NULL called at [%s:4] wt: %f ct: %f
    < call_recursive(36) = NULL called at [%s:8] wt: %f ct: %f
< {main}() = 1 called at [%s:2] wt: %f ct: %f
