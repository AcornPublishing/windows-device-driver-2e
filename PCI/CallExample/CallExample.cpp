// CallExample.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <windows.h>
#include <setupapi.h>

#include <winioctl.h>

#pragma comment(lib,"SetupApi")

#include <initguid.h>

DEFINE_GUID(GUID_DEVINTERFACE_PCISAMPLE,
	0x946A7B1A, 0xEBBC, 0x422A, 0xA8, 0x1F, 0xF0, 0x7C, 0x8E, 0x41, 0xD1, 0x11);

typedef struct {
	USHORT  VendorID;
	USHORT  DeviceID;
	USHORT  Command;
	USHORT  Status;
	UCHAR   RevisionID;
	UCHAR   ProgIf;
	UCHAR   SubClass;
	UCHAR   BaseClass;
	UCHAR   CacheLineSize;
	UCHAR   LatencyTimer;
	UCHAR   HeaderType;
	UCHAR   BIST;

	union {
		struct _PCIDEF_HEADER_TYPE_0 {
			ULONG   BaseAddresses[6];
			ULONG   CIS;
			USHORT  SubVendorID;
			USHORT  SubSystemID;
			ULONG   ROMBaseAddress;
			UCHAR   CapabilitiesPtr;
			UCHAR   Reserved1[3];
			ULONG   Reserved2;
			UCHAR   InterruptLine;
			UCHAR   InterruptPin;
			UCHAR   MinimumGrant;
			UCHAR   MaximumLatency;
		} type0;
	} u;
} PCIDEF_COMMON_HEADER, *PPCIDEF_COMMON_HEADER;

typedef struct _PCISAMPLE_MAPPING_MEMORY
{
	ULONGLONG				Reserved1;
	ULONGLONG				Reserved2;
	ULONGLONG				PhysicalAddress;
	ULONGLONG				Reserved3;
	ULONGLONG				UserAddress;
	ULONGLONG				Size;
}PCISAMPLE_MAPPING_MEMORY, *PPCISAMPLE_MAPPING_MEMORY;

#define IOCTL_PCISAMPLE_REGISTER_SHARED_EVENT				CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA00, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_MAPPING_MEMORY						CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA01, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_UNMAPPING_MEMORY					CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA02, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_ALLOCATE_CONTIGUOUS_MEMORY_FOR_DMA	CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA03, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_FREE_CONTIGUOUS_MEMORY_FOR_DMA		CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA04, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_GET_CONFIGURATION_REGISTER			CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA05, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ------------- 디바이스 스택의 이름을 구하는 함수를 정의한다

