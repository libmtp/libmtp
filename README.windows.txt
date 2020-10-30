This file was created by James Ravenscroft <ravenscroftj@gmail.com> as a direct revision of Farooq Zaman's work with LibMTP on Windows.

 CHANGELOG
----------------
14th January 2009: Created the first revision of this file taking information from the work of Farooq Zaman.
October 2020: updated with more current information

 1.0 Compilation of LibMTP on Windows
-------------------------------------
LibMTP currently compiles under Windows using MingW/MSys. The source relies upon the __WIN32__ macro which is defined by MinGW by default.

Libraries:
LibMTP currently depends on LibUSB and libiconv. There are currently projects that port both of these libraries to Windows. Binary files can be
obtained from:

LibUSB Win32 - https://libusb.info

LibIconv - https://github.com/pffang/libiconv-for-Windows

With both of these libraries extracted and placed in MinGW's search path, you can compile the library by opening the Msys prompt, navigating to
the path where the extracted LibMTP source files can be found and typing:

./configure
make all
make install

Alternatively, if you prefer to cross compile your application for Windows, http://mxe.cc contains the two library dependencies mentioned above
out of the box.

 2.0 LibUSB and Driver Issues for Windows
----------------------------------------------

Unfortunately, Windows does not have abstract USB support and depends upon specific drivers for each and every device you use. In the past, 
LibUSB-Win32 provided a solution to this problem.
These days https://zadig.akeo.ie/ appears to be a much easier solution that LibMTP can take advantage of.
Detailed instructions how to install those drivers are at the website given above.

