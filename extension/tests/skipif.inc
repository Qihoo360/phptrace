<?php
function trace_skipif_no_trace_start()
{
    if (!function_exists('trace_start') || !function_exists('trace_end')) {
        exit("skip trace_start() not exists");
    }
}

function trace_skipif_no_trace_filter()
{
    if (!function_exists('trace_set_filter')) {
        exit("skip trace_set_filter() not exists");
    }
}
