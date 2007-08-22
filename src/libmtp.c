/**
 * \file libmtp.c
 *
 * Copyright (C) 2005-2007 Linus Walleij <triad@df.lth.se>
 * Copyright (C) 2005-2007 Richard A. Low <richard@wentnet.com>
 * Copyright (C) 2007 Ted Bullock <tbullock@canada.com>
 * Copyright (C) 2007 Tero Saarni <tero.saarni@gmail.com>
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
 * This file provides an interface "glue" to the underlying
 * PTP implementation from libgphoto2. It uses some local
 * code to convert from/to UTF-8 (stored in unicode.c/.h)
 * and some small utility functions, mainly for debugging
 * (stored in util.c/.h).
 *
 * The three PTP files (ptp.c, ptp.h and ptp-pack.c) are
 * plain copied from the libhphoto2 codebase.
 *
 * The files libusb-glue.c/.h are just what they say: an
 * interface to libusb for the actual, physical USB traffic.
 */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "libmtp.h"
#include "unicode.h"
#include "ptp.h"
#include "libusb-glue.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _MSC_VER // For MSVC++
#define USE_WINDOWS_IO_H
#include <io.h>
#endif

/*
 * This is a mapping between libmtp internal MTP filetypes and
 * the libgphoto2/PTP equivalent defines. We need this because
 * otherwise the libmtp.h device has to be dependent on ptp.h
 * to be installed too, and we don't want that.
 */
//typedef struct filemap_struct filemap_t;
typedef struct filemap_struct {
  char *description; /**< Text description for the file type */
  LIBMTP_filetype_t id; /**< LIBMTP internal type for the file type */
  uint16_t ptp_id; /**< PTP ID for the filetype */
  struct filemap_struct *next;
} filemap_t;

// Global variables
// This holds the global filetype mapping table
static filemap_t *filemap = NULL;

/*
 * Forward declarations of local (static) functions.
 */
static int register_filetype(char const * const description, LIBMTP_filetype_t const id,
			     uint16_t const ptp_id);
static void init_filemap();
static void add_error_to_errorstack(LIBMTP_mtpdevice_t *device,
				    LIBMTP_error_number_t errornumber,
				    char const * const error_text);
static void add_ptp_error_to_errorstack(LIBMTP_mtpdevice_t *device,
					uint16_t ptp_error,
					char const * const error_text);
static void flush_handles(LIBMTP_mtpdevice_t *device);
static void get_handles_recursively(LIBMTP_mtpdevice_t *device, PTPParams *params, PTPObjectHandles *handles, uint32_t parent);
static void free_storage_list(LIBMTP_mtpdevice_t *device);
static int sort_storage_by(LIBMTP_mtpdevice_t *device, int const sortby);
static uint32_t get_first_storageid(LIBMTP_mtpdevice_t *device);
static int get_first_storage_freespace(LIBMTP_mtpdevice_t *device,uint64_t *freespace);
static int check_if_file_fits(LIBMTP_mtpdevice_t *device, uint64_t const filesize);
static uint16_t map_libmtp_type_to_ptp_type(LIBMTP_filetype_t intype);
static LIBMTP_filetype_t map_ptp_type_to_libmtp_type(uint16_t intype);
static int get_device_unicode_property(LIBMTP_mtpdevice_t *device,
				       char **unicstring, uint16_t property);
static uint16_t adjust_u16(uint16_t val, PTPObjectPropDesc *opd);
static uint32_t adjust_u32(uint32_t val, PTPObjectPropDesc *opd);
static char *get_string_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id);
static uint64_t get_u64_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
                                    uint16_t const attribute_id, uint64_t const value_default);
static uint32_t get_u32_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
				    uint16_t const attribute_id, uint32_t const value_default);
static uint16_t get_u16_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id, uint16_t const value_default);
static uint8_t get_u8_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				  uint16_t const attribute_id, uint8_t const value_default);
static int set_object_string(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			     uint16_t const attribute_id, char const * const string);
static int set_object_u32(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint32_t const value);
static int set_object_u16(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint16_t const value);
static int set_object_u8(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			 uint16_t const attribute_id, uint8_t const value);
static void get_track_metadata(LIBMTP_mtpdevice_t *device, uint16_t objectformat,
			       LIBMTP_track_t *track);
static int create_new_abstract_list(LIBMTP_mtpdevice_t *device,
				    char const * const name,
				    char const * const artist,
				    char const * const genre,
				    uint32_t const parenthandle,
				    uint16_t const objectformat,
				    char const * const suffix,
				    uint32_t * const newid,
				    uint32_t const * const tracks,
				    uint32_t const no_tracks);
static MTPPropList *new_mtp_prop_entry();
static MTPPropList *add_mtp_prop_to_proplist(MTPPropList *proplist, MTPPropList *prop);
static void destroy_mtp_prop_list(MTPPropList *proplist);
static void add_object_to_cache(LIBMTP_mtpdevice_t *device, uint32_t handle);
static void update_metadata_cache(LIBMTP_mtpdevice_t *device, uint32_t handle);

/**
 * Create a new file mapping entry
 * @return a newly allocated filemapping entry.
 */
static filemap_t *new_filemap_entry()
{
  filemap_t *filemap;

  filemap = (filemap_t *)malloc(sizeof(filemap_t));

  if( filemap != NULL ) {
    filemap->description = NULL;
    filemap->id = LIBMTP_FILETYPE_UNKNOWN;
    filemap->ptp_id = PTP_OFC_Undefined;
    filemap->next = NULL;
  }

  return filemap;
}

/**
 * Register an MTP or PTP filetype for data retrieval
 *
 * @param description Text description of filetype
 * @param id libmtp internal filetype id
 * @param ptp_id PTP filetype id
 * @return 0 for success any other value means error.
*/
static int register_filetype(char const * const description, LIBMTP_filetype_t const id,
			     uint16_t const ptp_id)
{
  filemap_t *new = NULL, *current;

  // Has this LIBMTP filetype been registered before ?
  current = filemap;
  while (current != NULL) {
    if(current->id == id) {
      break;
    }
    current = current->next;
  }

  // Create the entry
  if(current == NULL) {
    new = new_filemap_entry();
    if(new == NULL) {
      return 1;
    }

    new->id = id;
    if(description != NULL) {
      new->description = strdup(description);
    }
    new->ptp_id = ptp_id;

    // Add the entry to the list
    if(filemap == NULL) {
      filemap = new;
    } else {
      current = filemap;
      while (current->next != NULL ) current=current->next;
      current->next = new;
    }
    // Update the existing entry
  } else {
    if (current->description != NULL) {
      free(current->description);
    }
    current->description = NULL;
    if(description != NULL) {
      current->description = strdup(description);
    }
    current->ptp_id = ptp_id;
  }

  return 0;
}

static void init_filemap()
{
  register_filetype("MediaCard", LIBMTP_FILETYPE_MEDIACARD, PTP_OFC_MTP_MediaCard);
  register_filetype("RIFF WAVE file", LIBMTP_FILETYPE_WAV, PTP_OFC_WAV);
  register_filetype("ISO MPEG-1 Audio Layer 3", LIBMTP_FILETYPE_MP3, PTP_OFC_MP3);
  register_filetype("ISO MPEG-1 Audio Layer 2", LIBMTP_FILETYPE_MP2, PTP_OFC_MTP_MP2);
  register_filetype("Microsoft Windows Media Audio", LIBMTP_FILETYPE_WMA, PTP_OFC_MTP_WMA);
  register_filetype("Ogg container format", LIBMTP_FILETYPE_OGG, PTP_OFC_MTP_OGG);
  register_filetype("Free Lossless Audio Codec (FLAC)", LIBMTP_FILETYPE_FLAC, PTP_OFC_MTP_FLAC);
  register_filetype("Advanced Audio Coding (AAC)/MPEG-2 Part 7/MPEG-4 Part 3", LIBMTP_FILETYPE_AAC, PTP_OFC_MTP_AAC);
  register_filetype("MPEG-4 Part 14 Container Format (Audio Empahsis)", LIBMTP_FILETYPE_M4A, PTP_OFC_MTP_M4A);
  register_filetype("MPEG-4 Part 14 Container Format (Audio+Video Empahsis)", LIBMTP_FILETYPE_MP4, PTP_OFC_MTP_MP4);
  register_filetype("Audible.com Audio Codec", LIBMTP_FILETYPE_AUDIBLE, PTP_OFC_MTP_AudibleCodec);
  register_filetype("Undefined audio file", LIBMTP_FILETYPE_UNDEF_AUDIO, PTP_OFC_MTP_UndefinedAudio);
  register_filetype("Microsoft Windows Media Video", LIBMTP_FILETYPE_WMV, PTP_OFC_MTP_WMV);
  register_filetype("Audio Video Interleave", LIBMTP_FILETYPE_AVI, PTP_OFC_AVI);
  register_filetype("MPEG video stream", LIBMTP_FILETYPE_MPEG, PTP_OFC_MPEG);
  register_filetype("Microsoft Advanced Systems Format", LIBMTP_FILETYPE_ASF, PTP_OFC_ASF);
  register_filetype("Apple Quicktime container format", LIBMTP_FILETYPE_QT, PTP_OFC_QT);
  register_filetype("Undefined video file", LIBMTP_FILETYPE_UNDEF_VIDEO, PTP_OFC_MTP_UndefinedVideo);
  register_filetype("JPEG file", LIBMTP_FILETYPE_JPEG, PTP_OFC_EXIF_JPEG);
  register_filetype("JP2 file", LIBMTP_FILETYPE_JP2, PTP_OFC_JP2);
  register_filetype("JPX file", LIBMTP_FILETYPE_JPX, PTP_OFC_JPX);
  register_filetype("JFIF file", LIBMTP_FILETYPE_JFIF, PTP_OFC_JFIF);
  register_filetype("TIFF bitmap file", LIBMTP_FILETYPE_TIFF, PTP_OFC_TIFF);
  register_filetype("BMP bitmap file", LIBMTP_FILETYPE_BMP, PTP_OFC_BMP);
  register_filetype("GIF bitmap file", LIBMTP_FILETYPE_GIF, PTP_OFC_GIF);
  register_filetype("PICT bitmap file", LIBMTP_FILETYPE_PICT, PTP_OFC_PICT);
  register_filetype("Portable Network Graphics", LIBMTP_FILETYPE_PNG, PTP_OFC_PNG);
  register_filetype("Microsoft Windows Image Format", LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT, PTP_OFC_MTP_WindowsImageFormat);
  register_filetype("VCalendar version 1", LIBMTP_FILETYPE_VCALENDAR1, PTP_OFC_MTP_vCalendar1);
  register_filetype("VCalendar version 2", LIBMTP_FILETYPE_VCALENDAR2, PTP_OFC_MTP_vCalendar2);
  register_filetype("VCard version 2", LIBMTP_FILETYPE_VCARD2, PTP_OFC_MTP_vCard2);
  register_filetype("VCard version 3", LIBMTP_FILETYPE_VCARD3, PTP_OFC_MTP_vCard3);
  register_filetype("Undefined Windows executable file", LIBMTP_FILETYPE_WINEXEC, PTP_OFC_MTP_UndefinedWindowsExecutable);
  register_filetype("Text file", LIBMTP_FILETYPE_TEXT, PTP_OFC_Text);
  register_filetype("HTML file", LIBMTP_FILETYPE_HTML, PTP_OFC_HTML);
  register_filetype("XML file", LIBMTP_FILETYPE_XML, PTP_OFC_MTP_XMLDocument);
  register_filetype("DOC file", LIBMTP_FILETYPE_DOC, PTP_OFC_MTP_MSWordDocument);
  register_filetype("XLS file", LIBMTP_FILETYPE_XLS, PTP_OFC_MTP_MSExcelSpreadsheetXLS);
  register_filetype("PPT file", LIBMTP_FILETYPE_PPT, PTP_OFC_MTP_MSPowerpointPresentationPPT);
  register_filetype("MHT file", LIBMTP_FILETYPE_MHT, PTP_OFC_MTP_MHTCompiledHTMLDocument);
  register_filetype("Firmware file", LIBMTP_FILETYPE_FIRMWARE, PTP_OFC_MTP_Firmware);
  register_filetype("Undefined filetype", LIBMTP_FILETYPE_UNKNOWN, PTP_OFC_Undefined);
}

/**
 * Returns the PTP filetype that maps to a certain libmtp internal file type.
 * @param intype the MTP library interface type
 * @return the PTP (libgphoto2) interface type
 */
static uint16_t map_libmtp_type_to_ptp_type(LIBMTP_filetype_t intype)
{
  filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->id == intype) {
      return current->ptp_id;
    }
    current = current->next;
  }
  // printf("map_libmtp_type_to_ptp_type: unknown filetype.\n");
  return PTP_OFC_Undefined;
}


/**
 * Returns the PTP internal filetype that maps to a certain libmtp
 * interface file type.
 * @param intype the PTP (libgphoto2) interface type
 * @return the MTP library interface type
 */
static LIBMTP_filetype_t map_ptp_type_to_libmtp_type(uint16_t intype)
{
  filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->ptp_id == intype) {
      return current->id;
    }
    current = current->next;
  }
  // printf("map_ptp_type_to_libmtp_type: unknown filetype.\n");
  return LIBMTP_FILETYPE_UNKNOWN;
}


/**
 * Initialize the library. You are only supposed to call this
 * one, before using the library for the first time in a program.
 * Never re-initialize libmtp!
 *
 * The only thing this does at the moment is to initialise the
 * filetype mapping table.
 */
void LIBMTP_Init(void)
{
  init_filemap();
  return;
}


/**
 * This helper function returns a textual description for a libmtp
 * file type to be used in dialog boxes etc.
 * @param intype the libmtp internal filetype to get a description for.
 * @return a string representing the filetype, this must <b>NOT</b>
 *         be free():ed by the caller!
 */
char const * LIBMTP_Get_Filetype_Description(LIBMTP_filetype_t intype)
{
  filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->id == intype) {
      return current->description;
    }
    current = current->next;
  }

  return "Unknown filetype";
}

static uint16_t adjust_u16(uint16_t val, PTPObjectPropDesc *opd)
{
  switch (opd->FormFlag) {
  case PTP_DPFF_Range:
    if (val < opd->FORM.Range.MinimumValue.u16) {
      return opd->FORM.Range.MinimumValue.u16;
    }
    if (val > opd->FORM.Range.MaximumValue.u16) {
      return opd->FORM.Range.MaximumValue.u16;
    }
    // Round down to last step.
    if (val % opd->FORM.Range.StepSize.u16 != 0) {
      return val - (val % opd->FORM.Range.StepSize.u16);
    }
    return val;
    break;
  case PTP_DPFF_Enumeration:
    {
      int i;
      uint16_t bestfit = opd->FORM.Enum.SupportedValue[0].u16;
      
      for (i=0; i<opd->FORM.Enum.NumberOfValues; i++) {
	if (val == opd->FORM.Enum.SupportedValue[i].u16) {
	  return val;
	}
	// Rough guess of best fit
	if (opd->FORM.Enum.SupportedValue[i].u16 < val) {
	  bestfit = opd->FORM.Enum.SupportedValue[i].u16;
	}
      }
      // Just some default that'll work.
      return bestfit;
    }
  default:
    // Will accept any value
    break;
  }
  return val;
}

static uint32_t adjust_u32(uint32_t val, PTPObjectPropDesc *opd)
{
  switch (opd->FormFlag) {
  case PTP_DPFF_Range:
    if (val < opd->FORM.Range.MinimumValue.u32) {
      return opd->FORM.Range.MinimumValue.u32;
    }
    if (val > opd->FORM.Range.MaximumValue.u32) {
      return opd->FORM.Range.MaximumValue.u32;
    }
    // Round down to last step.
    if (val % opd->FORM.Range.StepSize.u32 != 0) {
      return val - (val % opd->FORM.Range.StepSize.u32);
    }
    return val;
    break;
  case PTP_DPFF_Enumeration:
    {
      int i;
      uint32_t bestfit = opd->FORM.Enum.SupportedValue[0].u32;

      for (i=0; i<opd->FORM.Enum.NumberOfValues; i++) {
	if (val == opd->FORM.Enum.SupportedValue[i].u32) {
	  return val;
	}
	// Rough guess of best fit
	if (opd->FORM.Enum.SupportedValue[i].u32 < val) {
	  bestfit = opd->FORM.Enum.SupportedValue[i].u32;
	}
      }
      // Just some default that'll work.
      return bestfit;
    }
  default:
    // Will accept any value
    break;
  }
  return val;
}


/**
 * Retrieves a string from an object
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @return valid string or NULL on failure. The returned string
 *         must bee <code>free()</code>:ed by the caller after
 *         use.
 */
static char *get_string_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id)
{
  PTPPropertyValue propval;
  char *retstring = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if ( device == NULL || object_id == 0) {
    return NULL;
  }
  
  // This O(n) search should not be used so often, since code
  // using the cached properties don't usually call this function.
  if (params->proplist) {
    MTPPropList    *prop = params->proplist;

    while (prop) {
      if (object_id == prop->ObjectHandle && attribute_id == prop->property) {
        if (prop->propval.str != NULL)
          return strdup(prop->propval.str);
        else
          return NULL;
      }
      prop = prop->next;
    }
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_STR);
  if (ret == PTP_RC_OK) {
    if (propval.str != NULL) {
      retstring = (char *) strdup(propval.str);
      free(propval.str);
    }
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_string_from_object(): could not get object string.");
  }

  return retstring;
}

/**
* Retrieves an unsigned 64-bit integer from an object attribute
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param value_default Default value to return on failure
 * @return the value
 */
static uint64_t get_u64_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
                                    uint16_t const attribute_id, uint64_t const value_default)
{
  PTPPropertyValue propval;
  uint64_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;
  
  if ( device == NULL ) {
    return value_default;
  }
  
  // This O(n) search should not be used so often, since code
  // using the cached properties don't usually call this function.
  if (params->proplist) {
    MTPPropList    *prop = params->proplist;
    
    while (prop) {
      if (object_id == prop->ObjectHandle && attribute_id == prop->property) {
        return prop->propval.u64;
      }
      prop = prop->next;
    }
  }
  
  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT64);
  if (ret == PTP_RC_OK) {
    retval = propval.u64;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u64_from_object(): could not get unsigned 64bit integer from object.");
  }
  
  return retval;
}

/**
 * Retrieves an unsigned 32-bit integer from an object attribute
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param value_default Default value to return on failure
 * @return the value
 */
static uint32_t get_u32_from_object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
				    uint16_t const attribute_id, uint32_t const value_default)
{
  PTPPropertyValue propval;
  uint32_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if ( device == NULL ) {
    return value_default;
  }

  // This O(n) search should not be used so often, since code
  // using the cached properties don't usually call this function.
  if (params->proplist) {
    MTPPropList    *prop = params->proplist;
    
    while (prop) {
      if (object_id == prop->ObjectHandle && attribute_id == prop->property) {
	return prop->propval.u32;
      }
      prop = prop->next;
    }
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT32);
  if (ret == PTP_RC_OK) {
    retval = propval.u32;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u32_from_object(): could not get unsigned 32bit integer from object.");
  }

  return retval;
}

/**
 * Retrieves an unsigned 16-bit integer from an object attribute
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param value_default Default value to return on failure
 * @return a value
 */
static uint16_t get_u16_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id, uint16_t const value_default)
{
  PTPPropertyValue propval;
  uint16_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if ( device == NULL ) {
    return value_default;
  }

  // This O(n) search should not be used so often, since code
  // using the cached properties don't usually call this function.
  if (params->proplist) {
    MTPPropList    *prop = params->proplist;
    
    while (prop) {
      if (object_id == prop->ObjectHandle && attribute_id == prop->property) {
	return prop->propval.u16;
      }
      prop = prop->next;
    }
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT16);
  if (ret == PTP_RC_OK) {
    retval = propval.u16;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u16_from_object(): could not get unsigned 16bit integer from object.");
  }

  return retval;
}

/**
 * Retrieves an unsigned 8-bit integer from an object attribute
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param value_default Default value to return on failure
 * @return a value
 */
static uint8_t get_u8_from_object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				  uint16_t const attribute_id, uint8_t const value_default)
{
  PTPPropertyValue propval;
  uint8_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if ( device == NULL ) {
    return value_default;
  }

  // This O(n) search should not be used so often, since code
  // using the cached properties don't usually call this function.
  if (params->proplist) {
    MTPPropList    *prop = params->proplist;
    
    while (prop) {
      if (object_id == prop->ObjectHandle && attribute_id == prop->property) {
	return prop->propval.u8;
      }
      prop = prop->next;
    }
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT8);
  if (ret == PTP_RC_OK) {
    retval = propval.u8;
  } else {
    add_ptp_error_to_errorstack(device, ret, "get_u8_from_object(): could not get unsigned 8bit integer from object.");
  }

  return retval;
}

/**
 * Sets an object attribute from a string
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param string string value to set
 * @return 0 on success, any other value means failure
 */
static int set_object_string(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			     uint16_t const attribute_id, char const * const string)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL || string == NULL) {
    return -1;
  }
  
  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_string(): could not set object string: "
				"PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  propval.str = (char *) string;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_string(): could not set object string.");
    return -1;
  }

  return 0;
}


/**
 * Sets an object attribute from an unsigned 32-bit integer
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param value 32-bit unsigned integer to set
 * @return 0 on success, any other value means failure
 */
static int set_object_u32(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint32_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return -1;
  }

  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_u32(): could not set unsigned 32bit integer property: "
				"PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  
  propval.u32 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT32);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_u32(): could not set unsigned 32bit integer property.");
    return -1;
  }

  return 0;
}

/**
 * Sets an object attribute from an unsigned 16-bit integer
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param value 16-bit unsigned integer to set
 * @return 0 on success, any other value means failure
 */
static int set_object_u16(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint16_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return 1;
  }

  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_u16(): could not set unsigned 16bit integer property: "
				"PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  propval.u16 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT16);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_u16(): could not set unsigned 16bit integer property.");
    return 1;
  }

  return 0;
}

