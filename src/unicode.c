/**
 * \file unicode.c
 *
 * This file contains general Unicode string manipulation functions.
 * It mainly consist of functions for converting between UCS-2 (used on
 * the devices) and UTF-8 (used by several applications).
 *
 * For a deeper understanding of Unicode encoding formats see the
 * Wikipedia entries for
 * <a href="http://en.wikipedia.org/wiki/UTF-16/UCS-2">UTF-16/UCS-2</a>
 * and <a href="http://en.wikipedia.org/wiki/UTF-8">UTF-8</a>.
 *
 * Copyright (C) 2005-2009 Linus Walleij <triad@df.lth.se>
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
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_ICONV
#include <iconv.h>
#endif
#include "libmtp.h"
#include "unicode.h"
#include "util.h"
#include "ptp.h"

/**
 * The size of the buffer (in characters) used for creating string copies.
 */
#define STRING_BUFFER_LENGTH 1024

/**
 * Gets the length (in characters, not bytes) of a unicode
 * UCS-2LE string, eg a string which physically is 0x00 0x41 0x00 0x00
 * will return a value of 1 (Add 1 to include needed trailing 0x0000).
 * mode=0: UCS-2LE, codes 0x0000...0xffff. 2xlength=total number of uint16_t chars.
 * mode=1: UTF-16LE, Unicode >= 2.0. {0xd800..0xdbff:0xdc00..0xdfff} counts as 1 char.
 * mode=2: UTF-16LE. Unicode >= 2.0. {0xdc00..0xdfff:0xd800..0xdbff}==err, return length=-1.
 *
 * @param unicstr a UCS-2LE/UTF-16LE Unicode string
 * @param mode count method, mode={0,1,2}
 * @return the length of the string in number of characters.
 */
int ucs2_strlen(uint16_t const * const unicstr, int mode)
{
  unsigned char *p8in; /* intentionally load as little-endian regardless of host CPU endianess */
  uint16_t chin;
  int length;

  if (mode == 0) {
    /* Unicode strings are terminated with 2 * 0x00 */
    for (length = 0; unicstr[length]; length++);
  } else {
    /* Need to account for {dc00...dfff} code pairs */
    p8in = (unsigned char *)(unicstr);
    chin = *p8in++;

    for (length = 0; (chin |= ((*p8in++) << 8)); length++) {
      /* look for {d800..dbff|dc00..dfff} code pair */
      if (chin >= 0xd800 && chin <= 0xdfff) {
	if (chin <= 0xdbff) {
	  chin = p8in[1];
	  chin = (chin << 8) | p8in[0];
	  if (chin >= 0xdC00 && chin <= 0xdfff) {
	    p8in += 2; /* this is a valid code pair */
	  } else if (mode == 2) {
	    length = -1;
	    break;
	  }
	} else if (mode == 2) {
	  length = -1;
	  break;
	}
      }
      chin = *p8in++;
    }
  }
  return length;
}

/**
 * Converts a big-endian UTF-16 2-byte string
 * to a UTF-8 string. Actually just a UCS-2 internal conversion
 * routine that strips off the BOM if there is one.
 *
 * @param device a pointer to the current device.
 * @param unicstr the UTF-16 unicode string to convert
 * @return a UTF-8 string.
 */
char *utf16_to_utf8(LIBMTP_mtpdevice_t *device, const uint16_t *unicstr)
{
  char loclstr[STRING_BUFFER_LENGTH*3+1]; // UTF-8 encoding is max 3 bytes per UCS2 char.

  loclstr[0]='\0';
  #if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
  PTPParams *params = (PTPParams *) device->params;
  char *stringp = (char *) unicstr;
  char *locp = loclstr;
  size_t nconv;
  size_t convlen = (ucs2_strlen(unicstr, 0)+1) * sizeof(uint16_t); // UCS-2 is 16 bit wide, include terminator
  size_t convmax = STRING_BUFFER_LENGTH*3;
  /* Do the conversion.  */
  nconv = iconv(params->cd_ucs2_to_locale, &stringp, &convlen, &locp, &convmax);
  if (nconv == (size_t) -1) {
    // Return partial string anyway.
    *locp = '\0';
  }
  #endif
  loclstr[STRING_BUFFER_LENGTH*3] = '\0';
  // Strip off any BOM, it's totally useless...
  if ((uint8_t) loclstr[0] == 0xEFU && (uint8_t) loclstr[1] == 0xBBU && (uint8_t) loclstr[2] == 0xBFU) {
    return strdup(loclstr+3);
  }
  return strdup(loclstr);
}

/**
 * Converts a UTF-8 string to a big-endian UTF-16 2-byte string
 * Actually just a UCS-2 internal conversion.
 *
 * @param device a pointer to the current device.
 * @param localstr the UTF-8 unicode string to convert
 * @return a UTF-16 string.
 */
uint16_t *utf8_to_utf16(LIBMTP_mtpdevice_t *device, const char *localstr)
{
  char unicstr[(STRING_BUFFER_LENGTH+1)*2]; // UCS2 encoding is 2 bytes per UTF-8 char.
  char *unip = unicstr;
  size_t nconv = 0;

  unicstr[0]='\0';
  unicstr[1]='\0';

  #if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
  PTPParams *params = (PTPParams *) device->params;
  char *stringp = (char *) localstr; // cast away "const"
  size_t convlen = strlen(localstr)+1; // utf8 bytes, include terminator
  size_t convmax = STRING_BUFFER_LENGTH*2;
  /* Do the conversion.  */
  nconv = iconv(params->cd_locale_to_ucs2, &stringp, &convlen, &unip, &convmax);
  #endif
  
  if (nconv == (size_t) -1) {
    // Return partial string anyway.
    unip[0] = '\0';
    unip[1] = '\0';
  }
  // make sure the string is null terminated
  unicstr[STRING_BUFFER_LENGTH*2] = '\0';
  unicstr[STRING_BUFFER_LENGTH*2+1] = '\0';

  // allocate the string to be returned
  // Note: can't use strdup since every other byte is a null byte
  int ret_len = ucs2_strlen((uint16_t*)unicstr, 0)*sizeof(uint16_t)+2;
  uint16_t* ret = malloc(ret_len);
  memcpy(ret,unicstr,(size_t)ret_len);
  return ret;
}

/**
 * This helper function simply removes any consecutive chars
 * > 0x7F and replaces them with an underscore. In UTF-8
 * consecutive chars > 0x7F represent one single character so
 * it has to be done like this (and it's elegant). It will only
 * shrink the string in size so no copying is needed.
 */
void strip_7bit_from_utf8(char *str)
{
  int i,j,k;
  i = j = 0;
  k = strlen(str);
  while (i < k) {
    if ((uint8_t) str[i] > 0x7FU) {
      str[j] = '_';
      i++;
      // Skip over any consecutive > 0x7F chars.
      while((uint8_t) str[i] > 0x7FU) {
	i++;
      }
    } else {
      str[j] = str[i];
      i++;
    }
    j++;
  }
  // Terminate stripped string...
  str[j] = '\0';
}
