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
#include "libmtp.h"
#include "unicode.h"
#include "util.h"

#ifdef USE_ICONV
#include <iconv.h>
// Default to not using iconv() until it's properly initialized.
static int use_fallbacks = 1;

/*
 * iconv converters, since these conversions are stateless
 * to/from UTF-8/UCS-2, we don't need any special thread
 * handling here. (Like making the iconv():erters part of
 * the device struct.)
 */
iconv_t cd_utf8_to_ucs2le;
iconv_t cd_ucs2le_to_utf8;
iconv_t cd_utf16_to_utf8;

void unicode_init(void)
{
  // printf("Using iconv()...\n");
  cd_utf16_to_utf8 = iconv_open("UTF-8", "UTF-16");
  cd_ucs2le_to_utf8 = iconv_open("UTF-8", "UCS-2LE");
  cd_utf8_to_ucs2le = iconv_open("UCS-2LE", "UTF-8");
  /*
   * If we cannot use the iconv implementation on this
   * machine, fall back on the old routines.
   */
  if (cd_utf16_to_utf8 == (iconv_t) -1 ||
      cd_ucs2le_to_utf8 == (iconv_t) -1 ||
      cd_utf8_to_ucs2le == (iconv_t) -1) {
    if (cd_utf16_to_utf8 != (iconv_t) -1)
      iconv_close(cd_utf16_to_utf8);
    if (cd_ucs2le_to_utf8 != (iconv_t) -1)
      iconv_close(cd_ucs2le_to_utf8);
    if (cd_utf8_to_ucs2le != (iconv_t) -1)
      iconv_close(cd_utf8_to_ucs2le);    
    use_fallbacks = 1;
  }
  // OK activate the iconv() stuff...
  use_fallbacks = 0;
}
#else
void unicode_init(void)
{
  return;
}
#endif

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
 * This routine returns the length in bytes that this
 * UCS-2 string would occupy if encoded as UTF-8
 *
 * @param unicstr the Unicode UCS-2 string to analyze
 * @return the number of bytes this string would occupy
 *         in UTF-8
 */
static int ucs2utf8len(uint16_t const * const unicstr){
  int length=0;
  int i;
  uint8_t *locstr = (uint8_t *) unicstr;
  for(i = 0; (locstr[i] | locstr[i+1]) != '\0'; i+=2) {
    if (locstr[i] == 0x00 && locstr[i+1] < 0x80)
      length ++;
    else if (locstr[i] < 0x08)
      length += 2;
    else
      length += 3;
  }
  return length;
}

/** 
 * Create a new, allocated UCS-2 string that is a copy
 * of the parameter
 *
 * @param unicstr the UCS-2 string to copy
 * @return a newly allocated copy of the string
 */
static uint16_t *ucs2_strdup(uint16_t const * const unicstr) {
  int length = ucs2_strlen(unicstr);
  uint8_t *data;
  
  data = (uint8_t *) malloc(length*2+2);
  if ( data == NULL ) {
    return NULL;
  }
  memcpy(data, unicstr, length*2+2);
  return (uint16_t *) data;
}

static char *builtin_ucs2le_to_utf8(uint16_t const * const unicstr) {
  char *data = NULL;
  int i = 0;
  int l = 0;
  int length8;
  uint8_t *locstr = (uint8_t *) unicstr;

  length8 = ucs2utf8len(unicstr);
  data = (char *) malloc(length8+1);
  if ( data == NULL ) {
    return NULL;
  }
  for(l = 0; (locstr[l] | locstr[l+1]) != '\0'; l += 2) {
    // This is for little-endian machines
    if (locstr[l+1] == 0x00 && locstr[l] < 0x80U) {
      data[i] = locstr[l];
      i ++;
    } else if (locstr[l+1] < 0x08) {
      data[i] = 0xc0 | (locstr[l+1]<<2 & 0x1C) | (locstr[l]>>6  & 0x03);
      data[i+1] = 0x80 | (locstr[l] & 0x3F);
      i+=2;
    } else {
      data[i] = 0xe0 | (locstr[l+1]>>4 & 0x0F);
      data[i+1] = 0x80 | (locstr[l+1]<<2 & 0x3C) | (locstr[l]>>6 & 0x03);
      data[i+2] = 0x80 | (locstr[l] & 0x3F);
      i+=3;
    }
  }
  /* Terminate string */
  data[i] = 0x00;

  return data;
}

/**
 * Converts a little-endian Unicode UCS-2 2-byte string
 * to a UTF-8 string.
 *
 * @param unicstr the UCS-2 unicode string to convert
 * @return a UTF-8 string.
 */
#ifdef USE_ICONV
char *ucs2le_to_utf8(uint16_t const * const unicstr) {
  if (use_fallbacks) {
    return builtin_ucs2le_to_utf8(unicstr);
  } else {
    char *stringp = (char *) unicstr;
    char loclstr[STRING_BUFFER_LENGTH*3+1]; // UTF-8 encoding is max 3 bytes per UCS2 char.
    char *locp = loclstr;
    size_t nconv;
    size_t convlen = (ucs2_strlen(unicstr)+1) * sizeof(uint16_t); // UCS-2 is 16 bit wide, include terminator
    size_t convmax = STRING_BUFFER_LENGTH*3;
    
    loclstr[0]='\0';
    /* Do the conversion.  */
    nconv = iconv(cd_ucs2le_to_utf8, &stringp, &convlen, &locp, &convmax);
    if (nconv == (size_t) -1) {
      return NULL;
    }
    loclstr[STRING_BUFFER_LENGTH*3] = '\0';
    return strdup(loclstr);
  }
}
#else
char *ucs2le_to_utf8(uint16_t const * const unicstr) {
  return builtin_ucs2le_to_utf8(unicstr);
}
#endif