/**
 * Sets an object attribute from an unsigned 8-bit integer
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @param value 8-bit unsigned integer to set
 * @return 0 on success, any other value means failure
 */
static int set_object_u8(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			 uint16_t const attribute_id, uint8_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return 1;
  }

  if (!ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "set_object_u8(): could not set unsigned 8bit integer property: "
			    "PTP_OC_MTP_SetObjectPropValue not supported.");
    return -1;
  }
  propval.u8 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT8);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "set_object_u8(): could not set unsigned 8bit integer property.");
    return 1;
  }

  return 0;
}

/**
 * Get the first (as in "first in the list of") connected MTP device.
 * @return a device pointer.
 * @see LIBMTP_Get_Connected_Devices()
 */
LIBMTP_mtpdevice_t *LIBMTP_Get_First_Device(void)
{
  LIBMTP_mtpdevice_t *first_device = NULL;
  
  switch(LIBMTP_Get_Connected_Devices(&first_device))
  {
    /* Specific Errors or Messages that connect_mtp_devices should return */
  case LIBMTP_ERROR_NO_DEVICE_ATTACHED:
    fprintf(stderr, "LIBMTP_Get_First_Device: No Devices Attached\n");
    return NULL;

  case LIBMTP_ERROR_CONNECTING:
    fprintf(stderr, "LIBMTP_Get_First_Device: Error Connecting\n");
    return NULL;

  case LIBMTP_ERROR_MEMORY_ALLOCATION:
    fprintf(stderr, "LIBMTP_Get_First_Device: Memory Alloc Error\n");
    return NULL;
  
  /* Unknown general errors - This should never execute */
  case LIBMTP_ERROR_GENERAL:
  default:
    fprintf(stderr, "LIBMTP_Get_First_Device: Unknown Connection Error\n");
    return NULL;
  
  /* Successfully connect at least one device, so continue */
  case LIBMTP_ERROR_NONE:
    break;
  }

  /* Only return the first device, release the rest */
  if(first_device->next != NULL)
  {
    LIBMTP_Release_Device_List(first_device->next);
    first_device->next = NULL;
  }
  
  return first_device;
}

/**
 * Recursive function that adds MTP devices to a linked list
 * @param numdevices The number of detected USB devices
 * @param interface_number Dynamic array of interface numbers
 * @param params Dynamic array of PTP parameters
 * @param ptp_usb Dynamic array of USB PTP devices
 * @return a device pointer to a newly created mtpdevice (used in linked
 * list creation
 */
static LIBMTP_mtpdevice_t * create_usb_mtp_devices(mtpdevice_list_t *devices)
{
  uint8_t i = 1;
  LIBMTP_mtpdevice_t *mtp_device_list = NULL;
  LIBMTP_mtpdevice_t *current_device = NULL;
  PTPParams *current_params;
  mtpdevice_list_t *tmplist = devices;
  
  while (tmplist != NULL) {
    LIBMTP_mtpdevice_t *mtp_device;
    
    /* Clear any handlers */
    tmplist->params->handles.Handler = NULL;
    tmplist->params->objectinfo = NULL;
    tmplist->params->proplist = NULL;
    
    /* Allocate dynamic space for our device */
    mtp_device = (LIBMTP_mtpdevice_t *) malloc(sizeof(LIBMTP_mtpdevice_t));
    
    /* Check if there was a memory allocation error */
    if(mtp_device == NULL) {
      /* There has been an memory allocation error. We are going to ignore this
	 device and attempt to continue */
      
      /* TODO: This error statement could probably be a bit more robust */
      fprintf(stderr, "LIBMTP PANIC: connect_usb_devices encountered a memory "
	      "allocation error with device %u, trying to continue",
	      i);
      
      /* Prevent memory leaks for this device */
      free(tmplist->ptp_usb);
      tmplist->ptp_usb = NULL;
      
      free(tmplist->params);
      tmplist->params = NULL;
      
      /* We have freed a bit of memory so try again with the next device */
      continue;
    }
    
    /* Copy device information to mtp_device structure */
    mtp_device->interface_number = tmplist->interface_number;
    mtp_device->params = tmplist->params;
    mtp_device->usbinfo = tmplist->ptp_usb;
    current_params = tmplist->params;
    
    /* Cache the device information for later use */
    if (ptp_getdeviceinfo(current_params,
			  &current_params->deviceinfo) != PTP_RC_OK) {
      fprintf(stderr, "LIBMTP PANIC: Unable to read device information on device "
	      "number %u, trying to continue", i);
      
      /* Prevent memory leaks for this device */
      free(tmplist->ptp_usb);
      tmplist->ptp_usb = NULL;
      
      free(current_params);
      current_params = NULL;
      free(mtp_device);
      
      /* try again with the next device */
      continue;
    }
    
    /* No Errors yet for this device */
    mtp_device->errorstack = NULL;
    
    /* Cache the device information */
    if (ptp_getdeviceinfo(current_params,
			  &current_params->deviceinfo) != PTP_RC_OK) {
      add_error_to_errorstack(mtp_device,
			      LIBMTP_ERROR_CONNECTING,
			      "Unable to read device information. Recommend "
			      "disconnecting this device\n");
    }
    
    /* Default Max Battery Level, we will adjust this if possible */
    mtp_device->maximum_battery_level = 100;
    
    /* Check if device supports reading maximum battery level */
    if(ptp_property_issupported( current_params,
				 PTP_DPC_BatteryLevel)) {
      PTPDevicePropDesc dpd;
      
      /* Try to read maximum battery level */
      if(ptp_getdevicepropdesc(current_params,
			       PTP_DPC_BatteryLevel,
			       &dpd) != PTP_RC_OK) {
	add_error_to_errorstack(mtp_device,
				LIBMTP_ERROR_CONNECTING,
				"Unable to read Maximum Battery Level for this "
				"device even though the device supposedly "
				"supports this functionality");
      }
      
      /* TODO: is this appropriate? */
      /* If max battery level is 0 then leave the default, otherwise assign */
      if (dpd.FORM.Range.MaximumValue.u8 != 0) {
	mtp_device->maximum_battery_level = dpd.FORM.Range.MaximumValue.u8;
      }
      
      ptp_free_devicepropdesc(&dpd);
    }
    
    /* Set all default folders to 0 (root directory) */
    mtp_device->default_music_folder = 0;
    mtp_device->default_playlist_folder = 0;
    mtp_device->default_picture_folder = 0;
    mtp_device->default_video_folder = 0;
    mtp_device->default_organizer_folder = 0;
    mtp_device->default_zencast_folder = 0;
    mtp_device->default_album_folder = 0;
    mtp_device->default_text_folder = 0;
    
    /*
     * Then get the handles and try to locate the default folders.
     * This has the desired side effect of caching all handles from
     * the device which speeds up later operations.
     */
    flush_handles(mtp_device);
    
    /* Set initial storage information */
    mtp_device->storage = NULL;
    if (LIBMTP_Get_Storage(mtp_device, LIBMTP_STORAGE_SORTBY_NOTSORTED) == -1) {
      add_error_to_errorstack(mtp_device,
			      LIBMTP_ERROR_GENERAL,
			      "Get Storage information failed.");
    }
    
    /* Add the device to the list */
    mtp_device->next = NULL;
    if (mtp_device_list == NULL) {
      mtp_device_list = current_device = mtp_device;
    } else {
      current_device->next = mtp_device;
      current_device = mtp_device;
    }

    tmplist = tmplist->next;
    i++;
  }
  return mtp_device_list;
}

/**
 * Get the number of devices that are available in the listed device list
 * @param device_list Pointer to a linked list of devices
 * @return Number of devices in the device list device_list
 * @see LIBMTP_Get_Connected_Devices()
 */ 
uint32_t LIBMTP_Number_Devices_In_List(LIBMTP_mtpdevice_t *device_list)
{
  uint32_t numdevices = 0;
  LIBMTP_mtpdevice_t *iter;
  for(iter = device_list; iter != NULL; iter = iter->next)
    numdevices++;
  
  return numdevices;
}

/**
 * Get the first connected MTP device node in the linked list of devices.
 * Currently this only provides access to USB devices
 * @param device_list A list of devices ready to be used by the caller. You
 *        need to know how many there are.
 * @return Any error information gathered from device connections
 * @see LIBMTP_Number_Devices_In_List()
 */
LIBMTP_error_number_t LIBMTP_Get_Connected_Devices(LIBMTP_mtpdevice_t **device_list)
{
  mtpdevice_list_t *devices;
  LIBMTP_error_number_t ret;

  ret = find_usb_devices(&devices);
  if (ret != LIBMTP_ERROR_NONE) {
    *device_list = NULL;
    return ret;
  }

  /* Assign linked list of devices */
  *device_list = create_usb_mtp_devices(devices);

  /* TODO: Add wifi device access here */
  free_mtpdevice_list(devices);

  return LIBMTP_ERROR_NONE;
}

/**
 * This closes and releases an allocated MTP device.
 * @param device a pointer to the MTP device to release.
 */
void LIBMTP_Release_Device_List(LIBMTP_mtpdevice_t *device)
{
  if(device != NULL)
  {
    if(device->next != NULL)
    {
      LIBMTP_Release_Device_List(device->next);
    }
    
    LIBMTP_Release_Device(device);
  }
}

/**
 * This closes and releases an allocated MTP device.
 * @param device a pointer to the MTP device to release.
 */
void LIBMTP_Release_Device(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  close_device(ptp_usb, params, device->interface_number);
  // Clear error stack
  LIBMTP_Clear_Errorstack(device);
  // Free iconv() converters...
  iconv_close(params->cd_locale_to_ucs2);
  iconv_close(params->cd_ucs2_to_locale);
  free(ptp_usb);  
  ptp_free_params(params);
  free_storage_list(device);
  free(device);
}

/**
 * This can be used by any libmtp-intrinsic code that
 * need to stack up an error on the stack. You are only
 * supposed to add errors to the error stack using this
 * function, do not create and reference error entries
 * directly.
 */
static void add_error_to_errorstack(LIBMTP_mtpdevice_t *device,
				    LIBMTP_error_number_t errornumber,
				    char const * const error_text)
{
  LIBMTP_error_t *newerror;
  
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to add error to a NULL device!\n");
    return;
  }
  newerror = (LIBMTP_error_t *) malloc(sizeof(LIBMTP_error_t));
  newerror->errornumber = errornumber;
  newerror->error_text = strdup(error_text);
  newerror->next = NULL;
  if (device->errorstack == NULL) {
    device->errorstack = newerror;
  } else {
    LIBMTP_error_t *tmp = device->errorstack;
    
    while (tmp->next != NULL) {
      tmp = tmp->next;
    }
    tmp->next = newerror;
  }
}

/**
 * Adds an error from the PTP layer to the error stack.
 */
static void add_ptp_error_to_errorstack(LIBMTP_mtpdevice_t *device,
					uint16_t ptp_error,
					char const * const error_text)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to add PTP error to a NULL device!\n");
    return;
  } else {
    char outstr[256];
    snprintf(outstr, sizeof(outstr), "PTP Layer error %04x: %s", ptp_error, error_text);
    outstr[sizeof(outstr)-1] = '\0';
    add_error_to_errorstack(device, LIBMTP_ERROR_PTP_LAYER, outstr);
    add_error_to_errorstack(device, LIBMTP_ERROR_PTP_LAYER, "(Look this up in ptp.h for an explanation.)");
  }
}

/**
 * This returns the error stack for a device in case you
 * need to either reference the error numbers (e.g. when
 * creating multilingual apps with multiple-language text
 * representations for each error number) or when you need
 * to build a multi-line error text widget or something like
 * that. You need to call the <code>LIBMTP_Clear_Errorstack</code>
 * to clear it when you're finished with it.
 * @param device a pointer to the MTP device to get the error
 *        stack for.
 * @return the error stack or NULL if there are no errors
 *         on the stack.
 * @see LIBMTP_Clear_Errorstack()
 * @see LIBMTP_Dump_Errorstack()
 */
LIBMTP_error_t *LIBMTP_Get_Errorstack(LIBMTP_mtpdevice_t *device)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to get the error stack of a NULL device!\n");
  }
  return device->errorstack;
}

/**
 * This function clears the error stack of a device and frees
 * any memory used by it. Call this when you're finished with
 * using the errors.
 * @param device a pointer to the MTP device to clear the error
 *        stack for.
 */
void LIBMTP_Clear_Errorstack(LIBMTP_mtpdevice_t *device)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to clear the error stack of a NULL device!\n");
  } else {
    LIBMTP_error_t *tmp = device->errorstack;
  
    while (tmp != NULL) {
      LIBMTP_error_t *tmp2;
      
      if (tmp->error_text != NULL) {
	free(tmp->error_text);
      }
      tmp2 = tmp;
      tmp = tmp->next;
      free(tmp2);
    }
    device->errorstack = NULL;
  }
}

/**
 * This function dumps the error stack to <code>stderr</code>.
 * (You still have to clear the stack though.)
 * @param device a pointer to the MTP device to dump the error
 *        stack for.
 */
void LIBMTP_Dump_Errorstack(LIBMTP_mtpdevice_t *device)
{
  if (device == NULL) {
    fprintf(stderr, "LIBMTP PANIC: Trying to dump the error stack of a NULL device!\n");
  } else {
    LIBMTP_error_t *tmp = device->errorstack;

    while (tmp != NULL) {
      if (tmp->error_text != NULL) {
	fprintf(stderr, "Error %d: %s\n", tmp->errornumber, tmp->error_text);
      } else {
	fprintf(stderr, "Error %d: (unknown)\n", tmp->errornumber);
      }
      tmp = tmp->next;
    }
  }
}

/**
 * This command gets all handles and stuff by FAST directory retrieveal 
 * which is available by getting all metadata for object 
 * <code>0xffffffff</code> which simply means "all metadata for all objects".
 * This works on the vast majority of MTP devices (there ARE exceptions!)
 * and is quite quick. Check the error stack to see if there were
 * problems getting the metadata.
 */
static void get_all_metadata_fast(LIBMTP_mtpdevice_t *device)
{
  PTPParams      *params = (PTPParams *) device->params;
  int		 cnt = 0;
  int            i;
  uint32_t	 lasthandle = 0xffffffff;
  MTPPropList    *proplist = NULL;
  MTPPropList    *prop;
  uint16_t       ret;
  
  ret = ptp_mtp_getobjectproplist (params, 0xffffffff, &proplist);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "get_all_metadata_fast(): could not get all object proplist.");
    return;
  }
  params->proplist = proplist; /* cache it */
  
  /* 
   * We count the number of objects by counting the ObjectHandle
   * references, whenever it changes we get a new objects, when it's
   * the same, it is just different properties of the same object.
   */
  prop = proplist;
  while (prop) {
      if (lasthandle != prop->ObjectHandle) {
	cnt++;
	lasthandle = prop->ObjectHandle;
      }
      prop = prop->next;
  }
  lasthandle = 0xffffffff;
  params->objectinfo = malloc (sizeof (PTPObjectInfo) * cnt);
  memset (params->objectinfo, 0, sizeof(PTPObjectInfo) * cnt);
  params->handles.Handler = malloc (sizeof (uint32_t) * cnt);
  params->handles.n = cnt;
  
  prop = proplist;
  i = -1;
  while (prop) {
    if (lasthandle != prop->ObjectHandle) {
      if (i >= 0) {
	if (!params->objectinfo[i].Filename) {
	  /* I have one such file on my Creative */
	  params->objectinfo[i].Filename = strdup("<null>");
	}
      }
      i++;
      lasthandle = prop->ObjectHandle;
      params->handles.Handler[i] = prop->ObjectHandle;
    }
    switch (prop->property) {
    case PTP_OPC_ParentObject:
      params->objectinfo[i].ParentObject = prop->propval.u32;
      break;
    case PTP_OPC_ObjectFormat:
      params->objectinfo[i].ObjectFormat = prop->propval.u16;
      break;
    case PTP_OPC_ObjectSize:
      // We loose precision here, up to 32 bits! However the commands that
      // retrieve metadata for files and tracks will make sure that the
      // PTP_OPC_ObjectSize is read in and duplicated again.
      params->objectinfo[i].ObjectCompressedSize = (uint32_t) prop->propval.u64;
      break;
    case PTP_OPC_StorageID:
      params->objectinfo[i].StorageID = prop->propval.u32;
      break;
    case PTP_OPC_ObjectFileName:
      if (prop->propval.str != NULL)
        params->objectinfo[i].Filename = strdup(prop->propval.str);
      break;
    default:
      /*
       * This was in libgphoto2 no idea what it tests for...
       * if ((prop->property & 0xfff0) == 0xdc00)
       * gp_log (GP_LOG_DEBUG, "ptp2/mtpfast", "case %x type %x unhandled.\n", prop->property, prop->datatype);
       */
      break;
      // FIXME: the rest of the metadata is readily available right here!
    }
    prop = prop->next;
  }
}

/**
 * This function will recurse through all the directories on the device,
 * starting at the root directory, gathering metadata as it moves along.
 * It works better on some devices that will only return data for a
 * certain directory and does not respect the option to get all metadata
 * for all objects.
 */
static void get_handles_recursively(LIBMTP_mtpdevice_t *device, PTPParams *params, PTPObjectHandles *handles, uint32_t parent)
{
  PTPObjectHandles currentHandles;
  int i = 0;
  uint32_t old_handles;
  
  uint16_t ret = ptp_getobjecthandles(params,
                                      PTP_GOH_ALL_STORAGE,
                                      PTP_GOH_ALL_FORMATS,
                                      parent,
                                      &currentHandles);
  
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "get_handles_recursively(): could not get object handles.");
    return;
  }
  
  if (currentHandles.Handler == NULL || currentHandles.n == 0)
    return;

  old_handles = handles->n;
  
  // Realloc main space
  handles->Handler = (uint32_t *) realloc(handles->Handler, 
					  (old_handles + currentHandles.n) * sizeof(uint32_t));
  // Realloc object info cache
  params->objectinfo = (PTPObjectInfo*) realloc(params->objectinfo, 
					  (old_handles + currentHandles.n) * sizeof(PTPObjectInfo));
  memset(&params->objectinfo[old_handles], 0, currentHandles.n * sizeof(PTPObjectInfo));

  // Copy new handles
  memcpy(&(handles->Handler[old_handles]), currentHandles.Handler, currentHandles.n * sizeof(uint32_t));
  handles->n = old_handles + currentHandles.n;
  
  // Now descend into any subdirectories found
  for (i = 0; i < currentHandles.n; i++) {    
    ret = ptp_getobjectinfo(params,
			    currentHandles.Handler[i],
			    &params->objectinfo[old_handles + i]);
    
    if (ret == PTP_RC_OK) {
      PTPObjectInfo *oi;

      oi = &params->objectinfo[old_handles + i];
      if (oi->ObjectFormat == PTP_OFC_Association) {
        get_handles_recursively(device, params, handles, currentHandles.Handler[i]);
      }
    } else {
      add_error_to_errorstack(device,
			      LIBMTP_ERROR_CONNECTING,
			      "Found a bad handle, trying to ignore it.");
    }
  }
   
  free(currentHandles.Handler);
}

/**
 * This function refresh the internal handle list whenever
 * the items stored inside the device is altered. On operations
 * that do not add or remove objects, this is typically not
 * called.
 * @param device a pointer to the MTP device to flush handles for.
 */
static void flush_handles(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint32_t i;

  if (params->handles.Handler != NULL) {
    free(params->handles.Handler);
  }
  if (params->objectinfo != NULL) {
    for (i=0;i<params->handles.n;i++)
      ptp_free_objectinfo (&params->objectinfo[i]);
    free(params->objectinfo);
  }
  if (params->proplist != NULL) {
    destroy_mtp_prop_list(params->proplist);
  }
  
  params->handles.n = 0;
  params->handles.Handler = NULL;
  params->objectinfo = NULL;
  params->proplist = NULL;

  if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)
      && !(ptp_usb->device_flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST) 
      && !(ptp_usb->device_flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL)) {
    // Use the fast method.
    get_all_metadata_fast(device);
  } else {
    // Get all the handles using just standard commands.
    get_handles_recursively(device, params,
			    &params->handles,
			    PTP_GOH_ROOT_PARENT);
  }

  for(i = 0; i < params->handles.n; i++) {
    PTPObjectInfo *oi;
    
    oi = &params->objectinfo[i];
    if (oi->Filename == NULL) {
      oi->Filename = strdup("<null>");
    }
    if (oi->Keywords == NULL) {
      oi->Keywords = strdup("<null>");
    }
    
    /* Ignore handles that point to non-folders */
    if(oi->ObjectFormat != PTP_OFC_Association)
      continue;
    if (oi->Filename == NULL)
      continue;
    
    /* Is this the Music Folder */
    if (!strcasecmp(oi->Filename, "My Music") ||
	!strcasecmp(oi->Filename, "Music")) {
      device->default_music_folder = 
	params->handles.Handler[i];
    }
    else if (!strcasecmp(oi->Filename, "My Playlists") ||
	     !strcasecmp(oi->Filename, "Playlists")) {
      device->default_playlist_folder =
	params->handles.Handler[i];
    }
    else if (!strcasecmp(oi->Filename, "My Pictures") ||
	     !strcasecmp(oi->Filename, "Pictures")) {
      device->default_picture_folder = 
	params->handles.Handler[i];
    }
    else if (!strcasecmp(oi->Filename, "My Video") ||
	     !strcasecmp(oi->Filename, "Video")) {
	device->default_video_folder = 
	  params->handles.Handler[i];
    }
    else if (!strcasecmp(oi->Filename, "My Organizer")) {
      device->default_organizer_folder =
	params->handles.Handler[i];
    }
    else if (!strcasecmp(oi->Filename, "ZENcast")) {
      device->default_zencast_folder = 
	params->handles.Handler[i];
    }
    else if (!strcasecmp(oi->Filename, "My Albums") ||
	     !strcasecmp(oi->Filename, "Albums")) {
      device->default_album_folder = 
	params->handles.Handler[i];
    }
    else if (!strcasecmp(oi->Filename, "Text")) {
      device->default_text_folder =
	params->handles.Handler[i];
    }
  }
}

