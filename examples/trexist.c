/** 
 * \file trexist.c
 * Example program to check if a certain track exists on the device.
 *
 * Copyright (C) 2006 The libmtp development team. 
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
#include "common.h"
#include <stdlib.h>
#include <limits.h>

static void usage (void)
{
  fprintf(stderr, "trexist <trackid>\n");
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  uint32_t id;
  char *endptr;
  
  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  // We need track ID 
  if ( argc != 2 ) {
    usage();
    return 1;
  }

  // Sanity check song ID
  id = strtoul(argv[1], &endptr, 10);
  if ( *endptr != 0 ) {
    fprintf(stderr, "illegal value %s\n", argv[1]);
    return 1;
  } else if ( ! id ) {
    fprintf(stderr, "bad song id %u\n", id);
    return 1;
  }
  
  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices. Connect/replug device and try again.\n");
    exit (0);
  }
  
  printf("%s\n", LIBMTP_Track_Exists(device, id) ? "Yes" : "No");
  
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

