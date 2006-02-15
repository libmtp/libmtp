/**
 * \file util.c
 *
 * This file contains generic utility functions such as can be
 * used for debugging for example.
 */

/* MSVC does not have these */
#ifndef _MSC_VER
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libmtp.h"
#include "util.h"

/**
 * This dumps out a number of bytes to a textual, hexadecimal
 * dump.
 *
 * @param f the file to dump to (e.g. stdout or stderr)
 * @param buf a pointer to the buffer containing the bytes to
 *            be dumped out in hex
 * @param n the number of bytes to dump from this buffer
 */
void data_dump (FILE *f, void *buf, uint32_t n)
{
  unsigned char *bp = (unsigned char *) buf;
  uint32_t i;
  
  for (i = 0; i < n; i++) {
    fprintf(f, "%02x ", *bp);
    bp++;
  }
  fprintf(f, "\n");
}

/**
 * This dumps out a number of bytes to a textual, hexadecimal
 * dump, and also prints out the string ASCII representation
 * for each line of bytes. It will also print the memory address
 * offset from a certain boundry.
 *
 * @param f the file to dump to (e.g. stdout or stderr)
 * @param buf a pointer to the buffer containing the bytes to
 *            be dumped out in hex
 * @param n the number of bytes to dump from this buffer
 * @param dump_boundry the address offset to start at (usually 0)
 */
void data_dump_ascii (FILE *f, void *buf, uint32_t n, uint32_t dump_boundry)
{
  uint32_t remain = n;
  uint32_t ln, lc;
  int i;
  unsigned char *bp = (unsigned char *) buf;
  
  lc = 0;
  while (remain) {
    fprintf(f, "\t%04x:", dump_boundry);
    
    ln = ( remain > 16 ) ? 16 : remain;
    
    for (i = 0; i < ln; i++) {
      if ( ! (i%2) ) fprintf(f, " ");
      fprintf(f, "%02x", bp[16*lc+i]);
    }
    
    if ( ln < 16 ) {
      int width = ((16-ln)/2)*5 + (2*(ln%2));
      fprintf(f, "%*.*s", width, width, "");
    }
    
    fprintf(f, "\t");
    for (i = 0; i < ln; i++) {
      unsigned char ch= bp[16*lc+i];
      fprintf(f, "%c", ( ch >= 0x20 && ch <= 0x7e ) ? 
	      ch : '.');
    }
    fprintf(f, "\n");
    
    lc++;
    remain -= ln;
    dump_boundry += ln;
  }
}
