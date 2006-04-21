#include "common.h"

static void usage (void)
{
  fprintf(stderr, "trexist <trackid>\n");
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  u_int32_t id;
  char *endptr;
  
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

