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

static int test_utf16_to_utf8() {
  /* little-endian   32,   64,   128, 256, 512,1024,2048,   0 */
  char str[16] = { 32,0, 64,0, 128,0, 0,1, 0,2, 0,4, 0,8, 0,0 };
  char res[14] = { 32,64,0xc2,0x80,0xc4,0x80,0xc8,0x80,0xd0,0x80,0xe0,0xa0,0x80,0 };
  char *ret;
  int err, i, length;

  printf("test ucs2_strlen(str,2) for this str[16]\n");
  length = ucs2_strlen((uint16_t *)(str),2);
  printf("expected length=7, returned length=%d\n", length);
  if (length != 7) return -1;
  err = -1;
  printf("testing utf16_to_utf8()\n");
  ret = utf16_to_utf8((uint16_t *)(str));
  length = strlen(ret);
  printf("expected length=13, returned %d. Characters returned are:\n", length);
  if (length != 13) goto err_test_utf16_to_utf8;
  for (i = 0; i <= length; i++) {
    printf("%x, ", (unsigned char)(ret[i]));
    if (ret[i] != res[i]) goto err_test_utf16_to_utf8;
  }
  printf("\nreturned string matches expected result string.\n\n");
  err = 0;
err_test_utf16_to_utf8:
  free(ret);
  return err;
}

static int test_utf16_to_utf8_noBOM() {
  /* little-endian (with BOM)   32,   64,   128, 256, 512,1024,2048,    0xd83d:0xde0e,      0 */
  char str[22] = { 0xff,0xfe, 32,0, 64,0, 128,0, 0,1, 0,2, 0,4, 0,8, 0x3d,0xd8,0x0e,0xde, 0,0 };
  char res[18] = { 32,64,0xc2,0x80,0xc4,0x80,0xc8,0x80,0xd0,0x80,0xe0,0xa0,0x80,0xf0,0x9f,0x98,0x8e,0 };
  char *ret;
  int err, i, length;

  printf("test ucs2_strlen(str,2) for this str[22]\n");
  length = ucs2_strlen((uint16_t *)(str),2);
  printf("expected length=BOM+8=9, returned length=%d\n", length);
  if (length != 9) return -1;
  err = -1;
  printf("testing utf16_to_utf8() (remove BOM)\n");
  ret = utf16_to_utf8((uint16_t *)(str));
  length = strlen(ret);
  printf("expected length=17, returned %d. Characters returned are:\n", length);
  if (length != 17) goto err_test_utf16_to_utf8_noBOM;
  for (i = 0; i <= length; i++) {
    printf("%x, ", (unsigned char)(ret[i]));
    if (ret[i] != res[i]) goto err_test_utf16_to_utf8_noBOM;
  }
  printf("\nreturned string matches expected result string.\n\n");
  err = 0;
err_test_utf16_to_utf8_noBOM:
  free(ret);
  return err;
}

static int test_utf16_to_utf8_buffMAX() {
  uint16_t *str;
  char *ret;
  int err, i, length;

  err = -1;
  printf("testing utf16_to_utf8() (1024MAX chars)\n");
  str = malloc((1030*2+1)*sizeof(uint16_t)); /* this will get chopped */
  if (str == NULL) return -1;
  for (i = 0; i < 1030; i++) {
    str[i*2] = 0xd9d9; /* creates four utf8 chars 0xf2,0x86,0x97,0x9d */
    str[i*2+1] = 0xdddd;
  }
  str[1030*2+1] = 0;
  ret = utf16_to_utf8((uint16_t *)(str));
  printf("test ucs2_strlen(str,2) for this oversized str[1031]\n");
  length = ucs2_strlen((uint16_t *)(str),2);
  printf("expected length=1030chars, returned length=%dchars\n", length);
  if (length != 1030) goto test_utf16_to_utf8_buffMAX_err;
  length = strlen(ret);
  printf("expected length=1024*4=4096bytes, returned length=%dbytes.\n\n", length);
  if (length != 4096) goto test_utf16_to_utf8_buffMAX_err;
  err = 0;
test_utf16_to_utf8_buffMAX_err:
  free(ret);
  free(str);
  return err;
}

