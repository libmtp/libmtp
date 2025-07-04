# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT([libmtp], [1.1.22], [libmtp-discuss@lists.sourceforge.net])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/libmtp.c])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_CHECK_TOOL([HOST_MTP_HOTPLUG],[mtp-hotplug],[:])
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AM_GNU_GETTEXT_VERSION([0.11.5])
AM_ICONV

# Optionally set install location of udev
UDEV=/usr/lib/udev
AC_ARG_WITH(udev,
    AS_HELP_STRING([--with-udev=DIR],
    [directory where udev is installed [default=/usr/lib/udev]]),
    [UDEV="${withval}"], [])
AC_DEFINE_UNQUOTED([UDEV_DIR], ["${UDEV}/"], [where mtp-probe is installed, default=/usr/lib/udev/])
AC_SUBST(UDEV)
dnl NOTE: Since the (default) UDEV directory is not part of libmtp,
dnl we cannot do a 'make distcheck' with it as non-root user, this
dnl is because 'make distcheck' uses $prefix to create a build dir.
dnl We default to using '$(libdir)/udev/' as user, so that we can
dnl run 'make distcheck' as well as run 'rpmbuild -ba' too. Do this
dnl here since automake can't process 'if/else/endif in Makefile.am
UDEVdata_SNIPPET='
noinst_DATA="libmtp.fdi libmtp.usermap"
ifeq ($(shell id -u),0)
    udevrulesdir=$(UDEV)/rules.d
    hwdbdir=$(UDEV)/hwdb.d
ifdef ENABLE_CROSSBUILD
    udevrulesdir=$(TARGET_UDEV)/rules.d
    hwdbdir=$(TARGET_UDEV)/hwdb.d
endif
else
    udevrulesdir=$(libdir)/udev/rules.d
    hwdbdir=$(libdir)/udev/hwdb.d
endif
'
AC_SUBST([UDEVdata_SNIPPET])
AM_SUBST_NOTMAKE([UDEVdata_SNIPPET])
UDEVbin_SNIPPET='
ifeq ($(shell id -u),0)
    mtp_probedir=$(UDEV)
ifdef ENABLE_CROSSBUILD
    mtp_probedir=$(TARGET_UDEV)
endif
else
    mtp_probedir=$(libdir)/udev
endif
'
AC_SUBST([UDEVbin_SNIPPET])
AM_SUBST_NOTMAKE([UDEVbin_SNIPPET])

# Optionally set name of udev rules file, default
# priority is 69, to appear before 70-acl.rules which handles
# access control to the devices. We try to let all other userspace
# libusb-based device handlers take precedence since libmtp may
# attempt to autoprobe the devices.
#
# Later rules can then use the identifiers for its processing.
UDEV_RULES=69-libmtp.rules
AC_ARG_WITH(udev-rules,
    AS_HELP_STRING([--with-udev-rules=NAME],
    [file name for udev rules [default=69-libmtp.rules]]),
    [UDEV_RULES="${withval}"], [])
AC_SUBST(UDEV_RULES)

# Optionally set group for device nodes
UDEV_GROUP=
AC_ARG_WITH(udev-group,
    AS_HELP_STRING([--with-udev-group=GROUP],
    [file group for device nodes [default: none specified]]),
    [UDEV_GROUP="-g\"${withval}\""], [])
AC_SUBST(UDEV_GROUP)

# Optionally set mode for device nodes
UDEV_MODE=
AC_ARG_WITH(udev-mode,
    AS_HELP_STRING([--with-udev-mode=GROUP],
    [file mode for device nodes [default: none specified]]),
    [UDEV_MODE="-m\"${withval}\""], [])
AC_SUBST(UDEV_MODE)

# Optionally enable and check for doxygen
AC_ARG_ENABLE([doxygen],
    AS_HELP_STRING([--enable-doxygen], [Build API documentation using Doxygen [default=auto]]),
    [ac_enable_doxygen=$enableval], [ac_enable_doxygen=auto])
if test "x$ac_enable_doxygen" != "xno"; then
    AC_CHECK_PROG(HAVE_DOXYGEN, doxygen, true, false)
    if test "x$HAVE_DOXYGEN" = "xfalse" -a "x$ac_enable_doxygen" = "xyes"; then
        AC_MSG_ERROR([*** API documentation explicitly requested but Doxygen not found])
    fi
    if test "x$HAVE_DOXYGEN" = "xtrue"; then
        AC_MSG_NOTICE([API documentation will be generated using Doxygen])
    fi
