/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    sSUsr.h

Abstract:

Environment:

    Kernel mode

Notes:

    Copyright (c) 2000 Microsoft Corporation.  
    All Rights Reserved.

--*/

#ifndef _BULKUSB_USER_H
#define _BULKUSB_USER_H

#include <initguid.h>

DEFINE_GUID(GUID_CLASS_I82930_BULK,
	0x873fdf61, 0x61a8, 0x11d1, 0xaa, 0x5e, 0x0, 0xc0, 0x4f, 0xb1, 0x72, 0x8b);


//
// registry path used for parameters 
// global to all instances of the driver
//

#define CHUNKLENGTH 512


//
// Vendor specific request code for Anchor Upload/Download
//
// This one is implemented in the core
//

#define ANCHOR_LOAD_INTERNAL  0xA0

//
// These commands are not implemented in the core.  Requires firmware
//
#define ANCHOR_LOAD_EXTERNAL  0xA3
#define ANCHOR_ISFX2          0xAC

//
// This is the highest internal RAM address for the AN2131Q
//
#define MAX_INTERNAL_ADDRESS  0x1B3F

#define INTERNAL_RAM(address) ((address <= MAX_INTERNAL_ADDRESS) ? 1 : 0)
//
// EZ-USB Control and Status Register.  Bit 0 controls 8051 reset
//
#define CPUCS_REG_EZUSB    0x7F92
#define CPUCS_REG_FX2      0xE600


#ifndef _BYTE_DEFINED
#define _BYTE_DEFINED
typedef unsigned char BYTE;
#endif // !_BYTE_DEFINED

#ifndef _WORD_DEFINED
#define _WORD_DEFINED
typedef unsigned short WORD;
#endif // !_WORD_DEFINED
typedef struct _BULKUSB_PIPE_CONTEXT {

	BOOLEAN PipeOpen;

} BULKUSB_PIPE_CONTEXT, *PBULKUSB_PIPE_CONTEXT;



typedef struct _VENDOR_REQUEST_IN
{
	BYTE    bRequest;
	WORD    wValue;
	WORD    wIndex;
	WORD    wLength;
	BYTE    direction;
	BYTE    bData;
} VENDOR_REQUEST_IN, *PVENDOR_REQUEST_IN;

///////////////////////////////////////////////////////////
//
// control structure for bulk and interrupt data transfers
//
///////////////////////////////////////////////////////////
typedef struct _BULK_TRANSFER_CONTROL
{
	ULONG pipeNum;
} BULK_TRANSFER_CONTROL, *PBULK_TRANSFER_CONTROL;

typedef struct _BULK_LATENCY_CONTROL
{
	ULONG bulkPipeNum;
	ULONG intPipeNum;
	ULONG loops;
} BULK_LATENCY_CONTROL, *PBULK_LATENCY_CONTROL;


///////////////////////////////////////////////////////////
//
// control structure isochronous loopback test
//
///////////////////////////////////////////////////////////
typedef struct _ISO_LOOPBACK_CONTROL
{
	// iso pipe to write to
	ULONG outPipeNum;

	// iso pipe to read from
	ULONG inPipeNum;

	// amount of data to read/write from/to the pipe each frame.  If not
	// specified, the MaxPacketSize of the out pipe is used.
	ULONG packetSize;

} ISO_LOOPBACK_CONTROL, *PISO_LOOPBACK_CONTROL;

///////////////////////////////////////////////////////////
//
// control structure for sending vendor or class specific requests
// to the control endpoint.
//
///////////////////////////////////////////////////////////
typedef struct _VENDOR_OR_CLASS_REQUEST_CONTROL
{
	// transfer direction (0=host to device, 1=device to host)
	UCHAR direction;

	// request type (1=class, 2=vendor)
	UCHAR requestType;

	// recipient (0=device,1=interface,2=endpoint,3=other)
	UCHAR recepient;
	//
	// see the USB Specification for an explanation of the
	// following paramaters.
	//
	UCHAR requestTypeReservedBits;
	UCHAR request;
	USHORT value;
	USHORT index;
} VENDOR_OR_CLASS_REQUEST_CONTROL, *PVENDOR_OR_CLASS_REQUEST_CONTROL;

typedef struct _SET_FEATURE_CONTROL
{
	USHORT FeatureSelector;
	USHORT Index;
} SET_FEATURE_CONTROL, *PSET_FEATURE_CONTROL;

