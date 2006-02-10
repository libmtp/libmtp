#include "libmtp.h"

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
mtpdevice_t *LIBMTP_Get_First_Device(void)
{
  uint8_t interface_number;
  PTPParams *params;
  PTP_USB *ptp_usb;
  PTPStorageIDs storageIDs;
  unsigned storageID = 0;
  PTPDevicePropDesc dpd;
  uint8_t batteryLevelMax = 100;
  uint16_t ret;
  mtpdevice_t *tmpdevice;

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
  tmpdevice = (mtpdevice_t *) malloc(sizeof(mtpdevice_t));
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
 */
void LIBMTP_Release_Device(mtpdevice_t *device)
{
  close_device(device->ptp_usb, device->params, &device->interface_number);
  // Free the device info and any handler
  ptp_free_deviceinfo(&device->params->deviceinfo);
  if (device->params->handles.Handler != NULL) {
    free(device->params->handles.Handler);
  }
  free(device);
}
