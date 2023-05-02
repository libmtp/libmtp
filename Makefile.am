SUBDIRS=src examples util doc
ACLOCAL_AMFLAGS=-I m4

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libmtp.pc

EXTRA_DIST=libmtp.pc libmtp.sh COPYING README.windows.txt RELEASE-CHECKLIST.md

# This stuff only makes sense on Linux so only
# build and ship it on Linux.
if USE_LINUX
MTP_HOTPLUG = util/mtp-hotplug

udevrulesdir=@UDEV@/rules.d
hwdbdir=@UDEV@/hwdb.d
udevrules_DATA=@UDEV_RULES@
hwdb_DATA=69-libmtp.hwdb
noinst_DATA=libmtp.usermap libmtp.fdi

libmtp.usermap: $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) > libmtp.usermap

@UDEV_RULES@: $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) -u -p"@UDEV@" @UDEV_GROUP@ @UDEV_MODE@ > @UDEV_RULES@

libmtp.fdi: $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) -H > libmtp.fdi

$(hwdb_DATA): $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) -w > $(hwdb_DATA)

CLEANFILES = libmtp.usermap @UDEV_RULES@ libmtp.fdi libmtp.hwdb
endif
