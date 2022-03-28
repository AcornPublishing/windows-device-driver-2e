#include "stdafx.h"

#include <Windows.h>
#include <winioctl.h>
#include ".\..\common\common.h"
#include ".\..\common\driverapi.h"

#include <stdio.h>      /* printf, scanf, NULL */
#include <stdlib.h>

#include <setupapi.h>

#pragma comment(lib,"setupapi.lib")

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
		BOOL bl;
		bl = SetupDiEnumDeviceInterfaces(Info, 0, pGuid, index, &interfaceData);
		if (bl == FALSE)
			break;
		index++;
	}

	SetupDiDestroyDeviceInfoList(Info);

	return index;
}

// 현재 제공되는 GUID를 지원하는 DeviceStack의 이름들을 알려줍니다
BOOL GetDeviceStackName(struct _GUID * pGuid, TCHAR ** ppDeviceName, int index)
// index : 같은 Interface GUID를 가진 장치가 여러개인경우, 가장 먼저 검색되는 장치를 선택하려면 이곳에 0을 기록합니다
{
	DWORD size;
	BOOL bl;
	SP_INTERFACE_DEVICE_DATA interfaceData;
	PSP_INTERFACE_DEVICE_DETAIL_DATA pData;
	HDEVINFO Info = SetupDiGetClassDevs(pGuid, 0, 0, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
	TCHAR *pDeviceName;
	*ppDeviceName = (TCHAR *)0;

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

	pDeviceName = (TCHAR *)malloc((wcslen(pData->DevicePath) + 1) * sizeof(TCHAR));
	memset(pDeviceName, 0, wcslen(pData->DevicePath) * sizeof(TCHAR));
	wcscpy(pDeviceName, pData->DevicePath);
	free(pData);

	SetupDiDestroyDeviceInfoList(Info);
	*ppDeviceName = pDeviceName;
	return TRUE;
}

///////////////////////////////////////////////////////////////

HANDLE _HK_ClientDriverApiFindDriverName()
{
	BOOL bl;
	int count = 0;
	int index = 0;
	WCHAR DeviceName[MAX_PATH];
	WCHAR *pDeviceName = NULL;
	HANDLE hDevice = (HANDLE)-1;

	count = GetDeviceStackNameCount((struct _GUID *)&GUID_VUSBBUS);

	if (count == 0)
		goto exit;

	bl = GetDeviceStackName((struct _GUID *)&GUID_VUSBBUS, &pDeviceName, 0);

	if (bl == TRUE)
	{
		wcscpy(DeviceName, pDeviceName);
		free(pDeviceName);
		hDevice = CreateFile(DeviceName, FILE_READ_DATA | FILE_WRITE_DATA,
			0, NULL, OPEN_EXISTING, 0, NULL);
	}

exit:
	return hDevice;
}

BOOLEAN _HK_ClientDriverApiSetLEDEvent(HANDLE hDriverHandle, HANDLE hEvent)
{
	BOOLEAN bRet = FALSE;
	DWORD ret;

	if (hDriverHandle == (HANDLE)-1)
		goto exit;

	bRet = DeviceIoControl(hDriverHandle, IOCTL_BUSDRIVER_SENDKEYBOARD_LED_EVENT, &hEvent, sizeof(HANDLE), NULL, 0, &ret, NULL);

exit:
	return bRet;
}

BOOLEAN _HK_ClientDriverApiQueryIndicators(HANDLE hDriverHandle, HIDLED * pIndicators)
{
	BOOLEAN bRet = FALSE;
	DWORD ret;

	if (hDriverHandle == (HANDLE)-1)
		goto exit;

	bRet = DeviceIoControl(hDriverHandle, _IOCTL_KEYBOARD_QUERY_INDICATORS, NULL, 0,
		pIndicators, sizeof(KEYBOARD_HOOK_INDICATOR_PARAMETERS), &ret, NULL);

exit:
	return bRet;
}

BOOLEAN _HK_ClientDriverApiSimulateKey(HANDLE hDriverHandle, HIDKEYCODE * pKeyCode)
{
	BOOLEAN bRet = FALSE;
	DWORD ret;

	if (hDriverHandle == (HANDLE)-1)
		goto exit;

	bRet = DeviceIoControl(hDriverHandle, IOCTL_BUSDRIVER_SENDKEYBOARD, pKeyCode, sizeof(HIDKEYCODE), NULL, 0, &ret, NULL);

exit:
	return bRet;
}

BOOLEAN _HK_ClientDriverApiSimulateMouse(HANDLE hDriverHandle, HIDMOUSECODE * pMouseCode)
{
	BOOLEAN bRet = FALSE;
	DWORD ret;

	if (hDriverHandle == (HANDLE)-1)
		goto exit;

	bRet = DeviceIoControl(hDriverHandle, IOCTL_BUSDRIVER_SENDMOUSE, pMouseCode, sizeof(HIDMOUSECODE), NULL, 0, &ret, NULL);

exit:
	return bRet;
}

void _HK_ClientDriverApiInitialize()
{
}

void _HK_ClientDriverApiCloseHandle(HANDLE hDriverHandle)
{
	CloseHandle(hDriverHandle);
}