/**
 * This function traverses a devices storage list freeing up the 
 * strings and the structs.
 * @param device a pointer to the MTP device to free the storage 
 * list for.
 */
static void free_storage_list(LIBMTP_mtpdevice_t *device)
{
  LIBMTP_devicestorage_t *storage;
  LIBMTP_devicestorage_t *tmp;

  storage = device->storage;
  while(storage != NULL) {  
    if (storage->StorageDescription != NULL) {
      free(storage->StorageDescription);
    }
    if (storage->VolumeIdentifier != NULL) {
      free(storage->VolumeIdentifier);
    }
    tmp = storage;
    storage = storage->next;
    free(tmp);
  }
  device->storage = NULL;

  return;
}

/**
 * This function traverses a devices storage list freeing up the
 * strings and the structs.
 * @param device a pointer to the MTP device to free the storage
 * list for.
 */
static int sort_storage_by(LIBMTP_mtpdevice_t *device,int const sortby)
{
  LIBMTP_devicestorage_t *oldhead, *ptr1, *ptr2, *newlist;

  if (device->storage == NULL)
    return -1;
  if (sortby == LIBMTP_STORAGE_SORTBY_NOTSORTED) 
    return 0;

  oldhead = ptr1 = ptr2 = device->storage;

  newlist = NULL;

  while(oldhead != NULL) {
    ptr1 = ptr2 = oldhead;
    while(ptr1 != NULL) {

      if (sortby == LIBMTP_STORAGE_SORTBY_FREESPACE && ptr1->FreeSpaceInBytes > ptr2->FreeSpaceInBytes) 
        ptr2 = ptr1;
      if (sortby == LIBMTP_STORAGE_SORTBY_MAXSPACE && ptr1->FreeSpaceInBytes > ptr2->FreeSpaceInBytes) 
        ptr2 = ptr1;

      ptr1 = ptr1->next;
    }

    // Make our previous entries next point to our next
    if(ptr2->prev != NULL) {
      ptr1 = ptr2->prev;
      ptr1->next = ptr2->next; 
    } else {
      oldhead = ptr2->next;
      if(oldhead != NULL)
        oldhead->prev = NULL;
    }

    // Make our next entries previous point to our previous
    ptr1 = ptr2->next;
    if(ptr1 != NULL) {
      ptr1->prev = ptr2->prev;
    } else {
      ptr1 = ptr2->prev;
      if(ptr1 != NULL)
        ptr1->next = NULL;
    }
  
    if(newlist == NULL) {
      newlist = ptr2;
      newlist->prev = NULL;
    } else {
      ptr2->prev = newlist;
      newlist->next = ptr2;
      newlist = newlist->next;
    }
  }
 
  newlist->next = NULL;
  while(newlist->prev != NULL) 
   newlist = newlist->prev;

  device->storage = newlist;

  return 0;
}

/**
 * This function grabs the first storageid from the device storage 
 * list.
 * @param device a pointer to the MTP device to free the storage
 * list for.
 */
static uint32_t get_first_storageid(LIBMTP_mtpdevice_t *device)
{
  LIBMTP_devicestorage_t *storage = device->storage;
  uint32_t store = 0;

  if(storage != NULL)
    store = storage->id;

  return store;
}

/**
 * This function grabs the freespace from the first storage in
 * device storage list.
 * @param device a pointer to the MTP device to free the storage
 * list for.
 */
static int get_first_storage_freespace(LIBMTP_mtpdevice_t *device, uint64_t *freespace)
{
  LIBMTP_devicestorage_t *storage = device->storage;
  PTPParams *params = (PTPParams *) device->params;

  if(storage == NULL) {
    return -1;
  }
  // Always query the device about this, since some models explicitly
  // needs that.
  if (ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    PTPStorageInfo storageInfo;
    uint16_t ret;
    
    ret = ptp_getstorageinfo(params, storage->id, &storageInfo);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "get_first_storage_freespace(): could not get storage info.");
      return -1;
    }
    if (storage->StorageDescription != NULL) {
      free(storage->StorageDescription);
    }
    if (storage->VolumeIdentifier != NULL) {
      free(storage->VolumeIdentifier);
    }
    storage->StorageType = storageInfo.StorageType;
    storage->FilesystemType = storageInfo.FilesystemType;
    storage->AccessCapability = storageInfo.AccessCapability;
    storage->MaxCapacity = storageInfo.MaxCapability;
    storage->FreeSpaceInBytes = storageInfo.FreeSpaceInBytes;
    storage->FreeSpaceInObjects = storageInfo.FreeSpaceInImages;
    storage->StorageDescription = storageInfo.StorageDescription;
    storage->VolumeIdentifier = storageInfo.VolumeLabel;
  }
  if(storage->FreeSpaceInBytes == (uint64_t) -1)
    return -1;
  *freespace = storage->FreeSpaceInBytes;
  return 0;
}

/**
 * This function dumps out a large chunk of textual information
 * provided from the PTP protocol and additionally some extra
 * MTP-specific information where applicable.
 * @param device a pointer to the MTP device to report info from.
 */
void LIBMTP_Dump_Device_Info(LIBMTP_mtpdevice_t *device)
{
  int i;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  LIBMTP_devicestorage_t *storage = device->storage;

  printf("USB low-level info:\n");
  dump_usbinfo(ptp_usb);
  /* Print out some verbose information */
  printf("Device info:\n");
  printf("   Manufacturer: %s\n", params->deviceinfo.Manufacturer);
  printf("   Model: %s\n", params->deviceinfo.Model);
  printf("   Device version: %s\n", params->deviceinfo.DeviceVersion);
  printf("   Serial number: %s\n", params->deviceinfo.SerialNumber);
  printf("   Vendor extension ID: 0x%08x\n", params->deviceinfo.VendorExtensionID);
  printf("   Vendor extension description: %s\n", params->deviceinfo.VendorExtensionDesc);
  printf("Supported operations:\n");
  for (i=0;i<params->deviceinfo.OperationsSupported_len;i++) {
    char txt[256];

    (void) ptp_render_opcode (params, params->deviceinfo.OperationsSupported[i], sizeof(txt), txt);
    printf("   %04x: %s\n", params->deviceinfo.OperationsSupported[i], txt);
  }
  printf("Events supported:\n");
  if (params->deviceinfo.EventsSupported_len == 0) {
    printf("   None.\n");
  } else {
    for (i=0;i<params->deviceinfo.EventsSupported_len;i++) {
      printf("   0x%04x\n", params->deviceinfo.EventsSupported[i]);
    }
  }
  printf("Device Properties Supported:\n");
  for (i=0;i<params->deviceinfo.DevicePropertiesSupported_len;i++) {
    char const *propdesc = ptp_get_property_description(params, params->deviceinfo.DevicePropertiesSupported[i]);

    if (propdesc != NULL) {
      printf("   0x%04x: %s\n", params->deviceinfo.DevicePropertiesSupported[i], propdesc);
    } else {
      uint16_t prop = params->deviceinfo.DevicePropertiesSupported[i];
      printf("   0x%04x: Unknown property\n", prop);
    }
  }

  if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjectPropsSupported)) {
    printf("Playable File (Object) Types and Object Properties Supported:\n");
    for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
      char txt[256];
      uint16_t ret;
      uint16_t *props = NULL;
      uint32_t propcnt = 0;
      int j;

      (void) ptp_render_ofc (params, params->deviceinfo.ImageFormats[i], sizeof(txt), txt);
      printf("   %04x: %s\n", params->deviceinfo.ImageFormats[i], txt);

      ret = ptp_mtp_getobjectpropssupported (params, params->deviceinfo.ImageFormats[i], &propcnt, &props);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Dump_Device_Info(): error on query for object properties.");
      } else {
	for (j=0;j<propcnt;j++) {
	  PTPObjectPropDesc opd;
	  int k;
	  
	  (void) ptp_render_mtp_propname(props[j],sizeof(txt),txt);
	  printf("      %04x: %s", props[j], txt);
	  // Get a more verbose description
	  ret = ptp_mtp_getobjectpropdesc(params, props[j], params->deviceinfo.ImageFormats[i], &opd);
	  if (ret != PTP_RC_OK) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Dump_Device_Info(): "
				    "could not get property description.");
	    break;
	  }

	  if (opd.DataType == PTP_DTC_STR) {
	    printf(" STRING data type");
	  } else {
	    if (opd.DataType & PTP_DTC_ARRAY_MASK) {
	      printf(" array of");
	    }

	    switch (opd.DataType & (~PTP_DTC_ARRAY_MASK)) {

	    case PTP_DTC_UNDEF:
	      printf(" UNDEFINED data type");
	      break;
	      
	    case PTP_DTC_INT8:
	      printf(" INT8 data type");
	      switch (opd.FormFlag) {
	      case PTP_DPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.i8,
		       opd.FORM.Range.MaximumValue.i8,
		       opd.FORM.Range.StepSize.i8);
		break;
	      case PTP_DPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].i8);
		}
		break;
	      default:
		printf(" ANY 8BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_UINT8:
	      printf(" UINT8 data type");
	      switch (opd.FormFlag) {
	      case PTP_DPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.u8,
		       opd.FORM.Range.MaximumValue.u8,
		       opd.FORM.Range.StepSize.u8);
		break;
	      case PTP_DPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].u8);
		}
		break;
	      default:
		printf(" ANY 8BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_INT16:
	      printf(" INT16 data type");
	      switch (opd.FormFlag) {
	      case PTP_DPFF_Range:
	      printf(" range: MIN %d, MAX %d, STEP %d",
		     opd.FORM.Range.MinimumValue.i16,
		     opd.FORM.Range.MaximumValue.i16,
		     opd.FORM.Range.StepSize.i16);
	      break;
	      case PTP_DPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].i16);
		}
		break;
	      default:
		printf(" ANY 16BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_UINT16:
	      printf(" UINT16 data type");
	      switch (opd.FormFlag) {
	      case PTP_DPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.u16,
		       opd.FORM.Range.MaximumValue.u16,
		       opd.FORM.Range.StepSize.u16);
		break;
	      case PTP_DPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].u16);
		}
		break;
	      default:
		printf(" ANY 16BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_INT32:
	      printf(" INT32 data type");
	      switch (opd.FormFlag) {
	      case PTP_DPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.i32,
		       opd.FORM.Range.MaximumValue.i32,
		       opd.FORM.Range.StepSize.i32);
		break;
	      case PTP_DPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].i32);
		}
		break;
	      default:
		printf(" ANY 32BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_UINT32:
	      printf(" UINT32 data type");
	      switch (opd.FormFlag) {
	      case PTP_DPFF_Range:
		printf(" range: MIN %d, MAX %d, STEP %d",
		       opd.FORM.Range.MinimumValue.u32,
		       opd.FORM.Range.MaximumValue.u32,
		       opd.FORM.Range.StepSize.u32);
		break;
	      case PTP_DPFF_Enumeration:
		printf(" enumeration: ");
		for(k=0;k<opd.FORM.Enum.NumberOfValues;k++) {
		  printf("%d, ", opd.FORM.Enum.SupportedValue[k].u32);
		}
		break;
	      default:
		printf(" ANY 32BIT VALUE form");
		break;
	      }
	      break;
	      
	    case PTP_DTC_INT64:
	      printf(" INT64 data type");
	      break;
	      
	    case PTP_DTC_UINT64:
	      printf(" UINT64 data type");
	      break;
	      
	    case PTP_DTC_INT128:
	      printf(" INT128 data type");
	      break;
	      
	    case PTP_DTC_UINT128:
	      printf(" UINT128 data type");
	      break;
	      
	    default:
	      printf(" UNKNOWN data type");
	      break;
	    }
	  }
	  if (opd.GetSet) {
	    printf(" GET/SET");
	  } else {
	    printf(" READ ONLY");
	  }
	  printf("\n");
	  ptp_free_objectpropdesc(&opd);
	}
	free(props);
      }
    }
  }

  if(storage != NULL && ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    printf("Storage Devices:\n");
    while(storage != NULL) {
      printf("   StorageID: 0x%08x\n",storage->id);
      printf("      StorageType: 0x%04x\n",storage->StorageType);
      printf("      FilesystemType: 0x%04x\n",storage->FilesystemType);
      printf("      AccessCapability: 0x%04x\n",storage->AccessCapability);
      printf("      MaxCapacity: %lld\n",storage->MaxCapacity);
      printf("      FreeSpaceInBytes: %lld\n",storage->FreeSpaceInBytes);
      printf("      FreeSpaceInObjects: %lld\n",storage->FreeSpaceInObjects);
      printf("      StorageDescription: %s\n",storage->StorageDescription);
      printf("      VolumeIdentifier: %s\n",storage->VolumeIdentifier);
      storage = storage->next;
    }
  }

  printf("Special directories:\n");
  printf("   Default music folder: 0x%08x\n", device->default_music_folder);
  printf("   Default playlist folder: 0x%08x\n", device->default_playlist_folder);
  printf("   Default picture folder: 0x%08x\n", device->default_picture_folder);
  printf("   Default video folder: 0x%08x\n", device->default_video_folder);
  printf("   Default organizer folder: 0x%08x\n", device->default_organizer_folder);
  printf("   Default zencast folder: 0x%08x\n", device->default_zencast_folder);
  printf("   Default album folder: 0x%08x\n", device->default_album_folder);
  printf("   Default text folder: 0x%08x\n", device->default_text_folder);
}

/**
 * This resets a device in case it supports the <code>PTP_OC_ResetDevice</code>
 * operation code (0x1010).
 * @param device a pointer to the device to reset.
 * @return 0 on success, any other value means failure.
 */
int LIBMTP_Reset_Device(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_operation_issupported(params,PTP_OC_ResetDevice)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Reset_Device(): device does not support resetting.");
    return -1;
  }
  ret = ptp_resetdevice(params);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error resetting.");
    return -1;
  }
  return 0;
}

/**
 * This retrieves the model name (often equal to product name)
 * of an MTP device.
 * @param device a pointer to the device to get the model name for.
 * @return a newly allocated UTF-8 string representing the model name.
 *         The string must be freed by the caller after use. If the call
 *         was unsuccessful this will contain NULL.
 */
char *LIBMTP_Get_Modelname(LIBMTP_mtpdevice_t *device)
{
  char *retmodel = NULL;
  PTPParams *params = (PTPParams *) device->params;

  if (params->deviceinfo.Model != NULL) {
    retmodel = strdup(params->deviceinfo.Model);
  }
  return retmodel;
}

/**
 * This retrieves the serial number of an MTP device.
 * @param device a pointer to the device to get the serial number for.
 * @return a newly allocated UTF-8 string representing the serial number.
 *         The string must be freed by the caller after use. If the call
 *         was unsuccessful this will contain NULL.
 */
char *LIBMTP_Get_Serialnumber(LIBMTP_mtpdevice_t *device)
{
  char *retnumber = NULL;
  PTPParams *params = (PTPParams *) device->params;

  if (params->deviceinfo.SerialNumber != NULL) {
    retnumber = strdup(params->deviceinfo.SerialNumber);
  }
  return retnumber;
}

/**
 * This retrieves the device version (hardware and firmware version) of an
 * MTP device.
 * @param device a pointer to the device to get the device version for.
 * @return a newly allocated UTF-8 string representing the device version.
 *         The string must be freed by the caller after use. If the call
 *         was unsuccessful this will contain NULL.
 */
char *LIBMTP_Get_Deviceversion(LIBMTP_mtpdevice_t *device)
{
  char *retversion = NULL;
  PTPParams *params = (PTPParams *) device->params;

  if (params->deviceinfo.DeviceVersion != NULL) {
    retversion = strdup(params->deviceinfo.DeviceVersion);
  }
  return retversion;
}


/**
 * This retrieves the "friendly name" of an MTP device. Usually
 * this is simply the name of the owner or something like
 * "John Doe's Digital Audio Player". This property should be supported
 * by all MTP devices.
 * @param device a pointer to the device to get the friendly name for.
 * @return a newly allocated UTF-8 string representing the friendly name.
 *         The string must be freed by the caller after use.
 * @see LIBMTP_Set_Friendlyname()
 */
char *LIBMTP_Get_Friendlyname(LIBMTP_mtpdevice_t *device)
{
  PTPPropertyValue propval;
  char *retstring = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_DeviceFriendlyName)) {
    return NULL;
  }

  ret = ptp_getdevicepropvalue(params,
			       PTP_DPC_MTP_DeviceFriendlyName,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error getting friendlyname.");
    return NULL;
  }
  if (propval.str != NULL) {
    retstring = strdup(propval.str);
    free(propval.str);
  }
  return retstring;
}

/**
 * Sets the "friendly name" of an MTP device.
 * @param device a pointer to the device to set the friendly name for.
 * @param friendlyname the new friendly name for the device.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Get_Ownername()
 */
int LIBMTP_Set_Friendlyname(LIBMTP_mtpdevice_t *device,
			 char const * const friendlyname)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_DeviceFriendlyName)) {
    return -1;
  }
  propval.str = (char *) friendlyname;
  ret = ptp_setdevicepropvalue(params,
			       PTP_DPC_MTP_DeviceFriendlyName,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error setting friendlyname.");
    return -1;
  }
  return 0;
}

/**
 * This retrieves the syncronization partner of an MTP device. This
 * property should be supported by all MTP devices.
 * @param device a pointer to the device to get the sync partner for.
 * @return a newly allocated UTF-8 string representing the synchronization
 *         partner. The string must be freed by the caller after use.
 * @see LIBMTP_Set_Syncpartner()
 */
char *LIBMTP_Get_Syncpartner(LIBMTP_mtpdevice_t *device)
{
  PTPPropertyValue propval;
  char *retstring = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_SynchronizationPartner)) {
    return NULL;
  }

  ret = ptp_getdevicepropvalue(params,
			       PTP_DPC_MTP_SynchronizationPartner,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error getting syncpartner.");
    return NULL;
  }
  if (propval.str != NULL) {
    retstring = strdup(propval.str);
    free(propval.str);
  }
  return retstring;
}


/**
 * Sets the synchronization partner of an MTP device. Note that
 * we have no idea what the effect of setting this to "foobar"
 * may be. But the general idea seems to be to tell which program
 * shall synchronize with this device and tell others to leave
 * it alone.
 * @param device a pointer to the device to set the sync partner for.
 * @param syncpartner the new synchronization partner for the device.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Get_Syncpartner()
 */
int LIBMTP_Set_Syncpartner(LIBMTP_mtpdevice_t *device,
			 char const * const syncpartner)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (!ptp_property_issupported(params, PTP_DPC_MTP_SynchronizationPartner)) {
    return -1;
  }
  propval.str = (char *) syncpartner;
  ret = ptp_setdevicepropvalue(params,
			       PTP_DPC_MTP_SynchronizationPartner,
			       &propval,
			       PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "Error setting syncpartner.");
    return -1;
  }
  return 0;
}

/**
 * Checks if the device can stora a file of this size or
 * if it's too big.
 * @param device a pointer to the device.
 * @param filesize the size of the file to check whether it will fit.
 * @return 0 if the file fits, any other value means failure.
 */
static int check_if_file_fits(LIBMTP_mtpdevice_t *device, uint64_t const filesize) {
  PTPParams *params = (PTPParams *) device->params;
  uint64_t freebytes;
  int ret;

  // If we cannot check the storage, no big deal.
  if (!ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    return 0;
  }
  
  ret = get_first_storage_freespace(device,&freebytes);
  if (ret != 0) {
    add_ptp_error_to_errorstack(device, ret, "check_if_file_fits(): error checking free storage.");
    return -1;
  } else {
    if (filesize > freebytes) {
      add_error_to_errorstack(device, LIBMTP_ERROR_STORAGE_FULL, "check_if_file_fits(): device storage is full.");
      return -1;
    }
  }
  return 0;
}


/**
 * This function retrieves the current battery level on the device.
 * @param device a pointer to the device to get the battery level for.
 * @param maximum_level a pointer to a variable that will hold the
 *        maximum level of the battery if the call was successful.
 * @param current_level a pointer to a variable that will hold the
 *        current level of the battery if the call was successful.
 *        A value of 0 means that the device is on external power.
 * @return 0 if the storage info was successfully retrieved, any other
 *        means failure. A typical cause of failure is that
 *        the device does not support the battery level property.
 */
int LIBMTP_Get_Batterylevel(LIBMTP_mtpdevice_t *device,
			    uint8_t * const maximum_level,
			    uint8_t * const current_level)
{
  PTPPropertyValue propval;
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;

  *maximum_level = 0;
  *current_level = 0;

  if (!ptp_property_issupported(params, PTP_DPC_BatteryLevel)) {
    return -1;
  }

  ret = ptp_getdevicepropvalue(params, PTP_DPC_BatteryLevel, &propval, PTP_DTC_UINT8);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Batterylevel(): could not get device property value.");
    return -1;
  }

  *maximum_level = device->maximum_battery_level;
  *current_level = propval.u8;

  return 0;
}


/**
 * Formats device storage (if the device supports the operation).
 * WARNING: This WILL delete all data from the device. Make sure you've
 * got confirmation from the user BEFORE you call this function.
 *
 * @param device a pointer to the device containing the storage to format.
 * @param storage the actual storage to format.
 * @return 0 on success, any other value means failure.
 */
int LIBMTP_Format_Storage(LIBMTP_mtpdevice_t *device, LIBMTP_devicestorage_t *storage)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;

  if (!ptp_operation_issupported(params,PTP_OC_FormatStore)) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, 
			    "LIBMTP_Format_Storage(): device does not support formatting storage.");
    return -1;
  }
  ret = ptp_formatstore(params, storage->id);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Format_Storage(): failed to format storage.");
    return -1;
  }
  return 0;
}

