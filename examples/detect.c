#include "common.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_file_t *files;
  uint32_t xmlfileid = 0;
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

  // Try to get device info XML file...
  files = LIBMTP_Get_Filelisting(device);
  if (files != NULL) {
    LIBMTP_file_t *file, *tmp;
    file = files;
    while (file != NULL) {
      if (!strcmp(file->filename, "WMPInfo.xml")) {
	xmlfileid = file->item_id;
      }
      tmp = file;
      file = file->next;
      LIBMTP_destroy_file_t(tmp);
    }
  }
  if (xmlfileid != 0) {
    char tmpfilename[] = "WMPInfo.xml.XXXXXX";
    int tmpfile = mkstemp(tmpfilename);
    if (tmpfile != -1) {
      int ret = LIBMTP_Get_Track_To_File_Descriptor(device, xmlfileid, tmpfile, NULL, NULL);
      if (ret == 0) {
	uint8_t buf[2];
	int endianness = 0; // 0 = LE, 1 = BE

	printf("\nDevice description WMPInfo.xml file:\n");
	lseek(tmpfile, 0, SEEK_SET);
	while (read(tmpfile, (void*) buf, 2) == 2) {
	  if (buf[0] == 0xFF && buf[1] == 0xFE) {
	    endianness = 0;
	  } else if (buf[0] == 0xFE && buf[1] == 0xff) {
	    endianness = 1;
	  } else {
	    uint16_t tmp;

	    if (endianness == 0) {
	      tmp = buf[1] << 8 | buf[0];
	    } else {
	      tmp = buf[0] << 8 | buf[1];
	    }
	    // Fix this some day.
	    printf("%c", (uint8_t) tmp);
	  }	  
	}
	printf("\n");
      }
      close(tmpfile);
    }
  }

  // King Fisher of Triad rocks your world!
  LIBMTP_Release_Device(device);
  printf("OK.\n");
  exit (0);
}