else
    HAVE_DOXYGEN=false
    AC_MSG_NOTICE([API documentation will not be generated])
fi
AM_CONDITIONAL(HAVE_DOXYGEN,$HAVE_DOXYGEN)

# Optionally crossbuild libmtp for a target CPU/OS
# Most users can safely ignore this option and leave it as 'off'.
# Users crossbuilding for another CPU/OS need to follow 3 steps
# 1) First, make sure the host has same version libmtp installed.
#    Easiest way to ensure this is to install this on host first.
#    crossbuild uses the host's mtp-hotplug to create files.
# 2) Next, run ./configure with appropriate settings.
#    Users will need to set these two options in configure:
#    --with-udev=UDEV_DIR_as_seen_later_by_the_target_cpu
#    --enable-crossbuilddir=UDEV_DIR_as_seen_now_by_host_cpu
#    here is an example for crossbuilding
#    ./configure --with-udev=/usr/lib/udev --enable-crossbuilddir=target_root_dir/usr/lib/udev
# 3) next, run 'make' and then 'sudo make install'
#    note that 'make install' will install mtp-hotprobe in the
#    target directory, but will not run target mtp-hotplug to
#    create target config files because the target CPU may not
#    be binary equivalent with host CPU. 69-libmtp.hwdb and
#    @UDEV_RULES@ should also get installed in target udev too.
#
crossbuilddir=off
AC_ARG_ENABLE(crossbuilddir,
    AS_HELP_STRING([--enable-crossbuilddir], [crossbuild libmtp (this is step 2of3) [default=off]]),
    [crossbuilddir=$enableval], [crossbuilddir=off])
if test "$crossbuilddir" != off; then
    if test x"${HOST_MTP_HOTPLUG}" = "x:"; then
        AC_MSG_ERROR([Error: Host PC 'mtp-hotplug' not found! Please install 'libmtp' on host PC before doing crossbuild!])
    fi
fi
AM_CONDITIONAL(ENABLE_CROSSBUILD,[test "$crossbuilddir" != off])
TARGET_UDEV=${crossbuilddir}
AC_SUBST(TARGET_UDEV)

# Check for Darwin
AC_MSG_CHECKING([if the host operating system is Darwin])
case "$host" in
  *-darwin*)
    AC_MSG_RESULT([yes])
    CFLAGS="$CFLAGS -DUSE_DARWIN"
    OSFLAGS="-framework IOKit"
    ;;
  *) AC_MSG_RESULT([no]) ;;
esac
AC_SUBST(OSFLAGS)

AC_MSG_CHECKING([if the host operating system is Linux])
AC_TRY_COMPILE([#ifndef __linux__
		#error "FAIL"
		#endif
		],
		[int test;],
		[ AC_MSG_RESULT(yes)
		  AM_CONDITIONAL(USE_LINUX, true)
		],
		[ AC_MSG_RESULT(no)
		  AM_CONDITIONAL(USE_LINUX, false)
		])

# Check for mingw compiler platform
AC_MSG_CHECKING([For MinGW32])
case "$host" in
  *-*-mingw*)
    AC_MSG_RESULT([yes])
    mingw_compiler=yes
    ;;
  *) AC_MSG_RESULT([no]) ;;
esac
AM_CONDITIONAL(COMPILE_MINGW32, [test "$mingw_compiler" = "yes"])

# Check if Microsoft LIB.EXE is available
if test "$mingw_compiler" = "yes"; then
  AC_CHECK_PROG(ms_lib_exe, lib.exe, yes, no)
fi
AM_CONDITIONAL(MS_LIB_EXE, test x$ms_lib_exe = xyes)

# Check for libusb-1.0
PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.0,[
   LIBUSB_VER=`$PKG_CONFIG --modversion libusb-1.0`
   LIBUSB_REQUIRES='libusb-1.0'
   AC_DEFINE(HAVE_LIBUSB1, [], [Have libusb 1.0])
   found_libusb1_pkg=yes
],[
   # Check for libopenusb second
   AC_CHECK_LIB([openusb], [openusb_init],[
	AC_DEFINE(HAVE_OPENUSB, [], [Have libopenusb 1.0])
        LIBUSB_VER=1.0
   ],[
      PKG_CHECK_MODULES(LIBUSB, libusb >= 0.1.8, [
         LIBUSB_VER=`$PKG_CONFIG --modversion libusb`
         LIBUSB_REQUIRES='libusb'
         AC_DEFINE(HAVE_LIBUSB0, [], [Have libusb 0.x])
         found_libusb0_pkg=yes
      ],[
         # Some systems don't use pkg-config for libusb, so double check.
         AC_CHECK_LIB([usb], [usb_open],[
            LIBUSB_VER=0.1
            AC_DEFINE(HAVE_LIBUSB0, [], [Have libusb 0.x])
            found_libusb0_pkg=yes
         ],[
            AC_MSG_ERROR([*** libusb not found! (missing -dev/-devel package?) ***\n*** prefer to install libusb-1 if you can! ***])
         ])
      ])
   ])
])

