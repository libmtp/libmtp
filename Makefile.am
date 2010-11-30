SUBDIRS=src examples util doc
ACLOCAL_AMFLAGS=-I m4

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libmtp.pc
EXTRA_DIST=libmtp.pc libmtp.sh COPYING README.windows.txt
noinst_DATA=libmtp.usermap libmtp.rules libmtp.fdi
libmtp.usermap: util/mtp-hotplug
	util/mtp-hotplug > libmtp.usermap
libmtp.rules: util/mtp-hotplug
	util/mtp-hotplug -u > libmtp.rules
libmtp.fdi: util/mtp-hotplug
	util/mtp-hotplug -H > libmtp.fdi

CLEANFILES = libmtp.usermap libmtp.rules libmtp.fdi