/**
 * Helper function to extract a unicode property off a device.
 * This is the standard way of retrieveing unicode device
 * properties as described by the PTP spec.
 * @param device a pointer to the device to get the property from.
 * @param unicstring a pointer to a pointer that will hold the
 *        property after this call is completed.
 * @param property the property to retrieve.
 * @return 0 on success, any other value means failure.
 */
static int get_device_unicode_property(LIBMTP_mtpdevice_t *device,
				       char **unicstring, uint16_t property)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t *tmp;
  uint16_t ret;
  int i;

  if (!ptp_property_issupported(params, property)) {
    return -1;
  }

  // Unicode strings are 16bit unsigned integer arrays.
  ret = ptp_getdevicepropvalue(params,
			       property,
			       &propval,
			       PTP_DTC_AUINT16);
  if (ret != PTP_RC_OK) {
    // TODO: add a note on WHICH property that we failed to get.
    *unicstring = NULL;
    add_ptp_error_to_errorstack(device, ret, "get_device_unicode_property(): failed to get unicode property.");
    return -1;
  }

  // Extract the actual array.
  // printf("Array of %d elements\n", propval.a.count);
  tmp = malloc((propval.a.count + 1)*sizeof(uint16_t));
  for (i = 0; i < propval.a.count; i++) {
    tmp[i] = propval.a.v[i].u16;
    // printf("%04x ", tmp[i]);
  }
  tmp[propval.a.count] = 0x0000U;
  free(propval.a.v);

  *unicstring = utf16_to_utf8(device, tmp);

  free(tmp);

  return 0;
}

/**
 * This function returns the secure time as an XML document string from
 * the device.
 * @param device a pointer to the device to get the secure time for.
 * @param sectime the secure time string as an XML document or NULL if the call
 *         failed or the secure time property is not supported. This string
 *         must be <code>free()</code>:ed by the caller after use.
 * @return 0 on success, any other value means failure.
 */
int LIBMTP_Get_Secure_Time(LIBMTP_mtpdevice_t *device, char ** const sectime)
{
  return get_device_unicode_property(device, sectime, PTP_DPC_MTP_SecureTime);
}

/**
 * This function returns the device (public key) certificate as an
 * XML document string from the device.
 * @param device a pointer to the device to get the device certificate for.
 * @param devcert the device certificate as an XML string or NULL if the call
 *        failed or the device certificate property is not supported. This
 *        string must be <code>free()</code>:ed by the caller after use.
 * @return 0 on success, any other value means failure.
 */
int LIBMTP_Get_Device_Certificate(LIBMTP_mtpdevice_t *device, char ** const devcert)
{
  return get_device_unicode_property(device, devcert, PTP_DPC_MTP_DeviceCertificate);
}

/**
 * This function retrieves a list of supported file types, i.e. the file
 * types that this device claims it supports, e.g. audio file types that
 * the device can play etc. This list is mitigated to
 * inlcude the file types that libmtp can handle, i.e. it will not list
 * filetypes that libmtp will handle internally like playlists and folders.
 * @param device a pointer to the device to get the filetype capabilities for.
 * @param filetypes a pointer to a pointer that will hold the list of
 *        supported filetypes if the call was successful. This list must
 *        be <code>free()</code>:ed by the caller after use.
 * @param length a pointer to a variable that will hold the length of the
 *        list of supported filetypes if the call was successful.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Get_Filetype_Description()
 */
int LIBMTP_Get_Supported_Filetypes(LIBMTP_mtpdevice_t *device, uint16_t ** const filetypes,
				  uint16_t * const length)
{
  PTPParams *params = (PTPParams *) device->params;
  uint16_t *localtypes;
  uint16_t localtypelen;
  uint32_t i;

  // This is more memory than needed if there are unknown types, but what the heck.
  localtypes = (uint16_t *) malloc(params->deviceinfo.ImageFormats_len * sizeof(uint16_t));
  localtypelen = 0;

  for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
    uint16_t localtype = map_ptp_type_to_libmtp_type(params->deviceinfo.ImageFormats[i]);
    if (localtype != LIBMTP_FILETYPE_UNKNOWN) {
      localtypes[localtypelen] = localtype;
      localtypelen++;
    }
  }

  *filetypes = localtypes;
  *length = localtypelen;

  return 0;
}

/**
 * This function retrieves all the storage id's of a device and there
 * properties. Then creates a linked list and puts the list head into 
 * the device struct. It also optionally sorts this list. If you want
 * to display storage information in your application you should call
 * this function, then dereference the device struct 
 * (<code>device->storage</code>) to get out information on the storage.
 *
 * @param device a pointer to the device to get the filetype capabilities for.
 * @param sortby an integer that determines the sorting of the storage list. 
 *        Valid sort methods are defined in libmtp.h with beginning with
 *        LIBMTP_STORAGE_SORTBY_. 0 or LIBMTP_STORAGE_SORTBY_NOTSORTED to not 
 *        sort.
 * @return 0 on success, 1 success but only with storage id's, storage 
 *        properities could not be retrieved and -1 means failure.
 * @see LIBMTP_Get_Filetype_Description()
 */
int LIBMTP_Get_Storage(LIBMTP_mtpdevice_t *device, int const sortby)
{
  uint32_t i = 0;
  PTPStorageInfo storageInfo;
  PTPParams *params = (PTPParams *) device->params;
  PTPStorageIDs storageIDs;
  LIBMTP_devicestorage_t *storage = NULL;
  LIBMTP_devicestorage_t *storageprev = NULL;

  if (device->storage != NULL)
    free_storage_list(device);

  // if (!ptp_operation_issupported(params,PTP_OC_GetStorageIDs)) 
  //   return -1;
  if (ptp_getstorageids (params, &storageIDs) != PTP_RC_OK) 
    return -1;
  if (storageIDs.n < 1) 
    return -1;

  if (!ptp_operation_issupported(params,PTP_OC_GetStorageInfo)) {
    for (i = 0; i < storageIDs.n; i++) {

      storage = (LIBMTP_devicestorage_t *) malloc(sizeof(LIBMTP_devicestorage_t));
      storage->prev = storageprev;
      if (storageprev != NULL)
        storageprev->next = storage;
      if (device->storage == NULL) 
        device->storage = storage;

      storage->id = storageIDs.Storage[i];
      storage->StorageType = PTP_ST_Undefined;
      storage->FilesystemType = PTP_FST_Undefined;
      storage->AccessCapability = PTP_AC_ReadWrite;
      storage->MaxCapacity = (uint64_t) -1;
      storage->FreeSpaceInBytes = (uint64_t) -1;
      storage->FreeSpaceInObjects = (uint64_t) -1;
      storage->StorageDescription = strdup("Unknown storage");
      storage->VolumeIdentifier = strdup("Unknown volume");
      storage->next = NULL;

      storageprev = storage;
    }
    free(storageIDs.Storage);
    return 1;
  } else {
    for (i = 0; i < storageIDs.n; i++) {
      uint16_t ret;
      ret = ptp_getstorageinfo(params, storageIDs.Storage[i], &storageInfo);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Storage(): Could not get storage info.");
	if (device->storage != NULL) {
          free_storage_list(device);
	}
	return -1;
      }

      storage = (LIBMTP_devicestorage_t *) malloc(sizeof(LIBMTP_devicestorage_t));
      storage->prev = storageprev;
      if (storageprev != NULL)
        storageprev->next = storage;
      if (device->storage == NULL)
        device->storage = storage;

      storage->id = storageIDs.Storage[i];
      storage->StorageType = storageInfo.StorageType;
      storage->FilesystemType = storageInfo.FilesystemType;
      storage->AccessCapability = storageInfo.AccessCapability;
      storage->MaxCapacity = storageInfo.MaxCapability;
      storage->FreeSpaceInBytes = storageInfo.FreeSpaceInBytes;
      storage->FreeSpaceInObjects = storageInfo.FreeSpaceInImages;
      storage->StorageDescription = storageInfo.StorageDescription;
      storage->VolumeIdentifier = storageInfo.VolumeLabel;
      storage->next = NULL;

      storageprev = storage;
    }

    storage->next = NULL;

    sort_storage_by(device,sortby);
    free(storageIDs.Storage);
    return 0;
  }
}

/**
 * This creates a new file metadata structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_destroy_file_t</code>
 * operation later, so be careful of using strdup() when assigning
 * strings, e.g.:
 *
 * <pre>
 * LIBMTP_file_t *file = LIBMTP_new_file_t();
 * file->filename = strdup(namestr);
 * ....
 * LIBMTP_destroy_file_t(file);
 * </pre>
 *
 * @return a pointer to the newly allocated metadata structure.
 * @see LIBMTP_destroy_file_t()
 */
LIBMTP_file_t *LIBMTP_new_file_t(void)
{
  LIBMTP_file_t *new = (LIBMTP_file_t *) malloc(sizeof(LIBMTP_file_t));
  if (new == NULL) {
    return NULL;
  }
  new->filename = NULL;
  new->filesize = 0;
  new->filetype = LIBMTP_FILETYPE_UNKNOWN;
  new->next = NULL;
  return new;
}

/**
 * This destroys a file metadata structure and deallocates the memory
 * used by it, including any strings. Never use a file metadata
 * structure again after calling this function on it.
 * @param file the file metadata to destroy.
 * @see LIBMTP_new_file_t()
 */
void LIBMTP_destroy_file_t(LIBMTP_file_t *file)
{
  if (file == NULL) {
    return;
  }
  if (file->filename != NULL)
    free(file->filename);
  free(file);
  return;
}

/**
* THIS FUNCTION IS DEPRECATED. PLEASE UPDATE YOUR CODE IN ORDER
 * NOT TO USE IT.
 * @see LIBMTP_Get_Filelisting_With_Callback()
 */
LIBMTP_file_t *LIBMTP_Get_Filelisting(LIBMTP_mtpdevice_t *device)
{
  printf("WARNING: LIBMTP_Get_Filelisting() is deprecated.\n");
  printf("WARNING: please update your code to use LIBMTP_Get_Filelisting_With_Callback()\n");
  return LIBMTP_Get_Filelisting_With_Callback(device, NULL, NULL);
}

/**
 * This returns a long list of all files available
 * on the current MTP device. Typical usage:
 *
 * <pre>
 * LIBMTP_file_t *filelist;
 *
 * filelist = LIBMTP_Get_Filelisting_With_Callback(device, callback, data);
 * while (filelist != NULL) {
 *   LIBMTP_file_t *tmp;
 *
 *   // Do something on each element in the list here...
 *   tmp = filelist;
 *   filelist = filelist->next;
 *   LIBMTP_destroy_file_t(tmp);
 * }
 * </pre>
 *
 * @param device a pointer to the device to get the file listing for.
 * @param callback a function to be called during the tracklisting retrieveal
 *               for displaying progress bars etc, or NULL if you don't want
 *               any callbacks.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return a list of files that can be followed using the <code>next</code>
 *         field of the <code>LIBMTP_file_t</code> data structure.
 *         Each of the metadata tags must be freed after use, and may
 *         contain only partial metadata information, i.e. one or several
 *         fields may be NULL or 0.
 * @see LIBMTP_Get_Filemetadata()
 */
LIBMTP_file_t *LIBMTP_Get_Filelisting_With_Callback(LIBMTP_mtpdevice_t *device,
                                                    LIBMTP_progressfunc_t const callback,
                                                    void const * const data)
{
  uint32_t i = 0;
  LIBMTP_file_t *retfiles = NULL;
  LIBMTP_file_t *curfile = NULL;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint16_t ret;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_file_t *file;
    PTPObjectInfo *oi;

    if (callback != NULL)
      callback(i, params->handles.n, data);

    oi = &params->objectinfo[i];

    if (oi->ObjectFormat == PTP_OFC_Association) {
      // MTP use this object format for folders which means
      // these "files" will turn up on a folder listing instead.
      continue;
    }

    // Allocate a new file type
    file = LIBMTP_new_file_t();

    file->parent_id = oi->ParentObject;

    // This is some sort of unique ID so we can keep track of the track.
    file->item_id = params->handles.Handler[i];

    // Set the filetype
    file->filetype = map_ptp_type_to_libmtp_type(oi->ObjectFormat);

    // Original file-specific properties
    // We only have 32-bit file size here; if we find it, we use the 
    // PTP_OPC_ObjectSize property which has 64bit precision.
    file->filesize = oi->ObjectCompressedSize;
    if (oi->Filename != NULL) {
      file->filename = strdup(oi->Filename);
    }

    /*
     * If we have a cached, large set of metadata, then use it!
     */
    if (params->proplist) {
      MTPPropList *prop = params->proplist;
      
      while (prop != NULL && prop->ObjectHandle != file->item_id) {
	prop = prop->next;
      }
      while (prop != NULL && prop->ObjectHandle == file->item_id) {
	// Pick ObjectSize here...
	if (prop->property == PTP_OPC_ObjectSize) {
	  // This may already be set, but this 64bit precision value 
	  // is better than the PTP 32bit value, so let it override.
	  file->filesize = prop->propval.u64;
	  break;
	}
	prop = prop->next;
      }
    } else if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)
	       && !(ptp_usb->device_flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST)) {
      MTPPropList *proplist = NULL;
      MTPPropList *prop;
      
      /*
       * This should retrieve all properties for an object, but on devices
       * which are inherently broken it will not, so these need the
       * special flag DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST.
       */
      ret = ptp_mtp_getobjectproplist(params, file->item_id, &proplist);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Filelisting_With_Callback(): call to ptp_mtp_getobjectproplist() failed.");
	// Silently fall through.
      }
      prop = proplist;
      while (prop != NULL && prop->ObjectHandle == file->item_id) {
	// Pick ObjectSize here...
	if (prop->property == PTP_OPC_ObjectSize) {
	  // This may already be set, but this 64bit precision value 
	  // is better than the PTP 32bit value, so let it override.
	  file->filesize = prop->propval.u64;
	  break;
	}
	prop = prop->next;
      }
      destroy_mtp_prop_list(proplist);
    } else {
      uint16_t *props = NULL;
      uint32_t propcnt = 0;
      
      // First see which properties can be retrieved for this object format
      ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(file->filetype), &propcnt, &props);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Filelisting_With_Callback(): call to ptp_mtp_getobjectpropssupported() failed.");
	// Silently fall through.
      } else {
	for (i=0;i<propcnt;i++) {
	  switch (props[i]) {
	  case PTP_OPC_ObjectSize:
	    file->filesize = get_u64_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
	    break;
	  default:
	    break;
	  }
	}
	free(props);
      }
    }
    
    // Add track to a list that will be returned afterwards.
    if (retfiles == NULL) {
      retfiles = file;
      curfile = file;
    } else {
      curfile->next = file;
      curfile = file;
    }

    // Call listing callback
    // double progressPercent = (double)i*(double)100.0 / (double)params->handles.n;
    
  } // Handle counting loop
  return retfiles;
}

/**
 * This function retrieves the metadata for a single file off
 * the device.
 *
 * Do not call this function repeatedly! The file handles are linearly
 * searched O(n) and the call may involve (slow) USB traffic, so use
 * <code>LIBMTP_Get_Filelisting()</code> and cache the file, preferably
 * as an efficient data structure such as a hash list.
 *
 * @param device a pointer to the device to get the file metadata from.
 * @param fileid the object ID of the file that you want the metadata for.
 * @return a metadata entry on success or NULL on failure.
 * @see LIBMTP_Get_Filelisting()
 */
LIBMTP_file_t *LIBMTP_Get_Filemetadata(LIBMTP_mtpdevice_t *device, uint32_t const fileid)
{
  uint32_t i = 0;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint16_t ret;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_file_t *file;
    PTPObjectInfo *oi;

    // Is this the file we're looking for?
    if (params->handles.Handler[i] != fileid) {
      continue;
    }

    oi = &params->objectinfo[i];

    if (oi->ObjectFormat == PTP_OFC_Association) {
      // MTP use thesis object format for folders which means
      // these "files" will turn up on a folder listing instead.
      return NULL;
    }

    // Allocate a new file type
    file = LIBMTP_new_file_t();
    
    file->parent_id = oi->ParentObject;

    // Set the filetype
    file->filetype = map_ptp_type_to_libmtp_type(oi->ObjectFormat);

    // Original file-specific properties
    
    // We only have 32-bit file size here; later we use the PTP_OPC_ObjectSize property
    file->filesize = oi->ObjectCompressedSize;
    if (oi->Filename != NULL) {
      file->filename = strdup(oi->Filename);
    }

    // This is some sort of unique ID so we can keep track of the file.
    file->item_id = params->handles.Handler[i];

    /*
     * If we have a cached, large set of metadata, then use it!
     */
    if (params->proplist) {
      MTPPropList *prop = params->proplist;
      
      while (prop != NULL && prop->ObjectHandle != file->item_id) {
	prop = prop->next;
      }
      while (prop != NULL && prop->ObjectHandle == file->item_id) {
	// Pick ObjectSize here...
	if (prop->property == PTP_OPC_ObjectSize) {
	  // This may already be set, but this 64bit precision value 
	  // is better than the PTP 32bit value, so let it override.
	  file->filesize = prop->propval.u64;
	  break;
	}
	prop = prop->next;
      }
    } else if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)
	       && !(ptp_usb->device_flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST)) {
      MTPPropList *proplist = NULL;
      MTPPropList *prop;
      
      /*
       * This should retrieve all properties for an object, but on devices
       * which are inherently broken it will not, so these need the
       * special flag DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST.
       */
      ret = ptp_mtp_getobjectproplist(params, file->item_id, &proplist);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Filelisting_With_Callback(): call to ptp_mtp_getobjectproplist() failed.");
	// Silently fall through.
      }
      prop = proplist;
      while (prop != NULL && prop->ObjectHandle == file->item_id) {
	// Pick ObjectSize here...
	if (prop->property == PTP_OPC_ObjectSize) {
	  // This may already be set, but this 64bit precision value 
	  // is better than the PTP 32bit value, so let it override.
	  file->filesize = prop->propval.u64;
	  break;
	}
	prop = prop->next;
      }
      destroy_mtp_prop_list(proplist);
    } else {
      uint16_t *props = NULL;
      uint32_t propcnt = 0;
      
      // First see which properties can be retrieved for this object format
      ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(file->filetype), &propcnt, &props);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Filemetadata(): call to ptp_mtp_getobjectpropssupported() failed.");
	// Silently fall through.
      } else {
	for (i=0;i<propcnt;i++) {
	  switch (props[i]) {
	  case PTP_OPC_ObjectSize:
	    file->filesize = get_u64_from_object(device, file->item_id, PTP_OPC_ObjectSize, 0);
	    break;
	  default:
	    break;
	  }
	}
	free(props);
      }
    }
    
    return file;
    
  }
  return NULL;
}

/**
 * This creates a new track metadata structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_destroy_track_t</code>
 * operation later, so be careful of using strdup() when assigning
 * strings, e.g.:
 *
 * <pre>
 * LIBMTP_track_t *track = LIBMTP_new_track_t();
 * track->title = strdup(titlestr);
 * ....
 * LIBMTP_destroy_track_t(track);
 * </pre>
 *
 * @return a pointer to the newly allocated metadata structure.
 * @see LIBMTP_destroy_track_t()
 */
LIBMTP_track_t *LIBMTP_new_track_t(void)
{
  LIBMTP_track_t *new = (LIBMTP_track_t *) malloc(sizeof(LIBMTP_track_t));
  if (new == NULL) {
    return NULL;
  }
  new->title = NULL;
  new->artist = NULL;
  new->album = NULL;
  new->genre = NULL;
  new->date = NULL;
  new->filename = NULL;
  new->duration = 0;
  new->tracknumber = 0;
  new->filesize = 0;
  new->filetype = LIBMTP_FILETYPE_UNKNOWN;
  new->samplerate = 0;
  new->nochannels = 0;
  new->wavecodec = 0;
  new->bitrate = 0;
  new->bitratetype = 0;
  new->rating = 0;
  new->usecount = 0;
  new->next = NULL;
  return new;
}

/**
 * This destroys a track metadata structure and deallocates the memory
 * used by it, including any strings. Never use a track metadata
 * structure again after calling this function on it.
 * @param track the track metadata to destroy.
 * @see LIBMTP_new_track_t()
 */
void LIBMTP_destroy_track_t(LIBMTP_track_t *track)
{
  if (track == NULL) {
    return;
  }
  if (track->title != NULL)
    free(track->title);
  if (track->artist != NULL)
    free(track->artist);
  if (track->album != NULL)
    free(track->album);
  if (track->genre != NULL)
    free(track->genre);
  if (track->date != NULL)
    free(track->date);
  if (track->filename != NULL)
    free(track->filename);
  free(track);
  return;
}

/**
 * This function maps and copies a property onto the track metadata if applicable.
 */
static void pick_property_to_track_metadata(MTPPropList *prop, LIBMTP_track_t *track)
{
  switch (prop->property) {
  case PTP_OPC_Name:
    if (prop->propval.str != NULL)
      track->title = strdup(prop->propval.str);
    else
      track->title = NULL;
    break;
  case PTP_OPC_Artist:
    if (prop->propval.str != NULL)
      track->artist = strdup(prop->propval.str);
    else
      track->artist = NULL;
    break;
  case PTP_OPC_Duration:
    track->duration = prop->propval.u32;
    break;
  case PTP_OPC_Track:
    track->tracknumber = prop->propval.u16;
    break;
  case PTP_OPC_Genre:
    if (prop->propval.str != NULL)
      track->genre = strdup(prop->propval.str);
    else
      track->genre = NULL;
    break;
  case PTP_OPC_AlbumName:
    if (prop->propval.str != NULL)
      track->album = strdup(prop->propval.str);
    else
      track->album = NULL;
    break;
  case PTP_OPC_OriginalReleaseDate:
    if (prop->propval.str != NULL)
      track->date = strdup(prop->propval.str);
    else
      track->date = NULL;
    break;
    // These are, well not so important.
  case PTP_OPC_SampleRate:
    track->samplerate = prop->propval.u32;
    break;
  case PTP_OPC_NumberOfChannels:
    track->nochannels = prop->propval.u16;
    break;
  case PTP_OPC_AudioWAVECodec:
    track->wavecodec = prop->propval.u32;
    break;
  case PTP_OPC_AudioBitRate:
    track->bitrate = prop->propval.u32;
    break;
  case PTP_OPC_BitRateType:
    track->bitratetype = prop->propval.u16;
    break;
  case PTP_OPC_Rating:
    track->rating = prop->propval.u16;
    break;
  case PTP_OPC_UseCount:
    track->usecount = prop->propval.u32;
    break;
  case PTP_OPC_ObjectSize:
    // This may already be set, but this 64bit precision value 
    // is better than the PTP 32bit value, so let it override.
    track->filesize = prop->propval.u64;
    break;
  default:
    break;
  }
}

