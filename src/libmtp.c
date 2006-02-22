#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "libmtp.h"
#include "unicode.h"
#include "ptp.h"
#include "ptp-pack.h"
#include "libptp-endian.h"

/**
 * Initialize the library.
 */
void LIBMTP_Init(void)
{
  return;
}

/**
 * Get the first connected MTP device.
 * @return a device pointer.
 */
LIBMTP_mtpdevice_t *LIBMTP_Get_First_Device(void)
{
  uint8_t interface_number;
  PTPParams *params;
  PTP_USB *ptp_usb;
  PTPStorageIDs storageIDs;
  unsigned storageID = 0;
  PTPDevicePropDesc dpd;
  uint8_t batteryLevelMax = 100;
  uint16_t ret;
  LIBMTP_mtpdevice_t *tmpdevice;

  // Allocate a parameter block
  params = (PTPParams *) malloc(sizeof(PTPParams));
  ptp_usb = (PTP_USB *) malloc(sizeof(PTP_USB));
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
  
  if (ptp_getdeviceinfo(params, &params->deviceinfo) == PTP_RC_OK) {
    printf("Model: %s\n", params->deviceinfo.Model);
    printf("Serial number: %s\n", params->deviceinfo.SerialNumber);
    printf("Device version: %s\n", params->deviceinfo.DeviceVersion);
  } else {
    goto error_handler;
  }
  
  // Get battery maximum level
  if (ptp_getdevicepropdesc(params, PTP_DPC_BatteryLevel, &dpd) != PTP_RC_OK) {
    printf("Unable to retrieve battery max level.\n");
    goto error_handler;
  }
  // if is NULL, just leave as default
  if (dpd.FORM.Range.MaximumValue != NULL) {
    batteryLevelMax = *(uint8_t *)dpd.FORM.Range.MaximumValue;
    printf("Maximum battery level: %d\n", batteryLevelMax);
  }
  ptp_free_devicepropdesc(&dpd);

  // OK everything got this far, so it is time to create a device struct!
  tmpdevice = (LIBMTP_mtpdevice_t *) malloc(sizeof(LIBMTP_mtpdevice_t));
  tmpdevice->interface_number = interface_number;
  tmpdevice->params = params;
  tmpdevice->ptp_usb = ptp_usb;
  tmpdevice->storage_id = storageID;
  tmpdevice->maximum_battery_level = batteryLevelMax;

  return tmpdevice;
  
  // Then close it again.
 error_handler:
  close_device(ptp_usb, params, interface_number);
  ptp_free_deviceinfo(&params->deviceinfo);
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
  close_device(device->ptp_usb, device->params, device->interface_number);
  // Free the device info and any handler
  ptp_free_deviceinfo(&device->params->deviceinfo);
  if (device->params->handles.Handler != NULL) {
    free(device->params->handles.Handler);
  }
  free(device);
}

/**
 * This retrieves the owners name of an MTP device.
 * @param device a pointer to the device to get the owner for.
 * @return a newly allocated UTF-8 string representing the owner. 
 *         The string must be freed by the caller after use.
 */
char *LIBMTP_Get_Ownername(LIBMTP_mtpdevice_t *device)
{
  uint16_t *unistring = NULL;
  char *retstring = NULL;

  if (ptp_getdevicepropvalue(device->params, 
			     PTP_DPC_DeviceFriendlyName, 
			     (void **) &unistring, 
			     PTP_DTC_UNISTR) != PTP_RC_OK) {
    return NULL;
  }
  // Convert from UTF-16 to UTF-8
  retstring = ucs2_to_utf8(unistring);
  free(unistring);
  return retstring;
}

/**
 * This creates a new track metadata structure and allocates memory
 * for it. Notice that if you add strings to this structure they
 * will be freed by the corresponding <code>LIBMTP_destroy_track_t</code>
 * operation later, so be careful of using strdup() when assigning 
 * strings, e.g.:
 *
 * <code>
 * LIBMTP_track_t *track = LIBMTP_new_track_t();
 * track->title = strdup(titlestr);
 * ....
 * LIBMTP_destroy_track_t(track);
 * </code>
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
  new->codec = LIBMTP_CODEC_UNKNOWN;
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
 * This returns a long list of all tracks available
 * on the current MTP device.
 * @param device a pointer to the device to get the track listing for.
 */
