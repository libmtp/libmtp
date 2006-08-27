#include "common.h"

#define PTP_OPC_StorageID               0xDC01
#define PTP_OPC_ObjectFormat                0xDC02
#define PTP_OPC_ProtectionStatus            0xDC03
#define PTP_OPC_ObjectSize              0xDC04
#define PTP_OPC_AssociationType             0xDC05
#define PTP_OPC_AssociationDesc             0xDC06
#define PTP_OPC_ObjectFileName              0xDC07
#define PTP_OPC_DateCreated             0xDC08
#define PTP_OPC_DateModified                0xDC09
#define PTP_OPC_Keywords                0xDC0A
#define PTP_OPC_ParentObject                0xDC0B
#define PTP_OPC_PersistantUniqueObjectIdentifier    0xDC41
#define PTP_OPC_SyncID                  0xDC42
#define PTP_OPC_PropertyBag             0xDC43
#define PTP_OPC_Name                    0xDC44
#define PTP_OPC_CreatedBy               0xDC45
#define PTP_OPC_Artist                  0xDC46
#define PTP_OPC_DateAuthored                0xDC47
#define PTP_OPC_Description             0xDC48
#define PTP_OPC_URLReference                0xDC49
#define PTP_OPC_LanguageLocale              0xDC4A
#define PTP_OPC_CopyrightInformation            0xDC4B
#define PTP_OPC_Source                  0xDC4C
#define PTP_OPC_OriginLocation              0xDC4D
#define PTP_OPC_DateAdded               0xDC4E
#define PTP_OPC_NonConsumable               0xDC4F
#define PTP_OPC_CorruptOrUnplayable         0xDC50
#define PTP_OPC_RepresentativeSampleFormat      0xDC81
#define PTP_OPC_RepresentativeSampleSize        0xDC82
#define PTP_OPC_RepresentativeSampleHeight      0xDC83
#define PTP_OPC_RepresentativeSampleWidth       0xDC84
#define PTP_OPC_RepresentativeSampleDuration        0xDC85
#define PTP_OPC_RepresentativeSampleData        0xDC86
#define PTP_OPC_Width                   0xDC87
#define PTP_OPC_Height                  0xDC88
#define PTP_OPC_Duration                0xDC89
#define PTP_OPC_Rating                  0xDC8A
#define PTP_OPC_Track                   0xDC8B
#define PTP_OPC_Genre                   0xDC8C
#define PTP_OPC_Credits                 0xDC8D
#define PTP_OPC_Lyrics                  0xDC8E
#define PTP_OPC_SubscriptionContentID           0xDC8F
#define PTP_OPC_ProducedBy              0xDC90
#define PTP_OPC_UseCount                0xDC91
#define PTP_OPC_SkipCount               0xDC92
#define PTP_OPC_LastAccessed                0xDC93
#define PTP_OPC_ParentalRating              0xDC94
#define PTP_OPC_MetaGenre               0xDC95
#define PTP_OPC_Composer                0xDC96
#define PTP_OPC_EffectiveRating             0xDC97
#define PTP_OPC_Subtitle                0xDC98
#define PTP_OPC_OriginalReleaseDate         0xDC99
#define PTP_OPC_AlbumName               0xDC9A

static void test_mp3_datafunc(LIBMTP_mtpdevice_t *device, uint32_t object_id, void *data)
{
  LIBMTP_track_t *track;
  
  if(data == NULL) return;
  
  track = (LIBMTP_track_t *) data;
  
  track->title = LIBMTP_Get_String_From_Object(device, object_id, PTP_OPC_Name);
  track->artist = LIBMTP_Get_String_From_Object(device, object_id, PTP_OPC_Artist);
  track->duration = LIBMTP_Get_U32_From_Object(device, object_id, PTP_OPC_Duration, 0);
  track->duration = LIBMTP_Get_U16_From_Object(device, object_id, PTP_OPC_Track, 0);
  track->artist = LIBMTP_Get_String_From_Object(device, object_id, PTP_OPC_Genre);
  track->album = LIBMTP_Get_String_From_Object(device, object_id, PTP_OPC_AlbumName);
  track->date = LIBMTP_Get_String_From_Object(device, object_id, PTP_OPC_OriginalReleaseDate);
}

int main (int argc, char **argv)
{
  LIBMTP_mtpdevice_t *device;
  LIBMTP_object_t *list = NULL;
  uint32_t filter[] = {0x3001, 0x3009};

  LIBMTP_Init();

  device = LIBMTP_Get_First_Device();
  if (device == NULL) {
    printf("No devices.\n");
    exit (0);
  }
  
  LIBMTP_Set_Datafunc(LIBMTP_FILETYPE_MP3, test_mp3_datafunc);

  list = LIBMTP_Make_List(device, filter, sizeof(filter)/sizeof(uint32_t), NULL, 0);

  LIBMTP_Dump_List(list);

  LIBMTP_destroy_object_t(list, 1);

  LIBMTP_Release_Device(device);

  printf("OK.\n");
  exit (0);
}

