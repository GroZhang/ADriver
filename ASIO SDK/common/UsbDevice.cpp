#include <string.h>
#include "UsbDevice.h"

CUsbDevice::CUsbDevice()
{
	m_DevHandle = NULL;
	m_write_handle = NULL;
	m_read_handle = NULL;
	memset(&m_DevList, 0, sizeof(DevcieInfo));
	GetUsbDeviceList(&m_DevList, (LPGUID)&GUID_CLASS_I82930_ISO);
	m_frame_num = 0;
}

CUsbDevice::~CUsbDevice()
{
	if (NULL != m_DevHandle)
	{
		CloseHandle(m_DevHandle);
		m_DevHandle = NULL;
	}

	if (NULL != m_read_handle)
	{
		CloseHandle(m_read_handle);
		m_read_handle = NULL;
	}
	if (NULL != m_write_handle)
	{
		CloseHandle(m_write_handle);
		m_write_handle = NULL;
	}
}

HANDLE
CUsbDevice::OpenOneDevice (
    IN       HDEVINFO                    HardwareDeviceInfo,
    IN       PSP_DEVICE_INTERFACE_DATA   DeviceInfoData,
	IN		 LPSTR            devName
    )
/*++
Routine Description:

    Given the HardwareDeviceInfo, representing a handle to the plug and
    play information, and deviceInfoData, representing a specific usb device,
    open that device and fill in all the relevant information in the given
    USB_DEVICE_DESCRIPTOR structure.

Arguments:

    HardwareDeviceInfo:  handle to info obtained from Pnp mgr via SetupDiGetClassDevs()
    DeviceInfoData:      ptr to info obtained via SetupDiEnumDeviceInterfaces()

Return Value:

    return HANDLE if the open and initialization was successfull,
        else INVLAID_HANDLE_VALUE.

--*/
{
    PSP_DEVICE_INTERFACE_DETAIL_DATA     functionClassDeviceData = NULL;
    ULONG                                predictedLength = 0;
    ULONG                                requiredLength = 0;
	HANDLE								 hOut = INVALID_HANDLE_VALUE;

    //
    // allocate a function class device data structure to receive the
    // goods about this particular device.
    //
    SetupDiGetDeviceInterfaceDetail (
            HardwareDeviceInfo,
            DeviceInfoData,
            NULL, // probing so no output buffer yet
            0, // probing so output buffer length of zero
            &requiredLength,
            NULL); // not interested in the specific dev-node


    predictedLength = requiredLength;
    // sizeof (SP_FNCLASS_DEVICE_DATA) + 512;

	functionClassDeviceData = (PSP_DEVICE_INTERFACE_DETAIL_DATA )malloc(predictedLength);
    functionClassDeviceData->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);

    //
    // Retrieve the information from Plug and Play.
    //
    if (! SetupDiGetDeviceInterfaceDetail (
               HardwareDeviceInfo,
               DeviceInfoData,
               functionClassDeviceData,
               predictedLength,
               &requiredLength,
               NULL)) {
                free( functionClassDeviceData );
        return INVALID_HANDLE_VALUE;
    }

        wsprintf( devName,"%s",functionClassDeviceData->DevicePath) ;
        printf( "Attempting to open %s\r\n", devName );


    hOut = CreateFile (
                  functionClassDeviceData->DevicePath,
                  GENERIC_READ | GENERIC_WRITE,
                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                  NULL, // no SECURITY_ATTRIBUTES structure
                  OPEN_EXISTING, // No special create flags
                  0, // No special attributes
                  NULL); // No template file

    if (INVALID_HANDLE_VALUE == hOut) {
                printf( "FAILED to open %s\r\n", devName );
    }
        free( functionClassDeviceData );
        return hOut;
}

