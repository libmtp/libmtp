SUBDIRS=src examples util doc
ACLOCAL_AMFLAGS=-I m4

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libmtp.pc

EXTRA_DIST=libmtp.pc libmtp.sh COPYING README.windows.txt

# This stuff only makes sense on Linux so only
# build and ship it on Linux.
if USE_LINUX
udevrulesdir=$(prefix)/@UDEV_RULES_PATH@
udevrules_DATA=@UDEV_RULES_NAME@
noinst_DATA=libmtp.usermap libmtp.fdi

libmtp.usermap: util/mtp-hotplug
	util/mtp-hotplug > libmtp.usermap
@UDEV_RULES_NAME@: util/mtp-hotplug
	util/mtp-hotplug -u > @UDEV_RULES_NAME@
libmtp.fdi: util/mtp-hotplug
	util/mtp-hotplug -H > libmtp.fdi

CLEANFILES = libmtp.usermap @UDEV_RULES_NAME@ libmtp.fdi
endif
