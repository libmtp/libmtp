#!/bin/sh
# usage ./a.sh /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-8 mtp
# here $1 is devicepath
# $2 is the string descriptor to match against
# Use from udev:
# PROGRAM="/<prefix>/check_mtp_device.sh /sys$env{DEVPATH} mtp" SYMLINK+="libmtp-%k", MODE="666", GROUP="plugdev"
checklog="/tmp/check_mtp_device_log.txt"
echo $1 > $checklog

if [ -d $1 ]
then
	cd $1
	#check if the string 'interface' exists and contains
	# the string descriptor
	grep -i $2 `find -name interface`
	#echo $result >> $checklog
	if [ "$?" -eq "0" ]
	then
		echo "found $2 string descriptor, return 0" >> $checklog
		exit 0;
	else
		echo "did not find $2 string descriptor, return 1" >> $checklog
		exit 1;
	fi
else
	echo "$1 does not exist, return 1\n" >> $checklog
	exit 1;
fi