/**
 * This function retrieves the track metadata for a track
 * given by a unique ID.
 * @param device a pointer to the device to get the track metadata off.
 * @param trackid the unique ID of the track.
 * @param objectformat the object format of this track, so we know what it supports.
 * @param track a metadata set to fill in.
 */
static void get_track_metadata(LIBMTP_mtpdevice_t *device, uint16_t objectformat,
			       LIBMTP_track_t *track)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  uint32_t i;

  /*
   * If we have a cached, large set of metadata, then use it!
   */
  if (params->proplist) {
    MTPPropList *prop = params->proplist;
    
    while (prop != NULL && prop->ObjectHandle != track->item_id) {
      prop = prop->next;
    }
    while (prop != NULL && prop->ObjectHandle == track->item_id) {
      pick_property_to_track_metadata(prop, track);
      prop = prop->next;
    }
  } else if (ptp_operation_issupported(params,PTP_OC_MTP_GetObjPropList)
	     && !(ptp_usb->device_flags & DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST)) {
    MTPPropList *proplist = NULL;
    MTPPropList *prop;

    /*
     * This should retrieve all properties for an object, but on devices
     * which are inherently broken it will not, so these need the
     * special flag DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST.
     */
    ret = ptp_mtp_getobjectproplist(params, track->item_id, &proplist);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "get_track_metadata(): call to ptp_mtp_getobjectproplist() failed.");
      return;
    }
    prop = proplist;
    while (prop != NULL && prop->ObjectHandle == track->item_id) {
      pick_property_to_track_metadata(prop, track);
      prop = prop->next;
    }
    destroy_mtp_prop_list(proplist);
  } else {
    uint16_t *props = NULL;
    uint32_t propcnt = 0;

    // First see which properties can be retrieved for this object format
    ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(track->filetype), &propcnt, &props);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "get_track_metadata(): call to ptp_mtp_getobjectpropssupported() failed.");
      // Just bail out for now, nothing is ever set.
      return;
    } else {
      for (i=0;i<propcnt;i++) {
	switch (props[i]) {
	case PTP_OPC_Name:
	  track->title = get_string_from_object(device, track->item_id, PTP_OPC_Name);
	  break;
	case PTP_OPC_Artist:
	  track->artist = get_string_from_object(device, track->item_id, PTP_OPC_Artist);
	  break;
	case PTP_OPC_Duration:
	  track->duration = get_u32_from_object(device, track->item_id, PTP_OPC_Duration, 0);
	  break;
	case PTP_OPC_Track:
	  track->tracknumber = get_u16_from_object(device, track->item_id, PTP_OPC_Track, 0);
	  break;
	case PTP_OPC_Genre:
	  track->genre = get_string_from_object(device, track->item_id, PTP_OPC_Genre);
	  break;
	case PTP_OPC_AlbumName:
	  track->album = get_string_from_object(device, track->item_id, PTP_OPC_AlbumName);
	  break;
	case PTP_OPC_OriginalReleaseDate:
	  track->date = get_string_from_object(device, track->item_id, PTP_OPC_OriginalReleaseDate);
	  break;
	  // These are, well not so important.
	case PTP_OPC_SampleRate:
	  track->samplerate = get_u32_from_object(device, track->item_id, PTP_OPC_SampleRate, 0);
	  break;
	case PTP_OPC_NumberOfChannels:
	  track->nochannels = get_u16_from_object(device, track->item_id, PTP_OPC_NumberOfChannels, 0);
	  break;
	case PTP_OPC_AudioWAVECodec:
	  track->wavecodec = get_u32_from_object(device, track->item_id, PTP_OPC_AudioWAVECodec, 0);
	  break;
	case PTP_OPC_AudioBitRate:
	  track->bitrate = get_u32_from_object(device, track->item_id, PTP_OPC_AudioBitRate, 0);
	  break;
	case PTP_OPC_BitRateType:
	  track->bitratetype = get_u16_from_object(device, track->item_id, PTP_OPC_BitRateType, 0);
	  break;
	case PTP_OPC_Rating:
	  track->rating = get_u16_from_object(device, track->item_id, PTP_OPC_Rating, 0);
	  break;
	case PTP_OPC_UseCount:
	  track->usecount = get_u32_from_object(device, track->item_id, PTP_OPC_UseCount, 0);
	  break;
	case PTP_OPC_ObjectSize:
	  // This may already be set, but this 64bit precision value 
	  // is better than the PTP 32bit value, so let it override.
	  track->filesize = get_u64_from_object(device, track->item_id, PTP_OPC_ObjectSize, 0);
	  break;
	}
      }
      free(props);
    }
  }
}

/**
 * THIS FUNCTION IS DEPRECATED. PLEASE UPDATE YOUR CODE IN ORDER
 * NOT TO USE IT.
 * @see LIBMTP_Get_Tracklisting_With_Callback()
 */
LIBMTP_track_t *LIBMTP_Get_Tracklisting(LIBMTP_mtpdevice_t *device)
{
  printf("WARNING: LIBMTP_Get_Tracklisting() is deprecated.\n");
  printf("WARNING: please update your code to use LIBMTP_Get_Tracklisting_With_Callback()\n");
  return LIBMTP_Get_Tracklisting_With_Callback(device, NULL, NULL);
}

/**
 * This returns a long list of all tracks available
 * on the current MTP device. Typical usage:
 *
 * <pre>
 * LIBMTP_track_t *tracklist;
 *
 * tracklist = LIBMTP_Get_Tracklisting_With_Callback(device, callback, data);
 * while (tracklist != NULL) {
 *   LIBMTP_track_t *tmp;
 *
 *   // Do something on each element in the list here...
 *   tmp = tracklist;
 *   tracklist = tracklist->next;
 *   LIBMTP_destroy_track_t(tmp);
 * }
 * </pre>
 *
 * @param device a pointer to the device to get the track listing for.
 * @param callback a function to be called during the tracklisting retrieveal
 *               for displaying progress bars etc, or NULL if you don't want
 *               any callbacks.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return a list of tracks that can be followed using the <code>next</code>
 *         field of the <code>LIBMTP_track_t</code> data structure.
 *         Each of the metadata tags must be freed after use, and may
 *         contain only partial metadata information, i.e. one or several
 *         fields may be NULL or 0.
 * @see LIBMTP_Get_Trackmetadata()
 */
LIBMTP_track_t *LIBMTP_Get_Tracklisting_With_Callback(LIBMTP_mtpdevice_t *device,
                                                      LIBMTP_progressfunc_t const callback,
                                                      void const * const data)
{
  uint32_t i = 0;
  LIBMTP_track_t *retracks = NULL;
  LIBMTP_track_t *curtrack = NULL;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_track_t *track;
    PTPObjectInfo *oi;

    if (callback != NULL)
      callback(i, params->handles.n, data);

    oi = &params->objectinfo[i];

    // Ignore stuff we don't know how to handle...
    // TODO: get this list as an intersection of the sets
    // supported by the device and the from the device and
    // all known audio track files?
    if ( oi->ObjectFormat != PTP_OFC_WAV &&
	 oi->ObjectFormat != PTP_OFC_MP3 &&
	 oi->ObjectFormat != PTP_OFC_MTP_MP2 &&
	 oi->ObjectFormat != PTP_OFC_MTP_WMA &&
	 oi->ObjectFormat != PTP_OFC_MTP_OGG &&
	 oi->ObjectFormat != PTP_OFC_MTP_FLAC &&
	 oi->ObjectFormat != PTP_OFC_MTP_AAC &&
	 oi->ObjectFormat != PTP_OFC_MTP_M4A &&
	 oi->ObjectFormat != PTP_OFC_MTP_MP4 &&
	 oi->ObjectFormat != PTP_OFC_MTP_UndefinedAudio &&
	 // This row lets through undefined files for examination since they may be forgotten OGG files.
	 (oi->ObjectFormat != PTP_OFC_Undefined || !(ptp_usb->device_flags & DEVICE_FLAG_IRIVER_OGG_ALZHEIMER))
	 ) {
      // printf("Not a music track (format: %d), skipping...\n",oi.ObjectFormat);
      continue;
    }

    // Allocate a new track type
    track = LIBMTP_new_track_t();
      
    // This is some sort of unique ID so we can keep track of the track.
    track->item_id = params->handles.Handler[i];
    track->parent_id = oi->ParentObject;

    track->filetype = map_ptp_type_to_libmtp_type(oi->ObjectFormat);

    // Original file-specific properties
    track->filesize = oi->ObjectCompressedSize;
    if (oi->Filename != NULL) {
      track->filename = strdup(oi->Filename);
    }

    get_track_metadata(device, oi->ObjectFormat, track);

    /*
     * A special quirk for iriver devices that doesn't quite
     * remember that some files marked as "unknown" type are
     * actually OGG files. We look at the filename extension
     * and see if it happens that this was atleast named "ogg"
     * and fall back on this heuristic approach in that case, 
     * for these bugged devices only.
     */
    if (track->filetype == LIBMTP_FILETYPE_UNKNOWN &&
	ptp_usb->device_flags & DEVICE_FLAG_IRIVER_OGG_ALZHEIMER) {
      // Repair forgotten OGG filetype
      char *ptype;
      
      ptype = rindex(track->filename,'.')+1;
      if (ptype != NULL && !strcasecmp (ptype, "ogg")) {
	// Fix it.
	track->filetype = LIBMTP_FILETYPE_OGG;
      } else {
	// This was not an OGG file so discard it and continue
	LIBMTP_destroy_track_t(track);
	continue;
      }
    }
    
    // Add track to a list that will be returned afterwards.
    if (retracks == NULL) {
      retracks = track;
      curtrack = track;
    } else {
      curtrack->next = track;
      curtrack = track;
    }
    
    // Call listing callback
    // double progressPercent = (double)i*(double)100.0 / (double)params->handles.n;
    
    
  } // Handle counting loop
  return retracks;
}

/**
 * This function retrieves the metadata for a single track off
 * the device.
 *
 * Do not call this function repeatedly! The track handles are linearly
 * searched O(n) and the call may involve (slow) USB traffic, so use
 * <code>LIBMTP_Get_Tracklisting()</code> and cache the tracks, preferably
 * as an efficient data structure such as a hash list.
 *
 * @param device a pointer to the device to get the track metadata from.
 * @param trackid the object ID of the track that you want the metadata for.
 * @return a track metadata entry on success or NULL on failure.
 * @see LIBMTP_Get_Tracklisting()
 */
LIBMTP_track_t *LIBMTP_Get_Trackmetadata(LIBMTP_mtpdevice_t *device, uint32_t const trackid)
{
  uint32_t i = 0;
  PTPParams *params = (PTPParams *) device->params;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    PTPObjectInfo *oi;
    LIBMTP_track_t *track;

    // Skip if this is not the track we want.
    if (params->handles.Handler[i] != trackid) {
      continue;
    }

    oi = &params->objectinfo[i];

    // Ignore stuff we don't know how to handle...
    if ( oi->ObjectFormat != PTP_OFC_WAV &&
	 oi->ObjectFormat != PTP_OFC_MP3 &&
	 oi->ObjectFormat != PTP_OFC_MTP_MP2 &&
	 oi->ObjectFormat != PTP_OFC_MTP_WMA &&
	 oi->ObjectFormat != PTP_OFC_MTP_OGG &&
	 oi->ObjectFormat != PTP_OFC_MTP_FLAC &&
	 oi->ObjectFormat != PTP_OFC_MTP_AAC &&
	 oi->ObjectFormat != PTP_OFC_MTP_M4A &&
	 oi->ObjectFormat != PTP_OFC_MTP_MP4 &&
	 oi->ObjectFormat != PTP_OFC_MTP_UndefinedAudio ) {
      return NULL;
    }

    // Allocate a new track type
    track = LIBMTP_new_track_t();
    
    // This is some sort of unique ID so we can keep track of the track.
    track->item_id = params->handles.Handler[i];
    track->parent_id = oi->ParentObject;

    track->filetype = map_ptp_type_to_libmtp_type(oi->ObjectFormat);

    // Original file-specific properties
    track->filesize = oi->ObjectCompressedSize;
    if (oi->Filename != NULL) {
      track->filename = strdup(oi->Filename);
    }

    get_track_metadata(device, oi->ObjectFormat, track);
    
    return track;
    
  }
  return NULL;
}


/**
 * This gets a file off the device to a local file identified
 * by a filename.
 * @param device a pointer to the device to get the track from.
 * @param id the file ID of the file to retrieve.
 * @param path a filename to use for the retrieved file.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Get_File_To_File_Descriptor()
 */
int LIBMTP_Get_File_To_File(LIBMTP_mtpdevice_t *device, uint32_t const id,
			 char const * const path, LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  int fd = -1;
  int ret;

  // Sanity check
  if (path == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File(): Bad arguments, path was NULL.");
    return -1;
  }

  // Open file
#ifdef __WIN32__
#ifdef USE_WINDOWS_IO_H
  if ( (fd = _open(path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY,_S_IREAD)) == -1 ) {
#else
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY,S_IRWXU|S_IRGRP)) == -1 ) {
#endif
#else
#ifdef __USE_LARGEFILE64
  if ( (fd = open64(path, O_RDWR|O_CREAT|O_TRUNC|O_LARGEFILE,S_IRWXU|S_IRGRP)) == -1) {
#else
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRGRP)) == -1) {
#endif
#endif
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File(): Could not create file.");
    return -1;
  }

  ret = LIBMTP_Get_File_To_File_Descriptor(device, id, fd, callback, data);

  // Close file
  close(fd);

  return ret;
}

/**
 * This gets a file off the device to a file identified
 * by a file descriptor.
 *
 * This function can potentially be used for streaming
 * files off the device for playback or broadcast for example,
 * by downloading the file into a stream sink e.g. a socket.
 *
 * @param device a pointer to the device to get the file from.
 * @param id the file ID of the file to retrieve.
 * @param fd a local file descriptor to write the file to.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Get_File_To_File()
 */
int LIBMTP_Get_File_To_File_Descriptor(LIBMTP_mtpdevice_t *device,
					uint32_t const id,
					int const fd,
					LIBMTP_progressfunc_t const callback,
					void const * const data)
{
  PTPObjectInfo *oi;
  uint32_t i;
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  oi = NULL;
  for (i = 0; i < params->handles.n; i++) {
    if (params->handles.Handler[i] == id) {
      oi = &params->objectinfo[i];
      break;
    }
  }
  if (oi == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File_Descriptor(): Could not get object info.");
    return -1;
  }
  if (oi->ObjectFormat == PTP_OFC_Association) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Get_File_To_File_Descriptor(): Bad object format.");
    return -1;
  }

  // Callbacks
  ptp_usb->callback_active = 1;
  ptp_usb->current_transfer_total = oi->ObjectCompressedSize+
    PTP_USB_BULK_HDR_LEN+sizeof(uint32_t); // Request length, one parameter
  ptp_usb->current_transfer_complete = 0;
  ptp_usb->current_transfer_callback = callback;
  ptp_usb->current_transfer_callback_data = data;

  // This now exist in upstream
  ret = ptp_getobject_tofd(params, id, fd);

  ptp_usb->callback_active = 0;
  ptp_usb->current_transfer_callback = NULL;
  ptp_usb->current_transfer_callback_data = NULL;

  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_File_To_File_Descriptor(): Could not get file from device.");
    return -1;
  }

  return 0;
}

/**
 * This gets a track off the device to a file identified
 * by a filename. This is actually just a wrapper for the
 * \c LIBMTP_Get_Track_To_File() function.
 * @param device a pointer to the device to get the track from.
 * @param id the track ID of the track to retrieve.
 * @param path a filename to use for the retrieved track.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Get_Track_To_File_Descriptor()
 */
int LIBMTP_Get_Track_To_File(LIBMTP_mtpdevice_t *device, uint32_t const id,
			 char const * const path, LIBMTP_progressfunc_t const callback,
			 void const * const data)
{
  // This is just a wrapper
  return LIBMTP_Get_File_To_File(device, id, path, callback, data);
}

/**
 * This gets a track off the device to a file identified
 * by a file descriptor. This is actually just a wrapper for
 * the \c LIBMTP_Get_File_To_File_Descriptor() function.
 * @param device a pointer to the device to get the track from.
 * @param id the track ID of the track to retrieve.
 * @param fd a file descriptor to write the track to.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Get_Track_To_File()
 */
int LIBMTP_Get_Track_To_File_Descriptor(LIBMTP_mtpdevice_t *device,
					uint32_t const id,
					int const fd,
					LIBMTP_progressfunc_t const callback,
					void const * const data)
{
  // This is just a wrapper
  return LIBMTP_Get_File_To_File_Descriptor(device, id, fd, callback, data);
}

/**
 * This function sends a track from a local file to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 * @param device a pointer to the device to send the track to.
 * @param path the filename of a local file which will be sent.
 * @param metadata a track metadata set to be written along with the file.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @param parenthandle the parent (e.g. folder) to store this file
 *             in. Since some devices are a bit picky about where files
 *             are placed, a default folder will be chosen if libmtp
 *             has detected one for the current filetype and this
 *             parameter is set to 0. If this is 0 and no default folder
 *             can be found, the file will be stored in the root folder.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_Track_From_File_Descriptor()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_Track_From_File(LIBMTP_mtpdevice_t *device,
			 char const * const path, LIBMTP_track_t * const metadata,
                         LIBMTP_progressfunc_t const callback,
			 void const * const data, uint32_t const parenthandle)
{
  int fd;
  int ret;

  // Sanity check
  if (path == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_Track_From_File(): Bad arguments, path was NULL.");
    return -1;
  }

  // Open file
#ifdef __WIN32__
#ifdef USE_WINDOWS_IO_H
  if ( (fd = _open(path, O_RDONLY|O_BINARY) == -1 ) {
#else
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1 ) {
#endif
#else
#ifdef __USE_LARGEFILE64
  if ( (fd = open64(path, O_RDONLY|O_LARGEFILE)) == -1) {
#else
  if ( (fd = open(path, O_RDONLY)) == -1) {
#endif
#endif
    printf("LIBMTP_Send_Track_From_File(): Could not open source file \"%s\"\n", path);
    return -1;
  }

  ret = LIBMTP_Send_Track_From_File_Descriptor(device, fd, metadata, callback, data, parenthandle);

  // Close file.
#ifdef USE_WINDOWS_IO_H
  _close(fd);
#else
  close(fd);
#endif

  return ret;
}


static MTPPropList *new_mtp_prop_entry()
{
  MTPPropList *prop;
  prop = (MTPPropList *) malloc(sizeof(MTPPropList));
  prop->property = PTP_OPC_StorageID; /* Should be "unknown" */
  prop->datatype = PTP_DTC_UNDEF;
  prop->ObjectHandle = 0x00000000U;
  prop->next = NULL;
  return prop;
}

static MTPPropList *add_mtp_prop_to_proplist(MTPPropList *proplist, MTPPropList *prop)
{
  if (proplist == NULL) {
    return prop;
  } else {
    MTPPropList *tmp = proplist;
    while (tmp->next != NULL) {
      tmp = tmp->next;
    }
    tmp->next = prop;
  }
  return proplist;
}

static void destroy_mtp_prop_list(MTPPropList *proplist)
{
  MTPPropList *tmp = proplist;
  while (tmp != NULL) {
    MTPPropList *prop = tmp;
    
    tmp = tmp->next;
    if (prop->datatype == PTP_DTC_STR) {
      free(prop->propval.str);
    }
    free(prop);
  }
}

