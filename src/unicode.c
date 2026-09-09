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
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "unicode.h"

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
 * Converts a little-endian UTF-16 string into a UTF-8 string.
 * Actually this is just a UCS-2 internal conversion
 * routine that also strips off the BOM if there is one.
 * NOTE: UTF-16 to UTF-8 conversion is limited to worst-case 1024chars x 4utf8 + 1 buffer space.
 *
 * @param unicstr the UTF-16LE unicode string to convert
 * @return a UTF-8 string.
 */
char *utf16_to_utf8(const uint16_t *unicstr)
{
  char loclstr[STRING_BUFFER_LENGTH*4+1], *ploclstr; // UTF-8 encoding max 4 bytes per UTF-16 char.
  unsigned char *p8in; // intentionally load string little-endian regardless of host CPU endianess.
  uint32_t chin, chin2;
  int lin;

  ploclstr = loclstr;
  p8in = (unsigned char *)(unicstr);
  chin = *p8in++;

  /* Strip off any BOM, it's totally useless... */
  /* no 0xfffe, BOM sometimes used for id BE/LE */
  if (chin == 0xff && *p8in == 0xfe) {
    p8in++;
    chin = *p8in++;
  }

  lin = STRING_BUFFER_LENGTH * 4 - 3;
  while ((lin >= 0) && (chin |= ((*p8in++) << 8))) {
    /* look for {d800..dbff|dc00..dfff} code pair */
    if (chin >= 0xd800 && chin <= 0xdfff) {
      if (chin > 0xdbff) {
	chin = '_'; /* error, skip and continue */
      } else {
	chin2 = *p8in++;
	chin2 |= ((*p8in++) << 8);
	if (chin2 < 0xdC00 || chin2 > 0xdfff) {
	  chin = '_'; /* error, skip and continue */
	} else {
	  chin2 -= 0xdc00;
	  chin = ((chin - 0xd800) << 10) + chin2 + 0x10000;
	}
      }
    }
    /* exclude off-limits characters */
    chin2 = chin & 0xfffe;
    if ( chin2 == 0xfffe) {
      chin = '_';
    }
    /* convert chin to UTF-8 */
    if (chin > 127) {
      if (chin <= 0x7ff) {
	/* chin >= 0x80 && chin <= 0x7ff */
	*ploclstr++ = 0xc0 | (chin >> 6);
	--lin;
      } else {
	if (chin <= 0xffff) {
	  /* chin >= 0x800 && chin <= 0xffff */
	  *ploclstr++ = 0xe0 | (chin >> 12);
	  --lin;
	} else {
	  /* chin >= 0x10000 && chin <= 0x10ffff */
	  *ploclstr++ = 0xf0 | (chin >> 18);
	  *ploclstr++ = 0x80 | ((chin >> 12) & 0x3f);
	  lin -= 2;
	}
	*ploclstr++ = 0x80 | ((chin >> 6) & 0x3f);
	--lin;
      }
      chin = 0x80 | (chin & 0x3f);
    }
    *ploclstr++ = chin;
    --lin;
    chin = *p8in++;
  }
  *ploclstr++ = '\0';

  return strdup(loclstr);
}

/**
 * Converts a UTF-8 string to a UTF-16LE 2-byte, or 4-byte string.
 * Actually just a UCS-2 internal conversion.
 * mode=0: default, return empty zero length string if bad utf8 code encountered.
 * mode=1: return NULL if bad UTF-8 code encountered.
 *
 * @param localstr the UTF-8 unicode string to convert
 * @param mode count method, mode={0,1}
 * @return a UTF-16LE string. User frees string when done.
 */
uint16_t *utf8_to_utf16(const char *localstr, int mode)
{
  unsigned char unicstr[STRING_BUFFER_LENGTH*4+2]; // UTF-16 'max' encoding is 4 bytes per UTF-8 char.
  uint16_t *ret;
  unsigned char *p8in, *p8out;
  uint32_t chout, chout2;
  int err, lout;

  p8in = (unsigned char *)(localstr);
  p8out = unicstr;

  err = -1;
  lout = STRING_BUFFER_LENGTH * 2;
  while ((lout-- > 0) && (chout = (*p8in++))) {
    /* 110xxxxx 10xxxxxx */
    /* 1110xxxx 10xxxxxx 10xxxxxx */
    /* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    if (chout >= 0x80) {
      chout2 = (*p8in++) & 0xff;
      if (chout <= 0xbf || chout2 < 0x80 || chout2 >= 0xc0)
	goto err0_utf8_to_utf16;
      chout2 &= 0x3f;
      if (chout <= 0xdf) {
	chout = ((chout & 0x1f) << 6) | chout2;
      } else {
	if (*p8in < 0x80 || *p8in >= 0xc0)
	  goto err0_utf8_to_utf16;
	chout2 = (chout2 << 6) | ((*p8in++) & 0x3f);
	if (chout <= 0xef) {
	  chout = ((chout & 0xf) << 12) | chout2;
	} else {
	  if (*p8in < 0x80 || *p8in >= 0xc0)
	    goto err0_utf8_to_utf16;
	  chout = ((chout & 0x7) << 18) | (chout2 << 6) | ((*p8in++) & 0x3f);
	}
      }
    }
    if ((chout <= 0xdfff && chout >= 0xd800) || chout > 0x10ffff || ((chout & 0xfffe) == 0xfffe))
      goto err0_utf8_to_utf16;
    /* U' = yyyyyyyyyyxxxxxxxxxx -> U - 0x10000 */
    /* W1 = 110110yyyyyyyyyy -> 0xD800 + yyyyyyyyyy */
    /* W2 = 110111xxxxxxxxxx -> 0xDC00 + xxxxxxxxxx */
    if (chout > 0xffff) {
      lout--;
      chout -= 0x10000;
      chout2 = (chout >> 10) + 0xd800;
      chout = (chout & 0x3ff) + 0xdc00;
      *p8out++ = (chout2 & 0xff);
      *p8out++ = ((chout2 >> 8) & 0xff);
    }
    *p8out++ = (chout & 0xff);
    *p8out++ = ((chout >> 8) & 0xff);
  }
  err = 0;
err0_utf8_to_utf16:
  if (err) {
    if (mode)
      goto err1_utf8_to_utf16;
    p8out = unicstr;
  }
  *p8out++ = '\0';
  *p8out++ = '\0';

  chout = p8out - unicstr;
  ret = (uint16_t *)(malloc(chout));
  if (ret == NULL)
    goto err1_utf8_to_utf16;
  memcpy(ret, unicstr, chout);
  return ret;
err1_utf8_to_utf16:
  return NULL;
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