LIBMTP_track_t *LIBMTP_Get_Tracklisting(LIBMTP_mtpdevice_t *device)
{
  uint32_t i = 0;
  LIBMTP_track_t *retracks = NULL;
  LIBMTP_track_t *curtrack = NULL;
  
  printf("Getting handles\n");
  if (device->params->handles.Handler == NULL) {
    // Get all the handles if we haven't already done that
    if (ptp_getobjecthandles(device->params,
			     PTP_GOH_ALL_STORAGE, 
			     PTP_GOH_ALL_FORMATS, 
			     PTP_GOH_ALL_ASSOCS, 
			     &device->params->handles) != PTP_RC_OK) {
      printf("LIBMTP panic: Could not get object handles...\n");
      return NULL;
    }
  }
  
  for (i = 0; i < device->params->handles.n; i++) {

    LIBMTP_track_t *track;
    PTPObjectInfo oi;
    int ret;
    char *stringvalue = NULL;
    unsigned short *unicodevalue = NULL;
    uint16_t *uint16value = NULL;
    uint32_t *uint32value = NULL;

    if (ptp_getobjectinfo(device->params, device->params->handles.Handler[i], &oi) == PTP_RC_OK) {
      
      // Ignore stuff we don't know how to handle...
      if (oi.ObjectFormat == PTP_OFC_Association || 
	  (oi.ObjectFormat != PTP_OFC_WAV && 
	   oi.ObjectFormat != PTP_OFC_MP3 && 
	   oi.ObjectFormat != PTP_OFC_WMA)) {
	printf("Unknown ObjectFormat (%d), skipping...\n",oi.ObjectFormat);
	continue;
      }
      
      // Allocate a new track type
      track = LIBMTP_new_track_t();

      switch (oi.ObjectFormat)
	{
	case PTP_OFC_WAV:
	  track->codec = LIBMTP_CODEC_WAV;
	  break;
	case PTP_OFC_MP3:
	  track->codec = LIBMTP_CODEC_MP3;
	  break;
	case PTP_OFC_WMA:
	  track->codec = LIBMTP_CODEC_WMA;
	  break;
	default:
	  track->codec = LIBMTP_CODEC_UNKNOWN;
	}

      // Original file-specific properties
      track->filesize = oi.ObjectCompressedSize;
      if (oi.Filename != NULL) {
	track->filename = strdup(oi.Filename);
	printf("Filename: %s\n", track->filename);
      }

      ret = ptp_getobjectpropvalue(device->params, PTP_OPC_Name, 
				   device->params->handles.Handler[i], 
				   (void**) &unicodevalue,
				   PTP_DTC_UNISTR);
      if (ret == PTP_RC_OK && unicodevalue != NULL) {
	printf("Getting unicode rep\n");
	track->title = ucs2_to_utf8(unicodevalue);
	free(unicodevalue);
	unicodevalue = NULL;
      }
      
      ret = ptp_getobjectpropvalue(device->params, 
				   PTP_OPC_Artist, 
				   device->params->handles.Handler[i], 
				   (void**) &unicodevalue, 
				   PTP_DTC_UNISTR);
      if (ret == PTP_RC_OK && unicodevalue != NULL) {
	track->artist = ucs2_to_utf8(unicodevalue);
	free(unicodevalue);
	unicodevalue = NULL;
      }
      
      ret = ptp_getobjectpropvalue(device->params, 
				   PTP_OPC_Duration, 
				   device->params->handles.Handler[i], 
				   (void**) &uint32value, 
				   PTP_DTC_UINT32);
      if (ret == PTP_RC_OK && uint32value != NULL) {
	track->duration = *uint32value;
	free(uint32value);
	uint32value = NULL;
      }
      
      ret = ptp_getobjectpropvalue(device->params, 
				   PTP_OPC_Track, 
				   device->params->handles.Handler[i], 
				   (void**) &uint16value, 
				   PTP_DTC_UINT16);
      if (ret == PTP_RC_OK && uint16value != NULL) {
	track->tracknumber = *uint16value;
	free(uint16value);
	uint16value = NULL;
      }
      
      ret = ptp_getobjectpropvalue(device->params, 
				   PTP_OPC_Genre, 
				   device->params->handles.Handler[i], 
				   (void**) &unicodevalue, 
				   PTP_DTC_UNISTR);
      if (ret == PTP_RC_OK && unicodevalue != NULL) {
	track->genre = ucs2_to_utf8(unicodevalue);
	free(unicodevalue);
	unicodevalue = NULL;
      }
      
      ret = ptp_getobjectpropvalue(device->params, 
				   PTP_OPC_AlbumName, 
				   device->params->handles.Handler[i], 
				   (void**) &unicodevalue, 
				   PTP_DTC_UNISTR);
      if (ret == PTP_RC_OK && unicodevalue != NULL) {
	track->album = ucs2_to_utf8(unicodevalue);
	free(unicodevalue);
	unicodevalue = NULL;
      }
      
      ret = ptp_getobjectpropvalue(device->params, 
				   PTP_OPC_OriginalReleaseDate, 
				   device->params->handles.Handler[i], 
				   (void**) &stringvalue, 
				   PTP_DTC_STR);
      if (ret == PTP_RC_OK && stringvalue != NULL) {
	track->date = strdup(stringvalue);
	free(stringvalue);
	stringvalue = NULL;
      }
      
      // This is some sort of unique ID so we can keep track of the track.
      track->item_id = device->params->handles.Handler[i];
      
      // Add track to a list that will be returned afterwards.
      if (retracks == NULL) {
	retracks = track;
	curtrack = track;
      } else {
	curtrack->next = track;
	curtrack = track;
      }
      
      // Call listing callback
      // double progressPercent = (double)i*(double)100.0 / (double)device->params->handles.n;

    } else {
      printf("LIBMTP panic: Found a bad handle, trying to ignore it.\n");
    }

  } // Handle counting loop
  return retracks;
}

