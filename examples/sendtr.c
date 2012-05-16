/**
 * \file sendtr.c
 * Example program to send a music track to a device.
 * This program is derived from the exact equivalent in libnjb.
 * based on Enrique Jorreto Ledesma's work on the original program by
 * Shaun Jackman and Linus Walleij.
 *
 * Copyright (C) 2003-2010 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2003-2005 Shaun Jackman
 * Copyright (C) 2003-2005 Enrique Jorrete Ledesma
 * Copyright (C) 2006 Chris A. Debenham <chris@adebenham.com>
 * Copyright (C) 2008 Nicolas Pennequin <nicolas.pennequin@free.fr>
 * Copyright (C) 2008 Joseph Nahmias <joe@nahmias.net>
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

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif

#include "config.h"
#include "common.h"
#include "util.h"
#include "connect.h"
#include "libmtp.h"
#include "pathutils.h"

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

void sendtrack_usage (void)
{
  fprintf(stderr, "usage: sendtr [ -D debuglvl ] [ -q ]\n");
  fprintf(stderr, "-t <title> -a <artist> -A <Album artist> -w <writer or composer>\n");
  fprintf(stderr, "    -l <album> -c <codec> -g <genre> -n <track number> -y <year>\n");
  fprintf(stderr, "       -d <duration in seconds> -s <storage_id> <local path> <remote path>\n");
  fprintf(stderr, "(-q means the program will not ask for missing information.)\n");
}

static char *prompt (const char *prompt, char *buffer, size_t bufsz, int required)
{
  char *cp, *bp;

  while (1) {
    fprintf(stdout, "%s> ", prompt);
    if ( fgets(buffer, bufsz, stdin) == NULL ) {
      if (ferror(stdin)) {
	perror("fgets");
      } else {
	fprintf(stderr, "EOF on stdin\n");
      }
      return NULL;
    }

    cp = strrchr(buffer, '\n');
    if ( cp != NULL ) *cp = '\0';

    bp = buffer;
    while ( bp != cp ) {
      if ( *bp != ' ' && *bp != '\t' ) return bp;
      bp++;
    }

    if (! required) return bp;
  }
}

static int add_track_to_album(LIBMTP_album_t *albuminfo, LIBMTP_track_t *trackmeta)
{
  LIBMTP_album_t *album;
  LIBMTP_album_t *album_orig;
  LIBMTP_album_t *found_album = NULL;
  int ret;

  /* Look for the album */
  album = LIBMTP_Get_Album_List(device);
  album_orig = album;
  while(album != NULL) {
    if ((album->name != NULL &&
	album->artist != NULL &&
	!strcmp(album->name, albuminfo->name) &&
	!strcmp(album->artist, albuminfo->artist)) ||
	  (album->name != NULL &&
	album->composer != NULL &&
	!strcmp(album->name, albuminfo->name) &&
	!strcmp(album->composer, albuminfo->composer))) {
      /* Disconnect this album for later use */
      found_album = album;
      album = album->next;
      found_album->next = NULL;
    } else {
      album = album->next;
    }
  }

  if (found_album == NULL) {
    printf("Could not find Album. Retrying with only Album name\n");
    album = album_orig;
    while(album != NULL) {
      if ((album->name != NULL) &&
          !strcmp(album->name, albuminfo->name) ){
        /* Disconnect this album for later use */
        found_album = album;
        album = album->next;
        found_album->next = NULL;
      } else {
        album = album->next;
      }
    }
  }

  if (found_album != NULL) {
    uint32_t *tracks;

    tracks = (uint32_t *)malloc((found_album->no_tracks+1) * sizeof(uint32_t));
    printf("Album \"%s\" found: updating...\n", found_album->name);
    if (!tracks) {
      printf("failed malloc in add_track_to_album()\n");
      return 1;
    }
    found_album->no_tracks++;
    if (found_album->tracks != NULL) {
      memcpy(tracks, found_album->tracks, found_album->no_tracks * sizeof(uint32_t));
      free(found_album->tracks);
    }
    tracks[found_album->no_tracks-1] = trackmeta->item_id;
    found_album->tracks = tracks;
    ret = LIBMTP_Update_Album(device, found_album);
  } else {
    uint32_t *trackid;

    trackid = (uint32_t *)malloc(sizeof(uint32_t));
    *trackid = trackmeta->item_id;
    albuminfo->tracks = trackid;
    albuminfo->no_tracks = 1;
    albuminfo->storage_id = trackmeta->storage_id;
    printf("Album doesn't exist: creating...\n");
    ret = LIBMTP_Create_New_Album(device, albuminfo);
    /* albuminfo will be destroyed later by caller */
  }

  /* Delete the earlier retrieved Album list */
  album=album_orig;
  while(album!=NULL){
    LIBMTP_album_t *tmp;

    tmp = album;
    album = album->next;
    LIBMTP_destroy_album_t(tmp);
  }

  if (ret != 0) {
    printf("Error creating or updating album.\n");
    printf("(This could be due to that your device does not support albums.)\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  } else {
    printf("success!\n");
  }
  return ret;
}

