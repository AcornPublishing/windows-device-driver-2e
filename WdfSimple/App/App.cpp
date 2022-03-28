// app.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <Windows.h>
#include <winioctl.h>
#include <setupapi.h>

#pragma comment(lib,"setupapi.lib")

// 드라이버와 연결하기 위해서 드라이버소스에서 사용한 동일한 GUID를 정의한다
#include <initguid.h>

DEFINE_GUID(GUID_SIMPLE, 
0x5665dec0, 0xa40a, 0x11d1, 0xb9, 0x84, 0x0, 0x20, 0xaf, 0xd7, 0x97, 0x62);

// DeviceIoControl API에 사용할 IOControlCode를 정의한다
#define SIMPLEIOCTLBASE		       0xA00
#define IOCTL_SIMPLE_CONTROL	CTL_CODE(FILE_DEVICE_UNKNOWN, SIMPLEIOCTLBASE, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define MAXDEVICENUMBER 10 // 같은 Interface GUID를 사용하는 디바이스는 최대 10개까지 검색하려는 의도를 가집니다

/*
	GetDeviceStackNameCount()함수와 GetDeviceStackName()함수는 제가 임의로 작성한 지역함수입니다.
	이곳에서는 어떤 방법으로 InterfaceGUID를 사용하여 디바이스스택의 이름을 구하는지를 알 수 있습니다
*/

// 현재 제공되는 GUID를 지원하는 DeviceStack의 개수를 알려줍니다
int GetDeviceStackNameCount( struct _GUID * pGuid )
{
	SP_INTERFACE_DEVICE_DATA interfaceData;
	int index=0;
	HDEVINFO Info = SetupDiGetClassDevs( pGuid, 0, 0, DIGCF_PRESENT|DIGCF_INTERFACEDEVICE );

	if( Info == (HDEVINFO) -1 )
		return 0; // 시스템은 이런 명령을 지원하지 못한다

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
	
	while( 1 )
	{
		BOOLEAN bl;
		bl = SetupDiEnumDeviceInterfaces( Info, 0, pGuid, index, &interfaceData );
		if( bl == FALSE )
			break;
		index++;
	}

	SetupDiDestroyDeviceInfoList( Info );

	return index;
}

// 현재 제공되는 GUID를 지원하는 DeviceStack의 이름들을 알려줍니다
BOOLEAN GetDeviceStackName( struct _GUID * pGuid, WCHAR ** ppDeviceName, int index )
// index : 같은 Interface GUID를 가진 장치가 여러개인경우, 가장 먼저 검색되는 장치를 선택하려면 이곳에 0을 기록합니다
{
	DWORD size;
	BOOLEAN bl;
	SP_INTERFACE_DEVICE_DATA interfaceData;
	PSP_INTERFACE_DEVICE_DETAIL_DATA pData;
	HDEVINFO Info = SetupDiGetClassDevs( pGuid, 0, 0, DIGCF_PRESENT|DIGCF_INTERFACEDEVICE );
	WCHAR *pDeviceName;
	*ppDeviceName = (WCHAR *)0;

	if( Info == (HANDLE) -1 )
		return FALSE;

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);
	
	bl = SetupDiEnumDeviceInterfaces( Info, 0, pGuid, index, &interfaceData );
	if( bl == FALSE )
		return bl;

	SetupDiGetDeviceInterfaceDetail( Info, &interfaceData, 0, 0, &size, 0 );
	pData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc( size );
	pData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
	SetupDiGetDeviceInterfaceDetail( Info, &interfaceData, pData, size, 0, 0 );

	pDeviceName = (WCHAR *)malloc( (wcslen(pData->DevicePath) + 1) * sizeof(WCHAR) );
	memset( pDeviceName, 0, (wcslen(pData->DevicePath) + 1) * sizeof(WCHAR));
	wcscpy( pDeviceName, pData->DevicePath );
	free( pData );

	SetupDiDestroyDeviceInfoList( Info );
	*ppDeviceName = pDeviceName;
	return TRUE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	int count;
	HANDLE handle;
	WCHAR *pDeviceName;
	BOOLEAN bl;

	count = GetDeviceStackNameCount( (struct _GUID *)&GUID_SIMPLE );
	if( count == 0 )
	{
		MessageBox( NULL, L"드라이버장치가 없습니다", L"정보", MB_ICONERROR );
		return 0; // 시스템은 SampleGuid를 지원하는 장치가 설치되지 않았습니다
	}

	bl = GetDeviceStackName( (struct _GUID *)&GUID_SIMPLE, &pDeviceName, 0 ); 

	if( bl == FALSE )
	{
		MessageBox( NULL, L"드라이버장치가 없습니다", L"정보", MB_ICONERROR );
		return 0;
	}

	// pDeviceName은 디바이스스택에 접근하기 위한 이름이 구해집니다
	handle = CreateFile( pDeviceName, GENERIC_READ|GENERIC_WRITE
					   , 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL
					   , 0 );
	// 이때 드라이버측으로 IRP_MJ_CREATE명령어가 전달됩니다
	if( handle == (HANDLE)-1 )
	{
		MessageBox( NULL, L"드라이버와 연결되지 않습니다(CreateFile 에러)", L"정보", MB_ICONERROR );

		free( pDeviceName );
		return 0; // Stack은 있지만, 현재 접근이 금지되어 있다
	}

	MessageBox( NULL, L"드라이버와 연결되었습니다. 통신을 시작합니다", L"정보", MB_OK );

	// 쓰기와 읽기를 합니다
	// 물론 특별한 결과는 없습니다
	DWORD ret;
	char WriteData[100];
	char ReadData[100];

	WriteFile( handle, WriteData, sizeof(WriteData), &ret, NULL );
	ReadFile( handle, ReadData, sizeof(ReadData), &ret, NULL );

	// DeviceIoControl 함수를 사용해봅니다
	DeviceIoControl( handle, IOCTL_SIMPLE_CONTROL, WriteData, sizeof(WriteData), ReadData, sizeof(ReadData), &ret, NULL );	

	CloseHandle( handle );

	MessageBox( NULL, L"드라이버와 연결을 해제하였습니다", L"정보", MB_OK );

	free( pDeviceName );
	return 0;
}

