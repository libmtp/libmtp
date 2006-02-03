/*
 *  mtp.h
 *
 *  Created by Richard Low on 26/12/2005.
 *
 */

#ifndef __MTP_H__
#define __MTP_H__

#include "ptp.h"

/* MTP operation codes */

#define PTP_OC_MTPUndefined 0x9800 
#define PTP_OC_GetObjectPropsSupported 0x9801 
#define PTP_OC_GetObjectPropDesc 0x9802 
#define PTP_OC_GetObjectPropValue 0x9803 
#define PTP_OC_SetObjectPropValue 0x9804 
#define PTP_OC_GetObjectReferences 0x9810 
#define PTP_OC_SetObjectReferences 0x9811

/* MTP Object Format types */

#define PTP_OFC_UndefinedFirmware 0xB802
#define PTP_OFC_WindowsImageFormat 0xB881
#define PTP_OFC_UndefinedAudio 0xB900
#define PTP_OFC_WMA 0xB901
#define PTP_OFC_OGG 0xB902
#define PTP_OFC_UndefinedVideo 0xB980
#define PTP_OFC_WMV 0xB981
#define PTP_OFC_MP4 0xB982
#define PTP_OFC_UndefinedCollection 0xBA00
#define PTP_OFC_AbstractMultimediaAlbum 0xBA01
#define PTP_OFC_AbstractImageAlbum 0xBA02
#define PTP_OFC_AbstractAudioAlbum 0xBA03
#define PTP_OFC_AbstractVideoAlbum 0xBA04
#define PTP_OFC_AbstractAudioVideoPlaylist 0xBA05
#define PTP_OFC_AbstractContactGroup 0xBA06
#define PTP_OFC_AbstractMessageFolder 0xBA07
#define PTP_OFC_AbstractChapteredProduction 0xBA08
#define PTP_OFC_WPLPlaylist 0xBA10
#define PTP_OFC_M3UPlaylist 0xBA11
#define PTP_OFC_MPLPlaylist 0xBA12
#define PTP_OFC_ASXPlaylist 0xBA13
#define PTP_OFC_PLSPlaylist 0xBA14
#define PTP_OFC_UndefinedDocument 0xBA80
#define PTP_OFC_AbstractDocument 0xBA81
#define PTP_OFC_UndefinedMessage 0xBB00
#define PTP_OFC_AbstractMessage 0xBB01
#define PTP_OFC_UndefinedContact 0xBB80
#define PTP_OFC_AbstractContact 0xBB81
#define PTP_OFC_vCard2 0xBB82
#define PTP_OFC_vCard3 0xBB83
#define PTP_OFC_UndefinedCalenderItem 0xBE00
#define PTP_OFC_AbstractCalenderItem 0xBE01
#define PTP_OFC_vCalendar1 0xBE02
#define PTP_OFC_vCalendar2 0xBE03
#define PTP_OFC_UndefinedWindowsExecutable 0xBE80

/* Device Property Form Flag */

#define PTP_DPFF_DateTime	0x03 
#define PTP_DPFF_FixedLengthArray	0x04 
#define PTP_DPFF_RegularExpression	0x05 
#define PTP_DPFF_ByteArray	0x06 
#define PTP_DPFF_LongString	0xFF 

/* MTP Event codes */

#define PTP_EC_MTPUndefined 0xB800 
#define PTP_EC_ObjectPropChanged 0xB801 
#define PTP_EC_ObjectPropDescChanged 0xB802 
#define PTP_EC_ObjectReferencesChanged 0xB803 
#define PTP_EC_DevicePropDescChanged 0xB804

/* MTP Responses */

#define PTP_RC_MTPUndefined 0xA800 
#define PTP_RC_Invalid_ObjectPropCode 0xA801 
#define PTP_RC_Invalid_ObjectProp_Format 0xA802 
#define PTP_RC_Invalid_ObjectProp_Value 0xA803 
#define PTP_RC_Invalid_ObjectReference 0xA804 
#define PTP_RC_Invalid_Dataset 0xA806 
#define PTP_RC_Specification_By_Group_Unsupported 0xA808 
#define PTP_RC_Object_Too_Large 0xA809 

struct _PTPObjPropDescFixedLengthArrayForm {
	uint16_t	Length;
};
typedef struct _PTPObjPropDescFixedLengthArrayForm PTPObjPropDescFixedLengthArrayForm;

struct _PTPObjPropDescRegularExpressionForm {
  uint16_t*	RegEx;
};
typedef struct _PTPObjPropDescRegularExpressionForm PTPObjPropDescRegularExpressionForm;

struct _PTPObjPropDescByteArrayForm {
	uint16_t	MaxLength;
};
typedef struct _PTPObjPropDescByteArrayForm PTPObjPropDescByteArrayForm;

struct _PTPObjPropDescLongStringForm {
	uint16_t	MaxLength;
};
typedef struct _PTPObjPropDescLongStringForm PTPObjPropDescLongStringForm;

/* Object Property Describing Dataset (ObjectPropDesc) */

struct _PTPObjectPropDesc {
	uint16_t	PropertyCode;
	uint16_t	DataType;
	uint8_t		GetSet;
	void *		DefaultValue;
	uint32_t	GroupCode;
	uint8_t		FormFlag;
	union	{
		PTPObjPropDescEnumForm	Enum;
		PTPObjPropDescRangeForm	Range;
		PTPObjPropDescFixedLengthArrayForm Array;
		PTPObjPropDescRegularExpressionForm RegularExpression;
		PTPObjPropDescByteArrayForm ByteArray;
		PTPObjPropDescLongStringForm LongString;
	} FORM;
};
typedef struct _PTPObjectPropDesc PTPObjectPropDesc;

