# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT([libmtp], [1.1.2], [libmtp-discuss@lists.sourceforge.net])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/libmtp.c])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AM_ICONV

# Optionally set install location of udev
UDEV=/lib/udev
AC_ARG_WITH(udev,
    AC_HELP_STRING([--with-udev=DIR],
    [directory where udev is installed [default=/lib/udev]]),
    [UDEV="${withval}"], [])
AC_SUBST(UDEV)

# Optionally set name of udev rules file, default
# priority is 69, to appear before 70-acl.rules which handles
# access control to the devices. We try to let all other userspace
# libusb-based device handlers take precedence since libmtp may
# attempt to autoprobe the devices.
#
# Later rules can then use the identifiers for its processing.
UDEV_RULES=69-libmtp.rules
AC_ARG_WITH(udev-rules,
    AC_HELP_STRING([--with-udev-rules=NAME],
    [file name for udev rules [default=69-libmtp.rules]]),
    [UDEV_RULES="${withval}"], [])
AC_SUBST(UDEV_RULES)

# Optionally set group for device nodes
UDEV_GROUP=
AC_ARG_WITH(udev-group,
    AC_HELP_STRING([--with-udev-group=GROUP],
    [file group for device nodes [default: none specified]]),
    [UDEV_GROUP="-g\"${withval}\""], [])
AC_SUBST(UDEV_GROUP)

# Optionally set mode for device nodes
UDEV_MODE=
AC_ARG_WITH(udev-mode,
    AC_HELP_STRING([--with-udev-mode=GROUP],
    [file mode for device nodes [default: none specified]]),
    [UDEV_MODE="-m\"${withval}\""], [])
AC_SUBST(UDEV_MODE)

# Optionally enable and check for doxygen
AC_ARG_ENABLE([doxygen],
    AS_HELP_STRING([--enable-doxygen], [Build API documentation using Doxygen [default=no]]),
    [ac_enable_doxygen=$enableval], [ac_enable_doxygen=auto])
if test "x$ac_enable_doxygen" == "xyes"; then
    AC_CHECK_PROG(HAVE_DOXYGEN, doxygen, true, false)
    if test "x$HAVE_DOXYGEN" = "xfalse" -a "x$ac_enable_doxygen" = "xyes"; then
        AC_MSG_ERROR([*** API documentation explicitly requested but Doxygen not found])
    fi
    AC_MSG_NOTICE([API documentation will be generated using Doxygen])
else
    HAVE_DOXYGEN=false
    AC_MSG_NOTICE([API documentation will not be generated])
fi
AM_CONDITIONAL(HAVE_DOXYGEN,$HAVE_DOXYGEN)

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
PKG_CHECK_MODULES(LIBUSB, libusb-1.0 >= 1.0.0, found_libusb1_pkg=yes, found_libusb1_pkg=no)
if test x"$found_libusb1_pkg" = "xyes" ; then
   LIBUSB_VER=`pkg-config --modversion libusb-1.0`
   AC_DEFINE(HAVE_LIBUSB1, [], [Have libusb 1.0])
fi

# Check for libopenusb
AC_CHECK_LIB([openusb], [openusb_init])
if test x"$found_libusb1_pkg" = "xno" ; then
   if test x"$ac_cv_lib_openusb_openusb_init" = "xyes" ; then
      AC_DEFINE(HAVE_OPENUSB, [], [Have libopenusb 1.0])
      LIBUSB_VER=1.0
   fi
fi

# Check for libusb 0.x
if test x"$found_libusb1_pkg" = "xno" ; then
   if test x"$ac_cv_lib_openusb_openusb_init" = "xno" ; then
      PKG_CHECK_MODULES(LIBUSB, libusb >= 0.1.8, found_libusb0_pkg=yes, found_libusb0_pkg=no)
      if test x"$found_libusb0_pkg" = "xyes" ; then
         LIBUSB_VER=`pkg-config --modversion libusb`
         AC_DEFINE(HAVE_LIBUSB0, [], [Have libusb 0.x])
      else
         # Some systems don't use pkg-config for libusb, so double check.
         AC_CHECK_LIB([usb], [usb_open], found_libusb0_pkg=yes, found_libusb0_pkg=no)
         if test x"$ac_cv_lib_usb_usb_open" = "xyes" ; then
            LIBUSB_VER=0.1
            AC_DEFINE(HAVE_LIBUSB0, [], [Have libusb 0.x])
            found_libusb0_pkg=xyes
         fi
         if test x"$found_libusb0_pkg" = "xno" ; then
            AC_MSG_ERROR([*** libusb not found! (missing -dev/-devel package?) ***\n*** prefer to install libusb-1 if you can! ***])
         fi
      fi
   fi
fi


AC_SUBST(LIBUSB_CFLAGS)
AC_SUBST(LIBUSB_LIBS)
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

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_STRUCT_ST_BLKSIZE

# Checks for library functions.
AC_FUNC_MALLOC
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
if test x"$GCC" == "xyes" ; then
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

AC_CONFIG_FILES([src/libmtp.h doc/Doxyfile Makefile doc/Makefile src/Makefile
	examples/Makefile util/Makefile libmtp.sh hotplug.sh libmtp.pc])
AC_OUTPUT
chmod +x hotplug.sh
