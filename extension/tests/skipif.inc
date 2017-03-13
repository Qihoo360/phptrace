<?php
function skip($text)
{
    exit('skip '.$text);
}

function require_debug_mode()
{
    if (!function_exists('trace_start') ||
            !function_exists('trace_end') ||
            !function_exists('trace_set_filter')) {
        skip('Debug mode is required');
    }
}

function require_function(/* ... */)
{
    $funcs = func_get_args();
    foreach ($funcs as $func) {
        if (!function_exists($func)) {
            skip('Function '.$func.'() not found');
        }
    }
}

function for_verion_gte($version)
{
    if (version_compare(PHP_VERSION, $version, '<')) {
        skip('Required version >= '.$version);
    }
}

function for_verion_lt($version)
{
    if (version_compare(PHP_VERSION, $version, '>=')) {
        skip('Required version < '.$version);
    }
}
