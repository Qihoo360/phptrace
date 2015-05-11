--TEST--
Trace call trigger by autoload  < 5.5
--SKIPIF--
<?php
if (version_compare(PHP_VERSION, '5.5', '>=')) {
    echo 'skip this test is for version < 5.5';
}
?>
--FILE--
<?php phptrace_start();

function handler_for_autoload($name) { eval('class '.$name.' {}'); }
spl_autoload_register('handler_for_autoload');
new UndefinedClass;

phptrace_end(); ?>
--EXPECTF--
> spl_autoload_register("handler_for_autoload") called at [%s:4]
    < spl_autoload_register("handler_for_autoload") = true called at [%s:4] wt: %f ct: %f
    > handler_for_autoload("UndefinedClass") called at [(null):0]
        > {eval} called at [%s:3]
        < {eval} = NULL called at [%s:3] wt: %f ct: %f
    < handler_for_autoload("UndefinedClass") = NULL called at [(null):0] wt: %f ct: %f
    > phptrace_end() called at [%s:7]
