/**
 * \file mtp-probe.c
 * Program to probe newly connected device interfaces from
 * userspace to determine if they are MTP devices, used for
 * udev rules.
 *
 * Invoke the program from udev to check it for MTP signatures,
 * e.g.
 * ATTR{bDeviceClass}=="ff",
 * PROGRAM="<path>/mtp-probe /sys$env{DEVPATH} $attr{busnum} $attr{devnum}",
 * RESULT=="1", ENV{ID_MTP_DEVICE}="1", ENV{ID_MEDIA_PLAYER}="1",
 * SYMLINK+="libmtp-%k", MODE="666"
 *
 * Is you issue this before testing your /var/log/messages
 * will be more verbose:
 *
 * udevadm control --log-priority=debug
 *
 * Exits with status code 1 if the device is an MTP device,
 * else exits with 0.
 *
 * Copyright (C) 2011-2012 Linus Walleij <triad@df.lth.se>
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
#ifndef __linux__
#error "This program should only be compiled for Linux!"
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libmtp.h>
#include <regex.h>
#include <fcntl.h>

enum ep_type {
  OTHER_EP,
  BULK_OUT_EP,
  BULK_IN_EP,
  INTERRUPT_IN_EP,
  INTERRUPT_OUT_EP,
};

static enum ep_type get_ep_type(char *path)
{
  char pbuf[FILENAME_MAX];
  int len = strlen(path);
  int fd;
  char buf[128];
  int bread;
  int is_out = 0;
  int is_in = 0;
  int is_bulk = 0;
  int is_interrupt = 0;
  int i;

  strcpy(pbuf, path);
  pbuf[len++] = '/';

  /* Check the type */
  strncpy(pbuf + len, "type", FILENAME_MAX - len);
  pbuf[FILENAME_MAX - 1] = '\0'; /* Sentinel */

  fd = open(pbuf, O_RDONLY);
  if (fd < 0)
    return OTHER_EP;
  bread = read(fd, buf, sizeof(buf));
  close(fd);
  if (bread < 2)
    return OTHER_EP;

  for (i = 0; i < bread; i++)
    if(buf[i] == 0x0d || buf[i] == 0x0a)
      buf[i] = '\0';

  if (!strcmp(buf, "Bulk"))
    is_bulk = 1;
  if (!strcmp(buf, "Interrupt"))
    is_interrupt = 1;

  /* Check the direction */
  strncpy(pbuf + len, "direction", FILENAME_MAX - len);
  pbuf[FILENAME_MAX - 1] = '\0'; /* Sentinel */

  fd = open(pbuf, O_RDONLY);
  if (fd < 0)
    return OTHER_EP;
  bread = read(fd, buf, sizeof(buf));
  close(fd);
  if (bread < 2)
    return OTHER_EP;

  for (i = 0; i < bread; i++)
    if(buf[i] == 0x0d || buf[i] == 0x0a)
      buf[i] = '\0';

  if (!strcmp(buf, "in"))
    is_in = 1;
  if (!strcmp(buf, "out"))
    is_out = 1;

  if (is_bulk && is_in)
    return BULK_IN_EP;
  if (is_bulk && is_out)
    return BULK_OUT_EP;
  if (is_interrupt && is_in)
    return INTERRUPT_IN_EP;
  if (is_interrupt && is_out)
    return INTERRUPT_OUT_EP;

  return OTHER_EP;
}

static int has_3_ep(char *path)
{
  char pbuf[FILENAME_MAX];
  int len = strlen(path);
  int fd;
  char buf[128];
  int bread;

  strcpy(pbuf, path);
  pbuf[len++] = '/';
  strncpy(pbuf + len, "bNumEndpoints", FILENAME_MAX - len);
  pbuf[FILENAME_MAX - 1] = '\0'; /* Sentinel */

  fd = open(pbuf, O_RDONLY);
  if (fd < 0)
    return -1;
  /* Read all contents to buffer */
  bread = read(fd, buf, sizeof(buf));
  close(fd);
  if (bread < 2)
    return 0;

  /* 0x30, 0x33 = "03", maybe we should parse it? */
  if (buf[0] == 0x30 && buf[1] == 0x33)
    return 1;

  return 0;
}

