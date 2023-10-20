/**
 * \file testunicode.c
 *
 * Copyright (C) 2023 Joe Da Silva <digital@joescat.com>
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
 *
 * Test functions in libmtp's unicode.c file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "libmtp.h"
#include "unicode.h"
#include "ptp.h"

static int test_ucs2_strlen() {
  uint16_t str0[6] = { 5,4,3,2,1,0 };
  uint16_t str1[6] = { 0xd9d9,0xdddd,3,0xd9d9,0xdddd,0 };
  /* next three are badly sequenced utf-16 codes */
  uint16_t str2[6] = { 5,4,3,0xd9d9,0xd9d9,0 };
  uint16_t str3[6] = { 5,4,3,0xdddd,0xdddd,0 };
  uint16_t str4[6] = { 5,4,3,0xdddd,0xd9d9,0 };
  int length;

  printf("test ucs2_strlen(str0,0)\n");
  length = ucs2_strlen(str0,0);
  printf("expected length=5, returned %d\n", length);
  if (length != 5) return -1;

  printf("test ucs2_strlen(str1,0)\n");
  length = ucs2_strlen(str1,0);
  printf("expected length=5, returned %d\n\n", length);
  if (length != 5) return -1;

  printf("test ucs2_strlen(str0,1)\n");
  length = ucs2_strlen(str0,1);
  printf("expected length=5, returned %d\n", length);
  if (length != 5) return -1;

  printf("test ucs2_strlen(str1,1)\n");
  length = ucs2_strlen(str1,1);
  printf("expected length=3, returned %d\n", length);
  if (length != 3) return -1;

  printf("test ucs2_strlen(str2,1)\n");
  length = ucs2_strlen(str2,1);
  printf("expected length=5, returned %d\n", length);
  if (length != 5) return -1;

  printf("test ucs2_strlen(str3,1)\n");
  length = ucs2_strlen(str3,1);
  printf("expected length=5, returned %d\n", length);
  if (length != 5) return -1;

  printf("test ucs2_strlen(str4,1)\n");
  length = ucs2_strlen(str4,1);
  printf("expected length=5, returned %d\n\n", length);
  if (length != 5) return -1;

  printf("test ucs2_strlen(str0,2)\n");
  length = ucs2_strlen(str0,2);
  printf("expected length=5, returned %d\n", length);
  if (length != 5) return -1;

  printf("test ucs2_strlen(str1,2)\n");
  length = ucs2_strlen(str1,2);
  printf("expected length=3, returned %d\n", length);
  if (length != 3) return -1;

  printf("test ucs2_strlen(str2,2)\n");
  length = ucs2_strlen(str2,2);
  printf("expected error=length=-1, returned %d\n", length);
  if (length != -1) return -1;

  printf("test ucs2_strlen(str3,2)\n");
  length = ucs2_strlen(str3,2);
  printf("expected error=length=-1, returned %d\n", length);
  if (length != -1) return -1;

  printf("test ucs2_strlen(str4,2)\n");
  length = ucs2_strlen(str4,2);
  printf("expected error=length=-1, returned %d\n\n", length);
  if (length != -1) return -1;

  return 0;
}

static int test_strip_7bit_from_utf8() {
  char str[9] = { '(',0xe2,0x97,0x8e,'A',0xc2,0xa2,')',0 };
  int length;

  length = strlen(str);
  printf("test strip_7bit_from_utf8()\nstr=%s, length=%d\n", str, length);
  strip_7bit_from_utf8(str);
  length = strlen(str);
  printf("expected str=(_A_), length=5, returned str=%s, length=%d\n\n", str, length);
  if (length != 5) return -1;
  return 0;
}

int main(int argc, char **argv) {

  if (test_ucs2_strlen()) return -1;

  if (test_strip_7bit_from_utf8()) return -2;

  return 0;
}
