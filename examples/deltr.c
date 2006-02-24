#include "common.h"

static void usage(void)
{
  printf("Usage: deltr <trackid>\n");
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  u_int32_t id;
  char *endptr;
  int ret;

  // We need track ID
  if ( argc != 2 ) {
    usage();
    return 1;
  }

  // Sanity check song ID
  id = strtoul(argv[1], &endptr, 10);
  if ( *endptr != 0 ) {
    fprintf(stderr, "illegal value %s\n", argv[1]);
    usage();
    return 1;
  } else if ( ! id ) {
    fprintf(stderr, "bad song id %u\n", id);
    usage();
    return 1;
  }
  
  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  ret = LIBMTP_Delete_Track(device, id);

  if ( ret != 0 ) {
    printf("Failed to delete track.\n");
    return 1;
  }
  
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}

