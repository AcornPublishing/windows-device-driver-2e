#if !defined(_SIMPLE_H_)
#define _SIMPLE_H_

#include <ntddk.h>
#include <wdf.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "devioctl.h"
#include <initguid.h>

DEFINE_GUID(GUID_SIMPLE, 
0x5665dec0, 0xa40a, 0x11d1, 0xb9, 0x84, 0x0, 0x20, 0xaf, 0xd7, 0x97, 0x63);

#define SIMPLEIOCTLBASE		       0xA00
#define IOCTL_SIMPLE_CONTROL	CTL_CODE(FILE_DEVICE_UNKNOWN, SIMPLEIOCTLBASE, METHOD_BUFFERED, FILE_ANY_ACCESS)
// 응용프로그램측에서 DeviceIoControl() API를 호출할때 사용하는 IOControlCode

typedef struct _FDO_DATA
{
	char Reserved;
}  FDO_DATA, *PFDO_DATA;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(FDO_DATA, SIMPLEFdoGetData)

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD SIMPLEEvtDeviceAdd;
EVT_WDF_IO_QUEUE_IO_READ SIMPLEEvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE SIMPLEEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL SIMPLEEvtIoDeviceControl;

#endif  // _SIMPLE_H_