static int GetDeviceStackNameCount()
{
	SP_INTERFACE_DEVICE_DATA interfaceData;
	int index = 0;
	HDEVINFO Info = SetupDiGetClassDevs((struct _GUID *)&GUID_DEVINTERFACE_PCISAMPLE, 0, 0, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

	if (Info == (HDEVINFO)-1)
		return 0; 

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

	while (1)
	{
		BOOLEAN bl;
		bl = SetupDiEnumDeviceInterfaces(Info, 0, (struct _GUID *)&GUID_DEVINTERFACE_PCISAMPLE, index, &interfaceData);
		if (bl == FALSE)
			break;
		index++;
	}

	SetupDiDestroyDeviceInfoList(Info);

	return index;
}

static BOOLEAN GetDeviceStackName(WCHAR * pDeviceName, int index)
{
	DWORD size;
	BOOLEAN bl;
	SP_INTERFACE_DEVICE_DATA interfaceData;
	PSP_INTERFACE_DEVICE_DETAIL_DATA pData;
	HDEVINFO Info = SetupDiGetClassDevs((struct _GUID *)&GUID_DEVINTERFACE_PCISAMPLE, 0, 0, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

	if (Info == (HANDLE)-1)
		return FALSE;

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

	bl = SetupDiEnumDeviceInterfaces(Info, 0, (struct _GUID *)&GUID_DEVINTERFACE_PCISAMPLE, index, &interfaceData);
	if (bl == FALSE)
		return bl;

	SetupDiGetDeviceInterfaceDetail(Info, &interfaceData, 0, 0, &size, 0);
	pData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(size);
	pData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
	SetupDiGetDeviceInterfaceDetail(Info, &interfaceData, pData, size, 0, 0);

	wcscpy(pDeviceName, pData->DevicePath);
	free(pData);

	SetupDiDestroyDeviceInfoList(Info);
	return TRUE;
}

// ------------- Main

int main()
{
	BOOLEAN bRet = FALSE;
	ULONG dwRet;
	HANDLE hInterruptEvent = NULL;
	HANDLE hFile = (HANDLE)-1;
	WCHAR DeviceStackName[MAX_PATH] = { 0, };
	PCIDEF_COMMON_HEADER PciConfigurationRegister = { 0, };
	PCISAMPLE_MAPPING_MEMORY	PCIMappingInformation = { 0, };
	PCISAMPLE_MAPPING_MEMORY	PCIDMAMappingInformation = { 0, };

	bRet = GetDeviceStackName(DeviceStackName, 0); // 첫번째로 발견되는 디바이스 스택의 이름을 구한다
	if (bRet == FALSE)
		goto exit; // 디바이스가 발견되지 않는다

	hFile = CreateFile(DeviceStackName, GENERIC_READ | GENERIC_WRITE
		, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0); // 디바이스 드라이버와 연결한다

	if(hFile == (HANDLE)-1)
		goto exit; // 디바이스가 발견되지 않는다

	/*
	IOCTL 명령어를 활용해서 다양한 테스트를 진행해볼 수 있다
	다만 개발자가 사용하는 실제 하드웨어를 위한 코드에 적용될 수 있는 부분과 그렇지 않은 부분을 엄밀하게 따져보아야 한다
	*/

	// 인터럽트가 발생하면 시그널될 이벤트를 등록한다
	hInterruptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hInterruptEvent == (HANDLE)-1)
	{
		goto exit; 
	}
	bRet = DeviceIoControl(hFile, IOCTL_PCISAMPLE_REGISTER_SHARED_EVENT, 
		&hInterruptEvent, sizeof(HANDLE), 
		NULL, 0, 
		&dwRet, NULL);

	// PCI Configuration Register를 읽어본다
	bRet = DeviceIoControl(hFile, IOCTL_PCISAMPLE_GET_CONFIGURATION_REGISTER, 
		NULL, 0, 
		&PciConfigurationRegister, sizeof(PCIDEF_COMMON_HEADER), 
		&dwRet, NULL);

	// PCI Configuration Register에서 얻은 물리주소(BAR)를 사용해서 응용프로그램이 접근하는 목적의 가상주소를 매핑한다
	// 위험한 부분이므로 정확하게 기능을 숙지하지 않은 독자를 위해 주석처리한다
	/*
	PCIMappingInformation.PhysicalAddress = 0xXXXXXXXX; // 접근하고자 하는 물리주소를 기록한다
	PCIMappingInformation.Size = 0xXXXXXXXX; // 접근하려는 물리주소의 크기를 기록한다

	bRet = DeviceIoControl(hFile, IOCTL_PCISAMPLE_MAPPING_MEMORY, 
		&PCIMappingInformation, sizeof(PCISAMPLE_MAPPING_MEMORY), 
		&PCIMappingInformation, sizeof(PCISAMPLE_MAPPING_MEMORY), 
		&dwRet, NULL);

	PCIMappingInformation.UserAddress; // 매핑된 가상주소가 얻어진다
	*/

	// 반드시 사용이 끝난 가상주소를 반납해야 한다
	/*
	bRet = DeviceIoControl(hFile, IOCTL_PCISAMPLE_UNMAPPING_MEMORY, 
		&PCIMappingInformation, sizeof(PCISAMPLE_MAPPING_MEMORY),
		NULL, 0, 
		&dwRet, NULL);
	*/

	// DMA를 사용하는 경우, 연속적인 크기를 가지는 호스트 물리메모리공간이 확보되어져야 한다
	// 이런 부분도 드라이버를 통해서 얻을 수 있다
	// 위험한 부분이므로 정확하게 기능을 숙지하지 않은 독자를 위해 주석처리한다
	/*
	ULONGLONG Size = 0xXXXXXX; // 할당하고자 하는 연속적인 물리메모리크기를 정의한다.이 크기는 최소 1MB이상이어야 한다

	bRet = DeviceIoControl(hFile, IOCTL_PCISAMPLE_ALLOCATE_CONTIGUOUS_MEMORY_FOR_DMA,
		&Size, sizeof(ULONGLONG), 
		&PCIDMAMappingInformation, sizeof(PCISAMPLE_MAPPING_MEMORY),
		&dwRet, NULL);
	*/

	// 반드시 사용이 끝난 가상주소를 반납해야 한다
	/*
	bRet = DeviceIoControl(hFile, IOCTL_PCISAMPLE_FREE_CONTIGUOUS_MEMORY_FOR_DMA,
		&PCIDMAMappingInformation, sizeof(PCISAMPLE_MAPPING_MEMORY),
		NULL, 0, 
		&dwRet, NULL);
	*/

exit:
	if(hInterruptEvent)
		CloseHandle(hInterruptEvent);

	if(hFile != (HANDLE)-1)
		CloseHandle(hFile); // 디바이스 드라이버와 연결을 해제한다
	
	return 0;
}

