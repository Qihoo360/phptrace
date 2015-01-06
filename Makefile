PHPDIR=/usr/local/php
default:cmdtool
all: cmdtool ext

cmdtool:cmdtool/php-trace 

cmdtool/php-trace:cmdtool/*.h cmdtool/*.c
	cd cmdtool && make

ext:phpext/phptrace.c
	cd phpext && $(PHPDIR)/bin/phpize
	cd phpext && ./configure --with-php-config=$(PHPDIR)/bin/php-config
	cd phpext && make
	touch ext

install:
	cd cmdtool && make install
	cd phpext  && make install

clean:
	cd cmdtool && make clean
	cd phpext && make clean
	rm -rf util/*.o util/*.lo util/.libs
	rm -rf ext
