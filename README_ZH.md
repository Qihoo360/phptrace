phptrace
========

## 介绍 [English](https://github.com/Qihoo360/phptrace/blob/master/README.md)

PhpTrace是一款跟踪和分析PHP脚本的工具，从PHP Runtime中获取程序的上下文及函数调用情况，适用于线上快速分析定位阻塞、负载高等疑难杂症。

[phptrace介绍及使用](https://github.com/Qihoo360/phptrace/wiki)

## 编译

* 解压源码包
```shell
tar -zxf phptrace-<version>.tar.gz
cd phptrace-<version>
```

* 编译PHP扩展
```shell
cd phpext
phpize
./configure --with-php-config=/path/to/php-config
make
```

* 编译命令行工具
```shell
cd cmdtool
make
```

## 安装
* 安装PHP扩展 (phptrace.so)到扩展目录
```shell
make install
```

* 编辑php.ini，如果需要的话重启PHP进程
```
extension=phptrace.so
phptrace.enabled=1
```

* 切换到cmdtool目录直接使用phptrace命令
```shell
cd cmdtool
./phptrace  [options]
```

## 举例

* 跟踪PHP函数的执行

```shell
$ ./phptrace -p 2459                # phptrace -p <PID>
1417506346.727223 run(<Null>)
1417506346.727232     say($msg = "hello world")
1417506346.727241         sleep($seconds = "1")
1417506347.727341         sleep =>      0       1.000100 
1417506347.727354     say =>    hello world     1.000122 
1417506347.727358 run =>        nil     1.000135
```

* 打印函数调用堆栈
```shell
$ ./phptrace -p 3130 -s             # phptrace -p <PID> -s
phptrace 0.1 demo, published by infra webcore team
process id = 3130
script_filename = /home/xxx/opt/nginx/webapp/block.php
[0x7f27b9a99dc8]  sleep /home/xxx/opt/nginx/webapp/block.php:6
[0x7f27b9a99d08]  say /home/xxx/opt/nginx/webapp/block.php:3
[0x7f27b9a99c50]  run /home/xxx/opt/nginx/webapp/block.php:10 
```

## 联系方式

* 邮箱: g-infra-webcore@list.qihoo.net
* QQ群: 428631461