void CUsbDevice::GetUsbDeviceList(UsbDeviceList* UsbList, LPGUID pguid)
{

	ULONG NumberDevices;
	CHAR outNameBuf[MAX_PATH];

	HANDLE hOut = INVALID_HANDLE_VALUE;
	HDEVINFO                 hardwareDeviceInfo;
	SP_DEVICE_INTERFACE_DATA deviceInfoData;
	ULONG                    i;
	PUSB_DEVICE_DESCRIPTOR   usbDeviceInst;
	PUSB_DEVICE_DESCRIPTOR   *UsbDevices = &usbDeviceInst;
	*UsbDevices = NULL;
	NumberDevices = 0;

	SP_DEVINFO_DATA spDevInfoData = { 0 };

	//
	// Open a handle to the plug and play dev node.
	// SetupDiGetClassDevs() returns a device information set that contains info on all
	// installed devices of a specified class.
	//

	hardwareDeviceInfo = SetupDiGetClassDevs(
		pguid,
		NULL, // Define no enumerator (global)
		NULL, // Define no
		(DIGCF_PRESENT | // Only Devices present
		DIGCF_DEVICEINTERFACE)); // Function class devices.

	//
	// Take a wild guess at the number of devices we have;
	// Be prepared to realloc and retry if there are more than we guessed
	//
	NumberDevices = MAX_DEVICE_COUNT;

	deviceInfoData.cbSize = sizeof (SP_DEVICE_INTERFACE_DATA);

	if (*UsbDevices)
	{
		*UsbDevices = (_USB_DEVICE_DESCRIPTOR *)
			realloc(*UsbDevices, (NumberDevices * sizeof (USB_DEVICE_DESCRIPTOR)));
	}
	else
	{
		*UsbDevices = (_USB_DEVICE_DESCRIPTOR *)calloc(NumberDevices, sizeof (USB_DEVICE_DESCRIPTOR));
	}

	if (NULL == *UsbDevices)
	{

		// SetupDiDestroyDeviceInfoList destroys a device information set
		// and frees all associated memory.

		SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
		return;
	}


	for (i = 0; i < NumberDevices; i++)
	{

		// SetupDiEnumDeviceInterfaces() returns information about device interfaces
		// exposed by one or more devices. Each call returns information about one interface;
		// the routine can be called repeatedly to get information about several interfaces
		// exposed by one or more devices.

		if (SetupDiEnumDeviceInterfaces(hardwareDeviceInfo,
			0, // We don't care about specific PDOs
			pguid,
			i,
			&deviceInfoData))
		{


			hOut = OpenOneDevice(hardwareDeviceInfo, &deviceInfoData, outNameBuf);
			if (hOut != INVALID_HANDLE_VALUE)
			{
				sprintf(UsbList->DeviceName[i], "%s", outNameBuf);
			}
		}
		else
		{
			if (ERROR_NO_MORE_ITEMS == GetLastError())
			{
				break;
			}
		}
	}
	UsbList->DeviceCount = i;

	// SetupDiDestroyDeviceInfoList() destroys a device information set
	// and frees all associated memory.

	SetupDiDestroyDeviceInfoList(hardwareDeviceInfo);
	free(*UsbDevices);
}


UINT   CUsbDevice::GetDeviceCount()
{
	return m_DevList.DeviceCount;

}

UINT
CUsbDevice::OpenFile(UINT index)
/*++
Routine Description:

    Called by main() to open an instance of our device after obtaining its name

Arguments:

    None

Return Value:

    Device handle on success else NULL

--*/
{

        int success = USB_RET_FAILD;

		fprintf(stdout, ("completeDeviceName = (%s)\r\n"), m_DevList.DeviceName[index]);

		m_DevHandle = CreateFile((LPCTSTR)m_DevList.DeviceName[index],
                GENERIC_WRITE | GENERIC_READ,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);

		if (m_DevHandle == INVALID_HANDLE_VALUE)
		{
				fprintf(stdout, ("Failed to open (%s) = %d\r\n"), m_DevList.DeviceName[index], GetLastError());
				success = USB_RET_HANDLE_INVALID;
        }
		else
		{
				fprintf(stdout, ("Opened successfully.\r\n"));
				success = USB_RET_SUCCESS;
	    }           
		m_DevNum = index;

		return success;
}

UINT CUsbDevice::GetDevcieStatus()
{
	UINT success;
	if (m_DevHandle)
	{
		ULONG siz, nBytes;
		char buf[2];
		
		siz = sizeof(buf);
		
		if (m_DevHandle == INVALID_HANDLE_VALUE)
		{
			fprintf(stdout, "Please open Device First\r\n");
			return USB_RET_HANDLE_INVALID;
		}
		
		//success = DeviceIoControl(m_DevHandle,
		//	IOCTL_USB_GET_ENDPOINT_STATUS,
		//	NULL,
		//	0,
		//	buf,
		//	siz,
		//	&nBytes,
		//	NULL);
		
		if (!success)
		{
			fprintf(stdout, ("IOCTL_BULKUSB_GET_ENDPOINT_STATUS successful!\r\n"));
			fprintf(stdout, ("The return byte length is %d\r\n"), nBytes);
			fprintf(stdout, ("The return byte1 value is %d\r\n"), buf[0]);
			fprintf(stdout, ("The return byte2 value is %d\r\n"), buf[1]);
			return USB_RET_SUCCESS;
		}
		else
		{
			fprintf(stdout, ("IOCTL_BULKUSB_GET_ENDPOINT_STATUS failer!\r\n"));
			return USB_RET_FAILD;
		}
	}
	return USB_RET_HANDLE_INVALID;
}