# Optionally disable MTPZ functionality.
AC_ARG_ENABLE([mtpz],
	AS_HELP_STRING([--disable-mtpz], [Disable functionality to connect to MTPZ devices (e.g. Zune)]),
	[use_mtpz=$enableval],
	[use_mtpz="yes"])

# Include MTPZ if not disabled.
if test x"$use_mtpz" = "xyes" ; then
	# Check for libgcrypt.
	AC_MSG_CHECKING([for libgcrypt])
	AC_CHECK_LIB([gcrypt], [gcry_check_version], , [AC_MSG_NOTICE([*** libgcrypt not found - disabling MTPZ support ***]) ; use_mtpz="no"])
fi

if test x"$use_mtpz" = "xyes" ; then
	AC_DEFINE(USE_MTPZ, [], [Enable MTPZ functionality])
	AC_MSG_NOTICE([MTPZ functionality enabled]);
else
	AC_MSG_NOTICE([MTPZ functionality disable]);
fi


AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)
AC_SUBST(LIBUSB_REQUIRES)
AM_CONDITIONAL(MTPZ_COMPILE, test x"$use_mtpz" = "xyes")
AM_CONDITIONAL(LIBUSB1_COMPILE, test x"$found_libusb1_pkg" = "xyes")
AM_CONDITIONAL(LIBUSB0_COMPILE, test x"$found_libusb0_pkg" = "xyes")
AM_CONDITIONAL(LIBOPENUSB_COMPILE, test x"$ac_cv_lib_openusb_openusb_init" = "xyes")
AC_MSG_NOTICE([*** using libusb $LIBUSB_VER ***])

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
# zlib.h the day we need to decompress firmware
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h getopt.h libgen.h \
	limits.h stdio.h string.h sys/stat.h sys/time.h unistd.h \
	langinfo.h locale.h arpa/inet.h byteswap.h sys/uio.h])
dnl glibc>=2.1 has iconv_open(), but older glibc or distros, and
dnl other OSes will need to install libiconv before building libmtp
dnl see Installation info: https://www.gnu.org/software/libiconv/
have_iconv=no
AC_CHECK_HEADERS([iconv.h],[AC_CHECK_FUNC([iconv_open],[have_iconv=yes])])
if test x"$have_iconv" = "xno" ; then
	AC_SEARCH_LIBS([iconv],[have_iconv=yes],[
	AC_MSG_ERROR([*** ERROR! iconv_open() required! Please install libiconv first. ***])])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLKSIZE

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_CHECK_FUNCS(basename memset select strdup strerror strndup strrchr strtoul usleep mkstemp)

# Switches.
# Enable LFS (Large File Support)
AC_SYS_LARGEFILE
# Stick in "-Werror" if you want to be more aggressive.
# (No need to use AC_SUBST on this default substituted environment variable.)
# Only add these additional CFLAGS if we are using GCC. Other C compilers may
# not support them.
if test x"$GCC" = "xyes" ; then
	CFLAGS="$CFLAGS -Wall -Wmissing-prototypes"
fi

# Output files

# Create a stdint.h-like file containing size-specific integer definitions
# that will always be available. The <stdint.h> file is required by the
# library, but we provide this anyway because the libptp2 stuff wants this
# file.
AX_NEED_STDINT_H([src/_stdint.h])

# Create a header file containing NetBSD-style byte swapping macros.
# This m4 macros has caused severe pain, I am considering creating a
# hard-coded byte swapper that will be eternally portable.
AC_NEED_BYTEORDER_H(src/gphoto2-endian.h)

# Put ifdef wrapper around config.h so it's not accidently called twice.
AH_TOP([#ifndef _LIBMTP_CONFIG_H
#define _LIBMTP_CONFIG_H 1])
AH_BOTTOM([#endif])

AC_CONFIG_FILES([src/libmtp.h doc/Doxyfile Makefile doc/Makefile src/Makefile
	examples/Makefile util/Makefile libmtp.sh hotplug.sh libmtp.pc])
AC_OUTPUT
chmod +x hotplug.sh
