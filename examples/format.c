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

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    return 0;
  }

  printf("I will now format your device. This means that\n");
  printf("all content (and licenses) will be lost forever.\n");
  printf("you will not be able to undo this operation.\n");
  printf("Continue? (y/n)\n");
  if (prompt() == 0) {
    // This will just format the first storage.
    ret = LIBMTP_Format_Storage(device, device->storage);
  } else {
    printf("Aborted.\n");
    ret = 0;
  }

  if ( ret != 0 ) {
    LIBMTP_Release_Device(device);
    printf("Failed to format device.\n");
    return 1;
  }

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}
