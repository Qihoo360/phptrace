Version 1.0.0-beta (2016-12-26)
------------------------------

### Added

- Add support for Linux environment ptrace
- Add support for MacOS
- Add filter by url/class/function name
- Add limit output count

### Changed

- Refactoring extensions and tool-side information interaction
- Optimize format and color of the output
- Remove unused messages such as wall_time、memory records
- Improve display status module

Version 0.5.0-dev (2016-03-17)
------------------------------

- Support PHP 7

Version 0.4.0-dev (2016-01-20)
------------------------------

### Added

- Add support for tracing multiple PHP process

### Changed

- Refactor communication module base on unix socket
- Rewrite cli tool
    - Introduce sub-commands
    - Only basic trace keeped in dev-version


Version 0.3.0 (2015-05-22)
------------------------------

### Added

- Added support for special function calls: `main`, `include`, `require`, `Closure`, `Labmda`, `eval()`
- Added support for PHP 5.1
- Keep collecting and sending back trace information after PHP bailout
- Added duplexing to communication module
- Compatible with Trait Alias
- Added checking for ZTS (Thread-safety support) during configuration

### Changed

- Refactor the PHP extension
- Unified the naming convention of Type, Function, Macro
- Change License to Apache 2.0
- Change Extension's name to "trace" (php is redundant for a PHP extension)
- Improve representation of `zval` and support for Array, Object
- Improve performance when trace if off
- Limit the length of print arguments and retvalue

### Fixed

- Fixed handling of large or small double value that needs scientific notation
- Fixed memory leaks related with SDS
