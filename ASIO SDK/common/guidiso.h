/*++

Copyright (c) 1997-1998  Microsoft Corporation

Module Name:

    GuidIso.h

Abstract:

 The below GUID is used to generate symbolic links to
  driver instances created from user mode

Environment:

    Kernel & user mode

Notes:

  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
  PURPOSE.

  Copyright (c) 1997-1998 Microsoft Corporation.  All Rights Reserved.

Revision History:

    2/11/98 : created

--*/
#ifndef GUIDISOH_INC
#define GUIDISOH_INC

#include <initguid.h>
#define BULK

// {A1155B78-A32C-11d1-9AED-00A0C98BA608} for IsoUsb.sys
#ifdef BULK
DEFINE_GUID(GUID_CLASS_I82930_ISO,
	0x873fdf61, 0x61a8, 0x11d1, 0xaa, 0x5e, 0x0, 0xc0, 0x4f, 0xb1, 0x72, 0x8b);
#else
DEFINE_GUID(GUID_CLASS_I82930_ISO, 
	0xa1155b78, 0xa32c, 0x11d1, 0x9a, 0xed, 0x0, 0xa0, 0xc9, 0x8b, 0xa6, 0x8);
#endif


#endif // end, #ifndef GUIDISOH_INC

