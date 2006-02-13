#include "common.h"

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  char *owner;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // Get owners name
  owner = LIBMTP_Get_Ownername(device);
  if (owner == NULL) {
    printf("Owner name: (NULL)\n");
  } else {
    printf("Owner name: %s\n", owner);
    free(owner);
  }

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