///////////////////////////////////////////////////////////
//
// control structure for isochronous data transfers
//
///////////////////////////////////////////////////////////
typedef struct _ISO_TRANSFER_CONTROL
{
	//
	// pipe number to perform the ISO transfer to/from.  Direction is
	// implied by the pipe number.
	//
	ULONG PipeNum;
	//
	// ISO packet size.  Determines how much data is transferred each
	// frame.  Should be less than or equal to the maxpacketsize for
	// the endpoint.
	//
	ULONG PacketSize;
	//
	// Total number of ISO packets to transfer.
	//
	ULONG PacketCount;
	//
	// The following two parameters detmine how buffers are managed for
	// an ISO transfer.  In order to maintain an ISO stream, the driver
	// must create at least 2 transfer buffers and ping pong between them.
	// BufferCount determines how many buffers the driver creates to ping
	// pong between.  FramesPerBuffer specifies how many USB frames of data
	// are transferred by each buffer.
	//
	ULONG FramesPerBuffer;     // 10 is a good value
	ULONG BufferCount;         // 2 is a good value
} ISO_TRANSFER_CONTROL, *PISO_TRANSFER_CONTROL;


///////////////////////////////////////////////////////////
//
// control structure for Anchor Downloads
//
///////////////////////////////////////////////////////////
typedef struct _ANCHOR_DOWNLOAD_CONTROL
{
	WORD Offset;
} ANCHOR_DOWNLOAD_CONTROL, *PANCHOR_DOWNLOAD_CONTROL;

#define MAX_INTEL_HEX_RECORD_LENGTH 16

typedef struct _INTEL_HEX_RECORD
{
	BYTE  Length;
	WORD  Address;
	BYTE  Type;
	BYTE  Data[MAX_INTEL_HEX_RECORD_LENGTH];
} INTEL_HEX_RECORD, *PINTEL_HEX_RECORD;

typedef struct _SET_INTERFACE_IN
{
	UCHAR interfaceNum;
	UCHAR alternateSetting;
} SET_INTERFACE_IN, *PSET_INTERFACE_IN;

typedef struct _GET_STRING_DESCRIPTOR_IN
{
	UCHAR    Index;
	USHORT   LanguageId;
} GET_STRING_DESCRIPTOR_IN, *PGET_STRING_DESCRIPTOR_IN;

typedef struct _EZUSB_DRIVER_VERSION
{
	WORD     MajorVersion;
	WORD     MinorVersion;
	WORD     BuildVersion;
} EZUSB_DRIVER_VERSION, *PEZUSB_DRIVER_VERSION;



typedef struct _RING_BUFFER
{
	PUCHAR      inPtr;
	PUCHAR      outPtr;
	ULONG       totalSize;
	ULONG       currentSize;
	KSPIN_LOCK	spinLock;
	PUCHAR      buffer;
} RING_BUFFER, *PRING_BUFFER;
//




#define USB_IOCTL_INDEX             0x0000


