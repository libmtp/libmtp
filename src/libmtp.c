/**
 * \file libmtp.c
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "libmtp.h"
#include "unicode.h"
#include "ptp.h"
#include "libusb-glue.h"

/*
 * On MacOS (Darwin) and *BSD we're not using glibc, but libiconv.
 * glibc knows that UCS-2 is to be in the local machine endianness,
 * whereas libiconv does not. So we construct this macro to get
 * things right. Reportedly, glibc 2.1.3 has a bug so that UCS-2
 * is always bigendian though, we would need to work around that
 * too...
 */
#ifndef __GLIBC__
#define UCS_2_INTERNAL "UCS-2-INTERNAL"
#else
#if (__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1 )
#error "Too old glibc. This versions iconv() implementation cannot be trusted."
#endif
#define UCS_2_INTERNAL "UCS-2"
#endif

/*
 * This is a mapping between libmtp internal MTP filetypes and
 * the libgphoto2/PTP equivalent defines. We need this because
 * otherwise the libmtp.h device has to be dependent on ptp.h
 * to be installed too, and we don't want that.
 */
typedef struct filemap_t LIBMTP_filemap_t;
struct filemap_t {
  char *description; /**< Text description for the file type */
  LIBMTP_filetype_t id; /**< LIBMTP internal type for the file type */
  uint16_t ptp_id; /**< PTP ID for the filetype */
  void *constructor; /**< Function to create the data structure for this file type */
  void *destructor; /**< Function to destroy the data structure for this file type */
  void *datafunc; /**< Function to fill in the data for this file type */
  LIBMTP_filemap_t *next;
};

// Global variables
// This holds the global filetype mapping table
static LIBMTP_filemap_t *filemap = NULL;

// Forward declarations of local functions
static void flush_handles(LIBMTP_mtpdevice_t *device);
static uint16_t map_libmtp_type_to_ptp_type(LIBMTP_filetype_t intype);
static LIBMTP_filetype_t map_ptp_type_to_libmtp_type(uint16_t intype);
static int get_device_unicode_property(LIBMTP_mtpdevice_t *device,
				       char **unicstring, uint16_t property);
static void get_track_metadata(LIBMTP_mtpdevice_t *device, uint16_t objectformat,
			       LIBMTP_track_t *track);

