#ifndef _USBDEVICE_H
#define _USBDEVICE_H

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "devioctl.h"

#include <setupapi.h>
#include <basetyps.h>
#include "isousb.h"

#include "usbdi.h"
#include "AudioCommon.h"
#include "UsbAudio2.h"

#define NOISY(_x_) printf _x_ ;

#define MAX_DEVICE_COUNT 16

#define USB_RET_FAILD			0
#define USB_RET_SUCCESS			1
#define USB_RET_HANDLE_INVALID	2

struct DevcieInfo
{
	CHAR DevicePath[MAX_PATH];
	CHAR InPipe[MAX_PATH];
	CHAR OutPipe[MAX_PATH];
	HANDLE InPipeHandle;
	HANDLE OutPipeHandle;
	HANDLE DeviceHandle;
};

struct UsbDeviceList
{
	UINT DeviceCount;
	CHAR DeviceName[MAX_DEVICE_COUNT][MAX_PATH];
};

/////////////////////////////////////////////////////////////////////////////
// CDropButton window
class CUsbDevice
{

// Construction
public:
	CUsbDevice();
	~CUsbDevice();

	UINT    GetDeviceCount();
	UINT	OpenFile(UINT index);

	UINT	GetDevcieStatus();
	UINT	ResetDevice();
	UINT	ResetPipe();

	UINT    GetEndPointStatus();
	UINT	GetPipeInfo();
	UINT	GetDeviceDescriptor();

	UINT    BulkOrInterruptWrite();
	UINT	BulkOrInterruptRead();

	//UINT	VendorRequest(PVENDOR_REQUEST_IN pRequest);
	UINT	GetCurrentConfig();

	UINT	AbortPipe();
	UINT	SettingInterface(PSET_INTERFACE_IN in);

	UINT	GetStringDescriptor();

	// Retrieve the current USB frame number from the Host Controller
	// lpInBuffer: NULL
	// nInBufferSize: 0
	// lpOutBuffer: PULONG to hold current frame number
	// nOutputBufferSize: sizeof(PULONG)
	UINT	GetCurrentFrameNumber();


	// Performs a vendor or class specific control transfer to EP0.  The contents of
	// the input parameter determine the type of request.  See the USB spec
	// for more information on class and vendor control transfers.
	//
	// lpInBuffer: PVENDOR_OR_CLASS_REQUEST_CONTROL
	// nInBufferSize: sizeof(VENDOR_OR_CLASS_REQUEST_CONTROL)
	// lpOutBuffer: pointer to a buffer if the request involves a data transfer
	// nOutputBufferSize: size of the transfer buffer (corresponds to the wLength
	//    field of the USB setup packet)
	UINT	VendorOrClassRequest(PVENDOR_OR_CLASS_REQUEST_CONTROL pRequest, UCHAR *buffer, UINT siz);

	// Reads from the specified ISO endpoint. (USB IN Transfer)
	// lpInBuffer: ISO_TRANSFER_CONTROL
	// nInBufferSize: sizeof(ISO_TRANSFER_CONTROL)
	// lpOutBuffer: buffer to hold data read from the device
	// nOutputBufferSize: size of the read buffer.
	UINT IsoRead(BYTE* buf, UINT siz);

	// Writes to the specified ISO endpoint. (USB OUT Transfer)
	// lpInBuffer: ISO_TRANSFER_CONTROL
	// nInBufferSize: sizeof(ISO_TRANSFER_CONTROL)
	// lpOutBuffer: buffer to hold data to write to the device
	// nOutputBufferSize: size of the write buffer.
	UINT IsoWrite(BYTE* buf, UINT siz);

	UINT ReadIsoStream(BYTE* buf, UINT siz);
	UINT WriteIsoStream(BYTE* buf, UINT siz);
	UINT StopIsoStream();
	UINT ReadIsoBuffer();
	UINT SetFeature();

	void PrintDescriptor();
protected:
	
	HANDLE OpenOneDevice (
		IN       HDEVINFO                    HardwareDeviceInfo,
		IN       PSP_DEVICE_INTERFACE_DATA   DeviceInfoData,
        IN		 LPSTR						devName
		);

	HANDLE OpenDev();

	char *usbDescriptorTypeString(UCHAR bDescriptorType );
	char *usbEndPointTypeString(UCHAR bmAttributes);
	char *usbConfigAttributesString(UCHAR bmAttributes);

	void GetUsbDeviceList(UsbDeviceList * DevList, const LPGUID pguid);

	void print_USB_CONFIGURATION_DESCRIPTOR(PUSB_CONFIGURATION_DESCRIPTOR cd);
	void print_USB_INTERFACE_DESCRIPTOR(PUSB_INTERFACE_DESCRIPTOR id, UINT ix);
	void print_USB_ENDPOINT_DESCRIPTOR(PUSB_ENDPOINT_DESCRIPTOR ed, int i);
	HANDLE	open_file(char *filename);
private:
	UsbDeviceList		m_DevList;
	HANDLE				m_DevHandle;
	SET_INTERFACE_IN	m_InterfaceiIn;
	UCHAR				m_buf_out[1024*8];
	UCHAR				m_buf_in[1024 * 8];
	ULONG				m_frame_num;
	ULONG				m_DevNum;
	HANDLE				m_write_handle;
	HANDLE				m_read_handle;
public:
	USB_Config_Descriptor_Audio2_t m_AudioClass2Des;
};

#endif
