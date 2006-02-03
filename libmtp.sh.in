#!/bin/sh
# Lifts a plugged in MTP device to user space and
# optionally runs a client program.
# Written by Linus Walleij 2006, based on the "usbcam"
# script by Nalin Dahyabhai.
DEVICEOWNER=root
DEVICEPERMS=666

# Special quirk for SuSE systems using "resmgr"
# (see http://rechner.lst.de/~okir/resmgr/)
if [ -f /sbin/resmgr ]
then
    /sbin/resmgr "${ACTION}" "${DEVICE}" desktop usb
    exit 0
fi

# This is for most other distributions
if [ "${ACTION}" = "add" ] && [ -f "${DEVICE}" ]
then
    # New code, using lock files instead of copying /dev/console permissions
    # This also works with non-gdm logins (e.g. on a virtual terminal)
    # Idea and code from Nalin Dahyabhai <nalin@redhat.com>
    if [ "x$DEVICEOWNER" = "xCONSOLE" ]
    then
	if [ -f /var/run/console/console.lock ]
	then
	    DEVICEOWNER=`cat /var/run/console/console.lock`
	elif [ -f /var/run/console.lock ]
	then
	    DEVICEOWNER=`cat /var/run/console.lock`
	elif [ -f /var/lock/console.lock ]
	then
	    DEVICEOWNER=`cat /var/lock/console.lock`
	else
	    DEVICEOWNER="nobody"
	    DEVICEPERMS="666"
	fi
    fi
    if [ -n "$DEVICEOWNER" ]
    then
        chmod 0000 "${DEVICE}"
        chown "${DEVICEOWNER}" "${DEVICE}"
        chmod "${DEVICEPERMS}" "${DEVICE}"
    fi
fi
