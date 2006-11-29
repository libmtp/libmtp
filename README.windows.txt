This file was created by Farooq Zaman <emotional_keats2h@hotmail.com>
as a guide to practical Windows porting of libmtp using MSVC (Microsoft
Visual C++). Some details may have changed, e.g. the things that could be 
fixed in libmtp sources by #ifdef __WIN32__ macros have been folded back 
in. This has been tested on Windows 2000.


libmtp.c
========
	1. Include <io.h> file.
	2. Comment out <sys/mman.h> file.
	3. Line# 2115 replace "open" with "_open" and replace "S_IRWXU|S_IRGRP" with "_S_IREAD".
	4. Line# 2126 replace "close" with "_close".
	5. Line# 2283 replace "open" with "_open" and add one more closing ")" at the end of _open API.
	6. Line# 2294 replace "close" with "_close".
	7. Line# 2502 and Line# 2513 repeat steps 5 and 6.
libmtp.h
========
	1. replace <usb.h> and <stdint.h> with "usb.h" and "stdint.h" respectively.

libusb-glue.c
=============
	1. Comment out <getopt.h>, <unistd.h>, <utime.h> and <sys/mman.h> includes.
	2. Replace <usb.h> with "usb.h"
	3. Line# 537 Add usb_set_configuration(device_handle, dev->config->bConfigurationValue); before claiming USB 	interface.

ptp.c
======
	1. Comment out <config.h> and <unistd.h> include macro.
	2. Include "libmtp.h" file.
	3. Line# 484 remove "inline" keyword from the function.

ptp.h
=====
	1. replace <iconv.h> with "iconv.h".

ptp-pack.c
==========
	1. replace <iconv.h> with "iconv.h".
	2. Include "stdint.h" and "ptp.h" files. Windows C doesn't have "stdint.h" file. I took this file from Cygwin.
	3. Remove "inline" keyword with all the functions in this file.

gphoto2-endian.h
================
	1. replace <arpa/inet.h> with <winsock.h> .


stdint.h
========
        1. MSVC doesn't have stdint.h file as part of its C. Get this file from someother source(e.g Cygwin in my case). And 	replace "long long" with "__int64".

byteswap.h
==========
        1. MSVC doesn't have this file too. I created this file myself by just copying bswap_16(), bswap_32() and bswap_64() 	macros from Linux /usr/include/bits/byteswap.h file. 



Libraries
=========
        You will need two libraries for windows "libusb" and "libiconv".
	1. You can download libusb from
 		http://libusb-win32.sourceforge.net/#downloads
	Download the Device driver version, unpack the archive and get libusb.lib and libusb0.dll files.
        Place libusb.lib in the libs folder and libusb0.dll file in the debug folder of MSVC project.
	Also get libusb.h file and add it to your project.

	2. You can Download libiconv-2.dll file from 
		http://sourceforge.net/project/showfiles.php?group_id=114505&package_id=147572&release_id=356820
	and libiconv.lib file from 
		http://gnuwin32.sourceforge.net/packages/libiconv.htm	
	Download Developer files and get libiconv.lib file.
	Place libiconv.lib in the libs folder and libicon-2.dll in debug folder of MSVC project. 
	Also get libiconv.h file and add it to your project.


	3. Goto Project->settings. In the "link" tab and "General" category, add "libusb.lib" and "libiconv.lib" to
	   "Object/library modules".

	4. Change Category to Input and add your libs folder Path to "Additional Library Path".

libusb filter Driver
====================
	You will need libusb driver for libusb to function properly. 
	Get libusb filter driver version from this link
		http://libusb-win32.sourceforge.net/#downloads
	Download the filter version and install it.