/**
 * This function sends a track from a file descriptor to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 * @param device a pointer to the device to send the track to.
 * @param fd the filedescriptor for a local file which will be sent.
 * @param metadata a track metadata set to be written along with the file.
 *                 After this call the field <code>item_id</code>
 *                 will contain the new track ID.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @param parenthandle the parent (e.g. folder) to store this file
 *             in. Since some devices are a bit picky about where files
 *             are placed, a default folder will be chosen if libmtp
 *             has detected one for the current filetype and this
 *             parameter is set to 0. If this is 0 and no default folder
 *             can be found, the file will be stored in the root folder.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_Track_From_File()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_Track_From_File_Descriptor(LIBMTP_mtpdevice_t *device,
			 int const fd, LIBMTP_track_t * const metadata,
                         LIBMTP_progressfunc_t const callback,
			 void const * const data, uint32_t const parenthandle)
{
  int subcall_ret;
  LIBMTP_file_t filedata;

  // Sanity check, is this really a track?
  if (metadata->filetype != LIBMTP_FILETYPE_WAV &&
      metadata->filetype != LIBMTP_FILETYPE_MP3 &&
      metadata->filetype != LIBMTP_FILETYPE_MP2 &&
      metadata->filetype != LIBMTP_FILETYPE_WMA &&
      metadata->filetype != LIBMTP_FILETYPE_OGG &&
      metadata->filetype != LIBMTP_FILETYPE_FLAC &&
      metadata->filetype != LIBMTP_FILETYPE_AAC &&
      metadata->filetype != LIBMTP_FILETYPE_M4A &&
      metadata->filetype != LIBMTP_FILETYPE_MP4 &&
      metadata->filetype != LIBMTP_FILETYPE_UNDEF_AUDIO) {
    printf("LIBMTP_Send_Track_From_File_Descriptor(): "
	   "I don't think this is actually a track, strange filetype...\n");
  }

  // Wrap around the file transfer function
  filedata.item_id = metadata->item_id;
  filedata.parent_id = metadata->parent_id;
  filedata.filename = metadata->filename;
  filedata.filesize = metadata->filesize;
  filedata.filetype = metadata->filetype;
  filedata.next = NULL;

  subcall_ret = LIBMTP_Send_File_From_File_Descriptor(device,
						      fd, 
						      &filedata,
						      callback,
						      data, 
						      parenthandle);

  if (subcall_ret != 0) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_Track_From_File_Descriptor(): "
			    "subcall to LIBMTP_Send_File_From_File_Descriptor failed.");
    // We used to delete the file here, but don't... It might be OK after all.
    // (void) LIBMTP_Delete_Object(device, metadata->item_id);
    return -1;
  }
  
  // Pick up new item (and parent) ID
  metadata->item_id = filedata.item_id;
  metadata->parent_id = filedata.parent_id;

  // Set track metadata for the new fine track
  subcall_ret = LIBMTP_Update_Track_Metadata(device, metadata);
  if (subcall_ret != 0) {
    // Subcall will add error to errorstack
    // We used to delete the file here, but don't... It might be OK after all.
    // (void) LIBMTP_Delete_Object(device, metadata->item_id);
    return -1;
  }

  add_object_to_cache(device, metadata->item_id);

  return 0;
}

/**
 * This function sends a local file to an MTP device.
 * A filename and a set of metadata must be
 * given as input.
 * @param device a pointer to the device to send the track to.
 * @param path the filename of a local file which will be sent.
 * @param filedata a file strtuct to pass in info about the file.
 *                 After this call the field <code>item_id</code>
 *                 will contain the new file ID.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @param parenthandle the parent (e.g. folder) to store this file
 *        in. Since some devices are a bit picky about where files
 *        are placed, a default folder will be chosen if libmtp
 *        has detected one for the current filetype and this
 *        parameter is set to 0. If this is 0 and no default folder
 *        can be found, the file will be stored in the root folder.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_File_From_File_Descriptor()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_File_From_File(LIBMTP_mtpdevice_t *device,
			       char const * const path, LIBMTP_file_t * const filedata,
			       LIBMTP_progressfunc_t const callback,
			       void const * const data, uint32_t const parenthandle)
{
  int fd;
  int ret;

  // Sanity check
  if (path == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_File_From_File(): Bad arguments, path was NULL.");
    return -1;
  }

  // Open file
#ifdef __WIN32__
#ifdef USE_WINDOWS_IO_H
  if ( (fd = _open(path, O_RDONLY|O_BINARY) == -1 ) {
#else
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1 ) {
#endif
#else
#ifdef __USE_LARGEFILE64
  if ( (fd = open64(path, O_RDONLY|O_LARGEFILE)) == -1) {
#else
  if ( (fd = open(path, O_RDONLY)) == -1) {
#endif
#endif
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_File_From_File(): Could not open source file.");
    return -1;
  }

  ret = LIBMTP_Send_File_From_File_Descriptor(device, fd, filedata, callback, data, parenthandle);

  // Close file.
#ifdef USE_WINDOWS_IO_H
  _close(fd);
#else
  close(fd);
#endif

  return ret;
}

/**
 * This function sends a generic file from a file descriptor to an
 * MTP device. A filename and a set of metadata must be
 * given as input.
 *
 * This can potentially be used for sending in a stream of unknown
 * length. Set <code>filedata->filesize = (uint64_t) -1</code> to
 * make libmtp send some dummy length to the device and just
 * accept a stream up to some device-determined max length. There
 * is not guarantee this will work on all devices... Remember to
 * set correct metadata for the track with
 * <code>LIBMTP_Update_Track_Metadata()</code> afterwards if it's
 * a music file. (This doesn't seem to work very well right now.)
 *
 * @param device a pointer to the device to send the file to.
 * @param fd the filedescriptor for a local file which will be sent.
 * @param filedata a file strtuct to pass in info about the file.
 *                 After this call the field <code>item_id</code>
 *                 will contain the new track ID.
 * @param callback a progress indicator function or NULL to ignore.
 * @param data a user-defined pointer that is passed along to
 *             the <code>progress</code> function in order to
 *             pass along some user defined data to the progress
 *             updates. If not used, set this to NULL.
 * @param parenthandle the parent (e.g. folder) to store this file
 *        in. Since some devices are a bit picky about where files
 *        are placed, a default folder will be chosen if libmtp
 *        has detected one for the current filetype and this
 *        parameter is set to 0. If this is 0 and no default folder
 *        can be found, the file will be stored in the root folder.
 * @return 0 if the transfer was successful, any other value means
 *           failure.
 * @see LIBMTP_Send_File_From_File()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Send_File_From_File_Descriptor(LIBMTP_mtpdevice_t *device,
			 int const fd, LIBMTP_file_t * const filedata,
                         LIBMTP_progressfunc_t const callback,
			 void const * const data, uint32_t const parenthandle)
{
  uint16_t ret;
  uint32_t store = get_first_storageid(device);
  uint32_t localph = parenthandle;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;
  int i;
  int subcall_ret;
  uint16_t of =  map_libmtp_type_to_ptp_type(filedata->filetype);
  uint8_t nonconsumable = 0x01U; /* By default it is non-consumable */

  subcall_ret = check_if_file_fits(device, filedata->filesize);
  if (subcall_ret != 0) {
    return -1;
  }

  // Sanity check: no zerolength files
  if (filedata->filesize == 0) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_File_From_File_Descriptor(): "
			    "File of zero size.");
    return -1;
  }


  /*
   * If this file is among the supported filetypes for this device,
   * then it is indeed consumable.
   */
  for (i=0;i<params->deviceinfo.ImageFormats_len;i++) {
    if (params->deviceinfo.ImageFormats[i] ==  of) {
      nonconsumable = 0x00U;
      break;
    }
  }

  /*
   * If no destination folder was given, look up a default
   * folder if possible. Perhaps there is some way of retrieveing
   * the default folder for different forms of content, what
   * do I know, we use a fixed list in lack of any better method.
   * Some devices obviously need to have their files in certain
   * folders in order to find/display them at all (hello Creative),
   * so we have to have a method for this.
   */

  if (localph == 0) {
    if (of == PTP_OFC_WAV ||
	of == PTP_OFC_MP3 ||
	of == PTP_OFC_MTP_MP2 ||
	of == PTP_OFC_MTP_WMA ||
	of == PTP_OFC_MTP_OGG ||
	of == PTP_OFC_MTP_FLAC ||
	of == PTP_OFC_MTP_AAC ||
	of == PTP_OFC_MTP_M4A ||
	of == PTP_OFC_AIFF ||
	//of == PTP_OFC_MTP_MP4 || 	/* ambiguous mp4 can contain video */
	of == PTP_OFC_MTP_AudibleCodec ||
	of == PTP_OFC_MTP_UndefinedAudio) {
      localph = device->default_music_folder;
    } else if (of == PTP_OFC_MTP_WMV ||
	       of == PTP_OFC_AVI ||
	       of == PTP_OFC_MPEG ||
	       of == PTP_OFC_ASF ||
	       of == PTP_OFC_QT ||
	       of == PTP_OFC_MTP_3GP ||
	       of == PTP_OFC_MTP_MP4 || /* ambiguous mp4 can also contain only audio */
	       of == PTP_OFC_MTP_UndefinedVideo) {
      localph = device->default_video_folder;
    } else if (of == PTP_OFC_EXIF_JPEG ||
	       of == PTP_OFC_JP2 ||
	       of == PTP_OFC_JPX ||
	       of == PTP_OFC_JFIF ||
	       of == PTP_OFC_TIFF ||
	       of == PTP_OFC_TIFF_IT ||
	       of == PTP_OFC_BMP ||
	       of == PTP_OFC_GIF ||
	       of == PTP_OFC_PICT ||
	       of == PTP_OFC_PNG ||
	       of == PTP_OFC_MTP_WindowsImageFormat) {
      localph = device->default_picture_folder;
    } else if (of == PTP_OFC_MTP_vCalendar1 ||
	       of == PTP_OFC_MTP_vCalendar2 ||
	       of == PTP_OFC_MTP_UndefinedContact ||
	       of == PTP_OFC_MTP_vCard2 ||
	       of == PTP_OFC_MTP_vCard3 ||
	       of == PTP_OFC_MTP_UndefinedCalendarItem) {
      localph = device->default_organizer_folder;
    } else if (of == PTP_OFC_Text
		) {
      localph = device->default_text_folder;
    }
  }

  if (ptp_operation_issupported(params,PTP_OC_MTP_SendObjectPropList)) {
    /*
     * MTP enhanched does it this way (from a sniff):
     * -> PTP_OC_MTP_SendObjectPropList (0x9808):
     *    20 00 00 00 01 00 08 98 1B 00 00 00 01 00 01 00
     *    FF FF FF FF 00 30 00 00 00 00 00 00 12 5E 00 00
     *    Length: 0x00000020
     *    Type:   0x0001 PTP_USB_CONTAINER_COMMAND
     *    Code:   0x9808
     *    Transaction ID: 0x0000001B
     *    Param1: 0x00010001 <- store
     *    Param2: 0xffffffff <- parent handle (-1 ?)
     *    Param3: 0x00003000 <- file type PTP_OFC_Undefined - we don't know about PDF files
     *    Param4: 0x00000000 <- file length MSB (-0x0c header len)
     *    Param5: 0x00005e12 <- file length LSB (-0x0c header len)
     *
     * -> PTP_OC_MTP_SendObjectPropList (0x9808):
     *    46 00 00 00 02 00 08 98 1B 00 00 00 03 00 00 00
     *    00 00 00 00 07 DC FF FF 0D 4B 00 53 00 30 00 36 - dc07 = file name
     *    00 30 00 33 00 30 00 36 00 2E 00 70 00 64 00 66
     *    00 00 00 00 00 00 00 03 DC 04 00 00 00 00 00 00 - dc03 = protection status
     *    00 4F DC 02 00 01                               - dc4f = non consumable
     *    Length: 0x00000046
     *    Type:   0x0002 PTP_USB_CONTAINER_DATA
     *    Code:   0x9808
     *    Transaction ID: 0x0000001B
     *    Metadata....
     *    0x00000003 <- Number of metadata items
     *    0x00000000 <- Object handle, set to 0x00000000 since it is unknown!
     *    0xdc07     <- metadata type: file name
     *    0xffff     <- metadata type: string
     *    0x0d       <- number of (uint16_t) characters
     *    4b 53 30 36 30 33 30 36 2e 50 64 66 00 "KS060306.pdf", null terminated
     *    0x00000000 <- Object handle, set to 0x00000000 since it is unknown!
     *    0xdc03     <- metadata type: protection status
     *    0x0004     <- metadata type: uint16_t
     *    0x0000     <- not protected
     *    0x00000000 <- Object handle, set to 0x00000000 since it is unknown!
     *    0xdc4f     <- non consumable
     *    0x0002     <- metadata type: uint8_t
     *    0x01       <- non-consumable (this device cannot display PDF)
     *
     * <- Read 0x18 bytes back
     *    18 00 00 00 03 00 01 20 1B 00 00 00 01 00 01 00
     *    00 00 00 00 01 40 00 00
     *    Length: 0x000000018
     *    Type:   0x0003 PTP_USB_CONTAINER_RESPONSE
     *    Code:   0x2001 PTP_OK
     *    Transaction ID: 0x0000001B
     *    Param1: 0x00010001 <- store
     *    Param2: 0x00000000 <- parent handle
     *    Param3: 0x00004001 <- new file/object ID
     *
     * -> PTP_OC_SendObject (0x100d)
     *    0C 00 00 00 01 00 0D 10 1C 00 00 00
     * -> ... all the bytes ...
     * <- Read 0x0c bytes back
     *    0C 00 00 00 03 00 01 20 1C 00 00 00
     *    ... Then update metadata one-by one, actually (instead of sending it first!) ...
     */
    MTPPropList *proplist = NULL;
    MTPPropList *prop = NULL;
    uint16_t *props = NULL;
    uint32_t propcnt = 0;
    
    // default handle
    if (localph == 0)
      localph = 0xFFFFFFFFU; // Set to -1

    // Must be 0x00000000U for new objects
    filedata->item_id = 0x00000000U;

    ret = ptp_mtp_getobjectpropssupported(params, of, &propcnt, &props);

    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;
      
      ret = ptp_mtp_getobjectpropdesc(params, props[i], of, &opd);
      if (ret != PTP_RC_OK) {
	add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_File_From_File_Descriptor(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (props[i]) {
	case PTP_OPC_ObjectFileName:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_ObjectFileName;
	  prop->datatype = PTP_DTC_STR;
	  if (filedata->filename != NULL)
	    prop->propval.str = strdup(filedata->filename);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_ProtectionStatus:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_ProtectionStatus;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = 0x0000U; /* Not protected */
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_NonConsumable:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_NonConsumable;
	  prop->datatype = PTP_DTC_UINT8;
	  prop->propval.u8 = nonconsumable;
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Name:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = filedata->item_id;
	  prop->property = PTP_OPC_Name;
	  prop->datatype = PTP_DTC_STR;
	  if (filedata->filename != NULL)
	    prop->propval.str = strdup(filedata->filename);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
    free(props);

    ret = ptp_mtp_sendobjectproplist(params, &store, &localph, &filedata->item_id,
				     of, filedata->filesize, proplist);

    /* Free property list */
    destroy_mtp_prop_list(proplist);

    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_File_From_File_Descriptor():" 
				  "Could not send object property list.");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }
  } else if (ptp_operation_issupported(params,PTP_OC_SendObjectInfo)) {
    PTPObjectInfo new_file;

    memset(&new_file, 0, sizeof(PTPObjectInfo));
  
    new_file.Filename = filedata->filename;
    if (filedata->filesize == (uint64_t) -1) {
      // This is a stream. Set a dummy length...
      new_file.ObjectCompressedSize = 1;
    } else {
      // We loose precision here.
      new_file.ObjectCompressedSize = (uint32_t) filedata->filesize;
    }
    new_file.ObjectFormat = of;
    new_file.StorageID = store;
    new_file.ParentObject = localph;

    // Create the object
    ret = ptp_sendobjectinfo(params, &store, &localph, &filedata->item_id, &new_file);

    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_File_From_File_Descriptor(): "
				  "Could not send object info.");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }
    // NOTE: the char* pointers inside new_file are not copies so don't
    // try to destroy this objectinfo!
  }

  // Now there IS an object with this parent handle.
  filedata->parent_id = localph;
    
  if (filedata->filesize != (uint64_t) -1) {
    // Callbacks
    ptp_usb->callback_active = 1;
    // The callback will deactivate itself after this amount of data has been sent
    // One BULK header for the request, one for the data phase. No parameters to the request.
    ptp_usb->current_transfer_total = filedata->filesize+PTP_USB_BULK_HDR_LEN*2;
    ptp_usb->current_transfer_complete = 0;
    ptp_usb->current_transfer_callback = callback;
    ptp_usb->current_transfer_callback_data = data;

    ret = ptp_sendobject_fromfd(params, fd, filedata->filesize);

    ptp_usb->callback_active = 0;
    ptp_usb->current_transfer_callback = NULL;
    ptp_usb->current_transfer_callback_data = NULL;
  } else {
    // This is a stream..
    ret = ptp_sendobject_fromfd(params, fd, 0xFFFFFFFFU-PTP_USB_BULK_HDR_LEN);
    if (ret == PTP_ERROR_IO) {
      // That's expected. The stream ends, simply...
      ret = PTP_RC_OK;
    } else {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_File_From_File_Descriptor(): "
				  "Error while sending stream.");
    }
  }

  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_File_From_File_Descriptor(): "
				"Could not send object.");
    return -1;
  }

  if (!ptp_operation_issupported(params,PTP_OC_MTP_SendObjectPropList) && nonconsumable != 0x00U) {
    // Flag it as non-consumable if it is
    subcall_ret = set_object_u8(device, filedata->item_id, PTP_OPC_NonConsumable, nonconsumable);
    if (subcall_ret != 0) {
      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_File_From_File_Descriptor(): "
			      "could not set non-consumable status.");
      return -1;
    }
  }

  add_object_to_cache(device, filedata->item_id);

  return 0;
}

/**
 * This function updates the MTP object metadata on a single file
 * identified by an object ID.
 * @param device a pointer to the device to update the track
 *        metadata on.
 * @param metadata a track metadata set to be written to the file.
 *        notice that the <code>track_id</code> field of the
 *        metadata structure must be correct so that the
 *        function can update the right file. If some properties
 *        of this metadata are set to NULL (strings) or 0
 *        (numerical values) they will be discarded and the
 *        track will not be tagged with these blank values.
 * @return 0 on success, any other value means failure. If some
 *        or all of the properties fail to update we will still
 *        return success. On some devices (notably iRiver T30)
 *        properties that exist cannot be updated.
 */
int LIBMTP_Update_Track_Metadata(LIBMTP_mtpdevice_t *device,
				 LIBMTP_track_t const * const metadata)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  uint32_t i;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;

  // First see which properties can be set on this file format and apply accordingly
  // i.e only try to update this metadata for object tags that exist on the current player.
  ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(metadata->filetype), &propcnt, &props);
  if (ret != PTP_RC_OK) {
    // Just bail out for now, nothing is ever set.
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
			    "could not retrieve supported object properties.");
    return -1;
  }
  if (ptp_operation_issupported(params, PTP_OC_MTP_SetObjPropList)) {
    MTPPropList *proplist = NULL;
    MTPPropList *prop = NULL;
    
    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;
      
      ret = ptp_mtp_getobjectpropdesc(params, props[i], map_libmtp_type_to_ptp_type(metadata->filetype), &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (props[i]) {
	case PTP_OPC_Name:
	  if (metadata->title == NULL)
	    break;
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_Name;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->title);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_AlbumName:
	  if (metadata->album == NULL)
	    break;
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_AlbumName;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->album);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Artist:
	  if (metadata->artist == NULL)
	    break;
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_Artist;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->artist);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Genre:
	  if (metadata->genre == NULL)
	    break;
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_Genre;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->genre);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Duration:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Duration;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->duration, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Track:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;
	  prop->property = PTP_OPC_Track;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->tracknumber, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_OriginalReleaseDate:
	  if (metadata->date == NULL)
	    break;
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_OriginalReleaseDate;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(metadata->date);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_SampleRate:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_SampleRate;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->samplerate, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_NumberOfChannels:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_NumberOfChannels;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->nochannels, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_AudioWAVECodec:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_AudioWAVECodec;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->wavecodec, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_AudioBitRate:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_AudioBitRate;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->bitrate, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_BitRateType:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_BitRateType;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->bitratetype, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Rating:
	  // TODO: shall this be set for rating 0?
	  if (metadata->rating == 0)
	    break;
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_Rating;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = adjust_u16(metadata->rating, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_UseCount:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->item_id;      
	  prop->property = PTP_OPC_UseCount;
	  prop->datatype = PTP_DTC_UINT32;
	  prop->propval.u32 = adjust_u32(metadata->usecount, &opd);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	}
      }
      ptp_free_objectpropdesc(&opd);
    }

    // NOTE: File size is not updated, this should not change anyway.
    // neither will we change the filename.
    
    ret = ptp_mtp_setobjectproplist(params, proplist);
    
    destroy_mtp_prop_list(proplist);
    
    if (ret != PTP_RC_OK) {
      // TODO: return error of which property we couldn't set
      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
			      "could not set object property list.");
      return -1;
    }
    
  } else if (ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;
      
      ret = ptp_mtp_getobjectpropdesc(params, props[i], map_libmtp_type_to_ptp_type(metadata->filetype), &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (props[i]) {
	case PTP_OPC_Name:
	  // Update title
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_Name, metadata->title);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track title.");
	  }
	  break;
	case PTP_OPC_AlbumName:
	  // Update album
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_AlbumName, metadata->album);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track album name.");
	  }
	  break;
	case PTP_OPC_Artist:
	  // Update artist
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_Artist, metadata->artist);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track artist name.");
	  }
	  break;
	case PTP_OPC_Genre:
	  // Update genre
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_Genre, metadata->genre);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track genre name.");
	  }
	  break;
	case PTP_OPC_Duration:
	  // Update duration
	  if (metadata->duration != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_Duration, adjust_u32(metadata->duration, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set track duration.");
	    }
	  }
	  break;
	case PTP_OPC_Track:
	  // Update track number.
	  if (metadata->tracknumber != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_Track, adjust_u16(metadata->tracknumber, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set track tracknumber.");
	    }
	  }
	  break;
	case PTP_OPC_OriginalReleaseDate:
	  // Update creation datetime
	  ret = set_object_string(device, metadata->item_id, PTP_OPC_OriginalReleaseDate, metadata->date);
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set track release date.");
	  }
	  break;
	  // These are, well not so important.
	case PTP_OPC_SampleRate:
	  // Update sample rate
	  if (metadata->samplerate != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_SampleRate, adjust_u32(metadata->samplerate, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set samplerate.");
	    }
	  }
	  break;
	case PTP_OPC_NumberOfChannels:
	  // Update number of channels
	  if (metadata->nochannels != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_NumberOfChannels, adjust_u16(metadata->nochannels, &opd));
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				    "could not set number of channels.");
	  }
	}
	  break;
	case PTP_OPC_AudioWAVECodec:
	  // Update WAVE codec
	  if (metadata->wavecodec != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_AudioWAVECodec, adjust_u32(metadata->wavecodec, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set WAVE codec.");
	    }
	  }
	  break;
	case PTP_OPC_AudioBitRate:
	  // Update bitrate
	  if (metadata->bitrate != 0) {
	    ret = set_object_u32(device, metadata->item_id, PTP_OPC_AudioBitRate, adjust_u32(metadata->bitrate, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set bitrate.");
	  }
	  }
	  break;
	case PTP_OPC_BitRateType:
	  // Update bitrate type
	  if (metadata->bitratetype != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_BitRateType, adjust_u16(metadata->bitratetype, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set bitratetype.");
	    }
	  }
	  break;
	case PTP_OPC_Rating:
	  // Update user rating
	  // TODO: shall this be set for rating 0?
	  if (metadata->rating != 0) {
	    ret = set_object_u16(device, metadata->item_id, PTP_OPC_Rating, adjust_u16(metadata->rating, &opd));
	    if (ret != 0) {
	      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				      "could not set user rating.");
	    }
	  }
	  break;
	case PTP_OPC_UseCount:
	  // Update use count, set even to zero if desired.
	  ret = set_object_u32(device, metadata->item_id, PTP_OPC_UseCount, adjust_u32(metadata->usecount, &opd));
	  if (ret != 0) {
	    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
				  "could not set use count.");
	  }
	  break;
	  
	  // NOTE: File size is not updated, this should not change anyway.
	  // neither will we change the filename.
	default:
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
    free(props);
  } else {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Track_Metadata(): "
                            "Your device doesn't seem to support any known way of setting metadata.");
    return -1;
  }
  
  // update cached object properties if metadata cache exists
  if (params->proplist != NULL) {
    update_metadata_cache(device, metadata->item_id);
  }

  return 0;
}