static int test_utf8_to_utf16_1() {
  /* little-endian            32,   64,   128, 256, 512,1024,2048,   0 */
  unsigned char res[16] = { 32,0, 64,0, 128,0, 0,1, 0,2, 0,4, 0,8, 0,0 };
  char str[14] = { 32,64,0xc2,0x80,0xc4,0x80,0xc8,0x80,0xd0,0x80,0xe0,0xa0,0x80,0 };
  uint16_t *ret;
  unsigned char *ret2;
  int err, i, length;

  err = -1;
  printf("testing utf8_to_utf16() (test1)\n");
  ret = utf8_to_utf16(str,0);
  length = ucs2_strlen(ret,2);
  printf("expected 7 chars, returned %d chars. Characters returned are:\n", length);
  if (length != 7) goto err_test_utf8_to_utf16;
  ret2 = (unsigned char *)(ret);
  for (i = 0; i < length*2; i++) {
    printf("%x, ", ret2[i]);
    if (ret2[i] != res[i]) goto err_test_utf8_to_utf16;
  }
  printf("\nreturned string matches expected result string.\n\n");
  err = 0;
err_test_utf8_to_utf16:
  free(ret);
  return err;
}

static int test_utf8_to_utf16_2() {
  /* little-endian            32,   64,   128, 256, 512,1024,2048,    0xd83d:0xde0e,      0 */
  unsigned char res[20] = { 32,0, 64,0, 128,0, 0,1, 0,2, 0,4, 0,8, 0x3d,0xd8,0x0e,0xde, 0,0 };
  char str[18] = { 32,64,0xc2,0x80,0xc4,0x80,0xc8,0x80,0xd0,0x80,0xe0,0xa0,0x80,0xf0,0x9f,0x98,0x8e,0 };
  uint16_t *ret;
  unsigned char *ret2;
  int err, i, length;

  err = -1;
  printf("testing utf8_to_utf16() (test2)\n");
  ret = utf8_to_utf16(str,0);
  length = ucs2_strlen(ret,2);
  printf("expected 8 chars, returned %d chars. UTF-16 Characters returned are:\n", length);
  if (length != 8) goto err_test_utf8_to_utf16_2;
  ret2 = (unsigned char *)(ret);
  for (i = 0; i < (length+1)*2; i++) {
    printf("%x, ", ret2[i]);
    if (ret2[i] != res[i]) goto err_test_utf8_to_utf16_2;
  }
  printf("\nreturned string matches expected result string.\n\n");
  err = 0;
err_test_utf8_to_utf16_2:
  free(ret);
  return err;
}

static int test_utf8_to_utf16_3() {
  char str[4] = { 32,64,0xc2,0 };
  uint16_t *ret;
  unsigned char *ret2;
  int err, i, length;

  err = -1;
  printf("testing utf8_to_utf16() (test3)\n");
  ret = utf8_to_utf16(str,0);
  length = -1;
  if (ret != NULL)
    length = ucs2_strlen(ret,2);
  printf("expected failure with zero length string\n", length);
  if (length != 0) goto err_test_utf8_to_utf16_3;
  free(ret);
  ret = utf8_to_utf16(str,1);
  length = -1;
  if (ret != NULL)
    length = ucs2_strlen(ret,2);
  printf("expected failure with NULL string\n\n", length);
  if (length != -1) goto err_test_utf8_to_utf16_3;
  err = 0;
err_test_utf8_to_utf16_3:
  free(ret);
  return err;
}

static int test_utf8_to_utf16_buffMAX() {
  char *str;
  uint16_t *ret;
  int err, i, length;

  err = -1;
  printf("testing utf8_to_utf16() (1024MAX chars)\n");
  str = malloc(1030*4+1); /* this will get chopped */
  if (str == NULL) return -1;
  for (i = 0; i < 1030; i++) {
    str[i*4+0] = 0xf2; /* expect 0xd9d9:0xdddd when converted */
    str[i*4+1] = 0x86;
    str[i*4+2] = 0x97;
    str[i*4+3] = 0x9d;
  }
  str[1030*4] = 0;
  length = strlen(str);
  printf("input utf8 string, expect length=1030*4=4120, returned length=%d.\n",length);
  ret = utf8_to_utf16(str,0);
  printf("test ucs2_strlen(ret,2) for this oversized str of 1030 extended chars\n");
  length = ucs2_strlen(ret,2);
  printf("expected chopped length=1024chars, returned length=%dchars\n\n", length);
  if (length != 1024) goto test_utf8_to_utf16_buffMAX_err;
  err = 0;
test_utf8_to_utf16_buffMAX_err:
  free(ret);
  free(str);
  return err;
}

int main(int argc, char **argv) {

  if (test_ucs2_strlen()) return -1;

  if (test_strip_7bit_from_utf8()) return -2;

  if (test_utf16_to_utf8()) return -3;
  if (test_utf16_to_utf8_noBOM()) return -4;
  if (test_utf16_to_utf8_buffMAX()) return -5;

  if (test_utf8_to_utf16_1()) return -6;
  if (test_utf8_to_utf16_2()) return -7;
  if (test_utf8_to_utf16_3()) return -8;
  if (test_utf8_to_utf16_buffMAX()) return -9;

  return 0;
}
