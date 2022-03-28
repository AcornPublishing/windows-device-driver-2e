// adddevice.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>

#include <initguid.h>
#include <setupapi.h>
#include <stdio.h>
#include ".\..\common\common.h"
#include ".\..\common\hidmapper.h"
#include ".\..\common\driverapi.h"

#pragma comment(lib,"setupapi.lib")

#define LEDSHOWTEST

int main(int argc, char* argv[])
{
	HANDLE hDevice;
	BOOLEAN bRet = FALSE;
	int count = 0;
	HIDKEYCODE HKeyCode = { 0, }; // 배열로 정의해서 한번에 여러개의 엔트리를 보내는것도 가능하다
	HIDMOUSECODE HMouseCode = { 0, }; // 배열로 정의해서 한번에 여러개의 엔트리를 보내는것도 가능하다

	_HK_ClientDriverApiInitialize();

	hDevice = _HK_ClientDriverApiFindDriverName();
	if (hDevice == (HANDLE)-1)
	{
		return 0; 
	}

#ifdef LEDEVENTTEST
	// 드라이버로 키보드 LED 이벤트를 전달한다
	HANDLE hLED_Event = 0;
	hLED_Event = CreateEvent(NULL, FALSE, FALSE, NULL);
	bRet = _HK_ClientDriverApiSetLEDEvent(hDevice, hLED_Event);

	// 키보드의 LED를 켜본다
	memset(&HKeyCode, 0, sizeof(HIDKEYCODE));

	HKeyCode.Key[0] = 0x53; // NUMLOCK버튼을 누른다
	_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
	Sleep(100);
	HKeyCode.Key[0] = 0;
	_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

	// 운영체제가 LED를 켜라는 메시지를 전달했는지를 확인한다
	WaitForSingleObject(hLED_Event, INFINITE);

	// 드라이버로부터 키보드 LED 데이타를 읽어본다
	HIDLED	HidLedData = { 0, };
	_HK_ClientDriverApiQueryIndicators(hDevice, &HidLedData);

	if (hLED_Event)
		CloseHandle(hLED_Event);
#endif

#ifdef LEDSHOWTEST 

	for (count = 0; count < 1000; count++)
	{
		// 키보드의 LED를 켜본다
		memset(&HKeyCode, 0, sizeof(HIDKEYCODE));

		HKeyCode.Key[0] = 0x53;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0x39;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0x47;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
	}

	for (count = 0; count < 10; count++)
	{
		// 키보드의 LED를 켜본다
		memset(&HKeyCode, 0, sizeof(HIDKEYCODE));

		HKeyCode.Key[0] = 0x53; // NUMLOCK버튼을 누른다
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		Sleep(100);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(100);

		HKeyCode.Key[0] = 0x39; // CAPSLOCK버튼을 누른다
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		Sleep(100);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(100);

		HKeyCode.Key[0] = 0x47; // SCROLLLOCK버튼을 누른다
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		Sleep(100);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(100);

		HKeyCode.u.Modifier.LWINDOW = 1; // Window 키가 눌려진것처럼 해본다
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(100);

		HKeyCode.u.Modifier.LWINDOW = 0; // Window 키가 띄어진것처럼 해본다
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(100);
	}

	for (count = 0; count < 1000; count++)
	{
		// 키보드의 LED를 켜본다
		memset(&HKeyCode, 0, sizeof(HIDKEYCODE));

		HKeyCode.Key[0] = 0x53;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0x39;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0x47;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
	}

	for (count = 0; count < 10; count++)
	{
		// 키보드의 LED를 켜본다
		memset(&HKeyCode, 0, sizeof(HIDKEYCODE));

		// NUMLOCK, CAPSLOCK, SCROLLLOCK버튼을 누른다
		HKeyCode.Key[0] = 0x53; 
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(10);

		HKeyCode.Key[0] = 0x39;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(10);
		HKeyCode.Key[0] = 0x47;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(10);

		HKeyCode.u.Modifier.LWINDOW = 1; // Window 키가 눌려진것처럼 해본다
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(10);

		HKeyCode.u.Modifier.LWINDOW = 0; // Window 키가 띄어진것처럼 해본다
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);

		Sleep(10);
	}

	for (count = 0; count < 1000; count++)
	{
		// 키보드의 LED를 켜본다
		memset(&HKeyCode, 0, sizeof(HIDKEYCODE));

		HKeyCode.Key[0] = 0x53;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0x39;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0x47;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
		HKeyCode.Key[0] = 0;
		_HK_ClientDriverApiSimulateKey(hDevice, &HKeyCode);
	}
#endif

#ifdef MOUSETEST 

	srand(GetTickCount());

	for (count = 0; count < 500; count++)
	{
		memset(&HMouseCode, 0, sizeof(HIDMOUSECODE));
		int z = rand();
		if (z%4 == 0)
		{
			HMouseCode.X = rand()%40;
			HMouseCode.Y = rand() % 40;
		}
		if (z%4 == 1)
		{
			HMouseCode.X = (rand() % 40) * -1;
			HMouseCode.Y = rand() % 40;
		}
		if (z%4 == 2)
		{
			HMouseCode.X = rand() % 40;
			HMouseCode.Y = (rand() % 40) * -1;
		}
		if (z%4 == 3)
		{
			HMouseCode.X = (rand() % 40) * -1;
			HMouseCode.Y = (rand() % 40) * -1;
		}
		_HK_ClientDriverApiSimulateMouse(handle, &HMouseCode);
		Sleep(8);
	}
#endif

	_HK_ClientDriverApiCloseHandle( hDevice );

	return 0;
}