UINT CUsbDevice::ResetPipe()
{
	UINT success;
	UINT endpoint_num = 0;
	if (m_DevHandle)
	{
		ULONG siz, nBytes;
		char buf[2];

		siz = sizeof(buf);

		if (m_DevHandle == INVALID_HANDLE_VALUE)
		{
			fprintf(stdout, "Please open Device First\r\n");
			return USB_RET_HANDLE_INVALID;
		}
		switch(m_InterfaceiIn.interfaceNum)
		{
		case 0:
			switch(m_InterfaceiIn.alternateSetting)
			{
			case 0:
				endpoint_num = m_AudioClass2Des.Audio_Out_StreamInterface_Alt0.bNumEndpoints;
				break;
			case 1:
				endpoint_num = m_AudioClass2Des.Audio_Out_StreamInterface_Alt1.bNumEndpoints;
				break;
			case 2:
				endpoint_num = m_AudioClass2Des.Audio_Out_StreamInterface_Alt1.bNumEndpoints;
				break;
			default:
				break;
			}
			break;
		case 1:
			switch (m_InterfaceiIn.alternateSetting)
			{
			case 0:
				endpoint_num = m_AudioClass2Des.Audio_Out_StreamInterface_Alt0.bNumEndpoints;
				break;
			case 1:
				endpoint_num = m_AudioClass2Des.Audio_Out_StreamInterface_Alt1.bNumEndpoints;
				break;
			case 2:
				endpoint_num = m_AudioClass2Des.Audio_Out_StreamInterface_Alt1.bNumEndpoints;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		//for(ULONG i = 0 ; i < endpoint_num;i++)
		//{
		//	success = DeviceIoControl(m_DevHandle,
		//		IOCTL_ISOUSB_RESET_PIPE,
		//		(PVOID)&i,
		//		sizeof(ULONG),
		//		buf,
		//		siz,
		//		&nBytes,
		//		NULL);

		//}
		ULONG i;
		success = DeviceIoControl(m_read_handle,
			IOCTL_USB_RESET_PIPE,
			(PVOID)&i,
			sizeof(ULONG),
			buf,
			siz,
			&nBytes,
			NULL);
		success = DeviceIoControl(m_write_handle,
			IOCTL_USB_RESET_PIPE,
			(PVOID)&i,
			sizeof(ULONG),
			buf,
			siz,
			&nBytes,
			NULL);		if (success >= 0)
		{
			fprintf(stdout, ("IOCTL_BULKUSB_RESET_PIPE successful!\r\n"));
			return USB_RET_SUCCESS;
		}
		else
		{
			fprintf(stdout, ("IOCTL_BULKUSB_RESET_PIPE failer!\r\n"));
			return USB_RET_FAILD;
		}
	}

	return USB_RET_HANDLE_INVALID;
}


UINT CUsbDevice::ResetDevice()
{
	UINT success;
	if (m_DevHandle)
	{
		ULONG siz, nBytes;
		char buf[2];
		siz = sizeof(buf);
		if (m_DevHandle == INVALID_HANDLE_VALUE)
		{
			fprintf(stdout, "Please open Device First");
			return USB_RET_HANDLE_INVALID;
		}
		success = DeviceIoControl(m_DevHandle,
			IOCTL_USB_RESET_DEVICE,
			NULL,
			0,
			buf,
			siz,
			&nBytes,
			NULL);
		if (!success) 
		{
			fprintf(stdout, ("IOCTL_BULKUSB_RESET_DEVICE successful!\r\n"));
			fprintf(stdout, ("The return byte length is %d\r\n"), nBytes);
			fprintf(stdout, ("The return byte1 value is %d\r\n"), buf[0]);
			fprintf(stdout, ("The return byte2 value is %d\r\n"), buf[1]);
			return USB_RET_SUCCESS;
		}
		else
		{
			fprintf(stdout, ("IOCTL_BULKUSB_RESET_DEVICE failer!\r\n"));
			return USB_RET_FAILD;
		}
	}

	return USB_RET_HANDLE_INVALID;
}


void
CUsbDevice::PrintDescriptor()
/*++
Routine Description:

    Called to do formatted ascii dump to console of  USB
    configuration, interface, and endpoint descriptors
    (Cmdline "rwbulk -u" )

Arguments:

    handle to device

Return Value:

    none

--*/
{
        UINT success;
        int siz, nBytes;
        UCHAR buf[512];
		PUSB_CONFIGURATION_DESCRIPTOR cd;
		PUSB_INTERFACE_DESCRIPTOR id;
		PUSB_ENDPOINT_DESCRIPTOR ed;

        siz = sizeof(buf);

		if (m_DevHandle == INVALID_HANDLE_VALUE) {
                NOISY(("DEV not open"));
                return;
        }
        
		success = DeviceIoControl(m_DevHandle,
			IOCTL_USB_GET_CONFIG_DESCRIPTOR,
                        buf,
                        siz,
                        buf,
                        siz,
                        (unsigned long *)&nBytes,
                        NULL);

        NOISY(("request complete, success = %d nBytes = %d\r\n", success, nBytes));
        
        if (success) 
		{
			UINT32 lenght = sizeof(USB_CONFIGURATION_DESCRIPTOR)+sizeof(USB_INTERFACE_ASSOCIATION_DESCRIPTOR)+sizeof(USB_INTERFACE_DESCRIPTOR);
			memcpy((UCHAR*)&m_AudioClass2Des, buf, lenght);
			memcpy((UCHAR*)&m_AudioClass2Des.Audio_CS_Control_Int, buf + lenght, sizeof(USB_CfgDesc_Audio2_CS_Control_Int));
			lenght += sizeof(USB_CfgDesc_Audio2_CS_Control_Int);
			memcpy((UCHAR*)&m_AudioClass2Des.Audio_Out_StreamInterface_Alt0, buf + lenght, sizeof(USB_Config_Descriptor_Audio2_t)-lenght);

        ULONG i;
                UINT  j, n;
        UCHAR *pch;

        pch = buf;
                n = 0;

        cd = (PUSB_CONFIGURATION_DESCRIPTOR) pch;

        print_USB_CONFIGURATION_DESCRIPTOR( cd );

        pch += cd->bLength;

        do {

            id = (PUSB_INTERFACE_DESCRIPTOR) pch;

            print_USB_INTERFACE_DESCRIPTOR(id, n++);

            pch += id->bLength;
            for (j=0; j<id->bNumEndpoints; j++) {

                ed = (PUSB_ENDPOINT_DESCRIPTOR) pch;

                print_USB_ENDPOINT_DESCRIPTOR(ed,j);

                pch += ed->bLength;
            }
            i = (ULONG)(pch - buf);
        } while (i < cd->wTotalLength);

        }
        
        return;

}

void
CUsbDevice::print_USB_CONFIGURATION_DESCRIPTOR(PUSB_CONFIGURATION_DESCRIPTOR cd)
/*++
Routine Description:

    Called to do formatted ascii dump to console of a USB config descriptor

Arguments:

    ptr to USB configuration descriptor

Return Value:

    none

--*/
{
	fprintf(stdout,"-----USB_CONFIGURATION_DESCRIPTOR-----\r\n");
	fprintf(stdout, "bLength = 0x%x, decimal %d\r\n", cd->bLength, cd->bLength);
	fprintf(stdout, "bDescriptorType = 0x%x ( %s )\r\n", cd->bDescriptorType, usbDescriptorTypeString(cd->bDescriptorType) );
	fprintf(stdout, "wTotalLength = 0x%x, decimal %d\r\n", cd->wTotalLength, cd->wTotalLength   );
	fprintf(stdout, "bNumInterfaces = 0x%x, decimal %d\r\n", cd->bNumInterfaces, cd->bNumInterfaces   );
	fprintf(stdout, "bConfigurationValue = 0x%x, decimal %d\r\n", cd->bConfigurationValue, cd->bConfigurationValue );
	fprintf(stdout, "iConfiguration = 0x%x, decimal %d\r\n", cd->iConfiguration, cd->iConfiguration);
	fprintf(stdout, "bmAttributes = 0x%x ( %s )\r\n", cd->bmAttributes, usbConfigAttributesString(cd->bmAttributes));
	fprintf(stdout, "MaxPower = 0x%x, decimal %d\r\n", cd->MaxPower, cd->MaxPower );
}


void
CUsbDevice::print_USB_INTERFACE_DESCRIPTOR(PUSB_INTERFACE_DESCRIPTOR id, UINT ix)
/*++
Routine Description:

    Called to do formatted ascii dump to console of a USB interface descriptor

Arguments:

    ptr to USB interface descriptor

Return Value:

    none

--*/
{
	fprintf(stdout, ("\n-------USB_INTERFACE_DESCRIPTOR #%d------\r\n"), ix);
	fprintf(stdout, ("bLength = 0x%x\r\n"), id->bLength );
	fprintf(stdout, ("bDescriptorType = 0x%x ( %s )\r\n"), id->bDescriptorType, usbDescriptorTypeString(id->bDescriptorType));
	fprintf(stdout, ("bInterfaceNumber = 0x%x\r\n"), id->bInterfaceNumber);
	fprintf(stdout, ("bAlternateSetting = 0x%x\r\n"), id->bAlternateSetting );
	fprintf(stdout, ("bNumEndpoints = 0x%x\r\n"), id->bNumEndpoints );
	fprintf(stdout, ("bInterfaceClass = 0x%x\r\n"), id->bInterfaceClass );
	fprintf(stdout, ("bInterfaceSubClass = 0x%x\r\n"), id->bInterfaceSubClass );
	fprintf(stdout, ("bInterfaceProtocol = 0x%x\r\n"), id->bInterfaceProtocol);
	fprintf(stdout, ("bInterface = 0x%x\r\n"), id->iInterface );
}


void
CUsbDevice::print_USB_ENDPOINT_DESCRIPTOR(PUSB_ENDPOINT_DESCRIPTOR ed, int i)
/*++
Routine Description:

    Called to do formatted ascii dump to console of a USB endpoint descriptor

Arguments:

    ptr to USB endpoint descriptor,
        index of this endpt in interface desc

Return Value:

    none

--*/
{
	fprintf(stdout, ("------USB_ENDPOINT_DESCRIPTOR for Pipe%02d------\r\n"), i );
	fprintf(stdout, ("bLength = 0x%x\r\n"), ed->bLength);
	fprintf(stdout, ("bDescriptorType = 0x%x ( %s )\r\n"), ed->bDescriptorType, usbDescriptorTypeString(ed->bDescriptorType));

    if ( USB_ENDPOINT_DIRECTION_IN( ed->bEndpointAddress ) ) {
		fprintf(stdout, ("bEndpointAddress= 0x%x ( INPUT )\r\n"), ed->bEndpointAddress);
    } else {
		fprintf(stdout, ("bEndpointAddress= 0x%x ( OUTPUT )\r\n"), ed->bEndpointAddress);
    }

	fprintf(stdout, ("bmAttributes= 0x%x ( %s )\r\n"), ed->bmAttributes, usbEndPointTypeString(ed->bmAttributes));
	fprintf(stdout, ("wMaxPacketSize= 0x%x, decimal %d\r\n"), ed->wMaxPacketSize, ed->wMaxPacketSize);
	fprintf(stdout, ("bInterval = 0x%x, decimal %d\r\n"), ed->bInterval, ed->bInterval);
}

char * CUsbDevice::usbDescriptorTypeString(UCHAR bDescriptorType )
/*++
Routine Description:

    Called to get ascii string of USB descriptor

Arguments:

        PUSB_ENDPOINT_DESCRIPTOR->bDescriptorType or
        PUSB_DEVICE_DESCRIPTOR->bDescriptorType or
        PUSB_INTERFACE_DESCRIPTOR->bDescriptorType or
        PUSB_STRING_DESCRIPTOR->bDescriptorType or
        PUSB_POWER_DESCRIPTOR->bDescriptorType or
        PUSB_CONFIGURATION_DESCRIPTOR->bDescriptorType

Return Value:

    ptr to string

--*/{
        switch(bDescriptorType) {

        case USB_DEVICE_DESCRIPTOR_TYPE:
                return "USB_DEVICE_DESCRIPTOR_TYPE";

        case USB_CONFIGURATION_DESCRIPTOR_TYPE:
                return "USB_CONFIGURATION_DESCRIPTOR_TYPE";

        case USB_STRING_DESCRIPTOR_TYPE:
                return "USB_STRING_DESCRIPTOR_TYPE";

        case USB_INTERFACE_DESCRIPTOR_TYPE:
                return "USB_INTERFACE_DESCRIPTOR_TYPE";

        case USB_ENDPOINT_DESCRIPTOR_TYPE:
                return "USB_ENDPOINT_DESCRIPTOR_TYPE";
                

#ifdef USB_POWER_DESCRIPTOR_TYPE // this is the older definintion which is actually obsolete
    // workaround for temporary bug in 98ddk, older USB100.h file
        case USB_POWER_DESCRIPTOR_TYPE:
                return "USB_POWER_DESCRIPTOR_TYPE";
#endif
                
#ifdef USB_RESERVED_DESCRIPTOR_TYPE  // this is the current version of USB100.h as in NT5DDK

        case USB_RESERVED_DESCRIPTOR_TYPE:
                return "USB_RESERVED_DESCRIPTOR_TYPE";

        case USB_CONFIG_POWER_DESCRIPTOR_TYPE:
                return "USB_CONFIG_POWER_DESCRIPTOR_TYPE";

        case USB_INTERFACE_POWER_DESCRIPTOR_TYPE:
                return "USB_INTERFACE_POWER_DESCRIPTOR_TYPE";
#endif // for current nt5ddk version of USB100.h

        default:
                return "??? UNKNOWN!!"; 
        }
}


char * CUsbDevice::usbEndPointTypeString(UCHAR bmAttributes)
/*++
Routine Description:

    Called to get ascii string of endpt descriptor type

Arguments:

        PUSB_ENDPOINT_DESCRIPTOR->bmAttributes

Return Value:

    ptr to string

--*/
{
        UINT typ = bmAttributes & USB_ENDPOINT_TYPE_MASK;

        switch( typ) {
        case USB_ENDPOINT_TYPE_INTERRUPT:
                return "USB_ENDPOINT_TYPE_INTERRUPT";

        case USB_ENDPOINT_TYPE_BULK:
                return "USB_ENDPOINT_TYPE_BULK";        

        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
                return "USB_ENDPOINT_TYPE_ISOCHRONOUS"; 
                
        case USB_ENDPOINT_TYPE_CONTROL:
                return "USB_ENDPOINT_TYPE_CONTROL";     
                
        default:
                return "??? UNKNOWN!!"; 
        }
}


char * CUsbDevice::usbConfigAttributesString(UCHAR bmAttributes)
/*++
Routine Description:

    Called to get ascii string of USB_CONFIGURATION_DESCRIPTOR attributes

Arguments:

        PUSB_CONFIGURATION_DESCRIPTOR->bmAttributes

Return Value:

    ptr to string

--*/
{
        UINT typ = bmAttributes & USB_CONFIG_POWERED_MASK;

        switch( typ) {

        case USB_CONFIG_BUS_POWERED:
                return "USB_CONFIG_BUS_POWERED";

        case USB_CONFIG_SELF_POWERED:
                return "USB_CONFIG_SELF_POWERED";
                
        case USB_CONFIG_REMOTE_WAKEUP:
                return "USB_CONFIG_REMOTE_WAKEUP";
        default:
                return "??? UNKNOWN!!"; 
        }
}

UINT    CUsbDevice::GetEndPointStatus()
{
	UINT success;

	if (m_DevHandle)
	{
		ULONG siz, nBytes;
		char buf[2];

		siz = sizeof(buf);

		if (m_DevHandle == INVALID_HANDLE_VALUE)
		{
			fprintf(stdout, "Please open Device First\r\n");
			return USB_RET_HANDLE_INVALID;
		}

		//success = DeviceIoControl(m_DevHandle,
		//	IOCTL_USB_GET_ENDPOINT_STATUS,
		//	NULL,
		//	0,
		//	buf,
		//	siz,
		//	&nBytes,
		//	NULL);

		if (success>=0)
		{
			fprintf(stdout, ("IOCTL_USB_GET_ENDPOINT_STATUS successful!\r\n"));
			fprintf(stdout, ("The return byte length is %d\r\n"), nBytes);
			fprintf(stdout, ("The return byte1 value is %d\r\n"), buf[0]);
			fprintf(stdout, ("The return byte2 value is %d\r\n"), buf[1]);
			return USB_RET_SUCCESS;
		}
		else
		{
			fprintf(stdout, ("IOCTL_BULKUSB_RESET_PIPE failer!\r\n"));
			return USB_RET_FAILD;
		}
	}
	else
	{
	}
	return USB_RET_HANDLE_INVALID;
}

UINT	CUsbDevice::GetPipeInfo()
{
	return USB_RET_SUCCESS;
}

UINT	CUsbDevice::GetDeviceDescriptor(){ return USB_RET_SUCCESS; }

UINT    CUsbDevice::BulkOrInterruptWrite(){ return USB_RET_SUCCESS; }
UINT	CUsbDevice::BulkOrInterruptRead(){ return USB_RET_SUCCESS; }

UINT	CUsbDevice::GetCurrentConfig(){ return  USB_RET_SUCCESS; }

UINT	CUsbDevice::AbortPipe(){ return USB_RET_SUCCESS; }

UINT	CUsbDevice::SettingInterface(PSET_INTERFACE_IN pinterface)
{
	UINT success;

	if (m_DevHandle)
	{
		ULONG siz, nBytes;
		char buf[2];
		siz = sizeof(buf);

		m_InterfaceiIn.alternateSetting = pinterface->alternateSetting;
		m_InterfaceiIn.interfaceNum =pinterface->interfaceNum;

		if (m_DevHandle == INVALID_HANDLE_VALUE)
		{
			fprintf(stdout, "Please open Device First\r\n");
			return USB_RET_HANDLE_INVALID;
		}

		success = DeviceIoControl(m_DevHandle,
			IOCTL_USB_SETINTERFACE,
			pinterface,
			sizeof(SET_INTERFACE_IN),
			buf,
			siz,
			&nBytes,
			NULL);

		if (success>=0)
		{
			m_write_handle = open_file("PIPE00");
			m_read_handle = open_file("PIPE01");
			return USB_RET_SUCCESS;
		}
		else
		{
			fprintf(stdout, ("SettingInterface failer!\r\n"));
			return USB_RET_FAILD;
		}
	}
	return USB_RET_HANDLE_INVALID;
}

UINT CUsbDevice::GetStringDescriptor(){ return  USB_RET_SUCCESS; }

UINT CUsbDevice::GetCurrentFrameNumber(){ return  USB_RET_SUCCESS; }

UINT CUsbDevice::VendorOrClassRequest(PVENDOR_OR_CLASS_REQUEST_CONTROL pRequest,UCHAR *buffer,UINT siz)
{
	UINT success;

	if (m_DevHandle)
	{
		ULONG nBytes;

		if (m_DevHandle == INVALID_HANDLE_VALUE)
		{
			fprintf(stdout, "Please open Device First\r\n");
			return USB_RET_HANDLE_INVALID;
		}

		success = DeviceIoControl(m_DevHandle,
			IOCTL_USB_VENDOR_OR_CLASS_REQUEST,
			pRequest,
			sizeof(VENDOR_OR_CLASS_REQUEST_CONTROL),
			buffer,
			siz,
			&nBytes,
			NULL);

		if (success>=0)
		{
			fprintf(stdout, ("VendorOrClassRequest successful!\r\n"));
			return USB_RET_SUCCESS;
		}
		else
		{
			fprintf(stdout, ("VendorOrClassRequest failer!\r\n"));
			return USB_RET_FAILD;
		}
	}
	else
	{
	}
	return USB_RET_HANDLE_INVALID;

}

UINT CUsbDevice::IsoRead(BYTE* buf, UINT siz)
{
	ULONG nBytesRead;
	ULONG success;
	if (m_DevHandle)
	{

		success = ReadFile(m_read_handle,
			buf,
			siz,
			&nBytesRead,
			NULL);

		printf("request %06.6d bytes -- %06.6d bytes read\n",
			siz, nBytesRead);
	}

	return nBytesRead;
}

UINT CUsbDevice::IsoWrite(BYTE* buf,UINT siz)
{
	ULONG nBytesWrite;
	if (m_frame_num % 4 == 0)
	{
		Sleep(20);
		int i = IsoRead(m_buf_in, 64);
		 while (i == 0){
			Sleep(200);
			i = IsoRead(m_buf_in, 64);
		}
	}
	m_frame_num++;
	if (m_DevHandle)
	{
		WriteFile(m_write_handle,
			buf,
			siz,
			&nBytesWrite,
			NULL);

		printf("request %06.6d bytes -- %06.6d bytes written\n",
			siz, nBytesWrite);
	}
	return USB_RET_HANDLE_INVALID;
}

UINT CUsbDevice::ReadIsoStream(BYTE* buf, UINT siz)
{ 
	//UINT32 success = 0;
	//ISO_TRANSFER_CONTROL transfer_control;
	//transfer_control.FramesPerBuffer = 0x20 ;
	//transfer_control.PacketCount = transfer_control.FramesPerBuffer;
	//transfer_control.BufferCount = 1;
	//transfer_control.PacketSize = siz / transfer_control.PacketCount;
	//if (0 != siz % transfer_control.PacketSize)
	//{
	//	transfer_control.PacketCount++;
	//}
	//transfer_control.PipeNum = (m_AudioClass2Des.Audio_Out_Fb_Endpoint.bEndpointAddress & 0x80) >> 7;
	//DWORD buf_out_siz = transfer_control.PacketCount * (transfer_control.PacketSize + sizeof(USBD_ISO_PACKET_DESCRIPTOR));

	//BYTE* buf_out = new BYTE[buf_out_siz];
	//memcpy(buf_out, buf, siz);
	//PUSBD_ISO_PACKET_DESCRIPTOR packet = (PUSBD_ISO_PACKET_DESCRIPTOR)buf_out;
	//packet->Length = siz;
	//packet->Offset = 0;
	//if (m_DevHandle)
	//{
	//	ULONG nBytes;

	//	if (m_DevHandle == INVALID_HANDLE_VALUE)
	//	{
	//		fprintf(stdout, "Please open Device First\r\n");
	//		return USB_RET_HANDLE_INVALID;
	//	}
	//	Sleep(1000);

	//	success = DeviceIoControl(m_DevHandle,
	//		IOCTL_USB_START_ISO_STREAM,
	//		&transfer_control,
	//		sizeof(ISO_TRANSFER_CONTROL),
	//		buf_out,
	//		buf_out_siz,
	//		&nBytes,
	//		NULL);
	//	if (success >= 0)
	//	{
	//		fprintf(stdout, ("ReadIsoStream successful!\r\n"));
	//		return USB_RET_SUCCESS;
	//	}
	//	else
	//	{
	//		fprintf(stdout, ("ReadIsoStream failer!\r\n"));
	//		return USB_RET_FAILD;
	//	}
	//}
	//else
	//{
	//}
	return USB_RET_HANDLE_INVALID;
}

//UINT CUsbDevice::ReadIsoStream(BYTE* buf, UINT siz)
//{ 
//	UINT32 success = 0;
//	ISO_TRANSFER_CONTROL transfer_control;
//	transfer_control.FramesPerBuffer = 0x20 ;
//	transfer_control.PacketCount = transfer_control.FramesPerBuffer;
//	transfer_control.BufferCount = 1;
//	transfer_control.PacketSize = siz / transfer_control.PacketCount;
//	if (0 != siz % transfer_control.PacketSize)
//	{
//		transfer_control.PacketCount++;
//	}
//	transfer_control.PipeNum = (m_AudioClass2Des.Audio_Out_Fb_Endpoint.bEndpointAddress & 0x80) >> 7;
//	DWORD buf_out_siz = transfer_control.PacketCount * (transfer_control.PacketSize + sizeof(USBD_ISO_PACKET_DESCRIPTOR));
//
//	BYTE* buf_out = new BYTE[buf_out_siz];
//	memcpy(buf_out, buf, siz);
//	PUSBD_ISO_PACKET_DESCRIPTOR packet = (PUSBD_ISO_PACKET_DESCRIPTOR)buf_out;
//	packet->Length = siz;
//	packet->Offset = 0;
//	if (m_DevHandle)
//	{
//		ULONG nBytes;
//
//		if (m_DevHandle == INVALID_HANDLE_VALUE)
//		{
//			fprintf(stdout, "Please open Device First\r\n");
//			return USB_RET_HANDLE_INVALID;
//		}
//		Sleep(1000);
//
//		success = DeviceIoControl(m_DevHandle,
//			IOCTL_USB_START_ISO_STREAM,
//			&transfer_control,
//			sizeof(ISO_TRANSFER_CONTROL),
//			buf_out,
//			buf_out_siz,
//			&nBytes,
//			NULL);
//		if (success >= 0)
//		{
//			fprintf(stdout, ("ReadIsoStream successful!\r\n"));
//			return USB_RET_SUCCESS;
//		}
//		else
//		{
//			fprintf(stdout, ("ReadIsoStream failer!\r\n"));
//			return USB_RET_FAILD;
//		}
//	}
//	else
//	{
//	}
//	return USB_RET_HANDLE_INVALID;
//}


UINT CUsbDevice::WriteIsoStream(BYTE* buf, UINT siz)
{
	return USB_RET_HANDLE_INVALID;

}

//UINT CUsbDevice::WriteIsoStream(BYTE* buf, UINT siz)
//{
//	UINT success;
//	ISO_TRANSFER_CONTROL transfer_control;
//	transfer_control.FramesPerBuffer = 0x20;
//	transfer_control.PacketCount = transfer_control.FramesPerBuffer;
//	transfer_control.BufferCount = 1;
//	transfer_control.PacketSize = siz / transfer_control.PacketCount;
//	if (0 != siz % transfer_control.PacketSize)
//	{
//		transfer_control.PacketCount++;
//	}
//	transfer_control.PipeNum = (m_AudioClass2Des.Audio_Out_Endpoint.bEndpointAddress & 0x80) >> 7;
//	DWORD buf_out_siz = transfer_control.PacketCount * (transfer_control.PacketSize + sizeof(USBD_ISO_PACKET_DESCRIPTOR));
//
//	BYTE* buf_out = new BYTE[buf_out_siz];
//	memcpy(buf_out, buf, siz);
//	PUSBD_ISO_PACKET_DESCRIPTOR packet = (PUSBD_ISO_PACKET_DESCRIPTOR)buf_out;
//	packet->Length = siz;
//	packet->Offset = 0;
//
//	if (m_DevHandle)
//	{
//		ULONG nBytes;
//
//		if (m_DevHandle == INVALID_HANDLE_VALUE)
//		{
//			fprintf(stdout, "Please open Device First\r\n");
//			return USB_RET_HANDLE_INVALID;
//		}
//		Sleep(2000);
//		success = DeviceIoControl(m_DevHandle,
//			IOCTL_USB_START_ISO_STREAM,
//			&transfer_control,
//			sizeof(ISO_TRANSFER_CONTROL),
//			buf_out,
//			buf_out_siz,
//			&nBytes,
//			NULL);
//		if (success >= 0)
//		{
//			fprintf(stdout, ("WriteIsoStream successful!\r\n"));
//			return USB_RET_SUCCESS;
//		}
//		else
//		{
//			fprintf(stdout, ("WriteIsoStream failer!\r\n"));
//			return USB_RET_FAILD;
//		}
//	}
//	else
//	{
//	}
//	delete buf_out;
//	return USB_RET_HANDLE_INVALID;
//
//}
UINT CUsbDevice::StopIsoStream()
{
	//UINT success;
	//CHAR buf[3];
	//if (m_DevHandle)
	//{
	//	ULONG nBytes;

	//	if (m_DevHandle == INVALID_HANDLE_VALUE)
	//	{
	//		fprintf(stdout, "Please open Device First\r\n");
	//		return USB_RET_HANDLE_INVALID;
	//	}

	//	success = DeviceIoControl(m_DevHandle,
	//		IOCTL_USB_STOP_ISO_STREAM,
	//		NULL,
	//		sizeof(VENDOR_OR_CLASS_REQUEST_CONTROL),
	//		buf,
	//		3,
	//		&nBytes,
	//		NULL);

	//	if (success >= 0)
	//	{
	//		fprintf(stdout, ("StopIsoStream successful!\r\n"));
	//		return USB_RET_SUCCESS;
	//	}
	//	else
	//	{
	//		fprintf(stdout, ("StopIsoStream failer!\r\n"));
	//		return USB_RET_FAILD;
	//	}
	//}

	return USB_RET_HANDLE_INVALID;
}

UINT CUsbDevice::ReadIsoBuffer(){ return USB_RET_SUCCESS; }
UINT CUsbDevice::SetFeature(){ return USB_RET_SUCCESS; }


HANDLE CUsbDevice::open_file(char *filename)
/*++
Routine Description:

Called by main() to open an instance of our device after obtaining its name

Arguments:

None

Return Value:

Device handle on success else NULL

--*/
{

	int success = 1;
	HANDLE h;
	char completeDeviceName[MAX_PATH];
	if (m_DevHandle)
	{
		sprintf(completeDeviceName, "%s\\%s", m_DevList.DeviceName[m_DevNum], filename);

		printf("completeDeviceName = (%s)\n", m_DevList.DeviceName[m_DevNum]);

		h = CreateFile(completeDeviceName,
			GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);

		if (h == INVALID_HANDLE_VALUE) {
			NOISY(("Failed to open (%s) = %d", completeDeviceName, GetLastError()));
			success = 0;
		}
		else {
			NOISY(("Opened successfully.\n"));
			return h;
		}
	}
	return 0;
}
