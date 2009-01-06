lib_LTLIBRARIES=libmtp.la
libmtp_la_SOURCES=libmtp.c unicode.c unicode.h util.c util.h playlist-spl.c \
	libusb-glue.c libusb-glue.h \
	gphoto2-endian.h libptp-stdint.h ptp.c ptp.h \
	music-players.h device-flags.h playlist-spl.h
include_HEADERS=libmtp.h
EXTRA_DIST=libmtp.h.in libmtp.sym ptp-pack.c

# ---------------------------------------------------------------------------
# Advanced information about versioning:
#   * "Writing shared libraries" by Mike Hearn
#         http://navi.cx/~mike/writing-shared-libraries.html
#   * libtool.info chapter "Versioning"
#   * libtool.info chapter "Updating library version information"
# ---------------------------------------------------------------------------
# Versioning:
#  - CURRENT (Major):  Increment if the interface has changes. AGE is always
#                      *changed* at the same time.
#  - AGE (Micro):      Increment if any interfaces have been added; set to 0
#		         if any interfaces have been removed. Removal has 
#                      precedence over adding, so set to 0 if both happened.
#                      It denotes upward compatibility.
#  - REVISION (Minor): Increment any time the source changes; set to 
#			 0 if you incremented CURRENT.
#
#  To summarize. Any interface *change* increment CURRENT. If that interface
#  change does not break upward compatibility (ie it is an addition), 
#  increment AGE, Otherwise AGE is reset to 0. If CURRENT has changed, 
#  REVISION is set to 0, otherwise REVISION is incremented.
# ---------------------------------------------------------------------------
CURRENT=10
AGE=2
REVISION=0
SOVERSION=$(CURRENT):$(REVISION):$(AGE)

if COMPILE_MINGW32
noinst_DATA=libmtp.lib
W32LF=-export-dynamic
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

libmtp_la_LDFLAGS=@LDFLAGS@  -no-undefined -export-symbols libmtp.sym -version-info $(W32LF) $(SOVERSION)
libmtp_la_DEPENDENCIES=libmtp.sym

DISTCLEANFILES = _stdint.h
