SUBDIRS=src examples doc
ACLOCAL_AMFLAGS=-I m4

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libmtp.pc
EXTRA_DIST=libmtp.pc libmtp.sh COPYING
noinst_DATA=libmtp.usermap libmtp.rules
libmtp.usermap: examples/hotplug
	examples/hotplug > libmtp.usermap
libmtp.rules: examples/hotplug
	examples/hotplug -u > libmtp.rules

