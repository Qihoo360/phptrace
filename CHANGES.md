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
