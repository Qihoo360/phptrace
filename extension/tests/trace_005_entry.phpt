--TEST--
Trace signal handler
--SKIPIF--
<?php
if (!function_exists('pcntl_signal')) {
    echo "skip this test is for pcntl_signal() only";
}
?>
--REDIRECTTEST--
return array(
    'ENV' => array(),
    'TESTS' => 'tests_redirect/trace_005',
);
