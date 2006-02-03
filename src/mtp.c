/*
 *  mtp.c
 *
 *  Created by Richard Low on 26/12/2005.
 *
 * This adds the MTP commands as the spec v0.83
 * MTP protocol is Copyright (C) Microsoft Corporation 2005
 *
 */

#include "ptp.h"
#include "mtp.h"
#include "ptp-pack.h"
#include "mtp-pack.h"
#include <stdlib.h>
#include <string.h>

uint16_t
ptp_getobjectpropvalue (PTPParams* params, uint16_t propcode, uint32_t handle,
												void** value, uint16_t datatype)
{
	PTPContainer ptp;
	uint16_t ret;
	char* dpv=NULL;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectPropValue;
	ptp.Param1=handle;
	ptp.Param2=propcode;
	ptp.Nparam=2;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpv);
	if (ret == PTP_RC_OK) ptp_unpack_DPV(params, dpv, value, datatype);
	free(dpv);
	return ret;
}

uint16_t
ptp_setobjectpropvalue (PTPParams* params, uint16_t propcode, uint32_t handle,
												void* value, uint16_t datatype)
{
	PTPContainer ptp;
	uint16_t ret;
	uint32_t size;
	char* dpv=NULL;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SetObjectPropValue;
	ptp.Param1=handle;
	ptp.Param2=propcode;
	ptp.Nparam=2;
	size=ptp_pack_DPV(params, value, &dpv, datatype);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &dpv);
	free(dpv);
	return ret;
}

uint16_t
ptp_getobjectpropssupported (PTPParams* params, uint32_t objectformatcode, uint16_t** opcArray, uint32_t* arraylen)
{
	PTPContainer ptp;
	uint16_t ret;
	char* dpv=NULL;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectPropsSupported;
	ptp.Param1=objectformatcode;
	ptp.Nparam=1;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpv);
	if (ret == PTP_RC_OK) ptp_unpack_array(params, dpv, (void**)opcArray, PTP_DTC_AUINT16, arraylen);
	free(dpv);
	return ret;
}

/* this is completely untested */
uint16_t
ptp_getobjectpropdesc (PTPParams* params, uint16_t propcode, uint32_t objectformatcode,
											 PTPObjectPropDesc* objectpropertydesc)
{
	PTPContainer ptp;
	uint16_t ret;
	char* opd=NULL;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectPropDesc;
	ptp.Param1=propcode;
	ptp.Param2=objectformatcode;
	ptp.Nparam=2;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &opd);
	if (ret == PTP_RC_OK) ptp_unpack_OPD(params, opd, objectpropertydesc);
	free(opd);
	return ret;
}

uint16_t
ptp_getobjectreferences (PTPParams* params, uint32_t handle, uint32_t** ohArray, uint32_t* arraylen)
{
	PTPContainer ptp;
	uint16_t ret;
	char* dpv=NULL;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_GetObjectReferences;
	ptp.Param1=handle;
	ptp.Nparam=1;
	ret=ptp_transaction(params, &ptp, PTP_DP_GETDATA, 0, &dpv);
	if (ret == PTP_RC_OK) ptp_unpack_array(params, dpv, (void**)ohArray, PTP_DTC_AUINT32, arraylen);
	free(dpv);
	return ret;
}

uint16_t
ptp_setobjectreferences (PTPParams* params, uint32_t handle, uint32_t* ohArray, uint32_t arraylen)
{
	PTPContainer ptp;
	uint16_t ret;
	uint32_t size;
	char* dpv=NULL;
	
	PTP_CNT_INIT(ptp);
	ptp.Code=PTP_OC_SetObjectReferences;
	ptp.Param1=handle;
	ptp.Nparam=1;
	size=ptp_pack_array(params, ohArray, &dpv, PTP_DTC_AUINT32, arraylen);
	ret=ptp_transaction(params, &ptp, PTP_DP_SENDDATA, size, &dpv);
	free(dpv);
	return ret;
}
