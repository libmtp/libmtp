#include "common.h"

static int progress (u_int64_t sent, u_int64_t total, const char* buf, unsigned len, void *data)
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

static void usage (void)
{
  fprintf(stderr, "gettr <trackid> <filename>\n");
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  char *owner;
  u_int32_t id;
  char *endptr;
  char *file;

  // We need track ID and filename
  if ( argc != 2 ) {
    usage();
    return 1;
  }

  // Sanity check song ID
  id = strtoul(argv[0], &endptr, 10);
  if ( *endptr != 0 ) {
    fprintf(stderr, "illegal value %s\n", argv[0]);
    return 1;
  } else if ( ! id ) {
    fprintf(stderr, "bad song id %u\n", id);
    return 1;
  }

  // Filename, e.g. "foo.mp3"
  file = argv[1];
  printf("Getting track %d to local file %s\n", id, file);

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices. Connect/replug device and try again.\n");
    exit (0);
  }
  
  if (LIBMTP_Get_Track_To_File(device, id, file, progress, NULL) == -1 ) {
    printf("Error getting track from MTP device.\n");
  }
  
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

