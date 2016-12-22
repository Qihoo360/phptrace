# phptrace-CLI:begin

ptcli_objects = ../src/cli.lo ../src/trace.lo ../src/status.lo common/trace_comm.lo common/trace_ctrl.lo common/trace_mmap.lo common/trace_filter.lo common/trace_type.lo deps/sds/sds.lo

ptcli_unames := $(shell uname -s)
ifeq ($(ptcli_unames),Linux)
	ptcli_objects += ../src/ptrace.lo
	CFLAGS_CLEAN += -DPT_PTRACE_ENABLE
endif

ptcli_dir = $(builddir)/../src

ptcli_executable = $(ptcli_dir)/phptrace

PHP_TRACE_BINDIR = ` \
	if test -n "${bindir}"; then \
		$(top_srcdir)/build/shtool echo -n -- "${bindir}"; \
	elif test -d ${exec_prefix}/bin; then \
		$(top_srcdir)/build/shtool echo -n -- "${exec_prefix}/bin"; \
	else \
		$(top_srcdir)/build/shtool path -d "${PHP_EXECUTABLE}"; \
	fi`

install-all: install install-cli

clean-all: clean
	find .. -name \*.gcno -o -name \*.gcda | xargs rm -f
	find .. -name \*.lo -o -name \*.o | xargs rm -f
	find .. -name \*.la -o -name \*.a | xargs rm -f
	find .. -name \*.so | xargs rm -f
	find .. -name .libs -a -type d|xargs rm -rf
	rm -f $(ptcli_executable)

install-cli: build-cli
	-@$(mkinstalldirs) $(INSTALL_ROOT)${PHP_TRACE_BINDIR}
	@echo "Installing PHP Trace binary:      $(INSTALL_ROOT)${PHP_TRACE_BINDIR}/"
	@$(INSTALL) $(ptcli_executable) $(INSTALL_ROOT)${PHP_TRACE_BINDIR}

cli: build-cli

build-cli: $(ptcli_executable)

../src/cli.lo: $(ptcli_dir)/cli.c
	$(LIBTOOL) --mode=compile $(CC) -I. -I$(ptcli_dir) $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) -c $(ptcli_dir)/cli.c -o $(ptcli_dir)/cli.lo

../src/trace.lo: $(ptcli_dir)/trace.c
	$(LIBTOOL) --mode=compile $(CC) -I. -I$(ptcli_dir) $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) -c $(ptcli_dir)/trace.c -o $(ptcli_dir)/trace.lo

../src/status.lo: $(ptcli_dir)/status.c
	$(LIBTOOL) --mode=compile $(CC) -I. -I$(ptcli_dir) $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) -c $(ptcli_dir)/status.c -o $(ptcli_dir)/status.lo

../src/ptrace.lo: $(ptcli_dir)/ptrace.c
	$(LIBTOOL) --mode=compile $(CC) -I. -I$(ptcli_dir) $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) -c $(ptcli_dir)/ptrace.c -o $(ptcli_dir)/ptrace.lo

../src/phptrace: $(ptcli_objects) $(TRACE_SHARED_DEPENDENCIES)
	$(LIBTOOL) --mode=link $(CC) $(COMMON_FLAGS) $(CFLAGS_CLEAN) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ -avoid-version $(EXTRA_LDFLAGS) $(ptcli_objects) $(TRACE_SHARED_LIBADD)

# phptrace-CLI:end
