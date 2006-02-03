/*
 *  ptp-pack.h
 *
 *  Created by Richard Low on 26/12/2005.
 *
 */

#ifndef __PTPPACK_H__ 
#define __PTPPACK_H__ 

#define htod8a(a,x)	*(uint8_t*)(a) = x
#define htod16a(a,x)	htod16ap(params,a,x)
#define htod32a(a,x)	htod32ap(params,a,x)
#define htod16(x)	htod16p(params,x)
#define htod32(x)	htod32p(params,x)

#define dtoh8a(x)	(*(uint8_t*)(x))
#define dtoh16a(a)	dtoh16ap(params,a)
#define dtoh32a(a)	dtoh32ap(params,a)
#define dtoh64a(a)	dtoh64ap(params,a)
#define dtoh16(x)	dtoh16p(params,x)
#define dtoh32(x)	dtoh32p(params,x)

#include "ptp.h"

inline uint16_t htod16p (PTPParams *params, uint16_t var);
inline uint32_t htod32p (PTPParams *params, uint32_t var);
inline void htod16ap (PTPParams *params, unsigned char *a, uint16_t val);
inline void htod32ap (PTPParams *params, unsigned char *a, uint32_t val);
inline uint16_t dtoh16p (PTPParams *params, uint16_t var);
inline uint32_t dtoh32p (PTPParams *params, uint32_t var);
inline uint16_t dtoh16ap (PTPParams *params, unsigned char *a);
inline uint32_t dtoh32ap (PTPParams *params, unsigned char *a);
inline uint64_t dtoh64ap (PTPParams *params, unsigned char *a);
inline char* ptp_unpack_string(PTPParams *params, char* data, uint16_t offset, uint8_t *len);
inline void ptp_pack_string(PTPParams *params, char *string, char* data, uint16_t offset, uint8_t *len);
inline uint32_t ptp_unpack_uint32_t_array(PTPParams *params, char* data, uint16_t offset, uint32_t **array);
inline uint32_t ptp_unpack_uint16_t_array(PTPParams *params, char* data, uint16_t offset, uint16_t **array);
inline void ptp_unpack_DI (PTPParams *params, char* data, PTPDeviceInfo *di);
inline void ptp_unpack_OH (PTPParams *params, char* data, PTPObjectHandles *oh);
inline void ptp_unpack_SIDs (PTPParams *params, char* data, PTPStorageIDs *sids);
inline void ptp_unpack_SI (PTPParams *params, char* data, PTPStorageInfo *si);
inline uint32_t ptp_pack_OI (PTPParams *params, PTPObjectInfo *oi, char** oidataptr);
inline void ptp_unpack_OI (PTPParams *params, char* data, PTPObjectInfo *oi);
inline uint32_t ptp_unpack_DPV (PTPParams *params, char* data, void** value, uint16_t datatype);
inline void ptp_unpack_DPD (PTPParams *params, char* data, PTPDevicePropDesc *dpd);
inline uint32_t ptp_pack_DPV (PTPParams *params, void* value, char** dpvptr, uint16_t datatype);
inline void ptp_unpack_EC (PTPParams *params, char* data, PTPUSBEventContainer *ec);
inline void ptp_unpack_Canon_FE (PTPParams *params, char* data, PTPCANONFolderEntry *fe);

inline uint32_t ptp_unpack_array (PTPParams *params, char* data, void** value, uint16_t datatype, uint32_t* arraylen);
inline uint32_t ptp_pack_array (PTPParams *params, void* value, char** dpvptr, uint16_t datatype, uint32_t arraylen);
#endif /* __PTPPACK_H__ */
