SUBDIRS=src examples util doc
ACLOCAL_AMFLAGS=-I m4

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libmtp.pc

EXTRA_DIST = autogen.sh libmtp.pc libmtp.sh COPYING README.windows.txt RELEASE-CHECKLIST.md sync-usbids.sh
nodist_EXTRA_DATA = .git .github .travis-translate-pkgs .travis.yml
DISTCHECK_CONFIGURE_FLAGS = --with-udev-group=nobody --with-udev-mode=760, --disable-mtpz

# This stuff only makes sense on Linux so only
# build and ship it on Linux.
if USE_LINUX
MTP_HOTPLUG = util/mtp-hotplug
@UDEVdata_SNIPPET@

libmtp.usermap: $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) > libmtp.usermap

@UDEV_RULES@: $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) -u -p"@UDEV@" @UDEV_GROUP@ @UDEV_MODE@ > @UDEV_RULES@

libmtp.fdi: $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) -H > libmtp.fdi

$(hwdb_DATA): $(MTP_HOTPLUG)
	$(MTP_HOTPLUG) -w > $(hwdb_DATA)

CLEANFILES = libmtp.usermap @UDEV_RULES@ libmtp.fdi 69-libmtp.hwdb
endif
