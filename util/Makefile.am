bin_PROGRAMS=mtp-hotplug
mtp_hotplug_SOURCES=mtp-hotplug.c

if USE_LINUX
libexec_PROGRAMS=mtp-probe
mtp_probe_SOURCES=mtp-probe.c
endif

AM_CPPFLAGS=-I$(top_builddir)/src
LDADD=../src/libmtp.la
