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
#include "config.h"
#include "libmtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void usage(void)
{
  fprintf(stderr, "usage: hotplug [-h -w -u -f -o -H -i -a\"ACTION\"] -p\"DIR\" -g\"GROUP\" -m\"MODE\"\n");
  fprintf(stderr, "       -h:  this help message\n");
  fprintf(stderr, "       -w:  use hwdb syntax\n");
  fprintf(stderr, "       -u:  use udev syntax\n");
  fprintf(stderr, "       -f:  use udev fast syntax\n");
  fprintf(stderr, "       -o:  use old udev syntax\n");
  fprintf(stderr, "       -H:  use hal syntax\n");
  fprintf(stderr, "       -i:  use usb.ids simple list syntax\n");
  fprintf(stderr, "       -a\"ACTION\": perform udev action ACTION on attachment\n");
  fprintf(stderr, "       -p\"DIR\": directory where mtp-probe will be installed\n");
  fprintf(stderr, "       -g\"GROUP\": file group for device nodes\n");
  fprintf(stderr, "       -m\"MODE\": file mode for device nodes\n");
}

static void free_str(char **str)
{
  if (*str != NULL) {
    free(*str);
  }
}

enum style {
  style_usbmap,
  style_udev,
  style_udev_fast,
  style_udev_old,
  style_hal,
  style_usbids,
  style_hwdb
};

#define UDEV_ACTION "SYMLINK+=\"libmtp-%k\""
#define FULL_UDEV_ACTION UDEV_ACTION ", ENV{ID_MTP_DEVICE}=\"1\", ENV{ID_MEDIA_PLAYER}=\"1\""

