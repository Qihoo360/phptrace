PHP_TRACE_BINDIR = ` \
	if test -n "${bindir}"; then \
		$(top_srcdir)/build/shtool echo -n -- "${bindir}"; \
	elif test -d ${exec_prefix}/bin; then \
		$(top_srcdir)/build/shtool echo -n -- "${exec_prefix}/bin"; \
	else \
		$(top_srcdir)/build/shtool path -d "${PHP_EXECUTABLE}"; \
	fi`

install-tracetool: tracetool
	-@$(mkinstalldirs) $(INSTALL_ROOT)${PHP_TRACE_BINDIR}
	@echo "Installing PHP Trace binary:      $(INSTALL_ROOT)${PHP_TRACE_BINDIR}/"
	@$(INSTALL) $(builddir)/../cmdtool/phptrace $(INSTALL_ROOT)${PHP_TRACE_BINDIR}

clean-tracetool:
	cd ../cmdtool && make clean

tracetool: $(builddir)/../cmdtool/phptrace

$(builddir)/../cmdtool/phptrace:
	cd ../cmdtool && make
