# phptrace

[![Build Status](https://travis-ci.org/Qihoo360/phptrace.svg)](https://travis-ci.org/Qihoo360/phptrace)

phptrace是一个低开销的用于跟踪、分析PHP运行情况的工具。

它可以跟踪PHP在运行时的函数调用、请求信息、执行流程，并且提供有过滤器、统计信息、当前状态等实用功能。
在任何环境下，它都能很好的定位阻塞问题以及在高负载下Debug，尤其是线上生产环境。

具有以下特性：
- 低开销，在只加载模块不开启Trace功能时对性能影响极低
- 稳定性，已经稳定运行在[Qihoo 360](http://360.cn)线上服务中，并针对主流框架进行测试
- 易用性，对于未安装trace扩展的环境，也能够追踪运行状态

更多介绍：
- [PECL下载](https://pecl.php.net/package/trace)
- [Wiki](https://github.com/Qihoo360/phptrace/wiki)

> 号外，我们团队开发了另外一个有意思的项目 [pika](https://github.com/Qihoo360/pika)，
> 它是一个兼容Redis协议的大容量的存储，用来解决Redis内存不够的问题，欢迎大家试试。


## 从源码安装

1. 解压缩源码包
    ```
    tar -xf phptrace-{version}.tar.gz
    cd phptrace-{version}/extension
    ```

2. 编译

    PHP扩展
    ```
    {php_bin_dir}/phpize
    ./configure --with-php-config={php_bin_dir}/php-config
    make
    ```

    命令行工具
    ```
    make cli
    ```

3. 安装配置

    安装PHP扩展、命令行工具至PHP目录
    ```
    make install-all
    ```

    编辑配置文件`php.ini`，增加下面配置信息。*php-fpm需要手动重启。*
    ```
    extension=trace.so
    ```

4. 验证安装情况
    ```
    php -r 'for ($i = 0; $i < 20; $i++) usleep(50000);' &
    phptrace -p $!
    ```

    如果一切正常，应该可以看到类似下面的输出
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


## 使用

### 命令行选项

* trace     追踪运行的PHP进程(默认)
* status    展示PHP进程的运行状态
* version   版本
* -p        指定php进程id('all'追踪所有的进程)
* -h        帮助
* -v        同version
* -f        通过类型(url,function,class)和内容过滤数据
* -l        限制输出次数
* --ptrace  在追踪状态的模式下通过ptrace获取数据

### 跟踪执行状态

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

### 打印当前状态

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

### 根据URL/类名/函数名进行过滤

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

### 限制帧/URL的输出次数

```
$ phptrace -p 3600 -l 2

[pid 3600]    > Me->run() called at [example.php:57]
[pid 3600]        > Me->say("good night") called at [example.php:33]
[pid 3600]        < Me->say("good night") = NULL called at [example.php:33] ~ 0.000s 0.000s
[pid 3600]        > Me->sleep() called at [example.php:34]
[pid 3600]            > Me->say("sleeping...") called at [example.php:27]
[pid 3600]            < Me->say("sleeping...") = NULL called at [example.php:27] ~ 0.000s 0.000s
```


## 贡献

非常欢迎感兴趣，愿意参与其中，共同打造更好PHP生态的开发者。

如果你乐于此，却又不知如何开始，可以试试下面这些事情：

- 在你的系统中使用，将遇到的[问题反馈](https://github.com/monque/phptrace/issues)。
- 这里有[我们的计划](https://github.com/monque/phptrace/projects)，挑一个功能尝试开发。
- 有更好的建议？欢迎联系phobosw@gmail.com。


## 许可

本项目遵循[Apache 2.0 许可](https://raw.githubusercontent.com/Qihoo360/phptrace/master/LICENSE)进行发布
