/**
 * \file mtp-hotplug.c
 * Program to create hotplug scripts.
 *
 * Copyright (C) 2005-2012 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2006-2008 Marcus Meissner <marcus@jet.franken.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <libmtp.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void usage(void)
{
  fprintf(stderr, "usage: hotplug [-u -H -i -a\"ACTION\"] -p\"DIR\" -g\"GROUP\" -m\"MODE\"\n");
  fprintf(stderr, "       -u:  use udev syntax\n");
  fprintf(stderr, "       -o:  use old udev syntax\n");
  fprintf(stderr, "       -H:  use hal syntax\n");
  fprintf(stderr, "       -i:  use usb.ids simple list syntax\n");
  fprintf(stderr, "       -a\"ACTION\": perform udev action ACTION on attachment\n");
  fprintf(stderr, "       -p\"DIR\": directory where mtp-probe will be installed\n");
  fprintf(stderr, "       -g\"GROUP\": file group for device nodes\n");
  fprintf(stderr, "       -m\"MODE\": file mode for device nodes\n");
  exit(1);
}

enum style {
  style_usbmap,
  style_udev,
  style_udev_old,
  style_hal,
  style_usbids
};

int main (int argc, char **argv)
{
  LIBMTP_device_entry_t *entries;
  int numentries;
  int i;
  int ret;
  enum style style = style_usbmap;
  int opt;
  extern int optind;
  extern char *optarg;
  char *udev_action = NULL;
  /*
   * You could tag on MODE="0666" here to enfore writeable
   * device nodes, use the command line argument for that.
   * Current udev default rules will make any device tagged
   * with ENV{ID_MEDIA_PLAYER}=1 writable for the console
   * user.
   */
  char default_udev_action[] = "SYMLINK+=\"libmtp-%k\", ENV{ID_MTP_DEVICE}=\"1\", ENV{ID_MEDIA_PLAYER}=\"1\"";
  char *action; // To hold the action actually used.
  uint16_t last_vendor = 0x0000U;
  char mtp_probe_dir[256];
  char *udev_group= NULL;
  char *udev_mode = NULL;

  while ( (opt = getopt(argc, argv, "uoiHa:p:g:m:")) != -1 ) {
    switch (opt) {
    case 'a':
      udev_action = strdup(optarg);
      break;
    case 'u':
      style = style_udev;
      break;
    case 'o':
      style = style_udev_old;
      break;
    case 'H':
      style = style_hal;
      break;
    case 'i':
      style = style_usbids;
      break;
    case 'p':
      strncpy(mtp_probe_dir,optarg,sizeof(mtp_probe_dir));
      mtp_probe_dir[sizeof(mtp_probe_dir)-1] = '\0';
      if (strlen(mtp_probe_dir) <= 1) {
	printf("Supply some sane mtp-probe dir\n");
	exit(1);
      }
      /* Make sure the dir ends with '/' */
      if (mtp_probe_dir[strlen(mtp_probe_dir)-1] != '/') {
	int index = strlen(mtp_probe_dir);
	if (index >= (sizeof(mtp_probe_dir)-1)) {
	  exit(1);
	}
	mtp_probe_dir[index] = '/';
	mtp_probe_dir[index+1] = '\0';
      }
      /* Don't add the standard udev path... */
      if (!strcmp(mtp_probe_dir, "/lib/udev/")) {
	mtp_probe_dir[0] = '\0';
      }
      break;
    case 'g':
      udev_group = strdup(optarg);
      break;
    case 'm':
      udev_mode = strdup(optarg);
      break;
 default:
      usage();
    }
  }

  if (udev_action != NULL) {
    action = udev_action;
  } else {
    action = default_udev_action;
  }

  LIBMTP_Init();
  ret = LIBMTP_Get_Supported_Devices_List(&entries, &numentries);
  if (ret == 0) {
    switch (style) {
    case style_udev:
      printf("# UDEV-style hotplug map for libmtp\n");
      printf("# Put this file in /etc/udev/rules.d\n\n");
      printf("ACTION!=\"add\", GOTO=\"libmtp_rules_end\"\n");
      printf("ENV{MAJOR}!=\"?*\", GOTO=\"libmtp_rules_end\"\n");
      printf("SUBSYSTEM==\"usb\", GOTO=\"libmtp_usb_rules\"\n"
	     "GOTO=\"libmtp_rules_end\"\n\n"
	     "LABEL=\"libmtp_usb_rules\"\n\n");
      printf("# Some sensitive devices we surely don\'t wanna probe\n");
      printf("# Color instruments\n");
      printf("ATTR{idVendor}==\"0670\", GOTO=\"libmtp_rules_end\"\n");
      printf("ATTR{idVendor}==\"0765\", GOTO=\"libmtp_rules_end\"\n");
      printf("ATTR{idVendor}==\"085c\", GOTO=\"libmtp_rules_end\"\n");
      printf("ATTR{idVendor}==\"0971\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Canon scanners that look like MTP devices (PID 0x22nn)\n");
      printf("ATTR{idVendor}==\"04a9\", ATTR{idProduct}==\"22*\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Canon digital camera (EOS 3D) that looks like MTP device (PID 0x3113)\n");
      printf("ATTR{idVendor}==\"04a9\", ATTR{idProduct}==\"3113\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Sensitive Atheros devices that look like MTP devices\n");
      printf("ATTR{idVendor}==\"0cf3\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Sensitive Atmel JTAG programmers\n");
      printf("ATTR{idVendor}==\"03eb\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Sensitive Philips device\n");
      printf("ATTR{idVendor}==\"0471\", ATTR{idProduct}==\"083f\", GOTO=\"libmtp_rules_end\"\n");
      break;
    case style_udev_old:
      printf("# UDEV-style hotplug map for libmtp\n");
      printf("# Put this file in /etc/udev/rules.d\n\n");
      printf("ACTION!=\"add\", GOTO=\"libmtp_rules_end\"\n");
      printf("ENV{MAJOR}!=\"?*\", GOTO=\"libmtp_rules_end\"\n");
      printf("SUBSYSTEM==\"usb_device\", GOTO=\"libmtp_usb_device_rules\"\n"
	     "GOTO=\"libmtp_rules_end\"\n\n"
	     "LABEL=\"libmtp_usb_device_rules\"\n\n");
      break;
    case style_usbmap:
      printf("# This usermap will call the script \"libmtp.sh\" whenever a known MTP device is attached.\n\n");
      break;
    case style_hal:
      printf("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?> <!-- -*- SGML -*- -->\n");
      printf("<!-- This file was generated by %s - - fdi -->\n", argv[0]);
      printf("<deviceinfo version=\"0.2\">\n");
      printf("  <device>\n");
      printf("    <match key=\"info.subsystem\" string=\"usb\">\n");
      break;
    case style_usbids:
      printf("# usb.ids style device list from libmtp\n");
      printf("# Compare: http://www.linux-usb.org/usb.ids\n");
      break;
    }

    for (i = 0; i < numentries; i++) {
      LIBMTP_device_entry_t * entry = &entries[i];

      switch (style) {
      case style_udev:
      case style_udev_old:
	printf("# %s %s\n", entry->vendor, entry->product);
	printf("ATTR{idVendor}==\"%04x\", ATTR{idProduct}==\"%04x\", %s", entry->vendor_id, entry->product_id, action);
	if (udev_group != NULL) printf(", GROUP=\"%s\"", udev_group);
	if (udev_mode != NULL) printf(", MODE=\"%s\"", udev_mode);
	printf("\n");
	break;
      case style_usbmap:
          printf("# %s %s\n", entry->vendor, entry->product);
          printf("libmtp.sh    0x0003  0x%04x  0x%04x  0x0000  0x0000  0x00    0x00    0x00    0x00    0x00    0x00    0x00000000\n", entry->vendor_id, entry->product_id);
          break;
        case style_hal:
          printf("      <!-- %s %s -->\n", entry->vendor, entry->product);
          printf("      <match key=\"usb.vendor_id\" int=\"0x%04x\">\n", entry->vendor_id);
          printf("        <match key=\"usb.product_id\" int=\"0x%04x\">\n", entry->product_id);
          /* FIXME: If hal >=0.5.10 can be depended upon, the matches below with contains_not can instead use addset */
          printf("          <match key=\"info.capabilities\" contains_not=\"portable_audio_player\">\n");
          printf("            <append key=\"info.capabilities\" type=\"strlist\">portable_audio_player</append>\n");
          printf("          </match>\n");
          printf("          <merge key=\"info.vendor\" type=\"string\">%s</merge>\n", entry->vendor);
          printf("          <merge key=\"info.product\" type=\"string\">%s</merge>\n", entry->product);
          printf("          <merge key=\"info.category\" type=\"string\">portable_audio_player</merge>\n");
          printf("          <merge key=\"portable_audio_player.access_method\" type=\"string\">user</merge>\n");
          printf("          <match key=\"portable_audio_player.access_method.protocols\" contains_not=\"mtp\">\n");
          printf("            <append key=\"portable_audio_player.access_method.protocols\" type=\"strlist\">mtp</append>\n");
          printf("          </match>\n");
          printf("          <append key=\"portable_audio_player.access_method.drivers\" type=\"strlist\">libmtp</append>\n");
          /* FIXME: needs true list of formats ... But all of them can do MP3 and WMA */
          printf("          <match key=\"portable_audio_player.output_formats\" contains_not=\"audio/mpeg\">\n");
          printf("            <append key=\"portable_audio_player.output_formats\" type=\"strlist\">audio/mpeg</append>\n");
          printf("          </match>\n");
          printf("          <match key=\"portable_audio_player.output_formats\" contains_not=\"audio/x-ms-wma\">\n");
          printf("            <append key=\"portable_audio_player.output_formats\" type=\"strlist\">audio/x-ms-wma</append>\n");
          printf("          </match>\n");
	  /* Special hack to support the OGG format - irivers, TrekStor and NormSoft (Palm) can always play these files! */
	  if (entry->vendor_id == 0x4102 || // iriver
	      entry->vendor_id == 0x066f || // TrekStor
	      entry->vendor_id == 0x1703) { // NormSoft, Inc.
	    printf("          <match key=\"portable_audio_player.output_formats\" contains_not=\"application/ogg\">\n");
	    printf("            <append key=\"portable_audio_player.output_formats\" type=\"strlist\">application/ogg</append>\n");
	    printf("          </match>\n");
	  }
          printf("          <merge key=\"portable_audio_player.libmtp.protocol\" type=\"string\">mtp</merge>\n");
          printf("        </match>\n");
          printf("      </match>\n");
        break;
        case style_usbids:
          if (last_vendor != entry->vendor_id) {
            printf("%04x\n", entry->vendor_id);
          }
          printf("\t%04x  %s %s\n", entry->product_id, entry->vendor, entry->product);
        break;
      }
      last_vendor = entry->vendor_id;
    }
  } else {
    printf("Error.\n");
    exit(1);
  }

  // Then the footer.
  switch (style) {
  case style_usbmap:
    break;
  case style_udev:
  case style_udev_old:
    /*
     * This is code that invokes the mtp-probe program on
     * every USB device that is either PTP or vendor specific
     */
    printf("\n# Autoprobe vendor-specific, communication and PTP devices\n");
    printf("ENV{ID_MTP_DEVICE}!=\"1\", ENV{MTP_NO_PROBE}!=\"1\", ENV{COLOR_MEASUREMENT_DEVICE}!=\"1\", ENV{libsane_matched}!=\"yes\", ATTR{bDeviceClass}==\"00|02|06|ef|ff\", PROGRAM=\"%smtp-probe /sys$env{DEVPATH} $attr{busnum} $attr{devnum}\", RESULT==\"1\", %s", mtp_probe_dir, action);
    if (udev_group != NULL) printf(", GROUP=\"%s\"", udev_group);
    if (udev_mode != NULL) printf(", MODE=\"%s\"", udev_mode);
    printf("\n");
   printf("\nLABEL=\"libmtp_rules_end\"\n");
    break;
  case style_hal:
    printf("    </match>\n");
    printf("  </device>\n");
    printf("</deviceinfo>\n");
    break;
  case style_usbids:
    printf("\n");
  }

  exit (0);
}
