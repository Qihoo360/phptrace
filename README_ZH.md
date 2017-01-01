phptrace
==============================

[![Build Status](https://travis-ci.org/Qihoo360/phptrace.svg)](https://travis-ci.org/Qihoo360/phptrace)

phptrace是一个低开销的用于跟踪、分析PHP运行情况的工具。

它可以跟踪PHP在运行时的函数调用、请求信息、执行流程，并且提供有过滤器、统计信息
、当前状态等实用功能。

在任何环境下，它都能很好的定位阻塞问题以及在高负载下Debug，尤其是线上产品环境。

它具有以下特性：
* 低开销，在只加载模块不开启Trace功能时对性能影响极低
* 稳定性，已经稳定运行在[Qihoo 360](http://360.cn)线上服务中，并针对主流框架进行测试
* PHP没有安装phptrace扩展，phptrace也能够追踪PHP运行状态

[介绍及使用Wiki](https://github.com/Qihoo360/phptrace/wiki)

最新版本下载： https://pecl.php.net/package/trace

> 号外
> 我们团队开发了另外一个有意思的项目 [pika](https://github.com/Qihoo360/pika),
> pika 是一个兼容redis 协议的大容量的存储, 用来解决redis 内存不够的问题, 欢迎大家试试


编译安装
------------------------------

1. 解压缩源码包
    ```
    tar -zxf phptrace-{version}.tar.gz
    cd phptrace-{version}
    ```

2. PHP扩展 - 编译安装
    ```
    cd extension
    {php_bin_dir}/phpize
    ./configure --with-php-config={php_bin_dir}/php-config
    # 编译
    make
    # 安装trace.so到扩展目录
    make install
    ```

3. PHP扩展 - 配置

    编辑配置文件`php.ini`，增加下面配置信息。如果需要的话重启PHP进程。

    ```
    extension=trace.so
    ```

4. 命令行工具编译安装
    ```
    make cli
    make install-cli
    ```

5. 验证安装情况
    ```
    php -r 'for ($i = 0; $i < 10; $i++) usleep(10000);' &
    phptrace -p $!
    ```

    如果一切正常，应该可以看到类似下面的输出

    ```
    1431681727.341829      usleep  =>  NULL   wt: 0.011979 ct: 0.011980
    1431681727.341847      usleep(10000) at [Command line code:1]
    1431681727.353831      usleep  =>  NULL   wt: 0.011984 ct: 0.011983
    1431681727.353849      usleep(10000) at [Command line code:1]
    1431681727.365829      usleep  =>  NULL   wt: 0.011980 ct: 0.011981
    1431681727.365848      usleep(10000) at [Command line code:1]
    ```

命令行选项
-----------------------------

* trace     追踪运行的PHP进程(默认) 
* status    展示PHP进程的运行状态
* version   版本 
* -p        指定php进程id('all'追踪所有的进程)
* -h        帮助
* -v        同version
* -f        通过类型(url,function,class)和内容过滤数据
* -l        限制输出次数
* --ptrace  在追踪状态的模式下通过ptrace获取数据


使用
------------------------------

* 跟踪执行状态

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

* 打印当前状态

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

* 根据url/类名/函数名过滤

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

* 限制帧/url的输出次数

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


联系方式
------------------------------

* 邮箱: g-infra-webcore@list.qihoo.net
* QQ群: 428631461


许可
------------------------------

本项目遵循[Apache 2.0 许可](https://raw.githubusercontent.com/Qihoo360/phptrace/master/LICENSE)进行发布
