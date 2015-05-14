default:
	@echo "You should make PHP Trace [cmdtool] and [extension] separately"
	@echo "[cmdtool]   run: \"cd cmdtool; make\""
	@echo "[extension] run: \"cd extension; phpize; ./configure; make\""

clean:
	cd cmdtool && make clean
	cd extension && make clean
	find . -name \*.gcno -o -name \*.gcda | xargs rm -f
	find . -name \*.lo -o -name \*.o | xargs rm -f
	find . -name \*.la -o -name \*.a | xargs rm -f
	find . -name \*.so | xargs rm -f
	find . -name .libs -a -type d|xargs rm -rf
