# phptrace

[![Join the chat at https://gitter.im/Qihoo360/phptrace](https://badges.gitter.im/Qihoo360/phptrace.svg)](https://gitter.im/Qihoo360/phptrace?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

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
* stable, running on [Qihoo 360](http://www.360safe.com/) and tested on mainstream frameworks
* ease of use, view PHP run-time status without extension installation

Misc:
- [PECL Download](https://pecl.php.net/package/trace)

> News
> We have build another interesting project [pika](https://github.com/Qihoo360/pika).
> It's a NoSQL compatible with Redis protocol with huge storage space.


## Install from source

1. Extracting tarball
    ```
    tar -xf phptrace-{version}.tar.gz
    cd phptrace-{version}/extension
    ```

2. Build

    PHP Extension
    ```
    {php_bin_dir}/phpize
    ./configure --with-php-config={php_bin_dir}/php-config
    make
    ```

    CLI Binary
    ```
    make cli
    ```

3. Install & Configure

    Install PHP Extension, CLI Binary into PHP path
    ```
    make install-all
    ```

    Edit `php.ini`, add the following line. A reload is needed if PHP running
    on php-fpm mode.
    ```
    extension=trace.so
    ```

4. Verify
    ```
    php -r 'for ($i = 0; $i < 20; $i++) usleep(50000);' &
    phptrace -p $!
    ```

    You should see something below if it works fine
    ```
    process attached
    [pid 3600]> cli php -
    [pid 3600]> {main}() called at [Command line code:1]
    [pid 3600]    > usleep(50000) called at [Command line code:1]
    [pid 3600]    < usleep(50000) = NULL called at [Command line code:1] ~ 0.051s 0.051s
    [pid 3600]    > usleep(50000) called at [Command line code:1]
    [pid 3600]    < usleep(50000) = NULL called at [Command line code:1] ~ 0.051s 0.051s
    [pid 3600]    > usleep(50000) called at [Command line code:1]
    [pid 3600]    < usleep(50000) = NULL called at [Command line code:1] ~ 0.051s 0.051s
    [pid 3600]    > usleep(50000) called at [Command line code:1]
    ...
    ```


## Usage

Just try `php example.php`.

### Command line options

* trace     trace running php process(default)
* status    display php process status
* version   show version
* -p        specify php process id ('all' to trace all processes)
* -h        show helper
* -v        same as version
* -f        filter data by type(url,function,class) and content
* -l        limit output count
* --ptrace  in status mode fetch data using ptrace

### Trace executing

```
$ phptrace -p 3600

[pid 3600]    > Me->run() called at [example.php:57]
[pid 3600]        > Me->say("good night") called at [example.php:33]
[pid 3600]        < Me->say("good night") = NULL called at [example.php:33] ~ 0.000s 0.000s
[pid 3600]        > Me->sleep() called at [example.php:34]
[pid 3600]            > Me->say("sleeping...") called at [example.php:27]
[pid 3600]            < Me->say("sleeping...") = NULL called at [example.php:27] ~ 0.000s 0.000s
[pid 3600]            > sleep(2) called at [example.php:28]
[pid 3600]            < sleep(2) = 0 called at [example.php:28] ~ 2.000s 2.000s
[pid 3600]        < Me->sleep() = NULL called at [example.php:34] ~ 2.000s 0.000s
[pid 3600]        > Me->say("wake up") called at [example.php:35]
[pid 3600]        < Me->say("wake up") = NULL called at [example.php:35] ~ 0.000s 0.000s
[pid 3600]    < Me->run() = NULL called at [example.php:57] ~ 2.000s 0.000s
```

### Print current status

```
$ phptrace status -p 3600

------------------------------- Status --------------------------------
PHP Version:       7.0.16
SAPI:              cli
script:            example.php
elapse:            26.958s
------------------------------ Arguments ------------------------------
$0
------------------------------ Backtrace ------------------------------
#0  fgets() called at [example.php:53]
#1  {main}() called at [example.php:53]
```

### Tracing with filter of url/class/function

```
$ phptrace -p 3600 -f type=class,content=Me

[pid 3600]> Me->run() called at [example.php:57]
[pid 3600]> Me->say("good night") called at [example.php:33]
[pid 3600]< Me->say("good night") = NULL called at [example.php:33] ~ 0.000s 0.000s
[pid 3600]> Me->sleep() called at [example.php:34]
[pid 3600]> Me->say("sleeping...") called at [example.php:27]
[pid 3600]< Me->say("sleeping...") = NULL called at [example.php:27] ~ 0.000s 0.000s
[pid 3600]< Me->sleep() = NULL called at [example.php:34] ~ 2.000s 2.000s
[pid 3600]> Me->say("wake up") called at [example.php:35]
[pid 3600]< Me->say("wake up") = NULL called at [example.php:35] ~ 0.000s 0.000s
[pid 3600]< Me->run() = NULL called at [example.php:57] ~ 2.001s 0.000s
```

### Limit frame/URL display times

```
$ phptrace -p 3600 -l 2

[pid 3600]    > Me->run() called at [example.php:57]
[pid 3600]        > Me->say("good night") called at [example.php:33]
[pid 3600]        < Me->say("good night") = NULL called at [example.php:33] ~ 0.000s 0.000s
[pid 3600]        > Me->sleep() called at [example.php:34]
[pid 3600]            > Me->say("sleeping...") called at [example.php:27]
[pid 3600]            < Me->say("sleeping...") = NULL called at [example.php:27] ~ 0.000s 0.000s
```


## Contributing

Welcome developers who willing to make PHP environment better.

If you are interested but have no idea about how to starting, please try these below:

- Use it on your system, [feedback](https://github.com/monque/phptrace/issues) problem, feature request.
- Here is [roadmap](https://github.com/monque/phptrace/projects), try to develop one.
- Any other? Contact phobosw@gmail.com.


## License

This project is released under the [Apache 2.0 License](https://raw.githubusercontent.com/Qihoo360/phptrace/master/LICENSE).
