SUBDIRS=src examples doc
ACLOCAL_AMFLAGS=-I m4

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libmtp.pc
EXTRA_DIST=libmtp.pc libmtp.sh COPYING README.windows.txt
noinst_DATA=libmtp.usermap libmtp.rules libmtp.fdi
libmtp.usermap: examples/hotplug
	examples/hotplug > libmtp.usermap
libmtp.rules: examples/hotplug
	examples/hotplug -u > libmtp.rules
libmtp.fdi: examples/hotplug
	examples/hotplug -H > libmtp.fdi

CLEANFILES = libmtp.usermap libmtp.rules libmtp.fdi
