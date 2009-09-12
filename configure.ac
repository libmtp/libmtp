# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT([libmtp], [1.0.1], [libmtp-discuss@lists.sourceforge.net])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/libmtp.c])
AM_CONFIG_HEADER(config.h)

# This can be overridden by the command line switch
if test "$program_prefix" = NONE; then
   program_prefix=mtp-
   program_transform_name="s,^,$program_prefix,;$program_transform_name"
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AM_ICONV

# Check for doxygen
AC_CHECK_PROG(HAVE_DOXYGEN, doxygen, true, false)
AM_CONDITIONAL(HAVE_DOXYGEN,$HAVE_DOXYGEN)
if test $HAVE_DOXYGEN = "false"; then
             AC_MSG_WARN([*** doxygen not found, docs will not be built])
fi

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

# Checks for libraries.
AC_CHECK_LIB([usb], [usb_control_msg],,
	AC_MSG_ERROR([I can't find the libusb libraries on your system. You
	may need to set the LDFLAGS environment variable to include the
	search path where you have libusb installed before running
	configure (e.g. setenv LDFLAGS=-L/usr/local/lib)]), "$OSFLAGS")

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_TIME
# zlib.h the day we need to decompress firmware
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h getopt.h libgen.h \
	limits.h stdio.h string.h sys/stat.h sys/time.h unistd.h \
	iconv.h langinfo.h locale.h arpa/inet.h byteswap.h sys/uio.h])
AC_CHECK_HEADER([usb.h],,
	AC_MSG_ERROR([I can't find the libusb header file on your system.
	You may need to set the CPPFLAGS environment variable to include
	the search path where you have libusb installed before running
	configure (e.g. setenv CPPFLAGS=-I/usr/local/include)]))

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
AC_CHECK_FUNCS(basename memset select strdup strerror strrchr strtoul usleep mkstemp)

# Switches.
# Enable LFS (Large File Support)
AC_SYS_LARGEFILE
# Stick in "-Werror" if you want to be more aggressive.
# (No need to use AC_SUBST on this default substituted environment variable.)
CFLAGS="$CFLAGS -Wall -Wmissing-prototypes"

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
	examples/Makefile libmtp.sh hotplug.sh libmtp.pc])
AC_OUTPUT
chmod +x hotplug.sh
