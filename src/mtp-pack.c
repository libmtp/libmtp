/*
 *  mtp-pack.c
 *
 *  Created by Richard Low on 26/12/2005.
 *
 * This adds some extra packing routines
 *
 */

#include "mtp-pack.h"
#include <stdlib.h>

#define PTP_opd_PropertyCode	0
#define PTP_opd_DataType	sizeof(uint16_t)
#define PTP_opd_GetSet	PTP_opd_DataType+sizeof(uint16_t)
#define PTP_opd_DefaultValue	PTP_opd_GetSet+sizeof(uint8_t)

/* this is completely untested */
inline void
ptp_unpack_OPD (PTPParams *params, char* data, PTPObjectPropDesc *opd)
{
	int totallen=0;
	uint16_t i=0;
	
	opd->PropertyCode=dtoh16a(&data[PTP_opd_PropertyCode]);
	opd->DataType=dtoh16a(&data[PTP_opd_DataType]);
	opd->GetSet=dtoh8a(&data[PTP_opd_GetSet]);
	totallen = ptp_unpack_DPV(params, &data[PTP_opd_DefaultValue], &(opd->DefaultValue), opd->DataType);
	
	/* if totallen==0 then Data Type format is not supported by this
		code or the Data Type is a string (with two empty strings as
																			 values). In both cases Form Flag should be set to 0x00 and FORM is
		not present. */
	opd->FormFlag=PTP_DPFF_None;
	if (totallen==0) return;
	
	opd->GroupCode=dtoh32a(&data[PTP_opd_DefaultValue+totallen]);
	totallen+=sizeof(uint32_t);
	opd->FormFlag=dtoh8a(&data[PTP_opd_DefaultValue+totallen]);
	totallen+=sizeof(uint8_t);
	switch (opd->FormFlag) {
		case PTP_DPFF_Range:
			totallen+=ptp_unpack_DPV(params, &data[PTP_opd_DefaultValue+totallen], &(opd->FORM.Range.MinimumValue), opd->DataType);
			totallen+=ptp_unpack_DPV(params, &data[PTP_opd_DefaultValue+totallen], &(opd->FORM.Range.MaximumValue), opd->DataType);
			totallen+=ptp_unpack_DPV(params, &data[PTP_opd_DefaultValue+totallen], &(opd->FORM.Range.StepSize), opd->DataType);
			break;
		case PTP_DPFF_Enumeration:
#define N	opd->FORM.Enum.NumberOfValues
			N = dtoh16a(&data[PTP_opd_DefaultValue+totallen]);
			totallen+=sizeof(uint16_t);
			opd->FORM.Enum.SupportedValue = malloc(N*sizeof(void *));
			
			for (i=0; i < N; i++)
			{
				totallen+=ptp_unpack_DPV(params, &data[PTP_opd_DefaultValue+totallen], &(opd->FORM.Enum.SupportedValue[i]), opd->DataType);
			}
			
			break;
		case PTP_DPFF_DateTime:
			/* no extra fields, so do nothing */
			break;
		case PTP_DPFF_FixedLengthArray:
			opd->FORM.Array.Length=dtoh16a(&data[PTP_opd_DefaultValue+totallen]);
			break;
		case PTP_DPFF_RegularExpression:
			ptp_unpack_DPV(params, &data[PTP_opd_DefaultValue+totallen], (void**)&(opd->FORM.RegularExpression.RegEx), PTP_DTC_UNISTR);
			break;
		case PTP_DPFF_ByteArray:
			opd->FORM.ByteArray.MaxLength=dtoh16a(&data[PTP_opd_DefaultValue+totallen]);
			break;
		case PTP_DPFF_LongString:
			opd->FORM.LongString.MaxLength=dtoh16a(&data[PTP_opd_DefaultValue+totallen]);
			break;
	}
}