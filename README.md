phptrace
========
php程序跟踪和分析工具，从PHP Runtime中获取程序的上下文及函数调用情况，适用于线上快速分析定位阻塞、负载高等疑难杂症

[phptrace介绍及使用](https://github.com/Qihoo360/phptrace/wiki)

## Building

编译cmdtool
```shell
tar -zxf phptrace-<version>.tar.gz
cd phptrace-<version>
cd cmdtool
make
```

编译PHP扩展
```shell
cd phpext
phpize
./configure --with-php-config=/path/to/php-config
make
```

## Installing
cmdtool可直接使用，扩展需要安装到PHP相关目录：
```shell
make install
```

## Usage
```shell
$ phptrace -p <PID>     #trace PHP函数调用
$ phptrace -p <PID> -s  #打印PHP调用栈
```

## Examples
打印调用栈
```shell
$ ./phptrace -p 3130 -s
phptrace 0.1 demo, published by infra webcore team
process id = 3130
script_filename = /home/xxx/opt/nginx/webapp/block.php
[0x7f27b9a99dc8]  sleep /home/xxx/opt/nginx/webapp/block.php:6
[0x7f27b9a99d08]  say /home/xxx/opt/nginx/webapp/block.php:3
[0x7f27b9a99c50]  run /home/xxx/opt/nginx/webapp/block.php:10
```

trace PHP函数调用
```shell
$ ./phptrace -p 2459
1417506346.727223 run(<Null>)
1417506346.727232     say($msg = "hello world")
1417506346.727241         sleep($seconds = "1")
1417506347.727341         sleep =>      0       1.000100 
1417506347.727354     say =>    hello world     1.000122 
1417506347.727358 run =>        nil     1.000135 
```
