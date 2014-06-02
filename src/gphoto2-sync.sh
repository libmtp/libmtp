#!/bin/sh
#set -e

SVN=`which svn`
if [ "x$SVN" = "x" ]; then
    echo "Install svn! (subversion client)"
    exit 1
fi

if [ ! -d ptp2 ] ; then
    echo "No copy of the gphoto trunk, checking out..."
    svn checkout svn://svn.code.sf.net/p/gphoto/code/trunk/libgphoto2/camlibs/ptp2 ptp2
fi
if [ ! -d ptp2 ] ; then
    echo "Could not clone gphoto trunk."
    exit 1
fi

cd ptp2
svn update
cd ..

cp ptp2/ptp.c ptp.c
cp ptp2/ptp.h ptp.h
cp ptp2/ptp-pack.c ptp-pack.c
cp ptp2/chdk_ptp.h chdk_ptp.h
cp ptp2/chdk_live_view.h chdk_live_view.h
diff -ur ptp2/device-flags.h device-flags.h

echo "Finished!"