int main (int argc, char **argv)
{
  LIBMTP_device_entry_t *entries;
  int numentries;
  int i, j, k;
  int ret, retval;
  enum style style = style_usbmap;
  int opt;
  extern int optind;
  extern char *optarg;
  /*
   * You could tag on MODE="0666" here to enforce writeable
   * device nodes, use the command line argument for that.
   * Current udev default rules will make any device tagged
   * with ENV{ID_MEDIA_PLAYER}=1 writable for the console
   * user.
   */

  char *action = NULL; // To hold the action when specified by the user.
  uint16_t last_vendor = 0x0000U;
  char mtp_probe_dir[256] = UDEV_DIR;
  char *udev_group= NULL;
  char *udev_mode = NULL;
  int *sorted_codes;

  retval = 0;
  while ( (opt = getopt(argc, argv, "hwufoiHa:p:g:m:")) != -1 ) {
    switch (opt) {
    case 'a':
      action = optarg;
      break;
    case 'u':
      style = style_udev;
      break;
    case 'f':
      style = style_udev_fast;
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
    case 'w':
      style = style_hwdb;
      break;
    case 'p':
      strncpy(mtp_probe_dir,optarg,sizeof(mtp_probe_dir));
      mtp_probe_dir[sizeof(mtp_probe_dir)-1] = '\0';
      if (strlen(mtp_probe_dir) <= 1) {
	printf("Supply some sane mtp-probe dir\n");
	goto main_exit;
      }
      /* Make sure the dir ends with '/' */
      if (mtp_probe_dir[strlen(mtp_probe_dir)-1] != '/') {
	int index = strlen(mtp_probe_dir);
	if (index >= (sizeof(mtp_probe_dir)-1)) {
	  goto main_exit;
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
      free_str(&udev_group);
      udev_group = strdup(optarg);
      break;
    case 'm':
      free_str(&udev_mode);
      udev_mode = strdup(optarg);
      break;
    default:
      retval = 1;
    case 'h':
      usage();
      goto main_exit;
    }
    retval = 1;
  }

  /* LIBMTP_Init(); */
  /*
   * LIBMTP_Get_Supported_Devices_List() is a completely static
   * function which only returns pointers to a static list, so
   * it is safe to avoid running LIBMTP_Init() for mtp-hotplug.c
   *
   * There are no other LIBMTP calls beyond the one listed below
   */
  ret = LIBMTP_Get_Supported_Devices_List(&entries, &numentries);
  if (ret == 0) {
    /* sort codes numerically */
    sorted_codes = malloc(numentries * sizeof(int));
    if (sorted_codes == NULL) {
      ret = -1;
  } else {
      sorted_codes[0] = 0;
      for (i = 1; i < numentries; i++) {
	for (j = 0; j < i; j++) {
	  if (entries[i].vendor_id < entries[sorted_codes[j]].vendor_id)
	    break;
	  if (entries[i].vendor_id == entries[sorted_codes[j]].vendor_id && \
	      entries[i].product_id < entries[sorted_codes[j]].product_id)
	    break;
	}
	if (j < i) {
	  for (k = i; k > j; k--) {
	    sorted_codes[k] = sorted_codes[k - 1];
	  }
	}
	sorted_codes[j] = i;
      }
    }
  }
  if (ret == 0) {
    switch (style) {
    case style_udev:
      printf("# UDEV-style hotplug map for libmtp\n");
      printf("# Put this file in /etc/udev/rules.d\n\n");
      printf("ACTION!=\"add\", ACTION!=\"bind\", GOTO=\"libmtp_rules_end\"\n");
      printf("ENV{MAJOR}!=\"?*\", GOTO=\"libmtp_rules_end\"\n");
      printf("SUBSYSTEM!=\"usb\", GOTO=\"libmtp_rules_end\"\n\n");

      printf("# If we have a hwdb entry for this device, act immediately!\n");
      printf("ENV{ID_MTP_DEVICE}==\"1\", %s", action ?: UDEV_ACTION);
      if (udev_group != NULL) printf(", GROUP=\"%s\"", udev_group);
      if (udev_mode != NULL) printf(", MODE=\"%s\"", udev_mode);
      printf(", GOTO=\"libmtp_rules_end\"\n\n");

      printf("# Fall back to probing.\n");
      printf("# Some sensitive devices we surely don\'t wanna probe\n");
      printf("# Color instruments\n");
      printf("ATTR{idVendor}==\"0670\", GOTO=\"libmtp_rules_end\"\n");
      printf("ATTR{idVendor}==\"0765\", GOTO=\"libmtp_rules_end\"\n");
      printf("ATTR{idVendor}==\"085c\", GOTO=\"libmtp_rules_end\"\n");
      printf("ATTR{idVendor}==\"0971\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Canon scanners that look like MTP devices (PID 0x22nn)\n");
      printf("ATTR{idVendor}==\"04a9\", ATTR{idProduct}==\"22*\", GOTO=\"libmtp_rules_end\"\n");
      printf("# HP scanners that look like MTP devices (PID 0xc5nn)\n");
      printf("ATTR{idVendor}==\"03f0\", ATTR{idProduct}==\"c5*\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Canon digital camera (EOS 3D) that looks like MTP device (PID 0x3113)\n");
      printf("ATTR{idVendor}==\"04a9\", ATTR{idProduct}==\"3113\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Sensitive Atheros devices that look like MTP devices\n");
      printf("ATTR{idVendor}==\"0cf3\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Sensitive Atmel JTAG programmers\n");
      printf("ATTR{idVendor}==\"03eb\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Sensitive Philips device\n");
      printf("ATTR{idVendor}==\"0471\", ATTR{idProduct}==\"083f\", GOTO=\"libmtp_rules_end\"\n");
      printf("# DUALi NFC readers\n");
      printf("ATTR{idVendor}==\"1db2\", ATTR{idProduct}==\"060*\", GOTO=\"libmtp_rules_end\"\n");
      printf("# Printers\n");
      printf("ENV{ID_USB_INTERFACES}==\"*:0701??:*\", GOTO=\"libmtp_rules_end\"\n");
      break;
    case style_udev_fast:
    case style_udev_old:
      printf("# UDEV-style hotplug map for libmtp\n");
      printf("# Put this file in /etc/udev/rules.d\n\n");
      printf("ACTION!=\"add\", ACTION!=\"bind\", GOTO=\"libmtp_rules_end\"\n");
      printf("ENV{MAJOR}!=\"?*\", GOTO=\"libmtp_rules_end\"\n");
      printf("SUBSYSTEM!=\"usb_device\", GOTO=\"libmtp_rules_end\"\n\n");
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
    case style_hwdb:
      printf("# hardware database file for libmtp supported devices\n");
      break;
    }

    last_vendor = 0xffff;
    for (i = 0; i < numentries; i++) {
      LIBMTP_device_entry_t * entry = &entries[i];

      switch (style) {
      case style_udev_fast:
	entry = &entries[sorted_codes[i]];
	if (last_vendor != entry->vendor_id) {
	  if (last_vendor != 0xffff) {
	    printf("GOTO=\"libmtp_rules_probe\"\n");
	    printf("LABEL=\"not_%04x\"\n\n", last_vendor);
	    last_vendor = entry->vendor_id;
	  }
	  printf("ATTR{idVendor}!=\"%04x\", GOTO=\"not_%04x\"\n", \
		 entry->vendor_id, entry->vendor_id);
	};
	printf("# %s %s\n", entry->vendor, entry->product);
	printf("ATTR{idProduct}==\"%04x\", GOTO=\"libmtp_rules_match\"\n", entry->product_id);
	break;
      case style_udev_old:
	printf("# %s %s\n", entry->vendor, entry->product);
	printf("ATTR{idVendor}==\"%04x\", ATTR{idProduct}==\"%04x\", %s",
               entry->vendor_id, entry->product_id, action ?: FULL_UDEV_ACTION);
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
      case style_hwdb:
          entry = &entries[sorted_codes[i]];
          printf("# %s %s\n", entry->vendor, entry->product);
          printf("usb:v%04Xp%04X*\n", entry->vendor_id, entry->product_id);
          printf(" ID_MEDIA_PLAYER=1\n");
          printf(" ID_MTP_DEVICE=1\n");
          printf("\n");
          break;
      }
      last_vendor = entry->vendor_id;
    }
    free(sorted_codes);
  } else {
    printf("Error.\n");
    goto main_exit;
  }

  // Then the footer.
  switch (style) {
  case style_usbmap:
  case style_hwdb:
    break;
  case style_udev_fast:
    printf("GOTO=\"libmtp_rules_end\"\n");
    printf("LABEL=\"not_%04x\"\n\n", last_vendor);
    printf("GOTO=\"libmtp_rules_probe\"\n");
    printf("\nLABEL=\"libmtp_rules_match\"\n");
    printf("%s", action ?: FULL_UDEV_ACTION);
    if (udev_group != NULL) printf(", GROUP=\"%s\"", udev_group);
    if (udev_mode != NULL) printf(", MODE=\"%s\"", udev_mode);
    printf("\nGOTO=\"libmtp_rules_end\"\n");
    printf("\nLABEL=\"libmtp_rules_probe\"");
  case style_udev:
  case style_udev_old:
    /*
     * This is code that invokes the mtp-probe program on
     * every USB device that is either PTP or vendor specific
     * also don't run probe if gphoto2 already matched it as camera.
     */
    printf("\n# Autoprobe vendor-specific, communication and PTP devices\n");
    printf("ENV{ID_MTP_DEVICE}!=\"1\", ENV{MTP_NO_PROBE}!=\"1\", ENV{COLOR_MEASUREMENT_DEVICE}!=\"1\", ENV{ID_GPHOTO}!=\"1\", ENV{libsane_matched}!=\"yes\", ATTR{bDeviceClass}==\"00|02|06|ef|ff\", PROGRAM=\"%smtp-probe /sys$env{DEVPATH} $attr{busnum} $attr{devnum}\", RESULT==\"1\", %s",
           mtp_probe_dir, action ?: FULL_UDEV_ACTION);
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
  retval = 0;

main_exit:
  free_str(&udev_mode);
  free_str(&udev_group);
  return retval;
}
