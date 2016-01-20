PHP_TRACE_BINDIR = ` \
	if test -n "${bindir}"; then \
		$(top_srcdir)/build/shtool echo -n -- "${bindir}"; \
	elif test -d ${exec_prefix}/bin; then \
		$(top_srcdir)/build/shtool echo -n -- "${exec_prefix}/bin"; \
	else \
		$(top_srcdir)/build/shtool path -d "${PHP_EXECUTABLE}"; \
	fi`

install-cli: tracetool
	-@$(mkinstalldirs) $(INSTALL_ROOT)${PHP_TRACE_BINDIR}
	@echo "Installing PHP Trace binary:      $(INSTALL_ROOT)${PHP_TRACE_BINDIR}/"
	@$(INSTALL) $(builddir)/../src/phptrace $(INSTALL_ROOT)${PHP_TRACE_BINDIR}

clean-tracetool:
	cd ../src && make clean

tracetool: $(builddir)/../src/phptrace

$(builddir)/../src/phptrace:
	cd ../src && make
