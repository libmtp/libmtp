#!/bin/sh
#set -e

srcdir=`dirname $0`

# Get sources from gphoto2 SVN
WGET=`which wget`
if [ "x$WGET" != "x" ]; then
    wget -O tmpfile http://sourceforge.net/p/gphoto/code/14266/tree/trunk/libgphoto2/camlibs/ptp2/ptp.c?format=raw
    mv tmpfile ptp.c.gphoto2
    wget -O tmpfile tmpfile http://sourceforge.net/p/gphoto/code/14266/tree/trunk/libgphoto2/camlibs/ptp2/ptp.h?format=raw
    mv tmpfile ptp.h.gphoto2
    wget -O tmpfile tmpfile http://sourceforge.net/p/gphoto/code/14266/tree/trunk/libgphoto2/camlibs/ptp2/ptp-pack.c?format=raw
    mv tmpfile ptp-pack.c.gphoto2
    wget -O tmpfile tmpfile http://sourceforge.net/p/gphoto/code/14266/tree/trunk/libgphoto2/camlibs/ptp2/library.c?format=raw
    mv tmpfile library.c.gphoto2
    wget -O tmpfile tmpfile http://sourceforge.net/p/gphoto/code/14266/tree/trunk/libgphoto2/camlibs/ptp2/music-players.h?format=raw
    mv tmpfile music-players.h.gphoto2
else
    echo "Could not sync to gphoto2. No WGET."
fi

echo "Finished!"