/**
 * This gets a track off the device to a file identified
 * by a filename.
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
			 char const * const path, LIBMTP_progressfunc_t const * const callback,
			 void const * const data)
{
  int fd = -1;
  int ret;

  // Sanity check
  if (path == NULL) {
    printf("LIBMTP_Get_Track_To_File(): Bad arguments, path was NULL\n");
    return -1;
  }

  // Open file
#ifdef __WIN32__
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC|O_BINARY,S_IRWXU|S_IRGRP) == -1 ) {
#else
  if ( (fd = open(path, O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRGRP)) == -1) {
#endif
    printf("LIBMTP_Get_Track_To_File(): Could not create file \"%s\"\n", path);
    return -1;
  }
  
  ret = LIBMTP_Get_Track_To_File_Descriptor(device, id, fd, callback, data);

  // Close file
  close(fd);
  
  return ret;
}

/**
 * This gets a track off the device to a file identified
 * by a file descriptor.
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
					LIBMTP_progressfunc_t const * const callback,
					void const * const data)
{
  PTPObjectInfo oi;
  void *image;
  int ret;
  // Map this to the LIBMTP callback type
  extern Progress_Callback* globalCallback;
  
  // Not yet compatible.
  globalCallback = NULL;

  if (ptp_getobjectinfo(device->params, id, &oi) != PTP_RC_OK) {
    printf("LIBMTP_Get_Track_To_File_Descriptor(): Could not get object info\n");
    return -1;
  }
  if (oi.ObjectFormat == PTP_OFC_Association) {
    printf("LIBMTP_Get_Track_To_File_Descriptor(): Bad object format\n");
    return -1;
  }
  // Seek to end of file and write a blank so that it is created with the
  // correct size and all.
  lseek(fd, oi.ObjectCompressedSize-1, SEEK_SET);
  write(fd, "", 1);

  // MAP_SHARED, MAP_PRIVATE
  image = mmap(0, oi.ObjectCompressedSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (image == MAP_FAILED) {
    printf("LIBMTP_Get_Track_To_File_Descriptor(): Could not map file to memory\n");
    return -1;
  }
  // Flush the file to disk.
  fflush(NULL);
  
  // Copy object to memory
  ret = ptp_getobject(device->params, id, (char **) &image);

  // Spool out to file
  munmap(image, oi.ObjectCompressedSize);
  
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Get_Track_To_File_Descriptor(): Could not get file from device\n");
    return -1;
  }

  return 0;
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
 * @return 0 if the transfer was successful, any other value means 
 *           failure.
 * @see LIBMTP_Send_Track_From_File_Descriptor()
 */
int LIBMTP_Send_Track_From_File(LIBMTP_mtpdevice_t *device, 
			 char const * const path, LIBMTP_track_t * const metadata,
                         LIBMTP_progressfunc_t const * const callback,
			 void const * const data)
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
    printf("LIBMTP_Get_Track_To_File(): Could not open source file \"%s\"\n", path);
    return -1;
  }

  ret = LIBMTP_Send_Track_From_File_Descriptor(device, fd, metadata, callback, data);
  
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
 * @return 0 if the transfer was successful, any other value means 
 *           failure.
 * @see LIBMTP_Send_Track_From_File()
 */
