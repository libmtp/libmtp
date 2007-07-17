/** 
 * \file reset.c
 * Example program that resets the device.
 *
 * Copyright (C) 2007 Linus Walleij <triad@df.lth.se>
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

/**
 * Returns 0 if OK (yes), 1 if not OK (no)
 */
static int prompt()
{
  char buff[2];
  
  while (1) {
    fprintf(stdout, "> ");
    if ( fgets(buff, sizeof(buff), stdin) == NULL ) {
      if (ferror(stdin)) {
        fprintf(stderr, "File error on stdin\n");
      } else {
        fprintf(stderr, "EOF on stdin\n");
      }
      return 1;
    }
    if (buff[0] == 'y') {
      return 0;
    } else if (buff[0] == 'n') {
      return 1;
    }
  }
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  int ret;

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  printf("I will now reset your device. This means that\n");
  printf("the device may go inactive immediately and may report errors.\n");
  printf("Continue? (y/n)\n");
  if (prompt() == 0) {
    ret = LIBMTP_Reset_Device(device);
  } else {
    printf("Aborted.\n");
    ret = 0;
  }

  if ( ret != 0 ) {
    printf("Failed to reset device.\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
    LIBMTP_Release_Device(device);
    return 1;
  }

  // It is not possible to release the device after successful reset!
  // LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}
