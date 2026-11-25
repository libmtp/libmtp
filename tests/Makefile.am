# Common CPPFLAGS
AM_CPPFLAGS = -I$(builddir) -I$(top_builddir) -I$(top_srcdir) -I$(srcdir) -I$(top_srcdir)/src/

AM_CFLAGS = -I$(builddir) -I$(top_builddir) -I$(srcdir) -I$(top_srcdir) $(DEP_CFLAGS) $(BABL_CFLAGS)
if !USE_LINUX
AM_CFLAGS = -liconv
endif

EXTRA_DIST = testunicode.c
GENERATED = testunicode

# The tests
noinst_PROGRAMS = testunicode

testunicode_SOURCES = testunicode.c ../src/unicode.c
testunicode.$(OBJEXT): ../src/unicode.h

TESTS = $(noinst_PROGRAMS)

clean-local:
	rm -rf $(GENERATED)
