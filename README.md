phptrace
========


[![Build Status](https://travis-ci.org/Qihoo360/phptrace.svg?branch=nightly)](https://travis-ci.org/Qihoo360/phptrace)

## Introduction [中文](https://github.com/Qihoo360/phptrace/blob/master/README_ZH.md)

Phptrace is a tracing and troubleshooting tool for PHP scripts. The information of php execute context and php function calls are fetched from PHP Runtime. It is very useful to locate blocking problems, heavy-load problems and tricky bugs both in the online environment and the development environment.

[Introduction and Usage](https://github.com/Qihoo360/phptrace/wiki)

## Building

* Extracting the tarball
```shell
tar -zxf phptrace-<version>.tar.gz
cd phptrace-<version>
```

* Compile the PHP extension
```shell
cd phpext
phpize
./configure --with-php-config=/path/to/php-config
make
```

* Compile the command tool
```shell
cd cmdtool
make
```

## Installing
* Install the php extension (phptrace.so) to the PHP extension directory.
```shell
make install
```

* Edit php.ini and restart the php process if needed.
```
extension=phptrace.so
phptrace.enabled=1
```

* Use the phptrace tool directly under the cmdtool directory.
```shell
cd cmdtool
./phptrace  [options]
```

## Examples

* Trace php function calls.

```shell
$ ./phptrace -p 2459                # phptrace -p <PID>
1417506346.727223 run(<Null>)
1417506346.727232     say($msg = "hello world")
1417506346.727241         sleep($seconds = "1")
1417506347.727341         sleep =>      0       1.000100 
1417506347.727354     say =>    hello world     1.000122 
1417506347.727358 run =>        nil     1.000135
```

* Print the stack of function call.
```shell
$ ./phptrace -p 3130 -s             # phptrace -p <PID> -s
phptrace 0.1 demo, published by infra webcore team
process id = 3130
script_filename = /home/xxx/opt/nginx/webapp/block.php
[0x7f27b9a99dc8]  sleep /home/xxx/opt/nginx/webapp/block.php:6
[0x7f27b9a99d08]  say /home/xxx/opt/nginx/webapp/block.php:3
[0x7f27b9a99c50]  run /home/xxx/opt/nginx/webapp/block.php:10 
```

## Comparison

### PhpTrace
* It can print call stack of executing php process, which is similar to pstack.
* It can trace php function callls, which is similar to strace.
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

## Contact

* Email: g-infra-webcore@list.qihoo.net
* QQ Group: 428631461
