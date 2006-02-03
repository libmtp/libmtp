EXTRA_DIST=Doxyfile.in examples.h mainpage.h

if HAVE_DOXYGEN
pkgdocdir=$(datadir)/doc/$(PACKAGE)-$(VERSION)
htmldocdir=$(pkgdocdir)/html

all-local:
	doxygen

install-data-local:
	$(INSTALL) -d $(DESTDIR)$(htmldocdir)
	$(INSTALL_DATA) html/* $(DESTDIR)$(htmldocdir)

uninstall-local:
	$(RM) -r $(DESTDIR)$(htmldocdir)
	$(RM) -r $(DESTDIR)$(pkgdocdir)

clean-local:
	$(RM) -r html latex man
endif