/**
 * Converts a big-endian UTF-16 2-byte string
 * to a UTF-8 string.
 *
 * @param unicstr the UTF-16 unicode string to convert
 * @return a UTF-8 string.
 */
#ifdef USE_ICONV
char *utf16_to_utf8(const uint16_t *unicstr)
{
  if (use_fallbacks) {
    if (unicstr[0] == 0xFFFEU || unicstr[0] == 0xFEFFU) {
      // Consume BOM, endianness is fixed at network layer.
      return builtin_ucs2le_to_utf8(unicstr+1);
    }
    return builtin_ucs2le_to_utf8(unicstr);
  } else {
    char *stringp = (char *) unicstr;
    char loclstr[STRING_BUFFER_LENGTH*3+1]; // UTF-8 encoding is max 3 bytes per UCS2 char.
    char *locp = loclstr;
    size_t nconv;
    size_t convlen = (ucs2_strlen(unicstr)+1) * sizeof(uint16_t); // UCS-2 is 16 bit wide, include terminator
    size_t convmax = STRING_BUFFER_LENGTH*3;

    loclstr[0]='\0';
    /* Do the conversion.  */
    nconv = iconv(cd_utf16_to_utf8, &stringp, &convlen, &locp, &convmax);
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
}
#else
char *utf16_to_utf8(const uint16_t *unicstr)
{
  if (unicstr[0] == 0xFFFEU) {
    // FIXME: big-endian, swap bytes around or something
    return NULL;
  }
  if (unicstr[0] == 0xFEFFU) {
    // Consume BOM
    return builtin_ucs2le_to_utf8(unicstr+1);
  }
  return builtin_ucs2le_to_utf8(unicstr);
}
#endif


static uint16_t *builtin_utf8_to_ucs2le(unsigned char const * const str) {
  uint16_t *retval;
  int i;
  unsigned char buffer[STRING_BUFFER_LENGTH*2];    
  int length=0;
    
  for(i = 0; str[i] != '\0' && length < (STRING_BUFFER_LENGTH*2-2);) {
    if (str[i] < 0x80) {
      buffer[length+1] = 0x00;
      buffer[length] = str[i];
      length += 2;
      i++;
    } else {
      unsigned char numbytes = 0;
      unsigned char lenbyte = 0;
      
      /* Read the number of encoded bytes */
      lenbyte = str[i];
      while (lenbyte & 0x80) {
	numbytes++;
	lenbyte = lenbyte<<1;
      }
      /* UCS-2 can handle no more than 3 UTF-8 encoded bytes */
      if (numbytes <= 3) {
	if (numbytes == 2 && str[i+1] > 0x80) {
	  buffer[length+1] = (str[i]>>2 & 0x07);
	  buffer[length] = (str[i]<<6 & 0xC0) | (str[i+1] & 0x3F);
	  i += 2;
	  length += 2;
	} else if (numbytes == 3 && str[i+1] > 0x80 && str[i+2] > 0x80) {
	  buffer[length+1] = (str[i]<<4 & 0xF0) | (str[i+1]>>2 & 0x0F);
	  buffer[length]= (str[i+1]<<6 & 0xC0) | (str[i+2] & 0x3F);
	  i += 3;
	  length += 2;
	} else {
	  /* Abnormal string character, just skip */
	  i += numbytes;
	}
      } else {
	/* Just skip that character */
	i += numbytes;
      }
    }
  }
  // Terminate string
  buffer[length] = 0x00;
  buffer[length+1] = 0x00;

  // Copy the buffer contents
  retval = ucs2_strdup((uint16_t *) buffer);
  return retval;
}

/**
 * Convert a UTF-8 string to a little-endian Unicode
 * UCS-2 string.
 *
 * @param str the UTF-8 string to convert.
 * @return a pointer to a newly allocated UCS-2 string.
 */
#ifdef USE_ICONV
uint16_t *utf8_to_ucs2le(unsigned char const * const str) {
  if (use_fallbacks) {
    return builtin_utf8_to_ucs2le(str);
  } else {
    uint16_t ucs2str[STRING_BUFFER_LENGTH+1];
    char *ucs2strp = (char *) ucs2str;
    char *stringp = (char *) str;
    size_t nconv;
    size_t convlen = strlen((char*)str) + 1; // Include the terminator in the conversion
    size_t convmax = STRING_BUFFER_LENGTH * 2; // Includes the terminator
    
    ucs2str[0] = 0x0000U;
    // memset(ucs2strp, 0, (STRING_BUFFER_LENGTH+1)*sizeof(uint16_t));
    nconv = iconv (cd_utf8_to_ucs2le, &stringp, &convlen, &ucs2strp, &convmax);
    if (nconv == (size_t) -1) {
      return NULL;
    }
    return ucs2_strdup(ucs2str);
  }
}
#else
uint16_t *utf8_to_ucs2le(unsigned char const * const str) {
  return builtin_utf8_to_ucs2le(str);
}
#endif
