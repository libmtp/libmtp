#include "common.h"

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

  ret = LIBMTP_Format_Storage(device);

  if ( ret != 0 ) {
    LIBMTP_Release_Device(device);
    printf("Failed to format device.\n");
    return 1;
  }

  LIBMTP_Release_Device(device);
  printf("OK.\n");
  return 0;
}