/**
 * This function deletes a single file, track, playlist or
 * any other object off the MTP device,
 * identified by an object ID.
 * @param device a pointer to the device to delete the file or track from.
 * @param item_id the item to delete.
 * @return 0 on success, any other value means failure.
 */
int LIBMTP_Delete_Object(LIBMTP_mtpdevice_t *device,
			 uint32_t object_id)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  int i;

  ret = ptp_deleteobject(params, object_id, 0);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Delete_Object(): could not delete object.");
    return -1;
  }

  // remove object from object info cache
  for (i = 0; i < params->handles.n; i++) {
    if (params->handles.Handler[i] == object_id) {
      ptp_free_objectinfo(&params->objectinfo[i]);
      memmove(params->handles.Handler+i, params->handles.Handler+i+1,
              (params->handles.n-i-1)*sizeof(uint32_t));
      memmove(params->objectinfo+i, params->objectinfo+i+1,
              (params->handles.n-i-1)*sizeof(PTPObjectInfo));
      params->handles.n--;
      params->handles.Handler = realloc(params->handles.Handler, sizeof(uint32_t)*params->handles.n);
      params->objectinfo = realloc(params->objectinfo, sizeof(PTPObjectInfo)*params->handles.n);
    }
  }

  // delete cached object properties if metadata cache exists
  if (params->proplist) {
    MTPPropList *prop = params->proplist;
    MTPPropList *prev = NULL;
    MTPPropList *last_deleted = NULL;
    MTPPropList *deleted_props = NULL;

    // find the place where properties for this object begin
    while (prop != NULL && prop->ObjectHandle != object_id) {
      prev = prop;
      prop = prop->next;
    }

    // safeguard in case object is not found
    if (!prop) return 0;

    // head of properties for the deleted object
    deleted_props = prop;

    // find the end of properties for this object
    while (prop != NULL && prop->ObjectHandle == object_id) {
      last_deleted = prop;
      prop = prop->next;
    }

    // the properties to be deleted are split into separate list
    prev->next = prop;
    last_deleted->next = NULL;

    destroy_mtp_prop_list(deleted_props);
  }
    
  return 0;
}

/**
 * Helper function. This indicates if a track exists on the device
 * @param device a pointer to the device to get the track from.
 * @param id the track ID of the track to retrieve.
 * @return TRUE (!=0) if the track exists, FALSE (0) if not
 */
int LIBMTP_Track_Exists(LIBMTP_mtpdevice_t *device,
           uint32_t const id)
{
  PTPParams *params = (PTPParams *) device->params;
  uint32_t i;

  for (i = 0; i < params->handles.n; i++) {
    if (params->handles.Handler[i] == id) {
      return -1;
    }
  }
  return 0;
}

/**
 * This creates a new folder structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_folder_track_t</code>
 * operation later, so be careful of using strdup() when assigning
 * strings, e.g.:
 *
 * @return a pointer to the newly allocated folder structure.
 * @see LIBMTP_destroy_folder_t()
 */
LIBMTP_folder_t *LIBMTP_new_folder_t(void)
{
  LIBMTP_folder_t *new = (LIBMTP_folder_t *) malloc(sizeof(LIBMTP_folder_t));
  if (new == NULL) {
    return NULL;
  }
  new->folder_id = 0;
  new->parent_id = 0;
  new->name = NULL;
  new->sibling = NULL;
  new->child = NULL;
  return new;
}

/**
 * This recursively deletes the memory for a folder structure.
 * This shall typically be called on a top-level folder list to
 * detsroy the entire folder tree.
 *
 * @param folder folder structure to destroy
 * @see LIBMTP_new_folder_t()
 */
void LIBMTP_destroy_folder_t(LIBMTP_folder_t *folder)
{

  if(folder == NULL) {
     return;
  }

  //Destroy from the bottom up
  if(folder->child != NULL) {
     LIBMTP_destroy_folder_t(folder->child);
  }

  if(folder->sibling != NULL) {
    LIBMTP_destroy_folder_t(folder->sibling);
  }

  if(folder->name != NULL) {
    free(folder->name);
  }

  free(folder);
}

/**
 * Helper function. Returns a folder structure for a
 * specified id.
 *
 * @param folderlist list of folders to search
 * @id id of folder to look for
 * @return a folder or NULL if not found
 */
LIBMTP_folder_t *LIBMTP_Find_Folder(LIBMTP_folder_t *folderlist, uint32_t id)
{
  LIBMTP_folder_t *ret = NULL;

  if(folderlist == NULL) {
    return NULL;
  }

  if(folderlist->folder_id == id) {
    return folderlist;
  }

  if(folderlist->sibling) {
    ret = LIBMTP_Find_Folder(folderlist->sibling, id);
  }

  if(folderlist->child && ret == NULL) {
    ret = LIBMTP_Find_Folder(folderlist->child, id);
  }

  return ret;
}

/**
 * This returns a list of all folders available
 * on the current MTP device.
 *
 * @param device a pointer to the device to get the track listing for.
 * @return a list of folders
 */
LIBMTP_folder_t *LIBMTP_Get_Folder_List(LIBMTP_mtpdevice_t *device)
{
  uint32_t i = 0;
  LIBMTP_folder_t *retfolders = NULL;
  PTPParams *params = (PTPParams *) device->params;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_folder_t *folder;
    PTPObjectInfo *oi;

    oi = &params->objectinfo[i];

    if (oi->ObjectFormat != PTP_OFC_Association) {
      continue;
    }
    folder = LIBMTP_new_folder_t();
    folder->folder_id = params->handles.Handler[i];
    folder->parent_id = oi->ParentObject;
    if (oi->Filename != NULL)
      folder->name = (char *)strdup(oi->Filename);
    else
      folder->name = NULL;

    // Work out where to put this new item
    if(retfolders == NULL) {
      retfolders = folder;
      continue;
    } else {
      LIBMTP_folder_t *parent_folder;
      LIBMTP_folder_t *current_folder;
      
      parent_folder = LIBMTP_Find_Folder(retfolders, folder->parent_id);
      
      if(parent_folder == NULL) {
	current_folder = retfolders;
      } else {
	if(parent_folder->child == NULL) {
	  parent_folder->child = folder;
	  continue;
	} else {
	  current_folder = parent_folder->child;
	}
      }
      
      while(current_folder->sibling != NULL) {
	current_folder=current_folder->sibling;
      }
      
      current_folder->sibling = folder;
    }
  }
  return retfolders;
}

/**
 * This create a folder on the current MTP device. The PTP name
 * for a folder is "association". The PTP/MTP devices does not
 * have an internal "folder" concept really, it contains a flat
 * list of all files and some file are "associations" that other
 * files and folders may refer to as its "parent".
 *
 * @param device a pointer to the device to create the folder on.
 * @param name the name of the new folder.
 * @param parent_id id of parent folder to add the new folder to,
 *        or 0 to put it in the root directory.
 * @return id to new folder or 0 if an error occured
 */
uint32_t LIBMTP_Create_Folder(LIBMTP_mtpdevice_t *device, char *name, uint32_t parent_id)
{
  PTPParams *params = (PTPParams *) device->params;
  uint32_t parenthandle = 0;
  uint32_t store = get_first_storageid(device);
  PTPObjectInfo new_folder;
  uint16_t ret;
  uint32_t new_id = 0;

  memset(&new_folder, 0, sizeof(new_folder));
  new_folder.Filename = name;
  new_folder.ObjectCompressedSize = 1;
  new_folder.ObjectFormat = PTP_OFC_Association;
  new_folder.ParentObject = parent_id;

  parenthandle = parent_id;
  // Create the object
  // FIXME: use send list here if available.
  ret = ptp_sendobjectinfo(params, &store, &parenthandle, &new_id, &new_folder);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Create_Folder: Could not send object info.");
    if (ret == PTP_RC_AccessDenied) {
      add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
    }
    return 0;
  }
  // NOTE: don't destroy the new_folder objectinfo, because it is statically referencing
  // several strings.

  add_object_to_cache(device, new_id);

  return new_id;
}

/**
 * This creates a new playlist metadata structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_destroy_playlist_t</code>
 * operation later, so be careful of using strdup() when assigning
 * strings, e.g.:
 *
 * <pre>
 * LIBMTP_playlist_t *pl = LIBMTP_new_playlist_t();
 * pl->name = strdup(str);
 * ....
 * LIBMTP_destroy_playlist_t(pl);
 * </pre>
 *
 * @return a pointer to the newly allocated metadata structure.
 * @see LIBMTP_destroy_playlist_t()
 */
LIBMTP_playlist_t *LIBMTP_new_playlist_t(void)
{
  LIBMTP_playlist_t *new = (LIBMTP_playlist_t *) malloc(sizeof(LIBMTP_playlist_t));
  if (new == NULL) {
    return NULL;
  }
  new->playlist_id = 0;
  new->name = NULL;
  new->tracks = NULL;
  new->no_tracks = 0;
  new->next = NULL;
  return new;
}

/**
 * This destroys a playlist metadata structure and deallocates the memory
 * used by it, including any strings. Never use a track metadata
 * structure again after calling this function on it.
 * @param playlist the playlist metadata to destroy.
 * @see LIBMTP_new_playlist_t()
 */
void LIBMTP_destroy_playlist_t(LIBMTP_playlist_t *playlist)
{
  if (playlist == NULL) {
    return;
  }
  if (playlist->name != NULL)
    free(playlist->name);
  if (playlist->tracks != NULL)
    free(playlist->tracks);
  free(playlist);
  return;
}

/**
 * This function returns a list of the playlists available on the
 * device. Typical usage:
 *
 * <pre>
 * </pre>
 *
 * @param device a pointer to the device to get the playlist listing from.
 * @return a playlist list on success, else NULL. If there are no playlists
 *         on the device, NULL will be returned as well.
 * @see LIBMTP_Get_Playlist()
 */
LIBMTP_playlist_t *LIBMTP_Get_Playlist_List(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  LIBMTP_playlist_t *retlists = NULL;
  LIBMTP_playlist_t *curlist = NULL;
  uint32_t i;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_playlist_t *pl;
    PTPObjectInfo *oi;
    uint16_t ret;

    oi = &params->objectinfo[i];

    // Ignore stuff that isn't playlists
    if ( oi->ObjectFormat != PTP_OFC_MTP_AbstractAudioVideoPlaylist ) {
      continue;
    }

    // Allocate a new playlist type
    pl = LIBMTP_new_playlist_t();

    // Ignoring the oi->Filename field.
    pl->name = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Name);

    // This is some sort of unique playlist ID so we can keep track of it
    pl->playlist_id = params->handles.Handler[i];

    // Then get the track listing for this playlist
    ret = ptp_mtp_getobjectreferences(params, pl->playlist_id, &pl->tracks, &pl->no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Playlist: Could not get object references.");
      pl->tracks = NULL;
      pl->no_tracks = 0;
    }
    
    // Add playlist to a list that will be returned afterwards.
    if (retlists == NULL) {
      retlists = pl;
      curlist = pl;
    } else {
      curlist->next = pl;
      curlist = pl;
    }
    
    // Call callback here if we decide to add that possibility...
  }
  return retlists;
}


/**
 * This function retrieves an individual playlist from the device.
 * @param device a pointer to the device to get the playlist from.
 * @param plid the unique ID of the playlist to retrieve.
 * @return a valid playlist metadata post or NULL on failure.
 * @see LIBMTP_Get_Playlist_List()
 */
LIBMTP_playlist_t *LIBMTP_Get_Playlist(LIBMTP_mtpdevice_t *device, uint32_t const plid)
{
  PTPParams *params = (PTPParams *) device->params;
  uint32_t i;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_playlist_t *pl;
    PTPObjectInfo *oi;
    uint16_t ret;
    
    if (params->handles.Handler[i] != plid) {
      continue;
    }
    
    oi = &params->objectinfo[i];

    // Ignore stuff that isn't playlists
    if ( oi->ObjectFormat != PTP_OFC_MTP_AbstractAudioVideoPlaylist ) {
      return NULL;
    }

    // Allocate a new playlist type
    pl = LIBMTP_new_playlist_t();

    // Ignoring the io.Filename field.
    pl->name = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Name);

    // This is some sort of unique playlist ID so we can keep track of it
    pl->playlist_id = params->handles.Handler[i];
    
    // Then get the track listing for this playlist
    ret = ptp_mtp_getobjectreferences(params, pl->playlist_id, &pl->tracks, &pl->no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Playlist(): Could not get object references.");
      pl->tracks = NULL;
      pl->no_tracks = 0;
    }

    return pl;
  }
  return NULL;
}

/*
 * This function creates a new abstract list such as a playlist
 * or an album.
 * 
 * @param device a pointer to the device to create the new abstract list
 *        on.
 * @param name the name of the new abstract list.
 * @param parenthandle the handle of the parent or 0 for no parent
 *        i.e. the root folder.
 * @param objectformat the abstract list type to create.
 * @param suffix the ".foo" (4 characters) suffix to use for the virtual
 *        "file" created by this operation.
 * @param newid a pointer to a variable that will hold the new object
 *        ID if this call is successful.
 * @param tracks an array of tracks to associate with this list.
 * @param no_tracks the number of tracks in the list.
 * @return 0 on success, any other value means failure.
 */
static int create_new_abstract_list(LIBMTP_mtpdevice_t *device,
				    char const * const name,
				    char const * const artist,
				    char const * const genre,
				    uint32_t const parenthandle,
				    uint16_t const objectformat,
				    char const * const suffix,
				    uint32_t * const newid,
				    uint32_t const * const tracks,
				    uint32_t const no_tracks)

{
  int i;
  int supported = 0;
  uint16_t ret;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  uint32_t store = get_first_storageid(device);
  uint32_t localph = parenthandle;
  uint8_t nonconsumable = 0x00U; /* By default it is consumable */
  PTPParams *params = (PTPParams *) device->params;
  char fname[256];
  uint8_t data[2];

  // Check if we can create an object of this type
  for ( i=0; i < params->deviceinfo.ImageFormats_len; i++ ) {
    if (params->deviceinfo.ImageFormats[i] == objectformat) {
      supported = 1;
      break;
    }
  }
  if (!supported) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): player does not support this abstract type.");
    printf("Unsupported type: %04x\n", objectformat);
    return -1;
  }

  // add the new suffix if it isn't there
  fname[0] = '\0';
  if (strlen(name) > strlen(suffix)) {
    char const * const suff = &name[strlen(name)-strlen(suffix)];
    if (!strcmp(suff, suffix)) {
      // Home free.
      strncpy(fname, name, sizeof(fname));
    }
  }
  // If it didn't end with "<suffix>" then add that here.
  if (fname[0] == '\0') {
    strncpy(fname, name, sizeof(fname)-strlen(suffix)-1);
    strcat(fname, suffix);
    fname[sizeof(fname)-1] = '\0';
  }

  if (ptp_operation_issupported(params,PTP_OC_MTP_SendObjectPropList)) {

    MTPPropList *proplist = NULL;
    MTPPropList *prop = NULL;
    
    *newid = 0x00000000U;

    ret = ptp_mtp_getobjectpropssupported(params, objectformat, &propcnt, &props);

    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;
      
      ret = ptp_mtp_getobjectpropdesc(params, props[i], objectformat, &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (props[i]) {
	case PTP_OPC_ObjectFileName:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = *newid;      
	  prop->property = PTP_OPC_ObjectFileName;
	  prop->datatype = PTP_DTC_STR;
	  prop->propval.str = strdup(fname);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_ProtectionStatus:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = *newid;
	  prop->property = PTP_OPC_ProtectionStatus;
	  prop->datatype = PTP_DTC_UINT16;
	  prop->propval.u16 = 0x0000U; /* Not protected */
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_NonConsumable:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = *newid;
	  prop->property = PTP_OPC_NonConsumable;
	  prop->datatype = PTP_DTC_UINT8;
	  prop->propval.u8 = nonconsumable;
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Name:
	  if (name != NULL) {
	    prop = new_mtp_prop_entry();
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_Name;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(name);
	    proplist = add_mtp_prop_to_proplist(proplist, prop);
	  }
	  break;
	case PTP_OPC_Artist:
	  if (artist != NULL) {
	    prop = new_mtp_prop_entry();
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_Artist;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(artist);
	    proplist = add_mtp_prop_to_proplist(proplist, prop);
	  }
	  break;
	case PTP_OPC_Genre:
	  if (genre != NULL) {
	    prop = new_mtp_prop_entry();
	    prop->ObjectHandle = *newid;
	    prop->property = PTP_OPC_Genre;
	    prop->datatype = PTP_DTC_STR;
	    prop->propval.str = strdup(genre);
	    proplist = add_mtp_prop_to_proplist(proplist, prop);
	  }
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
    free(props);

    ret = ptp_mtp_sendobjectproplist(params, &store, &localph, newid,
				     objectformat, 0, proplist);

    /* Free property list */
    destroy_mtp_prop_list(proplist);

    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send object property list.");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }
    
    // now send the blank object
    ret = ptp_sendobject(params, NULL, 0);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send blank object data.");
      return -1;
    }

  } else if (ptp_operation_issupported(params,PTP_OC_SendObjectInfo)) {
    PTPObjectInfo new_object;

    new_object.Filename = fname;
    new_object.ObjectCompressedSize = 1;
    new_object.ObjectFormat = objectformat;

    // Create the object
    ret = ptp_sendobjectinfo(params, &store, &localph, newid, &new_object);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send object info (the playlist itself).");
      if (ret == PTP_RC_AccessDenied) {
	add_ptp_error_to_errorstack(device, ret, "ACCESS DENIED.");
      }
      return -1;
    }
    // NOTE: don't destroy new_object objectinfo afterwards - the strings it contains are
    // not copies.
	
    /*
     * We have to send this one blank data byte.
     * If we don't, the handle will not be created and thus there is no playlist.
     */
    data[0] = '\0';
    data[1] = '\0';
    ret = ptp_sendobject(params, data, 1);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): Could not send blank object data.");
      return -1;
    }
	
    // Update title
    if (name != NULL) {
      ret = set_object_string(device, *newid, PTP_OPC_Name, name);
      if (ret != 0) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity name.");
	return -1;
      }
    }
    
    // Update artist
    if (artist != NULL) {
      ret = set_object_string(device, *newid, PTP_OPC_Artist, artist);
      if (ret != 0) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity artist.");
	return -1;
      }
    }

    // Update genre
    if (genre != NULL) {
      ret = set_object_string(device, *newid, PTP_OPC_Genre, genre);
      if (ret != 0) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "create_new_abstract_list(): could not set entity genre.");
	return -1;
      }
    }

  }

  if (no_tracks > 0) {
    // Add tracks to the new playlist as object references.
    ret = ptp_mtp_setobjectreferences (params, *newid, (uint32_t *) tracks, no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "create_new_abstract_list(): could not add tracks as object references.");
      return -1;
    }
  }

  add_object_to_cache(device, *newid);

  return 0;
}


/**
 * This routine creates a new playlist based on the metadata
 * supplied. If the <code>tracks</code> field of the metadata
 * contains a track listing, these tracks will be added to the
 * playlist.
 * @param device a pointer to the device to create the new playlist on.
 * @param metadata the metadata for the new playlist. If the function
 *        exits with success, the <code>playlist_id</code> field of this
 *        struct will contain the new playlist ID of the playlist.
 * @param parenthandle the parent (e.g. folder) to store this playlist
 *        in. Pass in 0 to put the playlist in the root directory.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Update_Playlist()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Create_New_Playlist(LIBMTP_mtpdevice_t *device,
			       LIBMTP_playlist_t * const metadata,
			       uint32_t const parenthandle)
{
  uint32_t localph = parenthandle;

  // Use a default folder if none given
  if (localph == 0) {
    localph = device->default_playlist_folder;
  }

  // Just create a new abstract audio/video playlist...
  return create_new_abstract_list(device,
				  metadata->name,
				  NULL,
				  NULL,
				  localph,
				  PTP_OFC_MTP_AbstractAudioVideoPlaylist,
				  ".zpl",
				  &metadata->playlist_id,
				  metadata->tracks,
				  metadata->no_tracks);
}

/**
 * This routine updates a playlist based on the metadata
 * supplied. If the <code>tracks</code> field of the metadata
 * contains a track listing, these tracks will be added to the
 * playlist in place of those already present, i.e. the
 * previous track listing will be deleted.
 * @param device a pointer to the device to create the new playlist on.
 * @param metadata the metadata for the playlist to be updated.
 *                 notice that the field <code>playlist_id</code>
 *                 must contain the apropriate playlist ID.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Create_New_Playlist()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Update_Playlist(LIBMTP_mtpdevice_t *device,
			   LIBMTP_playlist_t const * const metadata)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;

  // Update title
  ret = set_object_string(device, metadata->playlist_id, PTP_OPC_Name, metadata->name);
  if (ret != 0) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Playlist(): could not set playlist name.");
    return -1;
  }

  if (metadata->no_tracks > 0) {
    // Add tracks to the new playlist as object references.
    ret = ptp_mtp_setobjectreferences (params, metadata->playlist_id, metadata->tracks, metadata->no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Update_Playlist(): could not add tracks as object references.");
      return -1;
    }
  }
  return 0;
}

/**
 * This creates a new album metadata structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_destroy_album_t</code>
 * operation later, so be careful of using strdup() when assigning
 * strings.
 *
 * @return a pointer to the newly allocated metadata structure.
 * @see LIBMTP_destroy_album_t()
 */
