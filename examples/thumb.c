#include "common.h"
#include "string.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static void usage(void) {
  printf("Usage: thumb -i <fileid/trackid> <imagefile>\n");
  exit(0);
}

int main (int argc, char **argv) {
  int opt;
  extern int optind;
  extern char *optarg;
  LIBMTP_mtpdevice_t *device = NULL;
  int fd;
  uint32_t id = 0;
  uint64_t filesize;
  uint8_t *imagedata = NULL;
  char *path = NULL;
  struct stat statbuff;
  int ret;

  while ( (opt = getopt(argc, argv, "hi:")) != -1 ) {
    switch (opt) {
    case 'h':
      usage();
    case 'i':
      id = atoi(strdup(optarg));
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

  path = argv[0];

  if ( stat(path, &statbuff) == -1 ) {
    fprintf(stderr, "%s: ", path);
    perror("stat");
    exit(1);
  }
  filesize = (uint64_t) statbuff.st_size;
  imagedata = malloc(filesize * sizeof(uint16_t));

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
  
  LIBMTP_filesampledata_t *thumb = LIBMTP_new_filesampledata_t();

  int i;
  thumb->data = malloc(sizeof(uint16_t) * filesize);
  for (i = 0; i < filesize; i++) {
    thumb->data[i] = imagedata[i];
  }

  thumb->size = filesize;
  thumb->filetype = LIBMTP_FILETYPE_JPEG;
  
  ret = LIBMTP_Send_Representative_Sample(device,id,thumb);
  if (ret != 0) {
    printf("Couldn't send thumbnail\n");
    LIBMTP_Dump_Errorstack(device);
    LIBMTP_Clear_Errorstack(device);
  }

  free(imagedata);
  LIBMTP_destroy_filesampledata_t(thumb);

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}