int LIBMTP_Send_Track_From_File_Descriptor(LIBMTP_mtpdevice_t *device, 
			 int const fd, LIBMTP_track_t * const metadata,
                         LIBMTP_progressfunc_t const * const callback,
			 void const * const data)
{
  uint32_t parenthandle = 0;
  uint16_t ret;
  uint32_t store = 0;
  uint8_t *buffer;
  uint64_t remain;
  PTPObjectInfo new_track;
  PTPContainer ptp;
  PTPUSBBulkContainerSend usbdata;
  
  switch (metadata->codec) {
  case LIBMTP_CODEC_WAV:
    new_track.ObjectFormat = PTP_OFC_WAV;
    break;
  case LIBMTP_CODEC_MP3:
    new_track.ObjectFormat = PTP_OFC_MP3;
    break;
  case LIBMTP_CODEC_WMA:
    new_track.ObjectFormat = PTP_OFC_WMA;
    break;
  default:
    printf("LIBMTP_Send_Track_From_File_Descriptor: unknown codec.\n");
    new_track.ObjectFormat = PTP_OFC_Undefined;
  }

  new_track.Filename = metadata->filename;
  new_track.ObjectCompressedSize = metadata->filesize;

  // Create the object
  ret = ptp_sendobjectinfo(device->params, &store, &parenthandle, &metadata->item_id, &new_track);
  if (ret != PTP_RC_OK) {
    ptp_perror(device->params, ret);
    printf("LIBMTP_Send_Track_From_File_Descriptor: Could not send object info\n");
    return -1;
  }

  // Nullify and configure PTP container
  memset(&ptp, 0, sizeof(ptp));
  ptp.Code = PTP_OC_SendObject;
  ptp.Nparam = 0;
  ptp.Transaction_ID = device->params->transaction_id++;
  ptp.SessionID = device->params->session_id;
  // Send request to send an object
  ret = device->params->sendreq_func(device->params, &ptp);
  if (ret != PTP_RC_OK) {
    ptp_perror(device->params, ret);
    printf("LIBMTP_Send_Track_From_File_Descriptor: Could not send \"send object\" request\n");
    return -1;
  }
  
  // build appropriate USB container
  usbdata.length = htod32p(device->params,sizeof(usbdata) + metadata->filesize);
  usbdata.type = htod16p(device->params,PTP_USB_CONTAINER_DATA);
  usbdata.code = htod16p(device->params,PTP_OC_SendObject);
  usbdata.trans_id = htod32p(device->params,ptp.Transaction_ID);
  // Write request to USB
  ret = device->params->write_func((unsigned char *)&usbdata, sizeof(usbdata), device->params->data);
  if (ret != PTP_RC_OK) {
    printf("LIBMTP_Send_Track_From_File_Descriptor: Error initializing object sending\n");
    ptp_perror(device->params, ret);
    return -1;
  }
	
  // This space will be used as a reading ring buffer for the transfers
  buffer = (uint8_t *) malloc(BLOCK_SIZE);
  if (buffer == NULL) {
    printf("LIBMTP_Send_Track_From_File_Descriptor: Could not allocate send buffer\n");
    return -1;
  }
	
  remain = metadata->filesize;
  while (remain != 0) {
    int readsize = (remain > BLOCK_SIZE) ? BLOCK_SIZE : (int) remain;
    int bytesdone = (int) (metadata->filesize - remain);
    
    if (read(fd, buffer, readsize) < readsize) {
      printf("LIBMTP_Send_Track_From_File_Descriptor: error reading source file\n");
      free(buffer);
      return -1;
    }
    
    if (callback != NULL) {
      // If the callback return anything else than 0, interrupt the processing
      int callret = callback(bytesdone, metadata->filesize, buffer, readsize, data);
      if (callret != 0) {
	printf("LIBMTP_Send_Track_From_File_Descriptor: transfer interrupted by callback\n");
	free(buffer);
	return -1;
      }
    }
    
    // Write to USB
    ret = device->params->write_func(buffer, readsize, device->params->data);
    if (ret != PTP_RC_OK) {
      printf("LIBMTP_Send_Track_From_File_Descriptor: error writing data chunk to object\n");
      ptp_perror(device->params, ret);
      free(buffer);
      return -1;
    }
    remain -= BLOCK_SIZE;
  }
  
  if (callback != NULL) {
    // This last call will not be able to abort execution and is just
    // done do progress bars go up to 100%
    (void) callback(metadata->filesize, metadata->filesize, NULL, 0, data);
  }
  
  // Signal to USB that this is the last transfer if the last chunk
  // was exactly as large as the buffer
  if (metadata->filesize % MTP_DEVICE_BUF_SIZE == 0) {
    ret = device->params->write_func(NULL, 0, device->params->data);
    if (ret!=PTP_RC_OK) {
      printf("LIBMTP_Send_Track_From_File_Descriptor: error writing last zerolen data chunk for USB termination\n");
      ptp_perror(device->params, ret);
      free(buffer);
      return -1;
    }
  }

  // Get a response from device to make sure that the track was properly stored
  ret = device->params->getresp_func(device->params, &ptp);
  if (ret!=PTP_RC_OK) {
    printf("LIBMTP_Send_Track_From_File_Descriptor: error getting response from device\n");
    ptp_perror(device->params, ret);
    free(buffer);
    return -1;
  }
  
  // Free allocated buffer and return
  free(buffer);
  return 0;
}