LIBMTP_album_t *LIBMTP_new_album_t(void)
{
  LIBMTP_album_t *new = (LIBMTP_album_t *) malloc(sizeof(LIBMTP_album_t));
  if (new == NULL) {
    return NULL;
  }
  new->album_id = 0;
  new->name = NULL;
  new->artist = NULL;
  new->genre = NULL;
  new->tracks = NULL;
  new->no_tracks = 0;
  new->next = NULL;
  return new;
}

/**
 * This recursively deletes the memory for an album structure
 *
 * @param album structure to destroy
 * @see LIBMTP_new_album_t()
 */
void LIBMTP_destroy_album_t(LIBMTP_album_t *album)
{
  if (album == NULL) {
    return;
  }
  if (album->name != NULL)
    free(album->name);
  if (album->artist != NULL)
    free(album->artist);
  if (album->genre != NULL)
    free(album->genre);
  if (album->tracks != NULL)
    free(album->tracks);
  free(album);
  return;
}

/**
 * This function returns a list of the albums available on the
 * device.
 *
 * @param device a pointer to the device to get the album listing from.
 * @return an album list on success, else NULL. If there are no albums
 *         on the device, NULL will be returned as well.
 * @see LIBMTP_Get_Album()
 */
LIBMTP_album_t *LIBMTP_Get_Album_List(LIBMTP_mtpdevice_t *device)
{
  PTPParams *params = (PTPParams *) device->params;
  LIBMTP_album_t *retalbums = NULL;
  LIBMTP_album_t *curalbum = NULL;
  uint32_t i;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_album_t *alb;
    PTPObjectInfo *oi;
    uint16_t ret;

    oi = &params->objectinfo[i];

    // Ignore stuff that isn't an album
    if ( oi->ObjectFormat != PTP_OFC_MTP_AbstractAudioAlbum ) {
      continue;
    }

    // Allocate a new album type
    alb = LIBMTP_new_album_t();
    // Get metadata for it.
    alb->name = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Name);
    alb->artist = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Artist);
    alb->genre = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Genre);
    alb->album_id = params->handles.Handler[i];
    
    // Then get the track listing for this album
    ret = ptp_mtp_getobjectreferences(params, alb->album_id, &alb->tracks, &alb->no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Album_List(): Could not get object references.");
      alb->tracks = NULL;
      alb->no_tracks = 0;
    }
    
    // Add album to a list that will be returned afterwards.
    if (retalbums == NULL) {
      retalbums = alb;
      curalbum = alb;
    } else {
      curalbum->next = alb;
      curalbum = alb;
    }
    
  }
  return retalbums;
}

/**
 * This function retrieves an individual album from the device.
 * @param device a pointer to the device to get the album from.
 * @param albid the unique ID of the album to retrieve.
 * @return a valid album metadata or NULL on failure.
 * @see LIBMTP_Get_Album_List()
 */
LIBMTP_album_t *LIBMTP_Get_Album(LIBMTP_mtpdevice_t *device, uint32_t const albid)
{
  PTPParams *params = (PTPParams *) device->params;
  uint32_t i;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_album_t *alb;
    PTPObjectInfo *oi;
    uint16_t ret;

    if (params->handles.Handler[i] != albid) {
      continue;
    }

    oi = &params->objectinfo[i];

    // Ignore stuff that isn't an album
    if ( oi->ObjectFormat != PTP_OFC_MTP_AbstractAudioAlbum ) {
      return NULL;
    }

    // Allocate a new album type
    alb = LIBMTP_new_album_t();
    alb->album_id = params->handles.Handler[i];
    alb->name = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Name);
    alb->artist = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Artist);
    alb->genre = get_string_from_object(device, params->handles.Handler[i], PTP_OPC_Genre);
    ret = ptp_mtp_getobjectreferences(params, alb->album_id, &alb->tracks, &alb->no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Album: Could not get object references.");
      alb->tracks = NULL;
      alb->no_tracks = 0;
    }
    
    return alb;
  }
  return NULL;
}

/**
 * This routine creates a new album based on the metadata
 * supplied. If the <code>tracks</code> field of the metadata
 * contains a track listing, these tracks will be added to the
 * album.
 * @param device a pointer to the device to create the new album on.
 * @param metadata the metadata for the new album. If the function
 *        exits with success, the <code>album_id</code> field of this
 *        struct will contain the new ID of the album.
 * @param parenthandle the parent (e.g. folder) to store this album
 *        in. Pass in 0 to put the album in the default music directory.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Update_Album()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Create_New_Album(LIBMTP_mtpdevice_t *device,
			       LIBMTP_album_t * const metadata,
			       uint32_t const parenthandle)
{
  uint32_t localph = parenthandle;

  // Use a default folder if none given
  if (localph == 0) {
    localph = device->default_album_folder;
  }

  // Just create a new abstract album...
  return create_new_abstract_list(device,
				  metadata->name,
				  metadata->artist,
				  metadata->genre,
				  localph,
				  PTP_OFC_MTP_AbstractAudioAlbum,
				  ".alb",
				  &metadata->album_id,
				  metadata->tracks,
				  metadata->no_tracks);
}

/**
 * This creates a new sample data metadata structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_destroy_sampledata_t</code>
 * operation later, so be careful of using strdup() when assigning
 * strings.
 *
 * @return a pointer to the newly allocated metadata structure.
 * @see LIBMTP_destroy_sampledata_t()
 */
LIBMTP_filesampledata_t *LIBMTP_new_filesampledata_t(void)
{
  LIBMTP_filesampledata_t *new = (LIBMTP_filesampledata_t *) malloc(sizeof(LIBMTP_filesampledata_t));
  if (new == NULL) {
    return NULL;
  }
  new->height=0;
  new->width = 0;
  new->data = NULL;
  new->duration = 0;
  new->size = 0;
  return new;
}

/**
 * This destroys a file sample metadata type.
 * @param sample the file sample metadata to be destroyed.
 */
void LIBMTP_destroy_filesampledata_t(LIBMTP_filesampledata_t * sample)
{
  if (sample == NULL) {
    return;
  }
  if (sample->data != NULL) {
    free(sample->data);
  }
  free(sample);
}

/**
 * This routine figures out whether a certain filetype supports
 * representative samples (small thumbnail images) or not. This
 * typically applies to JPEG files, MP3 files and Album abstract
 * playlists, but in theory any filetype could support representative
 * samples.
 * @param device a pointer to the device which is to be examined.
 * @param the filetype to examine, and return the representative sample
 *        properties for.
 * @param sample this will contain a new sample type with the fields
 *        filled in with suitable default values. For example, the
 *        supported sample type will be set, the supported height and
 *        width will be set to max values if it is an image sample,
 *        and duration will also be given some suitable default value
 *        which should not be exceeded on audio samples. If the 
 *        device does not support samples for this filetype, this
 *        pointer will be NULL. If it is not NULL, the user must
 *        destroy this struct with <code>LIBMTP_destroy_filesampledata_t()</code>
 *        after use.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Send_Representative_Sample()
 * @see LIBMTP_Create_New_Album()
 */
int LIBMTP_Get_Representative_Sample_Format(LIBMTP_mtpdevice_t *device,
					    LIBMTP_filetype_t const filetype,
					    LIBMTP_filesampledata_t ** sample)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  int i;
  // TODO: Get rid of these when we can properly query the device.
  int support_data = 0;
  int support_format = 0;
  int support_height = 0;
  int support_width = 0;
  int support_duration = 0;

  PTPObjectPropDesc opd_height;
  PTPObjectPropDesc opd_width;
  PTPObjectPropDesc opd_format;
  PTPObjectPropDesc opd_duration;
  
  // Default to no type supported.
  *sample = NULL;
  
  ret = ptp_mtp_getobjectpropssupported(params, map_libmtp_type_to_ptp_type(filetype), &propcnt, &props);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Get_Representative_Sample_Format(): could not get object properties.");
    return -1;
  }
  /*
   * TODO: when walking through these object properties, make calls to
   * a new function in ptp.h/ptp.c that can send the command 
   * PTP_OC_MTP_GetObjectPropDesc to get max/min values of the properties
   * supported.
   */
  for (i = 0; i < propcnt; i++) {
    switch(props[i]) {
    case PTP_OPC_RepresentativeSampleData:
      support_data = 1;
      break;
    case PTP_OPC_RepresentativeSampleFormat:
      support_format = 1;
      break;
    case PTP_OPC_RepresentativeSampleSize:
      break;
    case PTP_OPC_RepresentativeSampleHeight:
      support_height = 1;
      break;
    case PTP_OPC_RepresentativeSampleWidth:
      support_width = 1;
      break;
    case PTP_OPC_RepresentativeSampleDuration:
      support_duration = 1;
      break;
    default:
      break;
    }
  }
  free(props);
  
  if (support_data && support_format && support_height && support_width && !support_duration) {
    // Something that supports height and width and not duration is likely to be JPEG
    LIBMTP_filesampledata_t *retsam = LIBMTP_new_filesampledata_t();
    /* 
     * Populate the sample format with the first supported format 
     * 
     * TODO: figure out how to pass back more than one format if more are
     * supported by the device.
     */
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleFormat, map_libmtp_type_to_ptp_type(filetype), &opd_format);    
    retsam->filetype = map_ptp_type_to_libmtp_type(opd_format.FORM.Enum.SupportedValue[0].u16);
    /* Populate the maximum image height */
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleWidth, map_libmtp_type_to_ptp_type(filetype), &opd_width);        
    retsam->width = opd_width.FORM.Range.MaximumValue.u32;
    /* Populate the maximum image width */
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleHeight, map_libmtp_type_to_ptp_type(filetype), &opd_height);    						
    retsam->height = opd_height.FORM.Range.MaximumValue.u32;
    *sample = retsam;
  } else if (support_data && support_format && !support_height && !support_width && support_duration) {
    // Another qualified guess
    LIBMTP_filesampledata_t *retsam = LIBMTP_new_filesampledata_t();
    /* 
     * Populate the sample format with the first supported format 
     * 
     * TODO: figure out how to pass back more than one format if more are
     * supported by the device.
     */
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleFormat, map_libmtp_type_to_ptp_type(filetype), &opd_format);    
    retsam->filetype = map_ptp_type_to_libmtp_type(opd_format.FORM.Enum.SupportedValue[0].u16);
	/* Populate the maximum duration */
    ptp_mtp_getobjectpropdesc (params, PTP_OPC_RepresentativeSampleDuration, map_libmtp_type_to_ptp_type(filetype), &opd_duration);    
    retsam->duration = opd_duration.FORM.Range.MaximumValue.u32;
    *sample = retsam;
  }
  return 0;
}

/**
 * This routine sends representative sample data for an object.
 * This uses the RepresentativeSampleData property of the album,
 * if the device supports it. The data should be of a format acceptable
 * to the player (for iRiver and Creative, this seems to be JPEG) and
 * must not be too large. (for a Creative, max seems to be about 20KB.)
 * TODO: there must be a way to find the max size for an ObjectPropertyValue.
 * @param device a pointer to the device which the object is on.
 * @param id unique id of the object to set artwork for.
 * @param data pointer to an array of uint8_t containing the representative 
 *        sample data.
 * @param size number of bytes in the sample.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Get_Representative_Sample_Format()
 * @see LIBMTP_Create_New_Album()
 */
int LIBMTP_Send_Representative_Sample(LIBMTP_mtpdevice_t *device,
                          uint32_t const id,
                          LIBMTP_filesampledata_t *sampledata)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTPPropertyValue propval;
  PTPObjectInfo *oi;
  uint32_t i;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  int supported = 0;

  // get the file format for the object we're going to send representative data for
  oi = NULL;
  for (i = 0; i < params->handles.n; i++) {
    if (params->handles.Handler[i] == id) {
      oi = &params->objectinfo[i];
      break;
    }
  }

  if (oi == NULL) {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_Representative_Sample(): could not get object info.");
    return -1;
  }

  // check that we can send representative sample data for this object format
  ret = ptp_mtp_getobjectpropssupported(params, oi->ObjectFormat, &propcnt, &props);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_Representative_Sample(): could not get object properties.");
    return -1;
  }

  for (i = 0; i < propcnt; i++) {
    if (props[i] == PTP_OPC_RepresentativeSampleData) {
      supported = 1;
      break;
    }
  }
  if (!supported) {
    free(props);
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Send_Representative_Sample(): object type doesn't support RepresentativeSampleData.");
    return -1;
  }
  free(props);
  
  // Go ahead and send the data
  propval.a.count = sampledata->size;
  propval.a.v = malloc(sizeof(PTPPropertyValue) * sampledata->size);
  for (i = 0; i < sampledata->size; i++) {
    propval.a.v[i].u8 = sampledata->data[i];
  }
  
  ret = ptp_mtp_setobjectpropvalue(params,id,PTP_OPC_RepresentativeSampleData,
				   &propval,PTP_DTC_AUINT8);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "LIBMTP_Send_Representative_Sample(): could not send sample data.");
    free(propval.a.v);
    return -1;
  }
  free(propval.a.v);

  /*
   * TODO: Send Representative Sample Height, Width and Size here if it is an
   * image (typically JPEG) thumbnail, send Duration and Size if it is an audio
   * sample (MP3, WAV etc).
   */
  
  /* Set the height and width if the sample is an image, otherwise just
   * set the duration and size */
  switch(sampledata->filetype) {
  case LIBMTP_FILETYPE_JPEG:
  case LIBMTP_FILETYPE_JFIF:
  case LIBMTP_FILETYPE_TIFF:
  case LIBMTP_FILETYPE_BMP:
  case LIBMTP_FILETYPE_GIF:
  case LIBMTP_FILETYPE_PICT:
  case LIBMTP_FILETYPE_PNG:
    // For images, set the height and width
    set_object_u32(device, id, PTP_OPC_RepresentativeSampleHeight, sampledata->height);
    set_object_u32(device, id, PTP_OPC_RepresentativeSampleWidth, sampledata->width);		
    break;
  default:
    // For anything not an image, set the duration and size
    set_object_u32(device, id, PTP_OPC_RepresentativeSampleDuration, sampledata->duration);
    set_object_u32(device, id, PTP_OPC_RepresentativeSampleSize, sampledata->size);
    break;  		
  }
    
  return 0;
}

/**
 * This routine updates an album based on the metadata
 * supplied. If the <code>tracks</code> field of the metadata
 * contains a track listing, these tracks will be added to the
 * album in place of those already present, i.e. the
 * previous track listing will be deleted.
 * @param device a pointer to the device to create the new album on.
 * @param metadata the metadata for the album to be updated.
 *                 notice that the field <code>album_id</code>
 *                 must contain the apropriate album ID.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Create_New_Album()
 * @see LIBMTP_Delete_Object()
 */
int LIBMTP_Update_Album(LIBMTP_mtpdevice_t *device,
			   LIBMTP_album_t const * const metadata)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  int i;
  
  // First see which properties can be set
  // i.e only try to update this metadata for object tags that exist on the current player.
  ret = ptp_mtp_getobjectpropssupported(params, PTP_OFC_MTP_AbstractAudioAlbum, &propcnt, &props);
  if (ret != PTP_RC_OK) {
    // Just bail out for now, nothing is ever set.
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Album(): "
			    "could not retrieve supported object properties.");
    return -1;
  }
  if (ptp_operation_issupported(params,PTP_OC_MTP_SetObjPropList)) {
    MTPPropList *proplist = NULL;
    MTPPropList *prop = NULL;
    
    for (i=0;i<propcnt;i++) {
      PTPObjectPropDesc opd;

      ret = ptp_mtp_getobjectpropdesc(params, props[i], PTP_OFC_MTP_AbstractAudioAlbum, &opd);
      if (ret != PTP_RC_OK) {
	add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Album(): "
				"could not get property description.");
      } else if (opd.GetSet) {
	switch (props[i]) {
	case PTP_OPC_Name:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->album_id;      
	  prop->property = PTP_OPC_Name;
	  prop->datatype = PTP_DTC_STR;
	  if (metadata->name != NULL)
	    prop->propval.str = strdup(metadata->name);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Artist:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->album_id;      
	  prop->property = PTP_OPC_Artist;
	  prop->datatype = PTP_DTC_STR;
	  if (metadata->artist != NULL)
	    prop->propval.str = strdup(metadata->artist);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	case PTP_OPC_Genre:
	  prop = new_mtp_prop_entry();
	  prop->ObjectHandle = metadata->album_id;
	  prop->property = PTP_OPC_Genre;
	  prop->datatype = PTP_DTC_STR;
	  if (metadata->genre != NULL)
	    prop->propval.str = strdup(metadata->genre);
	  proplist = add_mtp_prop_to_proplist(proplist, prop);
	  break;
	default:
	  break;
	}
      }
      ptp_free_objectpropdesc(&opd);
    }
    
    ret = ptp_mtp_setobjectproplist(params, proplist);

    destroy_mtp_prop_list(proplist);
          
    if (ret != PTP_RC_OK) {
      // TODO: return error of which property we couldn't set
      add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Album(): "
			      "could not set object property list.");
      return -1;
    }
      
  } else if (ptp_operation_issupported(params,PTP_OC_MTP_SetObjectPropValue)) {
    for (i=0;i<propcnt;i++) {
      switch (props[i]) {
      case PTP_OPC_Name:
	// Update title
	ret = set_object_string(device, metadata->album_id, PTP_OPC_Name, metadata->name);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Album(): "
				  "could not set album title.");
	}
	break;
      case PTP_OPC_Artist:
	// Update artist
	ret = set_object_string(device, metadata->album_id, PTP_OPC_Artist, metadata->artist);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Album(): "
				  "could not set album artist name.");
	}
	break;
      case PTP_OPC_Genre:
	// Update genre
	ret = set_object_string(device, metadata->album_id, PTP_OPC_Genre, metadata->genre);
	if (ret != 0) {
	  add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Album(): "
				  "could not set album genre.");
	}
	break;
      default:
	break;
      }
    }
    free(props);
  } else {
    add_error_to_errorstack(device, LIBMTP_ERROR_GENERAL, "LIBMTP_Update_Album(): "
                            "Your device doesn't seem to support any known way of setting metadata.");
    return -1;
  }
  
  // Then the object references...
  if (metadata->no_tracks > 0) {
    // Add tracks to the new album as object references.
    ret = ptp_mtp_setobjectreferences (params, metadata->album_id, metadata->tracks, metadata->no_tracks);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "LIBMTP_Update_Album(): could not add tracks as object references.");
      return -1;
    }
  }

  if (params->proplist != NULL) {
    update_metadata_cache(device, metadata->album_id);
  }
  return 0;
}

/**
 * Dummy function needed to interface to upstream
 * ptp.c/ptp.h files.
 */
void ptp_nikon_getptpipguid (unsigned char* guid) {
  return;
}

/**
 * Update cache after new object is added to the device
 */ 
void add_object_to_cache(LIBMTP_mtpdevice_t *device, uint32_t handle)
{
  PTPParams *params = (PTPParams *)device->params;
  MTPPropList *prop = NULL;
  uint16_t ret;
  int n;
  
  n = ++params->handles.n;

  params->objectinfo = (PTPObjectInfo*)realloc(params->objectinfo,
                                               sizeof(PTPObjectInfo)*n);
  params->handles.Handler = (uint32_t*)realloc(params->handles.Handler,
                                               sizeof(uint32_t)*n);

  memset(&params->objectinfo[n-1], 0, sizeof(PTPObjectInfo));
  params->handles.Handler[n-1] = handle;

  ptp_getobjectinfo(params, handle, &params->objectinfo[n-1]);

  if (params->proplist) {

    ret = ptp_mtp_getobjectproplist(params, handle, &prop);
    if (ret != PTP_RC_OK) {
      add_ptp_error_to_errorstack(device, ret, "add_object_to_cache(): call to ptp_mtp_getobjectproplist() failed.");
      return;
    }
    add_mtp_prop_to_proplist(params->proplist, prop);
  }
}


/**
 * Update cache after object has been modified
 */
void update_metadata_cache(LIBMTP_mtpdevice_t *device, uint32_t handle)
{
  PTPParams *params = (PTPParams *)device->params;
  MTPPropList *prop = params->proplist;
  MTPPropList *prev = NULL;
  MTPPropList *deleted_props = NULL;
  MTPPropList *updated_props;
  uint16_t ret;
    
  // find the place where the properties for this object begin
  while (prop != NULL && prop->ObjectHandle != handle) {
    prev = prop;
    prop = prop->next;
  }

  // safeguard in case object is not found
  if (!prop) return;
    
  // fetch updated properties and add them to the metadata cache in the same
  // place where outdated properties were found
  ret = ptp_mtp_getobjectproplist(params, handle, &updated_props);
  if (ret != PTP_RC_OK) {
    add_ptp_error_to_errorstack(device, ret, "update_metadata_cache(): call to ptp_mtp_getobjectproplist() failed.");
    return;
  }
  prev->next = updated_props;

  // head of outdated object properties that will be deleted
  deleted_props = prop;
    
  // find the end of properties for this object
  while (prop != NULL && prop->ObjectHandle == handle) {
    prev = prop;
    prop = prop->next;
  }

  // find the end of the fetched properties and link rest of the list to it
  while (updated_props->next != NULL) {
    updated_props = updated_props->next;
  }
  updated_props->next = prop;
    
  // split outdated properties into separate list and delete it
  prev->next = NULL;
  destroy_mtp_prop_list(deleted_props);

  return;
}
