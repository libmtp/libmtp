#include "common.h"

int main (int argc, char **argv)
{
  mtpdevice_t *device;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}

