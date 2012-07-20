/** 
 * \file albumart.c
 * Example program to send album art.
 *
 * Copyright (C) 2006 Andy Kelk <andy@mopoke.co.uk>
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
#include "config.h"
#include "common.h"
#include "string.h"
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

static void usage(void) {
  printf("Usage: albumart -d -i <fileid/trackid> -n <albumname> -s <storage_id> -p <parent_id> <imagefile>\n");
  exit(0);
}

int main (int argc, char **argv) {
  int opt;
  extern int optind;
  extern char *optarg;
  LIBMTP_mtpdevice_t *device = NULL;
  int idcount = 0;
  int fd;
  uint32_t *ids = NULL;
  uint32_t *tmp = NULL;
  uint64_t filesize;
  char *imagedata = NULL;
  char *albumname = NULL;
  char *path = NULL;
  char *rest;
  struct stat statbuff;
  uint32_t storageid = 0;
  uint32_t parentid = 0;

  fprintf(stdout, "libmtp version: " LIBMTP_VERSION_STRING "\n\n");

  while ( (opt = getopt(argc, argv, "dhn:i:s:p:")) != -1 ) {
    switch (opt) {
    case 'h':
      usage();
    case 'd':
      LIBMTP_Set_Debug(LIBMTP_DEBUG_PTP | LIBMTP_DEBUG_DATA);
      break;
    case 'i':
      idcount++;
      if ((tmp = realloc(ids, sizeof(uint32_t) * (idcount))) == NULL) {
        printf("realloc failed\n");
        return 1;
      }
      ids = tmp;
      ids[(idcount-1)] = strtoul(optarg, &rest, 0);
      break;
    case 'n':
      albumname = strdup(optarg);
      break;
    case 's':
      storageid = (uint32_t) strtoul(optarg, NULL, 0);
	  break;
    case 'p':
      parentid = (uint32_t) strtoul(optarg, NULL, 0);
	  break;
    default:
      usage();
    }
  }
  argc -= optind;
  argv += optind;

  if ( argc != 1 ) {
    printf("You need to pass a filename.\n");
    usage();
  }

  if ( albumname == NULL) {
    printf("You need to supply an album name.\n");
    usage();
  }

  if (idcount == 0) {
    printf("You need to supply one or more track IDs\n");
    usage();
  }

  path = argv[0];

  if ( stat(path, &statbuff) == -1 ) {
    fprintf(stderr, "%s: ", path);
    perror("stat");
    exit(1);
  }
  filesize = (uint64_t) statbuff.st_size;
  imagedata = malloc(filesize * sizeof(uint8_t));

#ifdef __WIN32__
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1) ) {
#else
  if ( (fd = open(path, O_RDONLY)) == -1) {
#endif
    printf("Couldn't open image file %s (%s)\n",path,strerror(errno));
    return 1;
  }
  else {
    read(fd, imagedata, filesize);
    close(fd);
  }

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  LIBMTP_filesampledata_t *albumart = LIBMTP_new_filesampledata_t();
  albumart->data = imagedata;
  albumart->size = filesize;
  albumart->filetype = LIBMTP_FILETYPE_JPEG;

  LIBMTP_album_t *album = LIBMTP_new_album_t();
  album->name = albumname;
  album->no_tracks = idcount;
  album->tracks = ids;
  album->parent_id = parentid;
  album->storage_id = storageid;
  int ret = LIBMTP_Create_New_Album(device,album);
  if (ret == 0) {
    ret = LIBMTP_Send_Representative_Sample(device,album->album_id, albumart);
    if (ret != 0) {
      printf("Couldn't send album art\n");
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
    }
  }
  else {
    printf("Couldn't create album object\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  }

  LIBMTP_destroy_filesampledata_t(albumart);
  LIBMTP_destroy_album_t(album);

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}