static int check_interface(char *sysfspath)
{
  char dirbuf[FILENAME_MAX];
  int len = strlen(sysfspath);
  DIR *dir;
  struct dirent *dent;
  regex_t r;
  int ret;
  int bulk_out_ep_found = 0;
  int bulk_in_ep_found = 0;
  int interrupt_in_ep_found = 0;

  ret = has_3_ep(sysfspath);
  if (ret <= 0)
    return ret;

  /* Yes it has three endpoints ... look even closer! */
  dir = opendir(sysfspath);
  if (!dir)
    return -1;

  strcpy(dirbuf, sysfspath);
  dirbuf[len++] = '/';

  /* Check for dirs that identify endpoints */
  ret = regcomp(&r, "^ep_[0-9a-f]+$", REG_EXTENDED | REG_NOSUB);
  if (ret) {
    closedir(dir);
    return -1;
  }

  while ((dent = readdir(dir))) {
    struct stat st;

    /* No need to check those beginning with a period */
    if (dent->d_name[0] == '.')
      continue;

    strncpy(dirbuf + len, dent->d_name, FILENAME_MAX - len);
    dirbuf[FILENAME_MAX - 1] = '\0'; /* Sentinel */
    ret = lstat(dirbuf, &st);
    if (ret)
      continue;
    if (S_ISDIR(st.st_mode) && !regexec(&r, dent->d_name, 0, 0, 0)) {
      enum ep_type ept;

      ept = get_ep_type(dirbuf);
      if (ept == BULK_OUT_EP)
	bulk_out_ep_found = 1;
      else if (ept == BULK_IN_EP)
	bulk_in_ep_found = 1;
      else if (ept == INTERRUPT_IN_EP)
	interrupt_in_ep_found = 1;
    }
  }

  regfree(&r);
  closedir(dir);

  /*
   * If this is fulfilled the interface is an MTP candidate
   */
  if (bulk_out_ep_found &&
      bulk_in_ep_found &&
      interrupt_in_ep_found) {
    return 1;
  }

  return 0;
}

static int check_sysfs(char *sysfspath)
{
  char dirbuf[FILENAME_MAX];
  int len = strlen(sysfspath);
  DIR *dir;
  struct dirent *dent;
  regex_t r;
  int ret;
  int look_closer = 0;

  dir = opendir(sysfspath);
  if (!dir)
    return -1;

  strcpy(dirbuf, sysfspath);
  dirbuf[len++] = '/';

  /* Check for dirs that identify interfaces */
  ret = regcomp(&r, "^[0-9]+-[0-9]+(\\.[0-9])*\\:[0-9]+\\.[0-9]+$", REG_EXTENDED | REG_NOSUB);
  if (ret) {
    closedir(dir);
    return -1;
  }

  while ((dent = readdir(dir))) {
    struct stat st;
    int ret;

    /* No need to check those beginning with a period */
    if (dent->d_name[0] == '.')
      continue;

    strncpy(dirbuf + len, dent->d_name, FILENAME_MAX - len);
    dirbuf[FILENAME_MAX - 1] = '\0'; /* Sentinel */
    ret = lstat(dirbuf, &st);
    if (ret)
      continue;

    /* Look closer at dirs that may be interfaces */
    if (S_ISDIR(st.st_mode)) {
      if (!regexec(&r, dent->d_name, 0, 0, 0))
      if (check_interface(dirbuf) > 0)
	/* potential MTP interface! */
	look_closer = 1;
    }
  }

  regfree(&r);
  closedir(dir);
  return look_closer;
}

int main (int argc, char **argv)
{
  char *fname;
  int busno;
  int devno;
  int ret;

  if (argc < 4) {
    syslog(LOG_INFO, "need device path, busnumber, device number as argument\n");
    printf("0");
    exit(0);
  }

  fname = argv[1];
  busno = atoi(argv[2]);
  devno = atoi(argv[3]);

  syslog(LOG_INFO, "checking bus %d, device %d: \"%s\"\n", busno, devno, fname);

  ret = check_sysfs(fname);
  /*
   * This means that regular directory check either agrees that this may be a
   * MTP device, or that it doesn't know (failed). In that case, kick the deeper
   * check inside LIBMTP.
   */
  if (ret != 0)
    ret = LIBMTP_Check_Specific_Device(busno, devno);
  if (ret) {
    syslog(LOG_INFO, "bus: %d, device: %d was an MTP device\n", busno, devno);
    printf("1");
  } else {
    syslog(LOG_INFO, "bus: %d, device: %d was not an MTP device\n", busno, devno);
    printf("0");
  }

  exit(0);
}
