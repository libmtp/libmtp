#include "common.h"
#include <stdlib.h>

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  uint64_t totalbytes;
  uint64_t freebytes;
  char *storage_description;
  char *volume_label;
  char *owner;
  uint8_t maxbattlevel;
  uint8_t currbattlevel;
  int ret;

  LIBMTP_Init();
  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  // The owner name
  owner = LIBMTP_Get_Ownername(device);
  if (owner == NULL) {
    printf("    Owner name: (NULL)\n");
  } else {
    printf("    Owner name: %s\n", owner);
    free(owner);
  }

  // Some storage info
  ret = LIBMTP_Get_Storageinfo(device, &totalbytes, &freebytes, &storage_description, &volume_label);
  if (ret == 0) {
#ifdef __WIN32__
    printf("    Total bytes on device: %I64u (%I64u MB)\n",
	   totalbytes, totalbytes/(1024*1024));
    printf("    Free bytes on device: %I64u (%I64u MB)\n",
	   freebytes, freebytes/(1024*1024));
#else
    printf("    Total bytes on device: %llu (%llu MB)\n",
	   totalbytes, totalbytes/(1024*1024));
    printf("    Free bytes on device: %llu (%llu MB)\n",
	   freebytes, freebytes/(1024*1024));
#endif
    if (storage_description != NULL) {
      printf("    Storage description: \"%s\"\n", storage_description);
      free(storage_description);
    }
    if (volume_label != NULL) {
      printf("    Volume label: \"%s\"\n", volume_label);
      free(volume_label);
    }
  } else {
    printf("    Error getting disk info...\n");
  }

  // Some battery info
  ret = LIBMTP_Get_Batterylevel(device, &maxbattlevel, &currbattlevel);
  if (ret == 0) {
    printf("    Battery level %d of %d (%d%%)\n",currbattlevel, maxbattlevel, 
	   (currbattlevel/maxbattlevel * 100));
  } else {
    printf("    Error getting battery info...\n");
  }

  // King Fisher of Triad rocks your world!
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}
