/**
 * \file mtp-probe.c
 * Program to probe newly connected device interfaces from
 * userspace to determine if they are MTP devices, used for
 * udev rules.
 *
 * Invoke the program on a sysfs device entry to check it
 * for MTP signatures, e.g.
 * mtp-probe /sys/devices/pci0000:00/0000:00:1d.7/usb1/1-8
 *
 * Exits with status code 0 if the device is an MTP device,
 * else exits with 1.
 *
 * Copyright (C) 2010 Linus Walleij <triad@df.lth.se>
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
#include <libmtp.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main (int argc, char **argv)
{
  char *fname;

  if (argc < 2) {
    printf("No device file to check\n");
    exit(1);
  }
  fname = argv[1];
  printf("Checking: \"%s\"...\n", fname);
  exit (1);
}
