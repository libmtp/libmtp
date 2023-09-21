if USE_LINUX
bin_PROGRAMS=mtp-hotplug
mtp_hotplug_SOURCES=mtp-hotplug.c

mtp_probe_SOURCES = mtp-probe.c
mtp_probe_PROGRAMS = mtp-probe
mtp_probedir = $(bindir)
@UDEVbin_SNIPPET@
endif

AM_CPPFLAGS=-I$(top_builddir)/src
LDADD=../src/libmtp.la
