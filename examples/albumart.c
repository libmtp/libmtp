#include "common.h"
#include "string.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static void usage(void) {
  printf("Usage: albumart -i <fileid/trackid> -n <albumname> <imagefile>\n");
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
  struct stat statbuff;

  while ( (opt = getopt(argc, argv, "hn:i:")) != -1 ) {
    switch (opt) {
    case 'h':
      usage();
    case 'i':
      idcount++;
      if ((tmp = realloc(ids, sizeof(uint32_t) * (idcount))) == NULL) {
        printf("realloc failed\n");
        return 1;
      }
      ids = tmp;
      ids[(idcount-1)] = atoi(strdup(optarg));
      break;
    case 'n':
      albumname = strdup(optarg);
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
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1 ) {
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

  LIBMTP_album_t *album = LIBMTP_new_album_t();
  album->name = albumname;
  album->no_tracks = idcount;
  album->tracks = ids;
  int ret = LIBMTP_Create_New_Album(device,album,0);
  if (ret == 0) {
    ret = LIBMTP_Send_Album_Art(device,album->album_id,(uint8_t *) imagedata,filesize);
    if (ret != 0) {
      printf("Couldn't send album art\n");
    }
  }
  else {
    printf("Couldn't create album object\n");
  }

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}

