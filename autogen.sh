#!/bin/sh
#set -e

# Before running autogen.sh, you wil need to install: autopoint automake autoconf libtool
# and the developer versions for: libgcrypt-dev libusb-1.0-0-dev
# and also the developer version of: libiconv (this may also be within 'gettext' for some distros).

srcdir=`dirname $0`

ACLOCAL_FLAGS="-I ${srcdir}/m4 ${ACLOCAL_FLAGS}"

fail() {
    status=$?
    echo "Last command failed with status $status in directory $(pwd)."
    echo "Aborting"
    exit $status
}

# Refresh GNU autotools toolchain: libtool
echo "Removing libtool cruft"
rm -f ltmain.sh config.guess config.sub
echo "Running libtoolize"
(glibtoolize --version) < /dev/null > /dev/null 2>&1 && LIBTOOLIZE=glibtoolize || LIBTOOLIZE=libtoolize
$LIBTOOLIZE --copy --force || fail

echo "Running gettextize --force"
gettextize --force

# Refresh GNU autotools toolchain: aclocal autoheader
echo "Removing aclocal cruft"
rm -f aclocal.m4
echo "Running aclocal $ACLOCAL_FLAGS"
aclocal $ACLOCAL_FLAGS || fail
echo "Removing autoheader cruft"
rm -f config.h.in src/config.h.in
echo "Running autoheader"
autoheader || fail

# Refresh GNU autotools toolchain: automake
echo "Removing automake cruft"
rm -f depcomp install-sh missing mkinstalldirs
rm -f stamp-h*
echo "Running automake"
touch config.rpath
automake --add-missing --gnu || fail

# Refresh GNU autotools toolchain: autoconf
echo "Removing autoconf cruft"
rm -f configure
rm -rf autom4te*.cache/
echo "Running autoconf"
autoconf -f

# Autoupdate config.sub and config.guess
# from GNU CVS
WGET=`which wget`
if [ "x$WGET" != "x" ]; then
    echo "Autoupdate config.sub and config.guess (y/n)?"
    read IN
    if [ "$IN" = "y" ] || [ "$IN" = "Y" ]; then
        wget -O config.guess https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess
        wget -O config.sub https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub
        chmod +x config.guess config.sub
        echo "config.guess and config.sub updated"
    fi
else
    echo "Could not autoupdate config.sub and config.guess"
fi

if [ ! -z "$NOCONFIGURE" ]; then
	echo "autogen.sh finished! ./configure skipped."
	exit $?
fi

echo "autogen.sh finished! Now going to run ./configure $@"
./configure $@ || {
    echo "./configure failed";
    exit 1;
}
