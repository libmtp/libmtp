/** 
 * \file common.h
 * These headers are used by absolutely all sample programs.
 * Special quirks that apply to all samples go here.
 */
#include <libmtp.h>
#ifndef _MSC_VER
#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include <unistd.h>
#endif
#else
#include "..\windows\getopt.h"
#endif