/* MTP Device property codes */

#define PTP_DPC_SynchronizationPartner	0xD401
#define PTP_DPC_DeviceFriendlyName	0xD402

/* MTP object property codes */

#define PTP_OPC_StorageID	0xDC01
#define PTP_OPC_ObjectFormat	0xDC02 
#define PTP_OPC_ProtectionStatus	0xDC03 
#define PTP_OPC_ObjectSize	0xDC04 
#define PTP_OPC_AssociationType	0xDC05 
#define PTP_OPC_AssociationDesc	0xDC06 
#define PTP_OPC_ObjectFileName	0xDC07 
#define PTP_OPC_DateCreated	0xDC08 
#define PTP_OPC_DateModified	0xDC09 
#define PTP_OPC_Keywords	0xDC0A 
#define PTP_OPC_ParentObject	0xDC0B 
#define PTP_OPC_PersistentUniqueObjectIdentifier	0xDC41 
#define PTP_OPC_SyncID	0xDC42 
#define PTP_OPC_PropertyBag	0xDC43 
#define PTP_OPC_Name	0xDC44 
#define PTP_OPC_CreatedBy	0xDC45 
#define PTP_OPC_Artist	0xDC46 
#define PTP_OPC_DateAuthored	0xDC47 
#define PTP_OPC_Description	0xDC48 
#define PTP_OPC_URLReference	0xDC49 
#define PTP_OPC_LanguageLocale	0xDC4A 
#define PTP_OPC_CopyrightInformation	0xDC4B 
#define PTP_OPC_Source	0xDC4C 
#define PTP_OPC_OriginLocation	0xDC4D 
#define PTP_OPC_DateAdded	0xDC4E 
#define PTP_OPC_NonConsumable	0xDC4F 
#define PTP_OPC_CorruptUnplayable	0xDC50 
#define PTP_OPC_RepresentativeSampleFormat	0xDC81 
#define PTP_OPC_RepresentativeSampleSize	0xDC82 
#define PTP_OPC_RepresentativeSampleHeight	0xDC83 
#define PTP_OPC_RepresentativeSampleWidth	0xDC84 
#define PTP_OPC_RepresentativeSampleDuration	0xDC85 
#define PTP_OPC_RepresentativeSampleData	0xDC86 
#define PTP_OPC_Width	0xDC87 
#define PTP_OPC_Height	0xDC88 
#define PTP_OPC_Duration	0xDC89 
#define PTP_OPC_Rating	0xDC8A 
#define PTP_OPC_Track	0xDC8B 
#define PTP_OPC_Genre	0xDC8C 
#define PTP_OPC_Credits 
#define PTP_OPC_Lyrics	0xDC8E 
#define PTP_OPC_SubscriptionContentID	0xDC8F 
#define PTP_OPC_ProducedBy	0xDC90 
#define PTP_OPC_UseCount	0xDC91 
#define PTP_OPC_SkipCount	0xDC92 
#define PTP_OPC_LastAccessed	0xDC93 
#define PTP_OPC_ParentalRating	0xDC94 
#define PTP_OPC_MetaGenre	0xDC95 
#define PTP_OPC_Composer	0xDC96 
#define PTP_OPC_EffectiveRating	0xDC97 
#define PTP_OPC_Subtitle	0xDC98 
#define PTP_OPC_OriginalReleaseDate	0xDC99 
#define PTP_OPC_AlbumName	0xDC9A 
#define PTP_OPC_AlbumArtist	0xDC9B 
#define PTP_OPC_Mood	0xDC9C 
#define PTP_OPC_DRMStatus	0xDC9D 
#define PTP_OPC_SubDescription	0xDC9E 
#define PTP_OPC_IsCropped	0xDCD1 
#define PTP_OPC_IsColourCorrected	0xDCD2 
#define PTP_OPC_TotalBitRate	0xDE91 
#define PTP_OPC_BitrateType	0xDE92 
#define PTP_OPC_SampleRate	0xDE93 
#define PTP_OPC_NumberOfChannels	0xDE94 
#define PTP_OPC_AudioBitDepth	0xDE95 
#define PTP_OPC_ScanType	0xDE97 
#define PTP_OPC_AudioWAVECodec	0xDE99 
#define PTP_OPC_AudioBitRate	0xDE9A 
#define PTP_OPC_VideoFourCCCodec	0xDE9B 
#define PTP_OPC_VideoBitRate	0xDE9C 
#define PTP_OPC_FramesPerThousandSeconds	0xDE9D 
#define PTP_OPC_KeyFrameDistance	0xDE9E 
#define PTP_OPC_BufferSize	0xDE9F 
#define PTP_OPC_EncodingQuality	0xDEA0

uint16_t ptp_getobjectpropvalue (PTPParams* params, uint16_t propcode, uint32_t handle,
																 void** value, uint16_t datatype);
uint16_t ptp_setobjectpropvalue (PTPParams* params, uint16_t propcode, uint32_t handle,
																 void* value, uint16_t datatype);

uint16_t ptp_getobjectpropssupported (PTPParams* params, uint32_t ofc, uint16_t** opcArray, uint32_t* arraylen);

uint16_t ptp_getobjectreferences (PTPParams* params, uint32_t handle, uint32_t** ohArray, uint32_t* arraylen);
uint16_t ptp_setobjectreferences (PTPParams* params, uint32_t handle, uint32_t* ohArray, uint32_t arraylen);

#endif /* __MTP_H__ */
