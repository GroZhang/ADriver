/*++

Copyright (c) 1997-1998  Microsoft Corporation

Module Name:

    IsoUsb.h

Abstract:

    Driver-defined special IOCTL's    

Environment:

    Kernel & user mode

Notes:

  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
  PURPOSE.

  Copyright (c) 1997-1998 Microsoft Corporation.  All Rights Reserved.
Revision History:

	11/17/97: created

--*/

#ifndef ISOUSBH_INC
#define ISOUSBH_INC
#include "guidiso.h"
typedef struct _SET_INTERFACE_IN
{
	UCHAR interfaceNum;
	UCHAR alternateSetting;
} SET_INTERFACE_IN, *PSET_INTERFACE_IN;

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
#ifdef BULK
#define USB_IOCTL_INDEX  0x0000


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

#define IOCTL_USB_ABORTPIPE  CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 11, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_SETINTERFACE  CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 12, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)


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


#define IOCTL_USB_START_ISO_STREAM     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 23, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_STOP_ISO_STREAM     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	USB_IOCTL_INDEX + 24, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)
#endif
#ifdef ISO
#define ISOUSB_IOCTL_INDEX  0x0000


#define IOCTL_USB_GET_CONFIG_DESCRIPTOR     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	ISOUSB_IOCTL_INDEX, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_RESET_DEVICE   CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	ISOUSB_IOCTL_INDEX + 1, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_RESET_PIPE  CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	ISOUSB_IOCTL_INDEX + 2, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_STOP_ISO_STREAM     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	ISOUSB_IOCTL_INDEX + 3, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_START_ISO_STREAM     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	ISOUSB_IOCTL_INDEX + 4, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_SETINTERFACE     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	ISOUSB_IOCTL_INDEX + 5, \
	METHOD_BUFFERED, \
	FILE_ANY_ACCESS)

#define IOCTL_USB_VENDOR_OR_CLASS_REQUEST     CTL_CODE(FILE_DEVICE_UNKNOWN,  \
	ISOUSB_IOCTL_INDEX + 6, \
	METHOD_IN_DIRECT, \
	FILE_ANY_ACCESS)
#endif
#endif // end, #ifndef ISOUSBH_INC


