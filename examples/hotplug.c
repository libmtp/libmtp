#include "common.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void usage(void)
{
  fprintf(stderr, "usage: hotplug [-u -a\"ACTION\"]\n");
  fprintf(stderr, "       -d:  use udev syntax\n");
  fprintf(stderr, "       -a\"ACTION\": perform udev action ACTION on attachment\n");
  exit(1);
}

int main (int argc, char **argv)
{
  LIBMTP_device_entry_t *entries;
  int numentries;
  int i;
  int ret;
  int udev_style = 0;
  int opt;
  extern int optind;
  extern char *optarg;
  char *udev_action = NULL;
  char default_udev_action[] = "SYMLINK+=\"libmtp-%k\", MODE=\"666\"";

  while ( (opt = getopt(argc, argv, "ua:")) != -1 ) {
    switch (opt) {
    case 'a':
      udev_action = strdup(optarg);
    case 'u':
      udev_style = 1;
      break;
    default:
      usage();
    }
  }

  LIBMTP_Init();
  ret = LIBMTP_Get_Supported_Devices_List(&entries, &numentries);
  if (ret == 0) {
    if (udev_style) {
      printf("# UDEV-style hotplug map for libmtp\n");
      printf("# Put this file in /etc/udev/rules.d\n\n");
      printf("SUBSYSTEM!=\"usb_device\", ACTION!=\"add\", GOTO=\"libmtp_rules_end\"\n\n");
    } else {
      printf("# This usermap will call the script \"libmtp.sh\" whenever a known MTP device is attached.\n\n");
    }

    for (i = 0; i < numentries; i++) {
      LIBMTP_device_entry_t * entry = &entries[i];
      
      printf("# %s\n", entry->name);
      if (udev_style) {
	char *action;

	if (udev_action != NULL) {
	  action = udev_action;
	} else {
	  action = default_udev_action;
	}
	printf("SYSFS{idVendor}==\"%04x\", SYSFS{idProduct}==\"%04x\", %s\n", entry->vendor_id, entry->product_id, action);
      } else {
	printf("libmtp.sh    0x0003  0x%04x  0x%04x  0x0000  0x0000  0x00    0x00    0x00    0x00    0x00    0x00    0x00000000\n", entry->vendor_id, entry->product_id);
      }
    }
  } else {
    printf("Error.\n");
    exit(1);
  }

  if (udev_style) {
    printf("\nLABEL=\"libmtp_rules_end\"\n");
  }

  exit (0);
}
