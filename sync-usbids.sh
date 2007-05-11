#!/bin/sh

# Script to compare the USB ID list from linux-usb
# with the internal list of libmtp.

WGET=`which wget`
if [ "x$WGET" != "x" ]; then
    wget -O usb.ids http://www.linux-usb.org/usb.ids
    examples/hotplug -i > usb.ids-libmtp
    echo "OK now compare usb.ids and usb.ids-libmtp..."
else
    echo "Could not sync to linux-usb USB ID list. No WGET."
fi
