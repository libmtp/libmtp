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
 */

#include <stdlib.h>
#include <string.h>
#include <iconv.h>
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
 * UCS-2 string, eg a string which physically is 0x00 0x41 0x00 0x00
 * will return a value of 1.
 *
 * @param unicstr a UCS-2 Unicode string
 * @return the length of the string, in number of characters. If you 
 *         want to know the length in bytes, multiply this by two and
 *         add two (for zero terminator).
 */
int ucs2_strlen(uint16_t const * const unicstr)
{
  int length;
  
  /* Unicode strings are terminated with 2 * 0x00 */
  for(length = 0; unicstr[length] != 0x0000U; length ++);
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
  PTPParams *params = (PTPParams *) device->params;
  char *stringp = (char *) unicstr;
  char loclstr[STRING_BUFFER_LENGTH*3+1]; // UTF-8 encoding is max 3 bytes per UCS2 char.
  char *locp = loclstr;
  size_t nconv;
  size_t convlen = (ucs2_strlen(unicstr)+1) * sizeof(uint16_t); // UCS-2 is 16 bit wide, include terminator
  size_t convmax = STRING_BUFFER_LENGTH*3;
  
  loclstr[0]='\0';
  /* Do the conversion.  */
  nconv = iconv(params->cd_ucs2_to_locale, &stringp, &convlen, &locp, &convmax);
  if (nconv == (size_t) -1) {
    // Return partial string anyway.
    *locp = '\0';
  }
  loclstr[STRING_BUFFER_LENGTH*3] = '\0';
  // Strip off any BOM, it's totally useless...
  if ((uint8_t) loclstr[0] == 0xEFU && (uint8_t) loclstr[1] == 0xBBU && (uint8_t) loclstr[2] == 0xBFU) {
    return strdup(loclstr+3);
  }
  return strdup(loclstr);
}
