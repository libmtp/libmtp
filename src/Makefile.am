lib_LTLIBRARIES=libmtp.la
libmtp_la_SOURCES=libmtp.c unicode.c unicode.h util.c util.h \
	mtp-utils.c mtp-utils.h mtp-pack.c mtp-pack.h mtp.c mtp.h \
	libptp-endian.h libptp-stdint.h ptp-pack.c ptp-pack.h ptp.c ptp.h
include_HEADERS=libmtp.h
EXTRA_DIST=libmtp.h.in libmtp.sym

# This is the version of the *library interface*, not the revision
# of libmtp itself. Do not change this unless you're absolutely 
# certain of what the difference is. (See the libtool manual,
# section 6.3 (http://www.gnu.org/software/libtool/manual.html)
CURRENT=0
REVISION=0
AGE=0
SOVERSION=$(CURRENT):$(REVISION):$(AGE)
libmtp_la_LDFLAGS=@LDFLAGS@ -version-info $(SOVERSION)

if COMPILE_MINGW32
noinst_DATA=libmtp.lib
libmtp_la_LDFLAGS += -export-dynamic -no-undefined -export-symbols libmtp.sym
libmtp.def: libmtp.sym
	echo "LIBRARY \"@PACKAGE@\"" > libmtp.def
	echo "DESCRIPTION \"Media Transfer Protocol (MTP) library\"" >> libmtp.def
	echo "VERSION @VERSION@" >> libmtp.def
	echo >> libmtp.def
	echo "EXPORTS" >> libmtp.def
	cat libmtp.sym >> libmtp.def
libmtp.lib: libmtp.la libmtp.def
	lib -name:libmtp-$(LT_CURRENT_MINUS_AGE).dll -def:libmtp.def -out:$@
install-data-local: libmtp.lib libmtp.def
	$(INSTALL) libmtp.def $(DESTDIR)$(libdir)
	$(INSTALL) libmtp.lib $(DESTDIR)$(libdir)
endif
