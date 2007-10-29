#!/bin/sh
#set -e

srcdir=`dirname $0`

# Get sources from gphoto2 SVN
WGET=`which wget`
if [ "x$WGET" != "x" ]; then
    wget -O tmpfile http://gphoto.svn.sourceforge.net/viewvc/*checkout*/gphoto/trunk/libgphoto2/camlibs/ptp2/ptp.c
    mv tmpfile ptp.c.gphoto2
    wget -O tmpfile http://gphoto.svn.sourceforge.net/viewvc/*checkout*/gphoto/trunk/libgphoto2/camlibs/ptp2/ptp.h
    mv tmpfile ptp.h.gphoto2
    wget -O tmpfile http://gphoto.svn.sourceforge.net/viewvc/*checkout*/gphoto/trunk/libgphoto2/camlibs/ptp2/ptp-pack.c
    mv tmpfile ptp-pack.c.gphoto2
    wget -O tmpfile http://gphoto.svn.sourceforge.net/viewvc/*checkout*/gphoto/trunk/libgphoto2/camlibs/ptp2/library.c
    mv tmpfile library.c.gphoto2
    wget -O tmpfile http://gphoto.svn.sourceforge.net/viewvc/*checkout*/gphoto/trunk/libgphoto2/camlibs/ptp2/music-players.h
    mv tmpfile music-players.h.gphoto2
else
    echo "Could not sync to gphoto2. No WGET."
fi

echo "Finished!"

