This file was created by James Ravenscroft <ravenscroftj@gmail.com> as a direct revision of Farooq Zaman's work with LibMTP on Windows.

 CHANGELOG
----------------
14th January 2009: Created the first revision of this file taking information from the work of Farooq Zaman.

 1.0 Compilation of LibMTP on Windows 2000/XP/NT 
-----------------------------------------------------------
LibMTP currently compiles under Windows using MingW/MSys. The source relies upon the __WIN32__ macro which is defined by MinGW by default.

Libraries:
LibMTP currently depends on LibUSB and libiconv. There are currently projects that port both of these libraries to Windows. Binary files can be
obtained from:

LibUSB Win32 - http://libusb-win32.sourceforge.net/

LibIconv - http://gnuwin32.sourceforge.net/packages/libiconv.htm

With both of these libraries extracted and placed in MinGW's search path, you can compile the library by opening the Msys prompt, navigating to
the path where the extracted LibMTP source files can be found and typing:

./configure
make all
make install



 2.0 LibUSB and Driver Issues for Windows
----------------------------------------------

Unfortunately, Windows does not have abstract USB support and depends upon specific drivers for each and every device you use. Fortunately, 
LibUSB-Win32 provide a solution to this problem. LibMTP takes advantage of the LibUSB-Win32 Device Driver package.

1. Download the latest device driver binary package (libusb-win32-device-bin-x.x.x.x.tar.gz) from http://sourceforge.net/project/showfiles.php?group_id=78138
2. Upon extraction, plug in your music device and run bin/inf-wizard.exe. Selecting your device and saving the inf file in the project root directory.
3. Copy the files "bin/libusb0.dll" and "libusb0.sys" or "libusb0_x64.dll" and "libusb0_x64.sys" for 32-bit or 64-bit operating systems respectively.
4. Goto Start -> Run, type "devmgmt.msc" and press "ok".
5. Select your music device from the list and click Action -> Update Driver, Choose "No, not this time" if prompted to connect to microsoft.
6. Choose "Install from a list or specific location".
7.  Choose "Don't search, I will choose the driver to install
8. Click the "Have Disk..." button in the bottom right corner of the prompt
9. Browse to your .inf file and select it. Press Ok 
10. The name of your music device should appear in the prompt, click it and click "Next>" (Ignore any prompts about Driver Signing, continuing 
installation of the selected driver).
11. Click finish to end the driver install process.

To get your old driver back:

1. Goto Start -> Run, type "devmgmt.msc" and press "ok".
2. Select your music device, right click on it and click "Properties"
3. Go to the "Driver" pane and select "Roll Back Driver".

 3.0 
----------------------------------------------
