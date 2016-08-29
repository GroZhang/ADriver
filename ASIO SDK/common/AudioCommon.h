
#ifndef _USBAUDIOCOMMON_H_
#define _USBAUDIOCOMMON_H_

#define NUM_USB_CHAN_IN   (2)         /* Device to Host */
#define NUM_USB_CHAN_OUT  (2)         /* Host to Device */

#pragma pack(1)
//TODO Audio2 only?
typedef struct
{
	UCHAR  bLength;             /* Size of  descriptor (bytes) */
	UCHAR  bDescriptorType;
	UCHAR  bDescriptorSubtype;
	USHORT bcdADC;              /* Binary coded decimal indicating the supported Audio Class version */
	UCHAR  bCatagory;           /* Primary use of this audio function. See Audio Function Category Codes */
	USHORT wTotalLength;        /* Total length of the Audio class-specific descriptors, including this descriptor */
	UCHAR  bmControls;         /* D[1:0]: Latency control. D[7:2]: Reserved. Must be set to 0 */
}  UAC_Descriptor_Interface_AC_t;

/* Table 4-9: Input Terminal Descriptor */
typedef struct
{
	UCHAR  bLength;             /* Size of the descriptor (bytes) */
	UCHAR  bDescriptorType;
	UCHAR  bDescriptorSubtype;
	UCHAR  bTerminalID;         /* Unique ID of this terminal unit */
	USHORT wTerminalType;
	UCHAR  bAssocTerminal;      /* ID of associated output terminal, for physically grouped terminals
										* such as the speaker and microphone of a phone handset */
	UCHAR  bCSourceID;          /* ID of the clock entity to which this Input Terminal is connected */
	UCHAR  bNrChannels;         /* Number of logicial output channels in the Terminal's
										output audio channel cluster */
	UINT32 bmChannelConfig;     /* Channel layout supported by this terminal */
	UCHAR  iChannelNames;       /* Index in string table describing channels (points to first channel) */
	USHORT bmControls;          /* Bitmap */
	UCHAR  iTerminal;           /* Index of string descriptor describing this terminal */
} USB_Descriptor_Audio_InputTerminal_t;

#if 0
Audio 1.0 verson
typedef struct
{
	UCHAR  bLength;             /* Size of the descriptor (bytes) */
	UCHAR  bDescriptorType;
	UCHAR  bDescriptorSubtype;
	UCHAR  bTerminalID;         /* Unique ID of this terminal unit */
	USHORT wTerminalType;
	UCHAR  bAssocTerminal;      /* ID of associated output terminal, for physically grouped terminals
										* such as the speaker and microphone of a phone handset */
	UCHAR  bNrChannels;         /* Total number of separate audio channels within this interface */
	USHORT wChannelConfig;      /* Channel layout supported by this terminal */
	UCHAR  iChannelNames;       /* Index in string table describing channels (points to first channel */
	UCHAR  iTerminal;           /* Index of string descriptor describing this terminal */
} USB_Descriptor_Audio_InputTerminal_t;
#endif

/* Table 4-10: Output Terminal Descriptor */
typedef struct
{
	UCHAR  bLength;
	UCHAR  bDescriptorType;
	UCHAR  bDescriptorSubtype;
	UCHAR  bTerminalID;
	USHORT wTerminalType;
	UCHAR  bAssocTerminal;
	UCHAR  bSourceID;
	UCHAR  bCSourceID;
	USHORT bmControls;
	UCHAR  iTerminal;
}  USB_Descriptor_Audio_OutputTerminal_t;

#if 0
/* Audio 1.0 version */
typedef struct
{
	UCHAR  bLength;
	UCHAR  bDescriptorType;
	UCHAR  bDescriptorSubtype;
	UCHAR  bTerminalID;
	USHORT wTerminalType;
	UCHAR  bAssocTerminal;
	UCHAR  bSourceID;
	UCHAR  iTerminal;
} USB_Descriptor_Audio_OutputTerminal_t;
#endif

/* Note, we need seperate _out and _in structs due to varying channel count */
/* Table 4-13: Feature Unit Descriptor */
typedef struct
{
	UCHAR bLength;
	UCHAR bDescriptorType;
	UCHAR bDescriptorSubtype;
	UCHAR bUnitID;              /* Unique ID for this feature unit */
	UCHAR bSourceID;            /* Source ID value of the audio source input into this feature unit */
	UINT32  bmaControls[NUM_USB_CHAN_OUT + 1]; /* Feature masks for the control channel, and each separate audio channel */
	UCHAR iFeature;             /* String table index describing this feature unit */
}  USB_Descriptor_Audio_FeatureUnit_Out_t;

typedef struct
{
	UCHAR bLength;
	UCHAR bDescriptorType;
	UCHAR bDescriptorSubtype;
	UCHAR bUnitID;              /* Unique ID for this feature unit */
	UCHAR bSourceID;            /* Source ID value of the audio source input into this feature unit */
	UINT32 bmaControls[NUM_USB_CHAN_IN + 1]; /* Feature masks for the control channel, and each separate audio channel */
	UCHAR iFeature;             /* String table index describing this feature unit */
}  USB_Descriptor_Audio_FeatureUnit_In_t;



typedef struct
{
	UCHAR bLength;
	UCHAR bDescriptorType;
	UCHAR bDescriptorSubType;
	UCHAR bTerminalLink;
	UCHAR bmControls;
	UCHAR bFormatType;
	UINT32      bmFormats;
	UCHAR bNrChannels;
	UINT32      bmChannelConfig;
	UCHAR iChannelNames;
}  USB_Descriptor_Audio_Interface_AS_t;

#if 0
/* Audio class version */
typedef struct
{
	UCHAR bLength;
	UCHAR bDescriptorType;
	UCHAR bDescriptorSubType;
	UCHAR bTerminalLink;
	UCHAR bmControls;
	UCHAR bFormatType;
	UINT32      bmFormats;
	UCHAR bNrChannels;
	UCHAR bmChannelConfig;
} __attribute__((packed)) USB_Descriptor_Audio_Interface_AS_t;
#endif

typedef struct
{
	UCHAR  bLength;
	UCHAR  bDescriptorType;
	UCHAR  bDescriptorSubtype;
	UCHAR  bmAttributes;
	UCHAR  bmControls;
	UCHAR  bLockDelayUnits;
	USHORT wLockDelay;
} USB_Descriptor_Audio_Class_AS_Endpoint_t;

#if 0
/* Audio class 1.0 version */
typedef struct
{
	UCHAR  bLength;
	UCHAR  bDescriptorType;
	UCHAR  bDescriptorSubtype;
	UCHAR  bmAttributes;
	UCHAR  bLockDelayUnits;
	USHORT wLockDelay;
} __attribute__((packed)) USB_Descriptor_Audio_Class_AS_Endpoint_t;
#endif
#endif