int sendtrack_function(char * from_path, char * to_path, char *partist, char *palbumartist, char *ptitle, char *pgenre, char *palbum, char *pcomposer, uint16_t tracknum, uint16_t length, uint16_t year, uint32_t storageid, uint16_t quiet)
{
  char *filename, *parent;
  char artist[80], albumartist[80], title[80], genre[80], album[80], composer[80];
  char num[80];
  uint64_t filesize;
  uint32_t parent_id = 0;
  struct stat sb;
  LIBMTP_track_t *trackmeta;
  LIBMTP_album_t *albuminfo;
  int ret;

  printf("Sending track %s to %s\n", from_path, to_path);

  trackmeta = LIBMTP_new_track_t();
  albuminfo = LIBMTP_new_album_t();

  parent = dirname(strdup(to_path));
  filename = basename(strdup(to_path));
  parent_id = parse_path (parent,files,folders);
  if (parent_id == -1) {
    printf("Parent folder could not be found, skipping\n");
    return 1;
  }

  if (stat(from_path, &sb) == -1) {
    fprintf(stderr, "%s: ", from_path);
    perror("stat");
    return 1;
  }

  if (!S_ISREG(sb.st_mode))
    return 0;

  filesize = sb.st_size;
  trackmeta->filetype = find_filetype (from_path);
  if (!LIBMTP_FILETYPE_IS_TRACK(trackmeta->filetype)) {
    printf("Not a valid track codec: \"%s\"\n", LIBMTP_Get_Filetype_Description(trackmeta->filetype));
    return 1;
  }

  if ((ptitle == NULL) && (quiet == 0)) {
    if ( (ptitle = prompt("Title", title, 80, 0)) != NULL )
      if (!strlen(ptitle)) ptitle = NULL;
  }

  if ((palbum == NULL) && (quiet == 0)) {
    if ( (palbum = prompt("Album", album, 80, 0)) != NULL )
      if (!strlen(palbum)) palbum = NULL;
  }

  if ((palbumartist == NULL) && (quiet == 0)) {
    if ( (palbumartist = prompt("Album artist", albumartist, 80, 0)) != NULL )
      if (!strlen(palbumartist)) palbumartist = NULL;
  }

  if ((partist == NULL) && (quiet == 0)) {
    if ( (partist = prompt("Artist", artist, 80, 0)) != NULL )
      if (!strlen(partist)) partist = NULL;
  }

  if ((pcomposer == NULL) && (quiet == 0)) {
    if ( (pcomposer = prompt("Writer or Composer", composer, 80, 0)) != NULL )
      if (!strlen(pcomposer)) pcomposer = NULL;
  }

  if ((pgenre == NULL) && (quiet == 0)) {
    if ( (pgenre = prompt("Genre", genre, 80, 0)) != NULL )
      if (!strlen(pgenre)) pgenre = NULL;
  }

  if ((tracknum == 0) && (quiet == 0)) {
    char *pnum;
    if ( (pnum = prompt("Track number", num, 80, 0)) == NULL )
      tracknum = 0;
    else
      tracknum = strtoul(pnum, 0, 10);
  }

  if ((year == 0) && (quiet == 0)) {
    char *pnum;
    if ( (pnum = prompt("Year", num, 80, 0)) == NULL )
      year = 0;
    else
      year = strtoul(pnum, 0, 10);
  }

  if ((length == 0) && (quiet == 0)) {
    char *pnum;
    if ( (pnum = prompt("Length", num, 80, 0)) == NULL )
      length = 0;
    else
      length = strtoul(pnum, 0, 10);
  }

  printf("Sending track:\n");
  printf("Codec:     %s\n", LIBMTP_Get_Filetype_Description(trackmeta->filetype));
  if (ptitle) {
    printf("Title:     %s\n", ptitle);
    trackmeta->title = strdup(ptitle);
  }
  if (palbum) {
    printf("Album:     %s\n", palbum);
    trackmeta->album = strdup(palbum);
    albuminfo->name = strdup(palbum);
  }
  if (palbumartist) {
    printf("Album artist:    %s\n", palbumartist);
    albuminfo->artist = strdup(palbumartist);
  }
  if (partist) {
    printf("Artist:    %s\n", partist);
    trackmeta->artist = strdup(partist);
    if (palbumartist == NULL)
      albuminfo->artist = strdup(partist);
  }
  if (pcomposer) {
    printf("Writer or Composer:    %s\n", pcomposer);
    trackmeta->composer = strdup(pcomposer);
    albuminfo->composer = strdup(pcomposer);
  }
  if (pgenre) {
    printf("Genre:     %s\n", pgenre);
    trackmeta->genre = strdup(pgenre);
    albuminfo->genre = strdup(pgenre);
  }
  if (year > 0) {
    char tmp[80];
    printf("Year:      %d\n", year);
    snprintf(tmp, sizeof(tmp)-1, "%4d0101T0000.0", year);
    tmp[sizeof(tmp)-1] = '\0';
    trackmeta->date = strdup(tmp);
  }
  if (tracknum > 0) {
    printf("Track no:  %d\n", tracknum);
    trackmeta->tracknumber = tracknum;
  }
  if (length > 0) {
    printf("Length:    %d\n", length);
    // Multiply by 1000 since this is in milliseconds
    trackmeta->duration = length * 1000;
  }
  // We should always have this
  if (filename != NULL) {
    trackmeta->filename = strdup(filename);
  }
  trackmeta->filesize = filesize;
  trackmeta->parent_id = parent_id;
  {
    int rc;
    char *desc = NULL;
    LIBMTP_devicestorage_t *pds = NULL;

    if (0 != (rc=LIBMTP_Get_Storage(device, LIBMTP_STORAGE_SORTBY_NOTSORTED))) {
      perror("LIBMTP_Get_Storage()");
      exit(-1);
    }
    for (pds = device->storage; pds != NULL; pds = pds->next) {
      if (pds->id == storageid) {
	desc = strdup(pds->StorageDescription);
	break;
      }
    }
    if (NULL != desc) {
      printf("Storage ID: %s (%u)\n", desc, storageid);
      free(desc);
    } else
      printf("Storage ID: %u\n", storageid);
    trackmeta->storage_id = storageid;
  }

  printf("Sending track...\n");
  ret = LIBMTP_Send_Track_From_File(device, from_path, trackmeta, progress, NULL);
  printf("\n");
  if (ret != 0) {
    printf("Error sending track.\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
    ret = 1;
  } else {
    printf("New track ID: %d\n", trackmeta->item_id);
  }

  /* Add here add to album call */
  if (palbum)
    ret = add_track_to_album(albuminfo, trackmeta);

  LIBMTP_destroy_album_t(albuminfo);
  LIBMTP_destroy_track_t(trackmeta);

  return ret;
}

int sendtrack_command (int argc, char **argv) {
  int opt;
  extern int optind;
  extern char *optarg;
  char *partist = NULL;
  char *palbumartist = NULL;
  char *pcomposer = NULL;
  char *ptitle = NULL;
  char *pgenre = NULL;
  char *pcodec = NULL;
  char *palbum = NULL;
  uint16_t tracknum = 0;
  uint16_t length = 0;
  uint16_t year = 0;
  uint16_t quiet = 0;
  uint32_t storageid = 0;
  while ( (opt = getopt(argc, argv, "qD:t:a:A:w:l:c:g:n:d:y:s:")) != -1 ) {
    switch (opt) {
    case 't':
      ptitle = strdup(optarg);
      break;
    case 'a':
      partist = strdup(optarg);
      break;
    case 'A':
      palbumartist = strdup(optarg);
      break;
    case 'w':
      pcomposer = strdup(optarg);
      break;
    case 'l':
      palbum = strdup(optarg);
      break;
    case 'c':
      pcodec = strdup(optarg); // FIXME: DSM check for MP3, WAV or WMA
      break;
    case 'g':
      pgenre = strdup(optarg);
      break;
    case 'n':
      tracknum = atoi(optarg);
      break;
    case 's':
      storageid = (uint32_t) strtoul(optarg, NULL, 0);
      break;
    case 'd':
      length = atoi(optarg);
      break;
    case 'y':
      year = atoi(optarg);
      break;
    case 'q':
      quiet = 1;
      break;
    default:
      sendtrack_usage();
    }
  }
  argc -= optind;
  argv += optind;

  if ( argc != 2 ) {
    printf("You need to pass a filename and destination.\n");
    sendtrack_usage();
    return 0;
  }

  checklang();

  printf("%s,%s,%s,%s,%s,%s,%s,%s,%d%d,%d,%u,%d\n",argv[0],argv[1],partist,palbumartist,ptitle,pgenre,palbum,pcomposer,tracknum, length, year, storageid, quiet);
  return sendtrack_function(argv[0],argv[1],partist,palbumartist,ptitle,pgenre,palbum,pcomposer, tracknum, length, year, storageid, quiet);
}
