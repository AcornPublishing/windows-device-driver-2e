// AddDevice.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"

#include <windows.h>

#include <initguid.h>
#include <setupapi.h>
#include <stdio.h>

DEFINE_GUID(SampleGuid, 0x5665dec0, 0xa40a, 0x11d1, 0xb9, 0x84, 0x0, 0x20, 0xaf, 0xd7, 0x97, 0x78);
// SampleGuid를 사용합니다. 이런 GUID는 현재 윈도우즈에서 정의되지 않은 값으로서, 샘플로 정의하여 사용합니다

#define MAXDEVICENUMBER 10 // 10개 까지의 같은 Guid를 사용하는 장치를 지원한다는 의미입니다

#include <winioctl.h>

// 응용프로그램과 주고받을 ControlCode를 정의합니다
#define IOCTL_BUSDRIVER_ADDDEVICE		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BUSDRIVER_REMOVEDEVICE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0801, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma comment(lib,"SetupApi")

// 현재 제공되는 GUID를 지원하는 DeviceStack의 개수를 알려줍니다
int GetDeviceStackNameCount(struct _GUID * pGuid)
{
	SP_INTERFACE_DEVICE_DATA interfaceData;
	int index = 0;
	HDEVINFO Info = SetupDiGetClassDevs(pGuid, 0, 0, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

	if (Info == (HDEVINFO)-1)
		return 0; // 시스템은 이런 명령을 지원하지 못한다

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

	while (1)
	{
		BOOLEAN bl;
		bl = SetupDiEnumDeviceInterfaces(Info, 0, pGuid, index, &interfaceData);
		if (bl == FALSE)
			break;
		index++;
	}

	SetupDiDestroyDeviceInfoList(Info);

	return index;
}

// 현재 제공되는 GUID를 지원하는 DeviceStack의 이름들을 알려줍니다
BOOLEAN GetDeviceStackName(struct _GUID * pGuid, WCHAR ** ppDeviceName, int index)
{
	DWORD size;
	BOOLEAN bl;
	SP_INTERFACE_DEVICE_DATA interfaceData;
	PSP_INTERFACE_DEVICE_DETAIL_DATA pData;
	HDEVINFO Info = SetupDiGetClassDevs(pGuid, 0, 0, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
	WCHAR *pDeviceName;
	*ppDeviceName = (WCHAR *)0;

	if (Info == (HANDLE)-1)
		return FALSE;

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

	bl = SetupDiEnumDeviceInterfaces(Info, 0, pGuid, index, &interfaceData);
	if (bl == FALSE)
		return bl;

	SetupDiGetDeviceInterfaceDetail(Info, &interfaceData, 0, 0, &size, 0);
	pData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(size);
	pData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
	SetupDiGetDeviceInterfaceDetail(Info, &interfaceData, pData, size, 0, 0);

	pDeviceName = (WCHAR *)malloc((wcslen(pData->DevicePath) + 1)*sizeof(WCHAR));
	pDeviceName[0] = NULL;
	wcscpy(pDeviceName, pData->DevicePath);
	free(pData);

	SetupDiDestroyDeviceInfoList(Info);
	*ppDeviceName = pDeviceName;
	return TRUE;
}

int main()
{
	HANDLE handle;
	int count;
	WCHAR *pDeviceName;
	BOOLEAN bl;
	ULONG ret;
	
	count = GetDeviceStackNameCount((struct _GUID *)&SampleGuid);
	if (count == 0)
		return 0; // 시스템은 SampleGuid를 지원하는 장치가 설치되지 않았습니다

	bl = GetDeviceStackName((struct _GUID *)&SampleGuid, &pDeviceName, 0); // 당연히 1나이상의 장치는 설치되어있으므로..0을 사용한다
																		   // pDeviceName는 함수내에서 할당되는 메모리이므로, 사용이 끝나면 반드시 해제하여야 한다

	if (bl == FALSE)
		return 0; // 이런경우는 없어야 한다

	handle = CreateFile(pDeviceName, GENERIC_READ | GENERIC_WRITE
		, 0, 0, OPEN_EXISTING, 0
		, 0);
	if (handle == (HANDLE)-1)
	{
		free(pDeviceName);
		return 0; // Stack은 있지만, 현재 접근이 금지되어 있다
	}

	DeviceIoControl(handle, IOCTL_BUSDRIVER_ADDDEVICE, NULL, 0, NULL, 0, &ret, NULL);

	CloseHandle(handle);

	free(pDeviceName);
	return 0;

	return 0;
}

