/**
 * \file unicode.c
 *
 * This file contains general Unicode string manipulation functions.
 * It mainly consist of functions for converting between UCS-2 (used on
 * the devices), UTF-8 (used by several applications) and 
 * ISO 8859-1 / Codepage 1252 (fallback).
 */

#include <stdlib.h>
#include <string.h>
#include "libmtp.h"
#include "unicode.h"
#include "util.h"

/**
 * The size of the buffer (in characters) used for creating string copies.
 */
#define STRING_BUFFER_LENGTH 256

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
int ucs2_strlen(const uint16_t *unicstr)
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
static int ucs2utf8len(const uint16_t *unicstr){
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
static uint16_t *ucs2strdup(const uint16_t *unicstr) {
  int length = ucs2_strlen(unicstr);
  uint8_t *data;
  
  data = (uint8_t *) malloc(length*2+2);
  if ( data == NULL ) {
    return NULL;
  }
  memcpy(data, unicstr, length*2+2);
  return (uint16_t *) data;
}


/**
 * Converts a Unicode UCS-2 2-byte string to a UTF-8
 * string.
 *
 * @param unicstr the UCS-2 unicode string to convert
 * @return a UTF-8 string.
 */
char *ucs2_to_utf8(const uint16_t *unicstr){
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
 * Convert a UTF-8 string to a unicode UCS-2 string.
 *
 * @param str the UTF-8 string to convert.
 * @return a pointer to a newly allocated UCS-2 string.
 */
uint16_t *utf8_to_ucs2(const unsigned char *str) {
  uint16_t *retval;
  int i;
  unsigned char buffer[STRING_BUFFER_LENGTH*2];    
  int length=0;
    
  for(i = 0; str[i] != '\0';) {
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
	  /* This character can always be handled correctly */
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
  /* Copy the buffer contents */
  buffer[length+1] = 0x00;
  buffer[length] = 0x00;

  retval = ucs2strdup((uint16_t *) buffer);
  return retval;
}
