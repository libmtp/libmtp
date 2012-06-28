/**
 * \file mtp-probe.c
 * Program to probe newly connected device interfaces from
 * userspace to determine if they are MTP devices, used for
 * udev rules.
 *
 * Invoke the program from udev to check it for MTP signatures,
 * e.g.
 * ATTR{bDeviceClass}=="ff",
 * PROGRAM="<path>/mtp-probe /sys$env{DEVPATH} $attr{busnum} $attr{devnum}",
 * RESULT=="1", ENV{ID_MTP_DEVICE}="1", ENV{ID_MEDIA_PLAYER}="1",
 * SYMLINK+="libmtp-%k", MODE="666"
 *
 * Is you issue this before testing your /var/log/messages
 * will be more verbose:
 *
 * udevadm control --log-priority=debug
 *
 * Exits with status code 1 if the device is an MTP device,
 * else exits with 0.
 *
 * Copyright (C) 2011-2012 Linus Walleij <triad@df.lth.se>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __linux__
#error "This program should only be compiled for Linux!"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <libmtp.h>

int main (int argc, char **argv)
{
  char *fname;
  int busno;
  int devno;
  int ret;

  if (argc < 4) {
    syslog(LOG_INFO, "need device path, busnumber, device number as argument\n");
    printf("0");
    exit(0);
  }

  fname = argv[1];
  busno = atoi(argv[2]);
  devno = atoi(argv[3]);

  syslog(LOG_INFO, "checking bus %d, device %d: \"%s\"\n", busno, devno, fname);

  ret = LIBMTP_Check_Specific_Device(busno, devno);
  if (ret) {
    syslog(LOG_INFO, "bus: %d, device: %d was an MTP device\n", busno, devno);
    printf("1");
  } else {
    syslog(LOG_INFO, "bus: %d, device: %d was not an MTP device\n", busno, devno);
    printf("0");
  }

  exit(0);
}
