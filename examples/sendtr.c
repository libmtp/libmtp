#include "common.h"
#include <string.h>
#include <sys/stat.h>
#ifdef HAVE_LIBGEN_H
#include <libgen.h> /* basename() */
#endif

/*
 * This program is derived from the exact equivalent in libnjb.
 *
 * This is an improved commandline track transfer program
 * based on Enrique Jorreto Ledesma's work on the original program by 
 * Shaun Jackman and Linus Walleij.
 */

/* Function that compensate for missing libgen.h on Windows */
#ifndef HAVE_LIBGEN_H
static char *basename(char *in) {
  char *p;
  if (in == NULL)
    return NULL;
  p = in + strlen(in) - 1;
  while (*p != '\\' && *p != '/' && *p != ':')
    { p--; }
  return ++p;
}
#endif

static int progress (uint64_t const sent, uint64_t const total, uint8_t const * const buf, uint32_t const len, void const * const data)
{
  int percent = (sent*100)/total;
#ifdef __WIN32__
  printf("Progress: %I64u of %I64u (%d%%)\r", sent, total, percent);
#else
  printf("Progress: %llu of %llu (%d%%)\r", sent, total, percent);
#endif
  fflush(stdout);
  return 0;
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

static void usage(void)
{
  fprintf(stderr, "usage: sendtr [ -D debuglvl ] [ -q ] -t <title> -a <artist> -l <album> -c <codec>\n");
  fprintf(stderr, "       -g <genre> -n <track number> -d <duration in seconds> <path>\n");
  fprintf(stderr, "(-q means the program will not ask for missing information.)\n");
  exit(1);
}

int main(int argc, char **argv)
{
  int opt;
  extern int optind;
  extern char *optarg;
  char *path, *filename;
  char artist[80], title[80], genre[80], codec[80], album[80];
  char num[80];
  char *partist = NULL;
  char *ptitle = NULL;
  char *pgenre = NULL;
  char *pcodec = NULL;
  char *palbum = NULL;
  uint16_t tracknum = 0;
  uint16_t length = 0;
  uint16_t year = 0;
  uint16_t quiet = 0;
  uint64_t filesize;
  struct stat sb;
  char *lang;
  LIBMTP_mtpdevice_t *device;
  LIBMTP_track_t *trackmeta;
  int ret;
  
  while ( (opt = getopt(argc, argv, "qD:t:a:l:c:g:n:d:")) != -1 ) {
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
      pcodec = strdup(optarg); // XXX DSM check for MP3, WAV or WMA
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
    case 'q':
      quiet = 1;
      break;
    default:
      usage();
    }
  }
  argc-= optind;
  argv+= optind;
  
  if ( argc != 1 ) {
    printf("You need to pass a filename.\n");
    usage();
  }
  
  /*
   * Check environment variables $LANG and $LC_CTYPE
   * to see if we want to support UTF-8 unicode
   */
  lang = getenv("LANG");
  if (lang != NULL) {
    if (strlen(lang) > 5) {
      if (!strcmp(&lang[strlen(lang)-5], "UTF-8")) {
	printf("Your system does not appear to have UTF-8 enabled ($LANG=\"%s\")\n", lang);
	printf("If you want to have support for diacritics and Unicode characters,\n");
	printf("please switch your locale to an UTF-8 locale, e.g. \"en_US.UTF-8\".\n");
      }
    }
  }
  
  path = argv[0];

  filename = basename(path);
  if (filename == NULL) {
    printf("Error: filename could not be based.\n");
    exit(1);
  }
  if ( stat(path, &sb) == -1 ) {
    fprintf(stderr, "%s: ", path);
    perror("stat");
    exit(1);
  }
  filesize = (uint64_t) sb.st_size;

  /* Ask for missing parameters if not quiet */
  if (!quiet) {
    if (pcodec == NULL) {
      if ( (pcodec = prompt("Codec", codec, 80, 1)) == NULL ) {
	printf("A codec name is required.\n");
	usage();
      }
    }
    if (!strlen(pcodec)) {
      printf("A codec name is required.\n");
      usage();
    }
    
    if (ptitle == NULL) {
      if ( (ptitle = prompt("Title", title, 80, 0)) == NULL )
	usage();
    }
    if (!strlen(ptitle))
      ptitle = NULL;
    
    
    if (palbum == NULL) {
      if ( (palbum = prompt("Album", album, 80, 0)) == NULL )
	usage();
    }
    if (!strlen(palbum))
      palbum = NULL;
    
    if (partist == NULL) {
    if ( (partist = prompt("Artist", artist, 80, 0)) == NULL )
      usage();
    }
    if (!strlen(partist))
      partist = NULL;
    
    if (pgenre == NULL) {
      if ( (pgenre = prompt("Genre", genre, 80, 0)) == NULL )
	usage();
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
  }
  
  LIBMTP_Init();
  trackmeta = LIBMTP_new_track_t();
    
  printf("Sending track:\n");
  printf("Codec:     %s\n", pcodec);
  
  if (!strcmp(pcodec,"MP3") || !strcmp(pcodec,"mp3")) {
    trackmeta->codec = LIBMTP_CODEC_MP3;
  }  else if (!strcmp(pcodec,"WAV") || !strcmp(pcodec,"wav")) {
    trackmeta->codec = LIBMTP_CODEC_WAV;
  } else if (!strcmp(pcodec,"WMA") || !strcmp(pcodec,"wma") ||
	     !strcmp(pcodec,"ASF") || !strcmp(pcodec,"asf")) {
    trackmeta->codec = LIBMTP_CODEC_WMA;
  } else {
    printf("Not a valid codec: \"%s\"\n", pcodec);
    exit(1);
  }  
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
    
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No MTP device found.\n");
    LIBMTP_destroy_track_t(trackmeta);
    exit(1);
  }
  
  printf("Sending track...\n");
  ret = LIBMTP_Send_Track_From_File(device, path, trackmeta, progress, NULL);
  
  LIBMTP_Release_Device(device);
  
  printf("New track ID: %d\n", trackmeta->item_id);

  LIBMTP_destroy_track_t(trackmeta);
  
  printf("OK.\n");

  return 0;
}
