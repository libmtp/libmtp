/** 
 * \file sendtr.c
 * Example program to send a music track to a device.
 * This program is derived from the exact equivalent in libnjb.
 * based on Enrique Jorreto Ledesma's work on the original program by 
 * Shaun Jackman and Linus Walleij.
 *
 * Copyright (C) 2003-2007 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2003-2005 Shaun Jackman
 * Copyright (C) 2003-2005 Enrique Jorrete Ledesma
 * Copyright (C) 2006 Chris A. Debenham <chris@adebenham.com>
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
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "common.h"
#include "libmtp.h"
#include "pathutils.h"

extern LIBMTP_folder_t *folders;
extern LIBMTP_file_t *files;
extern LIBMTP_mtpdevice_t *device;

int sendtrack_function (char *, char *, char *, char *, char *, char *, uint16_t, uint16_t, uint16_t);
void sendtrack_command (int, char **);
void sendtrack_usage (void);

void sendtrack_usage (void)
{
  fprintf(stderr, "usage: sendtr [ -D debuglvl ] [ -q ] -t <title> -a <artist> -l <album>\n");
  fprintf(stderr, "       -c <codec> -g <genre> -n <track number> -y <year> \n");
  fprintf(stderr, "       -d <duration in seconds> <local path> <remote path>\n");
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

int sendtrack_function(char * from_path, char * to_path, char *partist, char *ptitle, char *pgenre, char *palbum, uint16_t tracknum, uint16_t length, uint16_t year)
{
  printf("Sending track %s to %s\n",from_path,to_path);
  char *filename, *parent;
  char artist[80], title[80], genre[80], album[80];
  char num[80];
  uint64_t filesize;
  uint32_t parent_id = 0;
#ifdef __USE_LARGEFILE64
  struct stat64 sb;
#else
  struct stat sb;
#endif
  LIBMTP_track_t *trackmeta;
  trackmeta = LIBMTP_new_track_t();

  parent = dirname(to_path);
  filename = basename(to_path);
  parent_id = parse_path (parent,files,folders);
  if (parent_id == -1) {
    printf("Parent folder could not be found, skipping\n");
    return 1;
  }

#ifdef __USE_LARGEFILE64
  if ( stat64(from_path, &sb) == -1 ) {
#else
  if ( stat(from_path, &sb) == -1 ) {
#endif
    fprintf(stderr, "%s: ", from_path);
    perror("stat");
    return 1;
  } else if (S_ISREG (sb.st_mode)) {
#ifdef __USE_LARGEFILE64
    filesize = sb.st_size;
#else
    filesize = (uint64_t) sb.st_size;
#endif
    trackmeta->filetype = find_filetype (from_path);
    if ((trackmeta->filetype != LIBMTP_FILETYPE_MP3) 
	&& (trackmeta->filetype != LIBMTP_FILETYPE_WAV) 
	&& (trackmeta->filetype != LIBMTP_FILETYPE_OGG)
	&& (trackmeta->filetype != LIBMTP_FILETYPE_MP4) 
	&& (trackmeta->filetype != LIBMTP_FILETYPE_AAC) 
	&& (trackmeta->filetype != LIBMTP_FILETYPE_M4A) 
	&& (trackmeta->filetype != LIBMTP_FILETYPE_FLAC) 
	&& (trackmeta->filetype != LIBMTP_FILETYPE_WMA)) {
      printf("Not a valid codec: \"%s\"\n", LIBMTP_Get_Filetype_Description(trackmeta->filetype));
      printf("Supported formats: MP3, WAV, OGG, MP4, AAC, M4A, FLAC, WMA\n");
      return 1;
    }

    int ret;

    if (ptitle == NULL) {
      ptitle = prompt("Title", title, 80, 0);
    }
    if (!strlen(ptitle))
      ptitle = NULL;


    if (palbum == NULL) {
      palbum = prompt("Album", album, 80, 0);
    }
    if (!strlen(palbum))
      palbum = NULL;

    if (partist == NULL) {
      partist = prompt("Artist", artist, 80, 0);
    }
    if (!strlen(partist))
      partist = NULL;

    if (pgenre == NULL) {
      pgenre = prompt("Genre", genre, 80, 0);
    }
    if (!strlen(pgenre))
      pgenre = NULL;

    if (tracknum == 0) {
      char *pnum;
      if ( (pnum = prompt("Track number", num, 80, 0)) == NULL )
      tracknum = 0;
      if ( strlen(pnum) ) {
        tracknum = strtoul(pnum, 0, 10);
      } else {
        tracknum = 0;
      }
    }

    if (year == 0) {
      char *pnum;
      if ( (pnum = prompt("Year", num, 80, 0)) == NULL )
        year = 0;
      if ( strlen(pnum) ) {
        year = strtoul(pnum, 0, 10);
      } else {
        year = 0;
      }
    }

    if (length == 0) {
      char *pnum;
      if ( (pnum = prompt("Length", num, 80, 0)) == NULL )
        length = 0;
      if ( strlen(pnum) ) {
        length = strtoul(pnum, 0, 10);
      } else {
        length = 0;
      }
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
    }
    if (partist) {
      printf("Artist:    %s\n", partist);
      trackmeta->artist = strdup(partist);
    }
    if (pgenre) {
      printf("Genre:     %s\n", pgenre);
      trackmeta->genre = strdup(pgenre);
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
      
    printf("Sending track...\n");
    ret = LIBMTP_Send_Track_From_File(device, from_path, trackmeta, progress, NULL, parent_id);
    printf("\n");
    if (ret != 0) {
      printf("Error sending track.\n");
      LIBMTP_Dump_Errorstack(device);
      LIBMTP_Clear_Errorstack(device);
    } else {
      printf("New track ID: %d\n", trackmeta->item_id);
    }

    LIBMTP_destroy_track_t(trackmeta);
  
    return 0;
  }
  return 0;
}

void sendtrack_command (int argc, char **argv) {
  int opt;
  extern int optind;
  extern char *optarg;
  char *partist = NULL;
  char *ptitle = NULL;
  char *pgenre = NULL;
  char *pcodec = NULL;
  char *palbum = NULL;
  uint16_t tracknum = 0;
  uint16_t length = 0;
  uint16_t year = 0;
  uint16_t quiet = 0;
  char *lang;
  while ( (opt = getopt(argc, argv, "qD:t:a:l:c:g:n:d:y:")) != -1 ) {
    switch (opt) {
    case 't':
      ptitle = strdup(optarg);
      break;
    case 'a':
      partist = strdup(optarg);
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
  }
  /*
   * Check environment variables $LANG and $LC_CTYPE
   * to see if we want to support UTF-8 unicode
   */
  lang = getenv("LANG");
  if (lang != NULL) {
    if (strlen(lang) > 5) {
      char *langsuff = &lang[strlen(lang)-5];
      if (strcmp(langsuff, "UTF-8")) {
	printf("Your system does not appear to have UTF-8 enabled ($LANG=\"%s\")\n", lang);
	printf("If you want to have support for diacritics and Unicode characters,\n");
	printf("please switch your locale to an UTF-8 locale, e.g. \"en_US.UTF-8\".\n");
      }
    }
  }
  
  printf("%s,%s,%s,%s,%s,%s,%d%d,%d\n",argv[0],argv[1],partist,ptitle,pgenre,palbum,tracknum, length, year);
  sendtrack_function(argv[0],argv[1],partist,ptitle,pgenre,palbum, tracknum, length, year);
}
