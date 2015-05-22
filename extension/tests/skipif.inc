<?php
function trace_skipif_no_trace_start()
{
    if (!function_exists('trace_start') || !function_exists('trace_end')) {
        exit("skip trace_start() not exists");
    }
}