#define IOCTL_USB_GET_CONFIG_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,     \
	USB_IOCTL_INDEX, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_RESET_DEVICE          CTL_CODE(FILE_DEVICE_UNKNOWN,     \
	USB_IOCTL_INDEX + 1, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_RESET_PIPE            CTL_CODE(FILE_DEVICE_UNKNOWN,     \
	USB_IOCTL_INDEX + 2, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_GET_ENDPOINT_STATUS   CTL_CODE(FILE_DEVICE_UNKNOWN,     \
	USB_IOCTL_INDEX + 3, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_GET_PIPE_INFO     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 4, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_GET_DEVICE_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 5, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_BULK_OR_INTERRUPT_WRITE     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 6, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_BULK_OR_INTERRUPT_READ      CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 7, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_VENDOR_REQUEST              CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 8, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_GET_CURRENT_CONFIG          CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 9, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_CMD_USB_ANCHOR_DOWNLOAD             CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 10, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_ABORTPIPE  CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 11, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_SETINTERFACE  CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 12, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_GET_STRING_DESCRIPTOR CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 13, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)


//
// Perform an IN transfer over the specified bulk or interrupt pipe.
//
// lpInBuffer: BULK_TRANSFER_CONTROL stucture specifying the pipe number to read from
// nInBufferSize: sizeof(BULK_TRANSFER_CONTROL)
// lpOutBuffer: Buffer to hold data read from the device.  
// nOutputBufferSize: size of lpOutBuffer.  This parameter determines
//    the size of the USB transfer.
// lpBytesReturned: actual number of bytes read
// 
#define IOCTL_USB_BULK_READ             CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 14, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

//
// Perform an OUT transfer over the specified bulk or interrupt pipe.
//
// lpInBuffer: BULK_TRANSFER_CONTROL stucture specifying the pipe number to write to
// nInBufferSize: sizeof(BULK_TRANSFER_CONTROL)
// lpOutBuffer: Buffer of data to write to the device
// nOutputBufferSize: size of lpOutBuffer.  This parameter determines
//    the size of the USB transfer.
// lpBytesReturned: actual number of bytes written
// 
#define IOCTL_USB_BULK_WRITE            CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 15, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

//
// The following IOCTL's are defined as using METHOD_DIRECT_IN buffering.
// This means that the output buffer is directly mapped into system
// space and probed for read access by the driver.  This means that it is
// brought into memory if it happens to be paged out to disk.  Even though
// the buffer is only probed for read access, it is safe (probably) to
// write to it as well.  This read/write capability is used for the loopback
// IOCTL's
// 

// TODO Insert Loopback IOCTL's

//
// Retrieve the current USB frame number from the Host Controller
//
// lpInBuffer: NULL
// nInBufferSize: 0
// lpOutBuffer: PULONG to hold current frame number
// nOutputBufferSize: sizeof(PULONG)
// 
#define IOCTL_USB_GET_CURRENT_FRAME_NUMBER  CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 16, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)


//
// Performs a vendor or class specific control transfer to EP0.  The contents of
// the input parameter determine the type of request.  See the USB spec
// for more information on class and vendor control transfers.
//
// lpInBuffer: PVENDOR_OR_CLASS_REQUEST_CONTROL
// nInBufferSize: sizeof(VENDOR_OR_CLASS_REQUEST_CONTROL)
// lpOutBuffer: pointer to a buffer if the request involves a data transfer
// nOutputBufferSize: size of the transfer buffer (corresponds to the wLength
//    field of the USB setup packet)
// 
#define IOCTL_USB_VENDOR_OR_CLASS_REQUEST   CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 17, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

//
// Retrieves the actual USBD_STATUS code for the most recently failed
// URB.
//
// lpInBuffer: NULL
// nInBufferSize: 0
// lpOutBuffer: PULONG to hold the URB status
// nOutputBufferSize: sizeof(ULONG)
// 

//#define IOCTL_USB_GET_LAST_ERROR   CTL_CODE(FILE_DEVICE_UNKNOWN,  \
//	USB_IOCTL_INDEX + 18, \
//	METHOD_BUFFERED, \
//	FILE_ANY_ACCESS)

//
// Reads from the specified ISO endpoint. (USB IN Transfer)
//
// lpInBuffer: ISO_TRANSFER_CONTROL
// nInBufferSize: sizeof(ISO_TRANSFER_CONTROL)
// lpOutBuffer: buffer to hold data read from the device
// nOutputBufferSize: size of the read buffer.
//
// 
// 

#define IOCTL_USB_ISO_READ          CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 19, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

//
// Writes to the specified ISO endpoint. (USB OUT Transfer)
//
// lpInBuffer: ISO_TRANSFER_CONTROL
// nInBufferSize: sizeof(ISO_TRANSFER_CONTROL)
// lpOutBuffer: buffer to hold data to write to the device
// nOutputBufferSize: size of the write buffer.
//
// 
// 

#define IOCTL_USB_ISO_WRITE          CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 20, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

//
// Performs and Anchor Download.
//
// lpInBuffer: PANCHOR_DOWNLOAD_CONTROL
// nInBufferSize: sizeof(ANCHOR_DOWNLOAD_CONTROL)
// lpOutBuffer: pointer to a buffer of data to download to the device
// nOutputBufferSize: size of the transfer buffer
// 
#define IOCTL_USB_ANCHOR_DOWNLOAD   CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 21, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

//
// Returns driver version information
//
// lpInBuffer: NULL
// nInBufferSize: 0
// lpOutBuffer: PEZUSB_DRIVER_VERSION
// nOutputBufferSize: sizeof(EZUSB_DRIVER_VERSION)
// 
#define IOCTL_USB_GET_DRIVER_VERSION   CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 22, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_START_ISO_STREAM     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 23, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_STOP_ISO_STREAM     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 24, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_READ_ISO_BUFFER     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 25, \
	METHOD_OUT_DIRECT, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_SET_FEATURE         CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 26, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)


#endif