static LIBMTP_filemap_t *new_filemap_entry()
{
  LIBMTP_filemap_t *filemap;

  filemap = (LIBMTP_filemap_t *)malloc(sizeof(LIBMTP_filemap_t));

  if( filemap != NULL ) {
    filemap->description = NULL;
    filemap->id = LIBMTP_FILETYPE_UNKNOWN;
    filemap->ptp_id = PTP_OFC_Undefined;
    filemap->constructor = NULL;
    filemap->destructor = NULL;
    filemap->datafunc = NULL;
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
 * @param constructor Pointer to function to create data structure for filetype
 * @param destructor Pointer to function to destroy data structure for filetype
 * @param datafunc Pointer to function to fill data structure
 * @return 0 for success any other value means error.
*/
int LIBMTP_Register_Filetype(char const * const description, LIBMTP_filetype_t const id,
			     uint16_t const ptp_id, void const * const constructor,
			     void const * const destructor, void const * const datafunc)
{
  LIBMTP_filemap_t *new = NULL, *current;

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
    new->constructor = (void*) constructor;
    new->destructor = (void*) destructor;
    new->datafunc = (void*) datafunc;

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
    current->constructor = (void*) constructor;
    current->destructor = (void*) destructor;
    current->datafunc = (void*) datafunc;
  }

  return 0;
}

/**
 * Set the description for a MTP filetype
 *
 * @param id libmtp internal filetype id
 * @param description Text description of filetype
 * @return 0 on success, any other value means error.
*/
int LIBMTP_Set_Filetype_Description(LIBMTP_filetype_t const id, char const * const description)
{
  LIBMTP_filemap_t *current;

  if (filemap == NULL) {
    return -1;
  }

  // Go through the filemap until an entry is found
  current = filemap;

  while(current != NULL) {
    if(current->id == id) {
      break;
    }
    current = current->next;
  }

  if(current == NULL) {
    return -1;
  }

  if (current->description != NULL) {
    free(current->description);
    current->description = NULL;
  }
  if(description != NULL) {
    current->description = strdup(description);
  }
  return 0;
}

/**
 * Set the constructor for a MTP filetype
 *
 * @param id libmtp internal filetype id
 * @param constructor Pointer to a constructor function
 * @return 0 on success, any other value means failure
*/
int LIBMTP_Set_Constructor(LIBMTP_filetype_t const id, void const * const constructor)
{
  LIBMTP_filemap_t *current;

  if (filemap == NULL) {
    return -1;
  }

  // Go through the filemap until an entry is found
  current = filemap;

  while(current != NULL) {
    if(current->id == id) {
      break;
    }
    current = current->next;
  }

  if (current == NULL) {
    return -1;
  }

  current->constructor = (void*) constructor;
  return 0;
}

/**
 * Set the destructor for a MTP filetype
 *
 * @param id libmtp internal filetype id
 * @param destructor Pointer to a destructor function
 * @return 0 on success, any other value means failure
*/
int LIBMTP_Set_Destructor(LIBMTP_filetype_t const id, void const * const destructor)
{
  LIBMTP_filemap_t *current;

  if (filemap == NULL) {
    return -1;
  }

  // Go through the filemap until an entry is found
  current = filemap;

  while(current != NULL) {
    if(current->id == id) {
      break;
    }
    current = current->next;
  }

  if(current == NULL) {
    return -1;
  }

  current->destructor = (void *) destructor;
  return 0;
}

/**
 * Set the datafunc for a MTP filetype
 *
 * @param id libmtp internal filetype id
 * @param datafunc Pointer to a data function
 * @return 0 on success, any other value means failure
*/
int LIBMTP_Set_Datafunc(LIBMTP_filetype_t const id, void const * const datafunc)
{
  LIBMTP_filemap_t *current;

  if (filemap == NULL) {
    return -1;
  }

  // Go through the filemap until an entry is found
  current = filemap;

  while(current != NULL) {
    if(current->id == id) {
      break;
    }
    current = current->next;
  }

  if(current == NULL) {
    return -1;
  }

  current->datafunc = (void *) datafunc;
  return 0;
}

static void init_filemap()
{
  LIBMTP_Register_Filetype("RIFF WAVE file", LIBMTP_FILETYPE_WAV, PTP_OFC_WAV,LIBMTP_new_track_t,LIBMTP_destroy_track_t,NULL);
  LIBMTP_Register_Filetype("ISO MPEG Audio Layer 3", LIBMTP_FILETYPE_MP3, PTP_OFC_MP3,LIBMTP_new_track_t,LIBMTP_destroy_track_t,NULL);
  LIBMTP_Register_Filetype("Microsoft Windows Media Audio", LIBMTP_FILETYPE_WMA, PTP_OFC_MTP_WMA,LIBMTP_new_track_t,LIBMTP_destroy_track_t,NULL);
  LIBMTP_Register_Filetype("Ogg container format", LIBMTP_FILETYPE_OGG, PTP_OFC_MTP_OGG,LIBMTP_new_track_t,LIBMTP_destroy_track_t,NULL);
  LIBMTP_Register_Filetype("Audible.com Audio Codec", LIBMTP_FILETYPE_AUDIBLE, PTP_OFC_MTP_AudibleCodec,LIBMTP_new_track_t,LIBMTP_destroy_track_t,NULL);
  LIBMTP_Register_Filetype("Advanced Acoustic Coding", LIBMTP_FILETYPE_MP4, PTP_OFC_MTP_MP4,LIBMTP_new_track_t,LIBMTP_destroy_track_t,NULL);
  LIBMTP_Register_Filetype("Undefined audio file", LIBMTP_FILETYPE_UNDEF_AUDIO, PTP_OFC_MTP_UndefinedAudio,LIBMTP_new_track_t,LIBMTP_destroy_track_t,NULL);
  LIBMTP_Register_Filetype("Microsoft Windows Media Video", LIBMTP_FILETYPE_WMV, PTP_OFC_MTP_WMV,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Audio Video Interleave", LIBMTP_FILETYPE_AVI, PTP_OFC_AVI,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("MPEG video stream", LIBMTP_FILETYPE_MPEG, PTP_OFC_MPEG,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Microsoft Advanced Systems Format", LIBMTP_FILETYPE_ASF, PTP_OFC_ASF,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Apple Quicktime container format", LIBMTP_FILETYPE_QT, PTP_OFC_QT,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Undefined video file", LIBMTP_FILETYPE_UNDEF_VIDEO, PTP_OFC_MTP_UndefinedVideo,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("JPEG file", LIBMTP_FILETYPE_JPEG, PTP_OFC_EXIF_JPEG,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("JFIF file", LIBMTP_FILETYPE_JFIF, PTP_OFC_JFIF,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("TIFF bitmap file", LIBMTP_FILETYPE_TIFF, PTP_OFC_TIFF,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("BMP bitmap file", LIBMTP_FILETYPE_BMP, PTP_OFC_BMP,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("GIF bitmap file", LIBMTP_FILETYPE_GIF, PTP_OFC_GIF,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("PICT bitmap file", LIBMTP_FILETYPE_PICT, PTP_OFC_PICT,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Portable Network Graphics", LIBMTP_FILETYPE_PNG, PTP_OFC_PNG,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Microsoft Windows Image Format", LIBMTP_FILETYPE_WINDOWSIMAGEFORMAT, PTP_OFC_MTP_WindowsImageFormat,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("VCalendar version 1", LIBMTP_FILETYPE_VCALENDAR1, PTP_OFC_MTP_vCalendar1,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("VCalendar version 2", LIBMTP_FILETYPE_VCALENDAR2, PTP_OFC_MTP_vCalendar2,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("VCard version 2", LIBMTP_FILETYPE_VCARD2, PTP_OFC_MTP_vCard2,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("VCard version 3", LIBMTP_FILETYPE_VCARD3, PTP_OFC_MTP_vCard3,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Undefined Windows executable file", LIBMTP_FILETYPE_WINEXEC, PTP_OFC_MTP_UndefinedWindowsExecutable,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Text file", LIBMTP_FILETYPE_TEXT, PTP_OFC_Text,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("HTML file", LIBMTP_FILETYPE_HTML, PTP_OFC_HTML,NULL,NULL,NULL);
  LIBMTP_Register_Filetype("Undefined filetype", LIBMTP_FILETYPE_UNKNOWN, PTP_OFC_Undefined, NULL, NULL, NULL);
}

/**
 * Returns the PTP filetype that maps to a certain libmtp internal file type.
 * @param intype the MTP library interface type
 * @return the PTP (libgphoto2) interface type
 */
static uint16_t map_libmtp_type_to_ptp_type(LIBMTP_filetype_t intype)
{
  LIBMTP_filemap_t *current;

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
  LIBMTP_filemap_t *current;

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
 * Returns the data function for the file type
 * @param intype the PTP library interface
 * @return pointer to the data function
 */
static void *get_datafunc(uint16_t intype)
{
  LIBMTP_filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->ptp_id == intype) {
      return current->datafunc;
    }
    current = current->next;
  }
  return NULL;
}


/**
 * Returns the constructor for that file type data
 * @param intype the PTP library interface type
 * @return pointer to the constructor
 */
static void *get_constructor(uint16_t intype)
{
  LIBMTP_filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->ptp_id == intype) {
      return current->constructor;
    }
    current = current->next;
  }
  return NULL;
}

/**
 * Returns the destructor for that file type data
 * @param intype the PTP library interface type
 * @return pointer to the destructor
 */
static void *get_destructor(uint16_t intype)
{
  LIBMTP_filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->ptp_id == intype) {
      return current->destructor;
    }
    current = current->next;
  }
  return NULL;
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
  LIBMTP_filemap_t *current;

  current = filemap;

  while (current != NULL) {
    if(current->id == intype) {
      return current->description;
    }
    current = current->next;
  }

  return "Unknown filetype";
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
 * Retrieves a string from an object
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param attribute_id PTP attribute ID
 * @return valid string or NULL on failure. The returned string
 *         must bee <code>free()</code>:ed by the caller after
 *         use.
 */
char *LIBMTP_Get_String_From_Object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id)
{
  PTPPropertyValue propval;
  char *retstring = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if ( device == NULL || object_id == 0) {
    return NULL;
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_STR);
  if (ret == PTP_RC_OK) {
    if (propval.str != NULL) {
      retstring = (char *) strdup(propval.str);
      free(propval.str);
    }
  }

  return retstring;
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
uint32_t LIBMTP_Get_U32_From_Object(LIBMTP_mtpdevice_t *device,uint32_t const object_id,
				    uint16_t const attribute_id, uint32_t const value_default)
{
  PTPPropertyValue propval;
  uint32_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if ( device == NULL ) {
    return value_default;
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT32);
  if (ret == PTP_RC_OK) {
    retval = propval.u32;
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
uint16_t LIBMTP_Get_U16_From_Object(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				    uint16_t const attribute_id, uint16_t const value_default)
{
  PTPPropertyValue propval;
  uint16_t retval = value_default;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if ( device == NULL ) {
    return value_default;
  }

  ret = ptp_mtp_getobjectpropvalue(params, object_id,
                                   attribute_id,
                                   &propval,
                                   PTP_DTC_UINT16);
  if (ret == PTP_RC_OK) {
    retval = propval.u16;
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
int LIBMTP_Set_Object_String(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			     uint16_t const attribute_id, char const * const string)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL || string == NULL) {
    return -1;
  }

  propval.str = (char *) string;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_STR);
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Set_Object_String(): could not set object string.\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
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
int LIBMTP_Set_Object_U32(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint32_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return -1;
  }

  propval.u32 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT32);
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Set_Object_U32(): could not set unsigned 32bit integer property.\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
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
int LIBMTP_Set_Object_U16(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
			  uint16_t const attribute_id, uint16_t const value)
{
  PTPPropertyValue propval;
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL) {
    return 1;
  }

  propval.u16 = value;
  ret = ptp_mtp_setobjectpropvalue(params, object_id, attribute_id, &propval, PTP_DTC_UINT16);
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Set_Object_U16(): could not set unsigned 16bit integer property.\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return 1;
  }

  return 0;
}

/**
 * Gets an array of object ids associated with a specified object
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param items array of unsigned 32-bit integers
 * @param len length of array
 * @return 0 on success, any other value means failure
 */
int LIBMTP_Get_Object_References(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				 uint32_t **items, uint32_t *len)
{
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  // A device must be attached
  if (device == NULL ) {
    *items = NULL;
    *len = 0;
    return 1;
  }

  ret = ptp_mtp_getobjectreferences (params, object_id, items, len);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Get_Object_References: Could not get object references\n");
    return 1;
  }

  return 0;
}

/**
 * Sets an array of object ids associated with a specified object
 *
 * @param device a pointer to an MTP device.
 * @param object_id Object reference
 * @param items array of unsigned 32-bit integers
 * @param len length of array
 * @return 0 on success, any other value means failure
 */
int LIBMTP_Set_Object_References(LIBMTP_mtpdevice_t *device, uint32_t const object_id,
				 uint32_t const * const items, uint32_t const len)
{
  PTPParams *params = (PTPParams *) device->params;
  uint16_t ret;

  if (device == NULL || items == NULL) {
    return 1;
  }

  ret = ptp_mtp_setobjectreferences (params, object_id, (uint32_t *) items, len);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Set_Object_References: Could not set object references\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return 1;
  }

  return 0;
}

/**
 * Get a list of the supported devices.
 *
 * The developers depend on users of this library to constantly
 * add in to the list of supported devices. What we need is the
 * device name, USB Vendor ID (VID) and USB Product ID (PID).
 * put this into a bug ticket at the project homepage, please.
 * The VID/PID is used to let e.g. udev lift the device to
 * console userspace access when it's plugged in.
 *
 * @param devices a pointer to a pointer that will hold a device
 *        list after the call to this function, if it was
 *        successful.
 * @param numdevs a pointer to an integer that will hold the number
 *        of devices in the device list if the call was successful.
 * @return 0 if the list was successfull retrieved, any other
 *        value means failure.
 */
int LIBMTP_Get_Supported_Devices_List(LIBMTP_device_entry_t ** const devices, int * const numdevs)
{
  // Just dispatch to libusb glue file...
  return get_device_list(devices, numdevs);
}

/**
 * Get the first connected MTP device. There is currently no API for
 * retrieveing multiple devices.
 * @return a device pointer.
 */
LIBMTP_mtpdevice_t *LIBMTP_Get_First_Device(void)
{
  uint8_t interface_number;
  PTPParams *params;
  PTP_USB *ptp_usb;
  PTPStorageIDs storageIDs;
  uint32_t storageID = 0;
  PTPDevicePropDesc dpd;
  uint8_t batteryLevelMax = 100; // Some default
  uint16_t ret;
  uint32_t i;
  LIBMTP_mtpdevice_t *tmpdevice;
	uint8_t remaining_directories;

  // Allocate a parameter block
  params = (PTPParams *) malloc(sizeof(PTPParams));
  params->cd_locale_to_ucs2 = iconv_open(UCS_2_INTERNAL, "UTF-8");
  params->cd_ucs2_to_locale = iconv_open("UTF-8", UCS_2_INTERNAL);
  if (params->cd_locale_to_ucs2 == (iconv_t) -1 || params->cd_ucs2_to_locale == (iconv_t) -1) {
    printf("LIBMTP panic: could not open iconv() converters to/from UCS-2!\n");
    return NULL;
  }

  ptp_usb = (PTP_USB *) malloc(sizeof(PTP_USB));
  // Callbacks and stuff
  ptp_usb->callback_active = 0;
  ptp_usb->current_transfer_total = 0;
  ptp_usb->current_transfer_complete = 0;
  ptp_usb->current_transfer_callback = NULL;

  // get storage ID
  ret = connect_first_device(params, ptp_usb, &interface_number);

  switch (ret)
    {
    case PTP_CD_RC_CONNECTED:
      printf("Connected to MTP device.\n");
      break;
    case PTP_CD_RC_NO_DEVICES:
      printf("No MTP devices.\n");
      return NULL;
    case PTP_CD_RC_ERROR_CONNECTING:
      printf("Connection error.\n");
      return NULL;
    }

  // get storage ID
  if (ptp_getstorageids (params, &storageIDs) == PTP_RC_OK) {
    if (storageIDs.n > 0)
      storageID = storageIDs.Storage[0];
    free(storageIDs.Storage);
  }

  // Make sure there are no handlers
  params->handles.Handler = NULL;

  // Just cache the device information for any later use.
  if (ptp_getdeviceinfo(params, &params->deviceinfo) != PTP_RC_OK) {
    goto error_handler;
  }

  // Get battery maximum level
  if (ptp_property_issupported(params, PTP_DPC_BatteryLevel)) {
    if (ptp_getdevicepropdesc(params, PTP_DPC_BatteryLevel, &dpd) != PTP_RC_OK) {
      printf("LIBMTP_Get_First_Device(): Unable to retrieve battery max level.\n");
      goto error_handler;
    }
    // if is NULL, just leave as default
    if (dpd.FORM.Range.MaximumValue.u8 != 0) {
      batteryLevelMax = dpd.FORM.Range.MaximumValue.u8;
    }
    ptp_free_devicepropdesc(&dpd);
  }

  // OK everything got this far, so it is time to create a device struct!
  tmpdevice = (LIBMTP_mtpdevice_t *) malloc(sizeof(LIBMTP_mtpdevice_t));
  tmpdevice->interface_number = interface_number;
  tmpdevice->params = (void *) params;
  tmpdevice->usbinfo = (void *) ptp_usb;
  tmpdevice->storage_id = storageID;
  tmpdevice->maximum_battery_level = batteryLevelMax;

  // Set all default folders to 0 == root directory
  tmpdevice->default_music_folder = 0;
  tmpdevice->default_playlist_folder = 0;
  tmpdevice->default_picture_folder = 0;
  tmpdevice->default_video_folder = 0;
  tmpdevice->default_organizer_folder = 0;
  tmpdevice->default_zencast_folder = 0;

  /*
   * Then get the handles and try to locate the default folders.
   * This has the desired side effect of cacheing all handles from
   * the device which speeds up later operations.
   */
  flush_handles(tmpdevice);
  /*
   * Remaining directories to get the handles to.
   * We can stop when done this to save time
   */
  remaining_directories = 6;
  for (i = 0; i < params->handles.n && remaining_directories > 0; i++) {
    PTPObjectInfo oi;
    if (ptp_getobjectinfo(params, params->handles.Handler[i], &oi) == PTP_RC_OK) {
      // Ignore non-folders
      if ( oi.ObjectFormat != PTP_OFC_Association )
	continue;
      if ( oi.Filename == NULL)
	continue;
      if (!strcmp(oi.Filename, "Music")) {
	tmpdevice->default_music_folder = params->handles.Handler[i];
	remaining_directories--;
	continue;
      } else if (!strcmp(oi.Filename, "My Playlists")) {
	tmpdevice->default_playlist_folder = params->handles.Handler[i];
	remaining_directories--;
	continue;
      } else if (!strcmp(oi.Filename, "Pictures")) {
	tmpdevice->default_picture_folder = params->handles.Handler[i];
	remaining_directories--;
	continue;
      } else if (!strcmp(oi.Filename, "Video")) {
	tmpdevice->default_video_folder = params->handles.Handler[i];
	remaining_directories--;
	continue;
      } else if (!strcmp(oi.Filename, "My Organizer")) {
	tmpdevice->default_organizer_folder = params->handles.Handler[i];
	remaining_directories--;
	continue;
      } else if (!strcmp(oi.Filename, "ZENcast")) {
	tmpdevice->default_zencast_folder = params->handles.Handler[i];
	remaining_directories--;
	continue;
      }
    } else {
      printf("LIBMTP panic: Found a bad handle, trying to ignore it.\n");
    }
  }

  return tmpdevice;

  // Then close it again.
 error_handler:
  close_device(ptp_usb, params, interface_number);
  // TODO: libgphoto2 does not seem to be able to free the deviceinfo
  // ptp_free_deviceinfo(&params->deviceinfo);
  if (params->handles.Handler != NULL) {
    free(params->handles.Handler);
  }
  return NULL;
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
  // Free the device info and any handler
  // TODO: libgphoto2 does not seem to be able to free the deviceinfo
  // ptp_free_deviceinfo(&params->deviceinfo);
  if (params->handles.Handler != NULL) {
    free(params->handles.Handler);
    params->handles.Handler = NULL;
  }
  // Free iconv() converters...
  iconv_close(params->cd_locale_to_ucs2);
  iconv_close(params->cd_ucs2_to_locale);
  free(device);
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
  uint16_t ret;

  if (params->handles.Handler != NULL) {
    free(params->handles.Handler);
  }

  // Get all the handles if we haven't already done that
  ret = ptp_getobjecthandles(params,
			     PTP_GOH_ALL_STORAGE,
			     PTP_GOH_ALL_FORMATS,
			     PTP_GOH_ALL_ASSOCS,
			     &params->handles);
  if (ret != PTP_RC_OK) {
    printf("flush_handles(): LIBMTP panic: Could not get object handles...\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
  }

  return;
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
	printf("      Error on query for object properties.\n");
	printf("      Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
      } else {
	for (j=0;j<propcnt;j++) {
	  (void) ptp_render_mtp_propname(props[j],sizeof(txt),txt);
	  printf("      %04x: %s\n", props[j], txt);
	}
	free(props);
      }
    }
  }

  printf("Special directories:\n");
  printf("   Default music folder: 0x%08x\n", device->default_music_folder);
  printf("   Default playlist folder: 0x%08x\n", device->default_playlist_folder);
  printf("   Default picture folder: 0x%08x\n", device->default_picture_folder);
  printf("   Default video folder: 0x%08x\n", device->default_video_folder);
  printf("   Default organizer folder: 0x%08x\n", device->default_organizer_folder);
  printf("   Default zencast folder: 0x%08x\n", device->default_zencast_folder);
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

  if (!ptp_property_issupported(params, PTP_DPC_MTP_DeviceFriendlyName)) {
    return NULL;
  }

  if (ptp_getdevicepropvalue(params,
			     PTP_DPC_MTP_DeviceFriendlyName,
			     &propval,
			     PTP_DTC_STR) != PTP_RC_OK) {
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

  if (!ptp_property_issupported(params, PTP_DPC_MTP_DeviceFriendlyName)) {
    return -1;
  }
  propval.str = (char *) friendlyname;
  if (ptp_setdevicepropvalue(params,
			     PTP_DPC_MTP_DeviceFriendlyName,
			     &propval,
			     PTP_DTC_STR) != PTP_RC_OK) {
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

  if (!ptp_property_issupported(params, PTP_DPC_MTP_SynchronizationPartner)) {
    return NULL;
  }

  if (ptp_getdevicepropvalue(params,
			     PTP_DPC_MTP_SynchronizationPartner,
			     &propval,
			     PTP_DTC_STR) != PTP_RC_OK) {
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

  if (!ptp_property_issupported(params, PTP_DPC_MTP_SynchronizationPartner)) {
    return -1;
  }
  propval.str = (char *) syncpartner;
  if (ptp_setdevicepropvalue(params,
			     PTP_DPC_MTP_SynchronizationPartner,
			     &propval,
			     PTP_DTC_STR) != PTP_RC_OK) {
    return -1;
  }
  return 0;
}

/**
 * This function finds out how much storage space is currently used
 * and any additional storage information. Storage may be a hard disk
 * or flash memory or whatever.
 * @param device a pointer to the device to get the storage info for.
 * @param total a pointer to a variable that will hold the
 *        total the total number of bytes available on this volume after
 *        the call.
 * @param free a pointer to a variable that will hold the number of
 *        free bytes available on this volume right now after the call.
 * @param storage_description a description of the storage. This may
 *        be NULL after the call even if it succeeded. If it is not NULL,
 *        it must be freed by the callee after use.
 * @param volume_label a volume label or similar. This may be NULL after the
 *        call even if it succeeded. If it is not NULL, it must be
 *        freed by the callee after use.
 * @return 0 if the storage info was successfully retrieved, any other
 *        value means failure.
 */
int LIBMTP_Get_Storageinfo(LIBMTP_mtpdevice_t *device, uint64_t * const total,
			uint64_t * const free, char ** const storage_description,
			char ** const volume_label)
{
  PTPStorageInfo storageInfo;
  PTPParams *params = (PTPParams *) device->params;

  if (ptp_getstorageinfo(params, device->storage_id, &storageInfo) != PTP_RC_OK) {
    printf("LIBMTP_Get_Diskinfo(): failed to get disk info\n");
    *total = 0;
    *free = 0;
    *storage_description = NULL;
    *volume_label = NULL;
    return -1;
  }
  *total = storageInfo.MaxCapability;
  *free = storageInfo.FreeSpaceInBytes;
  *storage_description = storageInfo.StorageDescription;
  *volume_label = storageInfo.VolumeLabel;

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
    printf("LIBMTP_Get_Batterylevel(): could not get devcie property value.\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
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
 * @param device a pointer to the device to format.
 * @return 0 on success, any other value means failure.
 */
int LIBMTP_Format_Storage(LIBMTP_mtpdevice_t *device)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;

  if (!ptp_operation_issupported(params,PTP_OC_FormatStore)) {
    printf("LIBMTP_Format_Storage(): device cannot format storage\n");
    return -1;
  }
  ret = ptp_formatstore(params, device->storage_id);
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Format_Storage(): failed to format storage\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
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
  int i;

  if (!ptp_property_issupported(params, property)) {
    return -1;
  }

  // Unicode strings are 16bit unsigned integer arrays.
  if (ptp_getdevicepropvalue(params,
			     property,
			     &propval,
			     PTP_DTC_AUINT16) != PTP_RC_OK) {
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
 * This creates a new MTP object structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_destroy_object_t</code>
 * operation later, so be careful of using strdup() when assigning
 * strings, e.g.:
 *
 * <pre>
 * LIBMTP_object_t *object = LIBMTP_new_object_t();
 * object->name = strdup(namestr);
 * ....
 * LIBMTP_destroy_object_t(file);
 * </pre>
 *
 * @return a pointer to the newly allocated structure.
 * @see LIBMTP_destroy_object_t()
 */
LIBMTP_object_t *LIBMTP_new_object_t(void)
{
  LIBMTP_object_t *new = (LIBMTP_object_t *) malloc(sizeof(LIBMTP_object_t));
  if (new == NULL) {
    return NULL;
  }

  new->id = 0;
  new->parent = 0;
  new->type = LIBMTP_FILETYPE_UNKNOWN;
  new->size = 0;
  new->name = NULL;
  new->data = NULL;
  new->sibling = NULL;
  new->child = NULL;

  return new;
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
 * This returns a long list of all files available
 * on the current MTP device. Typical usage:
 *
 * <pre>
 * LIBMTP_file_t *filelist;
 *
 * filelist = LIBMTP_Get_Filelisting(device);
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
 * @return a list of files that can be followed using the <code>next</code>
 *         field of the <code>LIBMTP_file_t</code> data structure.
 *         Each of the metadata tags must be freed after use, and may
 *         contain only partial metadata information, i.e. one or several
 *         fields may be NULL or 0.
 * @see LIBMTP_Get_Filemetadata()
 */
LIBMTP_file_t *LIBMTP_Get_Filelisting(LIBMTP_mtpdevice_t *device)
{
  uint32_t i = 0;
  LIBMTP_file_t *retfiles = NULL;
  LIBMTP_file_t *curfile = NULL;
  PTPParams *params = (PTPParams *) device->params;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {

    LIBMTP_file_t *file;
    PTPObjectInfo oi;

    if (ptp_getobjectinfo(params, params->handles.Handler[i], &oi) == PTP_RC_OK) {

      if (oi.ObjectFormat == PTP_OFC_Association) {
	// MTP use thesis object format for folders which means
	// these "files" will turn up on a folder listing instead.
	continue;
      }

      // Allocate a new file type
      file = LIBMTP_new_file_t();

      file->parent_id = oi.ParentObject;

      // Set the filetype
      file->filetype = map_ptp_type_to_libmtp_type(oi.ObjectFormat);

      // Original file-specific properties
      file->filesize = oi.ObjectCompressedSize;
      if (oi.Filename != NULL) {
	file->filename = strdup(oi.Filename);
      }

      // This is some sort of unique ID so we can keep track of the track.
      file->item_id = params->handles.Handler[i];

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

    } else {
      printf("LIBMTP panic: Found a bad handle, trying to ignore it.\n");
    }

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

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_file_t *file;
    PTPObjectInfo oi;

    // Is this the file we're looking for?
    if (params->handles.Handler[i] != fileid) {
      continue;
    }

    if (ptp_getobjectinfo(params, params->handles.Handler[i], &oi) == PTP_RC_OK) {

      if (oi.ObjectFormat == PTP_OFC_Association) {
	// MTP use thesis object format for folders which means
	// these "files" will turn up on a folder listing instead.
	return NULL;
      }

      // Allocate a new file type
      file = LIBMTP_new_file_t();

      file->parent_id = oi.ParentObject;

      // Set the filetype
      file->filetype = map_ptp_type_to_libmtp_type(oi.ObjectFormat);

      // Original file-specific properties
      file->filesize = oi.ObjectCompressedSize;
      if (oi.Filename != NULL) {
	file->filename = strdup(oi.Filename);
      }

      // This is some sort of unique ID so we can keep track of the track.
      file->item_id = params->handles.Handler[i];

      return file;
    } else {
      return NULL;
    }

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
  uint32_t i;
  uint16_t *props = NULL;
  uint32_t propcnt = 0;

  // First see which properties can be retrieved for this object format
  ret = ptp_mtp_getobjectpropssupported (params, map_libmtp_type_to_ptp_type(track->filetype), &propcnt, &props);
  if (ret != PTP_RC_OK) {
    // Just bail out for now, nothing is ever set.
    return;
  } else {
    for (i=0;i<propcnt;i++) {
      switch (props[i]) {
      case PTP_OPC_Name:
	track->title = LIBMTP_Get_String_From_Object(device, track->item_id, PTP_OPC_Name);
	break;
      case PTP_OPC_Artist:
	track->artist = LIBMTP_Get_String_From_Object(device, track->item_id, PTP_OPC_Artist);
	break;
      case PTP_OPC_Duration:
	track->duration = LIBMTP_Get_U32_From_Object(device, track->item_id, PTP_OPC_Duration, 0);
	break;
      case PTP_OPC_Track:
	track->tracknumber = LIBMTP_Get_U16_From_Object(device, track->item_id, PTP_OPC_Track, 0);
	break;
      case PTP_OPC_Genre:
	track->genre = LIBMTP_Get_String_From_Object(device, track->item_id, PTP_OPC_Genre);
	break;
      case PTP_OPC_AlbumName:
	track->album = LIBMTP_Get_String_From_Object(device, track->item_id, PTP_OPC_AlbumName);
	break;
      case PTP_OPC_OriginalReleaseDate:
	track->date = LIBMTP_Get_String_From_Object(device, track->item_id, PTP_OPC_OriginalReleaseDate);
	break;
	// These are, well not so important.
      case PTP_OPC_SampleRate:
	track->samplerate = LIBMTP_Get_U32_From_Object(device, track->item_id, PTP_OPC_SampleRate, 0);
	break;
      case PTP_OPC_NumberOfChannels:
	track->nochannels = LIBMTP_Get_U16_From_Object(device, track->item_id, PTP_OPC_NumberOfChannels, 0);
	break;
      case PTP_OPC_AudioWAVECodec:
	track->wavecodec = LIBMTP_Get_U32_From_Object(device, track->item_id, PTP_OPC_AudioWAVECodec, 0);
	break;
      case PTP_OPC_AudioBitRate:
	track->bitrate = LIBMTP_Get_U32_From_Object(device, track->item_id, PTP_OPC_AudioBitRate, 0);
	break;
      case PTP_OPC_BitRateType:
	track->bitratetype = LIBMTP_Get_U16_From_Object(device, track->item_id, PTP_OPC_BitRateType, 0);
	break;
      case PTP_OPC_Rating:
	track->rating = LIBMTP_Get_U16_From_Object(device, track->item_id, PTP_OPC_Rating, 0);
	break;
      case PTP_OPC_UseCount:
	track->usecount = LIBMTP_Get_U32_From_Object(device, track->item_id, PTP_OPC_UseCount, 0);
	break;
      }
    }
    free(props);
  }
}

/**
 * This returns a long list of all tracks available
 * on the current MTP device. Typical usage:
 *
 * <pre>
 * LIBMTP_track_t *tracklist;
 *
 * tracklist = LIBMTP_Get_Tracklisting(device);
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
 * @return a list of tracks that can be followed using the <code>next</code>
 *         field of the <code>LIBMTP_track_t</code> data structure.
 *         Each of the metadata tags must be freed after use, and may
 *         contain only partial metadata information, i.e. one or several
 *         fields may be NULL or 0.
 * @see LIBMTP_Get_Trackmetadata()
 */
LIBMTP_track_t *LIBMTP_Get_Tracklisting(LIBMTP_mtpdevice_t *device)
{
  uint32_t i = 0;
  LIBMTP_track_t *retracks = NULL;
  LIBMTP_track_t *curtrack = NULL;
  PTPParams *params = (PTPParams *) device->params;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_track_t *track;
    PTPObjectInfo oi;

    if (ptp_getobjectinfo(params, params->handles.Handler[i], &oi) == PTP_RC_OK) {

      // Ignore stuff we don't know how to handle...
      // TODO: get this list as an intersection of the sets
      // supported by the device and the from the device and
      // all known audio track files?
      if ( oi.ObjectFormat != PTP_OFC_WAV &&
	   oi.ObjectFormat != PTP_OFC_MP3 &&
	   oi.ObjectFormat != PTP_OFC_MTP_WMA &&
	   oi.ObjectFormat != PTP_OFC_MTP_OGG &&
	   oi.ObjectFormat != PTP_OFC_MTP_MP4 &&
	   oi.ObjectFormat != PTP_OFC_MTP_UndefinedAudio ) {
	// printf("Not a music track (format: %d), skipping...\n",oi.ObjectFormat);
	continue;
      }

      // Allocate a new track type
      track = LIBMTP_new_track_t();

      // This is some sort of unique ID so we can keep track of the track.
      track->item_id = params->handles.Handler[i];

      track->filetype = map_ptp_type_to_libmtp_type(oi.ObjectFormat);

      // Original file-specific properties
      track->filesize = oi.ObjectCompressedSize;
      if (oi.Filename != NULL) {
	track->filename = strdup(oi.Filename);
      }

      get_track_metadata(device, oi.ObjectFormat, track);

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

    } else {
      printf("LIBMTP panic: Found a bad handle, trying to ignore it.\n");
    }

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
    PTPObjectInfo oi;

    // Skip if this is not the track we want.
    if (params->handles.Handler[i] != trackid) {
      continue;
    }

    if (ptp_getobjectinfo(params, params->handles.Handler[i], &oi) == PTP_RC_OK) {
      LIBMTP_track_t *track;

      // Ignore stuff we don't know how to handle...
      if ( oi.ObjectFormat != PTP_OFC_WAV &&
	   oi.ObjectFormat != PTP_OFC_MP3 &&
	   oi.ObjectFormat != PTP_OFC_MTP_WMA &&
	   oi.ObjectFormat != PTP_OFC_MTP_OGG &&
	   oi.ObjectFormat != PTP_OFC_MTP_MP4 &&
	   oi.ObjectFormat != PTP_OFC_MTP_UndefinedAudio ) {
	return NULL;
      }

      // Allocate a new track type
      track = LIBMTP_new_track_t();

      // This is some sort of unique ID so we can keep track of the track.
      track->item_id = params->handles.Handler[i];

      track->filetype = map_ptp_type_to_libmtp_type(oi.ObjectFormat);

      // Original file-specific properties
      track->filesize = oi.ObjectCompressedSize;
      if (oi.Filename != NULL) {
	track->filename = strdup(oi.Filename);
      }

      get_track_metadata(device, oi.ObjectFormat, track);

      return track;

    } else {
      return NULL;
    }

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
    printf("LIBMTP_Get_File_To_File(): Bad arguments, path was NULL\n");
    return -1;
  }

  // Open file
#ifdef __WIN32__
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY,S_IRWXU|S_IRGRP)) == -1 ) {
#else
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRGRP)) == -1) {
#endif
    printf("LIBMTP_Get_File_To_File(): Could not create file \"%s\"\n", path);
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
  PTPObjectInfo oi;
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  if (ptp_getobjectinfo(params, id, &oi) != PTP_RC_OK) {
    printf("LIBMTP_Get_File_To_File_Descriptor(): Could not get object info\n");
    return -1;
  }
  if (oi.ObjectFormat == PTP_OFC_Association) {
    printf("LIBMTP_Get_File_To_File_Descriptor(): Bad object format\n");
    return -1;
  }

  // Callbacks
  ptp_usb->callback_active = 1;
  ptp_usb->current_transfer_total = oi.ObjectCompressedSize+
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
    printf("LIBMTP_Get_File_To_File_Descriptor(): Could not get file from device (%d)\n", ret);
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
    printf("LIBMTP_Send_Track_From_File(): Bad arguments, path was NULL\n");
    return -1;
  }

  // Open file
#ifdef __WIN32__
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1 ) {
#else
  if ( (fd = open(path, O_RDONLY)) == -1) {
#endif
    printf("LIBMTP_Send_Track_From_File(): Could not open source file \"%s\"\n", path);
    return -1;
  }

  ret = LIBMTP_Send_Track_From_File_Descriptor(device, fd, metadata, callback, data, parenthandle);

  // Close file.
  close(fd);

  return ret;
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
  uint16_t ret;
  uint32_t store = 0;
  int subcall_ret;
  PTPObjectInfo new_track;
  PTPParams *params = (PTPParams *) device->params;
  uint32_t localph = parenthandle;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  if (localph == 0) {
    localph = device->default_music_folder;
  }

  // Sanity check, is this really a track?
  if (metadata->filetype != LIBMTP_FILETYPE_WAV &&
      metadata->filetype != LIBMTP_FILETYPE_MP3 &&
      metadata->filetype != LIBMTP_FILETYPE_WMA &&
      metadata->filetype != LIBMTP_FILETYPE_OGG &&
      metadata->filetype != LIBMTP_FILETYPE_MP4 &&
      metadata->filetype != LIBMTP_FILETYPE_UNDEF_AUDIO) {
    printf("LIBMTP_Send_Track_From_File_Descriptor: I don't think this is actually a track, strange filetype...\n");
  }


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

  new_track.Filename = metadata->filename;
  new_track.ObjectCompressedSize = metadata->filesize;
  new_track.ObjectFormat = map_libmtp_type_to_ptp_type(metadata->filetype);

  // Create the object
  ret = ptp_sendobjectinfo(params, &store, &localph, &metadata->item_id, &new_track);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Send_Track_From_File_Descriptor: Could not send object info\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
  }

  // Callbacks
  ptp_usb->callback_active = 1;
  // The callback will deactivate itself after this amount of data has been sent
  // One BULK header for the request, one for the data phase. No parameters to the request.
  ptp_usb->current_transfer_total = metadata->filesize+PTP_USB_BULK_HDR_LEN*2;
  ptp_usb->current_transfer_complete = 0;
  ptp_usb->current_transfer_callback = callback;
  ptp_usb->current_transfer_callback_data = data;

  ret = ptp_sendobject_fromfd(params, fd, metadata->filesize);

  ptp_usb->callback_active = 0;
  ptp_usb->current_transfer_callback = NULL;
  ptp_usb->current_transfer_callback_data = NULL;

  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Send_Track_From_File_Descriptor: Could not send object\n");
    return -1;
  }

  // Set track metadata for the new fine track
  subcall_ret = LIBMTP_Update_Track_Metadata(device, metadata);
  if (subcall_ret != 0) {
    printf("LIBMTP_Send_Track_From_File_Descriptor: error setting metadata for new track\n");
    (void) LIBMTP_Delete_Object(device, metadata->item_id);
    return -1;
  }

  // Added object so flush handles
  flush_handles(device);

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
    printf("LIBMTP_Send_File_From_File(): Bad arguments, path was NULL\n");
    return -1;
  }

  // Open file
#ifdef __WIN32__
  if ( (fd = open(path, O_RDONLY|O_BINARY) == -1 ) {
#else
  if ( (fd = open(path, O_RDONLY)) == -1) {
#endif
    printf("LIBMTP_Send_File_From_File(): Could not open source file \"%s\"\n", path);
    return -1;
  }

  ret = LIBMTP_Send_File_From_File_Descriptor(device, fd, filedata, callback, data, parenthandle);

  // Close file.
  close(fd);

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
  uint32_t store = 0;
  uint32_t localph = parenthandle;
  PTPObjectInfo new_file;
  PTPParams *params = (PTPParams *) device->params;
  PTP_USB *ptp_usb = (PTP_USB*) device->usbinfo;

  new_file.Filename = filedata->filename;
  if (filedata->filesize == (uint64_t) -1) {
    // This is a stream. Set a dummy length...
    new_file.ObjectCompressedSize = 1;
  } else {
    new_file.ObjectCompressedSize = filedata->filesize;
  }
  new_file.ObjectFormat = map_libmtp_type_to_ptp_type(filedata->filetype);

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
    uint16_t of = new_file.ObjectFormat;
    if (of == PTP_OFC_WAV ||
	of == PTP_OFC_MP3 ||
	of == PTP_OFC_MTP_WMA ||
	of == PTP_OFC_MTP_OGG ||
	of == PTP_OFC_MTP_MP4 ||
	of == PTP_OFC_MTP_UndefinedAudio) {
      localph = device->default_music_folder;
    } else if (of == PTP_OFC_MTP_WMV ||
	       of == PTP_OFC_AVI ||
	       of == PTP_OFC_MPEG ||
	       of == PTP_OFC_ASF ||
	       of == PTP_OFC_QT ||
	       of == PTP_OFC_MTP_UndefinedVideo) {
      localph = device->default_video_folder;
    } else if (of == PTP_OFC_EXIF_JPEG ||
	       of == PTP_OFC_JFIF ||
	       of == PTP_OFC_TIFF ||
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
    }
  }

  // Create the object
  ret = ptp_sendobjectinfo(params, &store, &localph, &filedata->item_id, &new_file);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Send_File_From_File_Descriptor: Could not send object info\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
  }

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
      printf("LIBMTP_Send_File_From_File_Descriptor: Error while sending stream.\n");
      printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    }
  }

  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Send_File_From_File_Descriptor: Could not send object\n");
    return -1;
  }

  // Added object so flush handles.
  flush_handles(device);
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
 * @return 0 on success, any other value means failure.
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
  ret = ptp_mtp_getobjectpropssupported (params, map_libmtp_type_to_ptp_type(metadata->filetype), &propcnt, &props);
  if (ret != PTP_RC_OK) {
    // Just bail out for now, nothing is ever set.
    return -1;
  } else {
    for (i=0;i<propcnt;i++) {
      switch (props[i]) {
      case PTP_OPC_Name:
	// Update title
	ret = LIBMTP_Set_Object_String(device, metadata->item_id, PTP_OPC_Name, metadata->title);
	if (ret != 0) {
	  printf("LIBMTP_Update_Track_Metadata(): could not set track title\n");
	  return -1;
	}
	break;
      case PTP_OPC_AlbumName:
	// Update album
	ret = LIBMTP_Set_Object_String(device, metadata->item_id, PTP_OPC_AlbumName, metadata->album);
	if (ret != 0) {
	  printf("LIBMTP_Update_Track_Metadata(): could not set track album name\n");
	  return -1;
	}
	break;
      case PTP_OPC_Artist:
	// Update artist
	ret = LIBMTP_Set_Object_String(device, metadata->item_id, PTP_OPC_Artist, metadata->artist);
	if (ret != 0) {
	  printf("LIBMTP_Update_Track_Metadata(): could not set track artist name\n");
	  return -1;
	}
	break;
      case PTP_OPC_Genre:
	// Update genre
	ret = LIBMTP_Set_Object_String(device, metadata->item_id, PTP_OPC_Genre, metadata->genre);
	if (ret != 0) {
	  printf("LIBMTP_Update_Track_Metadata(): could not set track genre name\n");
	  return -1;
	}
	break;
      case PTP_OPC_Duration:
	// Update duration
	if (metadata->duration != 0) {
	  ret = LIBMTP_Set_Object_U32(device, metadata->item_id, PTP_OPC_Duration, metadata->duration);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set track duration\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_Track:
	// Update track number.
	if (metadata->tracknumber != 0) {
	  ret = LIBMTP_Set_Object_U16(device, metadata->item_id, PTP_OPC_Track, metadata->tracknumber);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set track tracknumber\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_OriginalReleaseDate:
	// Update creation datetime
	ret = LIBMTP_Set_Object_String(device, metadata->item_id, PTP_OPC_OriginalReleaseDate, metadata->date);
	if (ret != 0) {
	  printf("LIBMTP_Update_Track_Metadata(): could not set track release date\n");
	  return -1;
	}
	break;
      // These are, well not so important.
      case PTP_OPC_SampleRate:
	// Update sample rate
	if (metadata->samplerate != 0) {
	  ret = LIBMTP_Set_Object_U32(device, metadata->item_id, PTP_OPC_SampleRate, metadata->samplerate);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set samplerate\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_NumberOfChannels:
	// Update number of channels
	if (metadata->nochannels != 0) {
	  ret = LIBMTP_Set_Object_U16(device, metadata->item_id, PTP_OPC_NumberOfChannels, metadata->nochannels);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set number of channels\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_AudioWAVECodec:
	// Update WAVE codec
	if (metadata->wavecodec != 0) {
	  ret = LIBMTP_Set_Object_U32(device, metadata->item_id, PTP_OPC_AudioWAVECodec, metadata->wavecodec);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set WAVE codec\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_AudioBitRate:
	// Update bitrate
	if (metadata->bitrate != 0) {
	  ret = LIBMTP_Set_Object_U32(device, metadata->item_id, PTP_OPC_AudioBitRate, metadata->bitrate);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set bitrate\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_BitRateType:
	// Update bitrate type
	if (metadata->bitratetype != 0) {
	  ret = LIBMTP_Set_Object_U16(device, metadata->item_id, PTP_OPC_BitRateType, metadata->bitratetype);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set bitratetype\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_Rating:
	// Update user rating
	// TODO: shall this be set for rating 0?
	if (metadata->rating != 0) {
	  ret = LIBMTP_Set_Object_U16(device, metadata->item_id, PTP_OPC_Rating, metadata->rating);
	  if (ret != 0) {
	    printf("LIBMTP_Update_Track_Metadata(): could not set user rating\n");
	    return -1;
	  }
	}
	break;
      case PTP_OPC_UseCount:
	// Update use count, set even to zero if desired.
	ret = LIBMTP_Set_Object_U32(device, metadata->item_id, PTP_OPC_UseCount, metadata->usecount);
	if (ret != 0) {
	  printf("LIBMTP_Update_Track_Metadata(): could not set use count\n");
	  return -1;
	}
	break;

	// NOTE: File size is not updated, this should not change anyway.
	// neither will we change the filename.
      }
    }
    return 0;
  }
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

  ret = ptp_deleteobject(params, object_id, 0);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Delete_Object(): could not delete object\n");
    return -1;
  }
  // Removed object so flush handles.
  flush_handles(device);
  return 0;
}

/**
 * Helper function. This indicates if a track exists on the device
 * @param device a pointer to the device to get the track from.
 * @param id the track ID of the track to retrieve.
 * @return TRUE (1) if the track exists, FALSE (0) if not
 */
int LIBMTP_Track_Exists(LIBMTP_mtpdevice_t *device,
           uint32_t const id)
{
  PTPObjectInfo oi;
  PTPParams *params = (PTPParams *) device->params;

  if (ptp_getobjectinfo(params, id, &oi) == PTP_RC_OK) {
    return -1;
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
 *
 * This deletes the memory for an object structure
 * and makes use of the registered destructor for the object
 * type data.
 *
 * @param object object structure to destroy
 * @param recurse indicate if the call should recursively delete
 * the object. Specify 1 for recursion.
 * @see LIBMTP_new_object_t()
 */
void LIBMTP_destroy_object_t(LIBMTP_object_t *object, uint32_t recursive)
{
  if(object == NULL) {
    return;
  }

  //Destroy from the bottom up
  if(recursive==1) {
    LIBMTP_destroy_object_t(object->child, recursive);
    object->child = NULL;
    LIBMTP_destroy_object_t(object->sibling, recursive);
    object->sibling = NULL;
  }

  if(object->name != NULL) free(object->name);

  //Use the data type destructor
  if(object->data != NULL) {
    void (*destructor)(void *);

    destructor = get_destructor(object->type);

    if(destructor != NULL) {
      (*destructor)(object->data);
    }
    object->data = NULL;
  }

  free(object);
}

/**
 * This recursively deletes the memory for a folder structure
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
    PTPObjectInfo oi;

    if (ptp_getobjectinfo(params, params->handles.Handler[i], &oi) == PTP_RC_OK) {
      if (oi.ObjectFormat != PTP_OFC_Association) {
        continue;
      }
      folder = LIBMTP_new_folder_t();
      folder->folder_id = params->handles.Handler[i];
      folder->parent_id = oi.ParentObject;
      folder->name = (char *)strdup(oi.Filename);

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
  uint32_t store = 0;
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
  ret = ptp_sendobjectinfo(params, &store, &parenthandle, &new_id, &new_folder);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_Create_Folder: Could not send object info\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return 0;
  }
  // Created new object so flush handles
  flush_handles(device);
  return new_id;
}


/**
 * Helper function. Returns a folder structure for a
 * specified id.
 *
 * @param objectlist list of objects to search
 * @id id of object to look for
 * @return a object or NULL if not found
 */
LIBMTP_object_t *LIBMTP_Find_Object(LIBMTP_object_t *objectlist, uint32_t id)
{
  LIBMTP_object_t *ret = NULL;

  if(objectlist == NULL) {
    return NULL;
  }

  if(objectlist->id == id) {
    return objectlist;
  }

  if(objectlist->sibling) {
    ret = LIBMTP_Find_Object(objectlist->sibling, id);
  }

  if(objectlist->child && ret == NULL) {
    ret = LIBMTP_Find_Object(objectlist->child, id);
  }

  return ret;
}

/**
 * This returns a list of objects on the current MTP device,
 * selected by a filter based on PTP object ID:s.
 *
 * @param device a pointer to the device to get the object listing for.
 * @param filter array of unsigned 32-bit integers specifying which types
 *        to include in the list
 * @param filter_len length of filter array in 32-bit words
 * @param exclusions array of unsigned 32-bit integers specifying which types
 *        to exclude from the list
 * @param exclusion_len length of exclusion array
 * @return a list of objects
 * @see LIBMTP_destroy_object_t()
 */
LIBMTP_object_t *LIBMTP_Make_List(LIBMTP_mtpdevice_t *device, uint32_t *filter,
				  uint32_t filter_len, uint32_t *exclusions, uint32_t exclusion_len)
{
  uint32_t i = 0;
  LIBMTP_object_t *objectlist = NULL;
  PTPParams *params = (PTPParams *) device->params;
  uint32_t max_exclusions = 0;
  uint32_t max_filter = 0;

  // Get all the handles if we haven't already done that
  if (params->handles.Handler == NULL) {
    flush_handles(device);
  }

  if(filter != NULL) max_filter = filter_len;
  if(exclusions != NULL) max_exclusions = exclusion_len;

  for (i = 0; i < params->handles.n; i++) {
    LIBMTP_object_t *object;
    PTPObjectInfo oi;

    if (ptp_getobjectinfo(params, params->handles.Handler[i], &oi) == PTP_RC_OK) {
      uint32_t x = 0;
      uint32_t exclude = 0, filter_allow = 0;
      void (*datafunc)(LIBMTP_mtpdevice_t *, uint32_t, void *);
      void *(*constructor)(void);

      // Is the ObjectFormat in the list of exclusions ?
      for(x = 0; x < max_exclusions; x++) {
	if (oi.ObjectFormat == exclusions[x]) {
	  exclude = 1;
	  break;
	}
      }
      if(exclude == 1) {
	continue;
      }

      // Is the ObjectFormat in the filter ?
      for(x = 0; x < max_filter; x++) {
	if (oi.ObjectFormat == filter[x]) {
	  filter_allow = 1;
	  break;
	}
      }
      if(filter_allow == 0) {
	continue;
      }

      object = LIBMTP_new_object_t();
      object->id = params->handles.Handler[i];
      object->parent = oi.ParentObject;
      object->name = (char *)strdup(oi.Filename);
      object->size = oi.ObjectCompressedSize;
      object->type = oi.ObjectFormat;

      // Get the function pointers for the constructor and datafunc
      constructor = get_constructor(oi.ObjectFormat);
      datafunc = get_datafunc(oi.ObjectFormat);

      if(constructor != NULL) {
	object->data = (*constructor)();
	if(datafunc != NULL) {
	  (*datafunc)(device, object->id, object->data);
	}
      }

      // Work out where to put this new item
      if(objectlist == NULL) {
        objectlist = object;
        continue;
      } else {
        LIBMTP_object_t *parent_object;
        LIBMTP_object_t *current_object;

        parent_object = LIBMTP_Find_Object(objectlist, object->parent);

        if(parent_object == NULL) {
	  current_object = objectlist;
        } else {
          if(parent_object->child == NULL) {
            parent_object->child = object;
            continue;
          } else {
            current_object = parent_object->child;
          }
	}

        while(current_object->sibling != NULL) {
          current_object=current_object->sibling;
        }
        current_object->sibling = object;
      }
    }
  }

  return objectlist;
}

/**
 * Debug function that dumps out some textual representation
 * of an object list.
 *
 * @param list object list returned from LIBMTP_Make_List
 *
 * @see LIBMTP_Make_List()
 */
void LIBMTP_Dump_List(LIBMTP_object_t *list)
{
  if(list == NULL) return;

  printf("Id    : %u\n", list->id);
  printf("Parent: %u\n", list->parent);
  printf("Size  : %u\n", list->size);
  printf("Name  : %s\n", (list->name ? list->name : ""));
  printf("Type  : 0x%04x\n", list->type);
  printf("--\n");

  LIBMTP_Dump_List(list->child);
  LIBMTP_Dump_List(list->sibling);
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
    PTPObjectInfo oi;
    uint16_t ret;

    ret = ptp_getobjectinfo(params, params->handles.Handler[i], &oi);
    if ( ret == PTP_RC_OK) {

      // Ignore stuff that isn't playlists
      if ( oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioVideoPlaylist ) {
	continue;
      }

      // Allocate a new playlist type
      pl = LIBMTP_new_playlist_t();

      // Ignoring the io.Filename field.
      pl->name = LIBMTP_Get_String_From_Object(device, params->handles.Handler[i], PTP_OPC_Name);

      // This is some sort of unique playlist ID so we can keep track of it
      pl->playlist_id = params->handles.Handler[i];

      // Then get the track listing for this playlist
      ret = ptp_mtp_getobjectreferences(params, pl->playlist_id, &pl->tracks, &pl->no_tracks);
      if (ret != PTP_RC_OK) {
	printf("LIBMTP_Get_Playlist: Could not get object references\n");
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

    } else {
      printf("LIBMTP panic: Found a bad handle, trying to ignore it.\n");
    }
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
    PTPObjectInfo oi;
    uint16_t ret;

    if (params->handles.Handler[i] != plid) {
      continue;
    }

    ret = ptp_getobjectinfo(params, params->handles.Handler[i], &oi);
    if ( ret == PTP_RC_OK) {

      // Ignore stuff that isn't playlists
      if ( oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioVideoPlaylist ) {
	return NULL;
      }

      // Allocate a new playlist type
      pl = LIBMTP_new_playlist_t();

      // Ignoring the io.Filename field.
      pl->name = LIBMTP_Get_String_From_Object(device, params->handles.Handler[i], PTP_OPC_Name);

      // This is some sort of unique playlist ID so we can keep track of it
      pl->playlist_id = params->handles.Handler[i];

      // Then get the track listing for this playlist
      ret = ptp_mtp_getobjectreferences(params, pl->playlist_id, &pl->tracks, &pl->no_tracks);
      if (ret != PTP_RC_OK) {
	printf("LIBMTP_Get_Playlist: Could not get object references\n");
	pl->tracks = NULL;
	pl->no_tracks = 0;
      }

      return pl;
    } else {
      return NULL;
    }
  }
  return NULL;
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
  uint16_t ret;
  uint32_t store = 0;
  PTPObjectInfo new_pl;
  PTPParams *params = (PTPParams *) device->params;
  uint32_t localph = parenthandle;
  char fname[256];
  uint8_t data[2];

  // Use a default folder if none given
  if (localph == 0) {
    localph = device->default_playlist_folder;
  }

  // .zpl is the "abstract audio/video playlist "file" suffix
  new_pl.Filename = NULL;
  if (strlen(metadata->name) > 4) {
    char *suff = &metadata->name[strlen(metadata->name)-4];
    if (!strcmp(suff, ".zpl")) {
      // Home free.
      new_pl.Filename = metadata->name;
    }
  }
  // If it didn't end with ".zpl" then add that here.
  if (new_pl.Filename == NULL) {
    strncpy(fname, metadata->name, sizeof(fname)-5);
    strcat(fname, ".zpl");
    fname[sizeof(fname)-1] = '\0';
    new_pl.Filename = fname;
  }

  // Playlists created on device have size (uint32_t) -1 = 0xFFFFFFFFU, but setting:
  // new_pl.ObjectCompressedSize = 0; <- DOES NOT WORK! (return PTP_RC_GeneralError)
  // new_pl.ObjectCompressedSize = (uint32_t) -1; <- DOES NOT WORK! (return PTP_RC_MTP_Object_Too_Large)
  new_pl.ObjectCompressedSize = 1;
  new_pl.ObjectFormat = PTP_OFC_MTP_AbstractAudioVideoPlaylist;

  // Create the object
  ret = ptp_sendobjectinfo(params, &store, &localph, &metadata->playlist_id, &new_pl);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_New_Playlist(): Could not send object info (the playlist itself)\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
  }

  /*
   * We have to send this one blank data byte.
   * If we don't, the handle will not be created and thus there is no playlist.
   */
  data[0] = '\0';
  data[1] = '\0';
  ret = ptp_sendobject(params, data, 1);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_New_Playlist(): Could not send blank object data\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
  }

  // Update title
  ret = LIBMTP_Set_Object_String(device, metadata->playlist_id, PTP_OPC_Name, metadata->name);
  if (ret != 0) {
    printf("LIBMTP_New_Playlist(): could not set playlist name\n");
    return -1;
  }

  if (metadata->no_tracks > 0) {
    // Add tracks to the new playlist as object references.
    ret = ptp_mtp_setobjectreferences (params, metadata->playlist_id, metadata->tracks, metadata->no_tracks);
    if (ret != PTP_RC_OK) {
      printf("LIBMTP_New_Playlist(): could not add tracks as object references\n");
      printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
      return -1;
    }
  }

  // Created new item, so flush handles
  flush_handles(device);

  return 0;
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
  ret = LIBMTP_Set_Object_String(device, metadata->playlist_id, PTP_OPC_Name, metadata->name);
  if (ret != 0) {
    printf("LIBMTP_Update_Playlist(): could not set playlist name\n");
    return -1;
  }

  if (metadata->no_tracks > 0) {
    // Add tracks to the new playlist as object references.
    ret = ptp_mtp_setobjectreferences (params, metadata->playlist_id, metadata->tracks, metadata->no_tracks);
    if (ret != PTP_RC_OK) {
      printf("LIBMTP_Update_Playlist(): could not add tracks as object references\n");
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
    PTPObjectInfo oi;
    uint16_t ret;

    ret = ptp_getobjectinfo(params, params->handles.Handler[i], &oi);
    if ( ret == PTP_RC_OK) {

      // Ignore stuff that isn't an album
      if ( oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioAlbum ) {
        continue;
      }

      // Allocate a new album type
      alb = LIBMTP_new_album_t();
      alb->name = LIBMTP_Get_String_From_Object(device, params->handles.Handler[i], PTP_OPC_Name);
      alb->album_id = params->handles.Handler[i];

      // Then get the track listing for this album
      ret = ptp_mtp_getobjectreferences(params, alb->album_id, &alb->tracks, &alb->no_tracks);
      if (ret != PTP_RC_OK) {
        printf("LIBMTP_Get_Album: Could not get object references\n");
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

    } else {
      printf("LIBMTP panic: Found a bad handle, trying to ignore it.\n");
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
    PTPObjectInfo oi;
    uint16_t ret;

    if (params->handles.Handler[i] != albid) {
      continue;
    }

    ret = ptp_getobjectinfo(params, params->handles.Handler[i], &oi);
    if ( ret == PTP_RC_OK) {

      // Ignore stuff that isn't an album
      if ( oi.ObjectFormat != PTP_OFC_MTP_AbstractAudioAlbum ) {
        return NULL;
      }

      // Allocate a new album type
      alb = LIBMTP_new_album_t();
      alb->name = LIBMTP_Get_String_From_Object(device, params->handles.Handler[i], PTP_OPC_Name);
      alb->album_id = params->handles.Handler[i];
      ret = ptp_mtp_getobjectreferences(params, alb->album_id, &alb->tracks, &alb->no_tracks);
      if (ret != PTP_RC_OK) {
        printf("LIBMTP_Get_Album: Could not get object references\n");
        alb->tracks = NULL;
        alb->no_tracks = 0;
      }

      return alb;
    } else {
      return NULL;
    }
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
  uint16_t ret;
  uint32_t store = 0;
  PTPObjectInfo new_alb;
  PTPParams *params = (PTPParams *) device->params;
  uint32_t localph = parenthandle;
  char fname[256];
  uint8_t data[1];

  // check we can create an object of type PTP_OFC_MTP_AbstractAudioAlbum
  int i;
  int supported = 0;
  for ( i=0; i < params->deviceinfo.ImageFormats_len; i++ ) {
    if (params->deviceinfo.ImageFormats[i] == PTP_OFC_MTP_AbstractAudioAlbum)
      supported = 1;
  }
  if (!supported) {
    printf("LIBMTP_Create_New_Album(): Player does not support the AbstractAudioAlbum type\n");
    return -1;
  }

  // Use a default folder if none given
  if (localph == 0) {
    localph = device->default_music_folder;
  }

  new_alb.Filename = NULL;
  if (strlen(metadata->name) > 4) {
    char *suff = &metadata->name[strlen(metadata->name)-4];
    if (!strcmp(suff, ".alb")) {
      new_alb.Filename = metadata->name;
    }
  }
  // If it didn't end with ".alb" then add that here.
  if (new_alb.Filename == NULL) {
    strncpy(fname, metadata->name, sizeof(fname)-5);
    strcat(fname, ".alb");
    fname[sizeof(fname)-1] = '\0';
    new_alb.Filename = fname;
  }

  new_alb.ObjectCompressedSize = 1;
  new_alb.ObjectFormat = PTP_OFC_MTP_AbstractAudioAlbum;

  // Create the object
  ret = ptp_sendobjectinfo(params, &store, &localph, &metadata->album_id, &new_alb);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_New_Album(): Could not send object info (the album itself)\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
  }
  data[0] = '\0';
  ret = ptp_sendobject(params, data, 1);
  if (ret != PTP_RC_OK) {
    ptp_perror(params, ret);
    printf("LIBMTP_New_Album(): Could not send blank object data\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
  }

  // Update title
  ret = LIBMTP_Set_Object_String(device, metadata->album_id, PTP_OPC_Name, metadata->name);
  if (ret != 0) {
    printf("LIBMTP_New_Album(): could not set album name\n");
    return -1;
  }

  if (metadata->no_tracks > 0) {
    // Add tracks to the new album as object references.
    ret = ptp_mtp_setobjectreferences (params, metadata->album_id, metadata->tracks, metadata->no_tracks);
    if (ret != PTP_RC_OK) {
      printf("LIBMTP_New_Album(): could not add tracks as object references\n");
      printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
      return -1;
    }
  }

  // Created new item, so flush handles
  flush_handles(device);

  return 0;
}

/**
 * This routine sends cover art for an album object. This uses the
 * RepresentativeSampleData property of the album, if the device
 * supports it. The data should be of a format acceptable to the
 * player (for iRiver and Creative, this seems to be JPEG) and
 * must not be too large. (for a Creative, max seems to be about 20KB.)
 * TODO: there must be a way to find the max size for an ObjectPropertyValue.
 * @param device a pointer to the device which the album is on.
 * @param id unique id of the album object.
 * @param imagedata pointer to an array of uint8_t containing the image data.
 * @param imagesize number of bytes in the image.
 * @return 0 on success, any other value means failure.
 * @see LIBMTP_Create_New_Album()
 */
int LIBMTP_Send_Album_Art(LIBMTP_mtpdevice_t *device,
                          uint32_t const id,
                          uint8_t * const imagedata,
                          uint32_t const imagesize)
{
  uint16_t ret;
  PTPParams *params = (PTPParams *) device->params;
  PTPPropertyValue propval;

  int i;
  propval.a.count = imagesize;
  propval.a.v = malloc(sizeof(PTPPropertyValue) * imagesize);
  for (i = 0; i < imagesize; i++) {
    propval.a.v[i].u8 = imagedata[i];
  }

  // check that we can send album art
  uint16_t *props = NULL;
  uint32_t propcnt = 0;
  ret = ptp_mtp_getobjectpropssupported(params, PTP_OFC_MTP_AbstractAudioAlbum, &propcnt, &props);
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Send_Album_Art(): could not get object properties\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
  }
  int supported = 0;
  for (i = 0; i < propcnt; i++) {
    if (props[i] == PTP_OPC_RepresentativeSampleData)
      supported = 1;
  }
  if (!supported) {
    printf("LIBMTP_Send_Album_Art(): device doesn't support RepresentativeSampleData\n");
    return -1;
  }

  // go ahead and send the data
  ret = ptp_mtp_setobjectpropvalue(params,id,PTP_OPC_RepresentativeSampleData,
                            &propval,PTP_DTC_AUINT8);
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Send_Album_Art(): could not send album art\n");
    printf("Return code: 0x%04x (look this up in ptp.h for an explanation).\n",  ret);
    return -1;
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

  // Update title
  ret = LIBMTP_Set_Object_String(device, metadata->album_id, PTP_OPC_Name, metadata->name);
  if (ret != 0) {
    printf("LIBMTP_Update_Album(): could not set album name\n");
    return -1;
  }

  if (metadata->no_tracks > 0) {
    // Add tracks to the new album as object references.
    ret = ptp_mtp_setobjectreferences (params, metadata->album_id, metadata->tracks, metadata->no_tracks);
    if (ret != PTP_RC_OK) {
      printf("LIBMTP_Update_Album(): could not add tracks as object references\n");
      return -1;
    }
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
