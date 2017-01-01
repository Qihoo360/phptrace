phptrace
==============================

> Readme in [Chinese 中文](https://github.com/Qihoo360/phptrace/blob/master/README_ZH.md)

[![Build Status](https://travis-ci.org/Qihoo360/phptrace.svg)](https://travis-ci.org/Qihoo360/phptrace)

phptrace is a low-overhead tracing tool for PHP.

It can trace all PHP executing, function calls, request information during
run-time. And provides features like Filter, Statistics, Current Status and so
on.

It is very useful to locate blocking, heavy-load problems and debug in all
environments, especially in production environments.

Features:
* low-overhead, when extension loaded and trace is off
* stable, running on [Qihoo 360](http://www.360safe.com/) and tested on main-stream frameworks
* trace PHP run-time status, even PHP do not install the extension of phptrace

Download the latest version: https://pecl.php.net/package/trace

> News
> We have build another interesting project [pika](https://github.com/Qihoo360/pika).
> Pika is a nosql compatible with redis protocol with huge storage space. You can have a try.


Building
------------------------------

1. Extracting tarball
    ```
    tar -zxf phptrace-{version}.tar.gz
    cd phptrace-{version}
    ```

2. PHP extension - Build & Install
    ```
    cd extension
    {php_bin_dir}/phpize
    ./configure --with-php-config={php_bin_dir}/php-config
    make
    make install
    ```

3. PHP extension - Add extension load directive

    Edit `php.ini`, add the following line. A reload is needed if PHP running
    by php-fpm.

    ```
    extension=trace.so
    ```

4. Command tool - Build & Install
    ```
    make cli
    make install-cli
    ```

5. Verify
    ```
    php -r 'for ($i = 0; $i < 10; $i++) usleep(10000);' &
    phptrace -p $!
    ```

    You should see something below if everything fine

    ```
    1431681727.341829      usleep  =>  NULL   wt: 0.011979 ct: 0.011980
    1431681727.341847      usleep(10000) at [Command line code:1]
    1431681727.353831      usleep  =>  NULL   wt: 0.011984 ct: 0.011983
    1431681727.353849      usleep(10000) at [Command line code:1]
    1431681727.365829      usleep  =>  NULL   wt: 0.011980 ct: 0.011981
    1431681727.365848      usleep(10000) at [Command line code:1]
    ```

Command line options
-----------------------------

* trace     trace running php process(default)
* status    display php process status
* version   show version
* -p        specify php process id ('all' to trace all processes)
* -h        show helper
* -v        same as version
* -f        filter data by type(url,function,class) and content
* -l        limit output count
* --ptrace  in status mode fetch data using ptrace


Usage
------------------------------

* Trace executing

    ```
    $ ./phptrace -p 3600
    1431682433.534909      run() at [sample.php:15]
    1431682433.534944        say("hello world") at [sample.php:7]
    1431682433.534956          sleep(1) at [sample.php:11]
    1431682434.538847          sleep  =>  0   wt: 1.003891 ct: 0.000000
    1431682434.538899          printf("hello world") at [sample.php:12]
    1431682434.538953          printf  =>  11   wt: 0.000054 ct: 0.000000
    1431682434.538959        say  =>  NULL   wt: 1.004015 ct: 0.000000
    1431682434.538966      run  =>  NULL   wt: 1.004057 ct: 0.000000
    ```

* Print current status

    ```
    $ ./phptrace -s -p 3600
    Memory
    usage: 235320
    peak_usage: 244072
    real_usage: 262144
    real_peak_usage: 262144

    Request
    request_script: sample.php
    request_time: 1431682554.245320

    Stack
    #1    printf("hello world") at [sample.php:8]
    #2    say("hello world") at [sample.php:3]
    #3    run() at [sample.php:12]
    ```

* Filter frame by url/class/function

    ```
    $ ./phptrace -p 3600 -f type=class,content=simple
    [pid 3600]> simple() called at [/mnt/windows/php-trace/optimize/bench1.php:83]
    [pid 3600]< simple() = NULL called at [/mnt/windows/php-trace/optimize/bench1.php:83] ~ 0.226s 0.226s
    [pid 3600]> simplecall() called at [/mnt/windows/php-trace/optimize/bench1.php:85]
    [pid 3600]< simplecall() = NULL called at [/mnt/windows/php-trace/optimize/bench1.php:85] ~ 0.612s 0.612s
    [pid 3600]> simpleucall() called at [/mnt/windows/php-trace/optimize/bench1.php:87]
    [pid 3600]< simpleucall() = NULL called at [/mnt/windows/php-trace/optimize/bench1.php:87] ~ 0.610s 0.610s
    [pid 3600]> simpleudcall() called at [/mnt/windows/php-trace/optimize/bench1.php:89]
    [pid 3600]< simpleudcall() = NULL called at [/mnt/windows/php-trace/optimize/bench1.php:89] ~ 0.633s 0.633s
    ```
* Limit frame/url display count

    ```
    $ ./phptrace -p 3600 -l 3
    [pid 3600]> {main}() called at [/mnt/windows/php-trace/optimize/bench1.php:2]
    [pid 3600]    > function_exists("date_default_timezone_set") called at [/mnt/windows/php-trace/optimize/bench1.php:2]
    [pid 3600]    < function_exists("date_default_timezone_set") = true called at [/mnt/windows/php-trace/optimize/bench1.php:2] ~ 0.000s 0.000s
    [pid 3600]    > date_default_timezone_set("UTC") called at [/mnt/windows/php-trace/optimize/bench1.php:3]
    [pid 3600]    < date_default_timezone_set("UTC") = true called at [/mnt/windows/php-trace/optimize/bench1.php:3] ~ 0.000s 0.000s
    [pid 3600]    > start_test() called at [/mnt/windows/php-trace/optimize/bench1.php:82]
    [pid 3600]        > ob_start() called at [/mnt/windows/php-trace/optimize/bench1.php:54]
    [pid 3600]        < ob_start() = true called at [/mnt/windows/php-trace/optimize/bench1.php:54] ~ 0.000s 0.000s
    ```



Comparison
------------------------------

### phptrace
* It can print call stack of executing php process, which is similar to pstack.
* It can trace php function calls, which is similar to strace.
* It cannot get performance summary of php scripts, which will be supported in the future.
* It can not debug php scripts.

### Phpdbg
* It is used to debug php program, which is similar to gdb.
* It cannot print call stack of executing php process.
* It cannot trace php function calls.

### Xhprof
* It is used to get performance summary, which is similar to gprof.
* It cannot print call stack of executing php process.
* It cannot trace php function calls.

### Xdebug
* It can print call stack only if some error occurs.
* It would hook many opcode handlers even you did not set the auto_trace flag in php.ini; it traces all the processes at the same time just without output. This is a big overhea
d in production envirenment.
* It can not be enabled to trace without modifying the ini or the php script.


Contact
------------------------------

* Email: g-infra-webcore@list.qihoo.net


License
------------------------------

This project is released under the [Apache 2.0 License](https://raw.githubusercontent.com/Qihoo360/phptrace/master/LICENSE).
