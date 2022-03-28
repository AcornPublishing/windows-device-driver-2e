#pragma once

#include "stdafx.h"
#include <windows.h>
#include "..\common\hidmapper.h"

void _HK_ClientDriverApiInitialize();
HANDLE _HK_ClientDriverApiFindDriverName();
BOOLEAN _HK_ClientDriverApiSetLEDEvent(HANDLE hDriverHandle, HANDLE hEvent);
BOOLEAN _HK_ClientDriverApiQueryIndicators(HANDLE hDriverHandle, HIDLED * pIndicators);
BOOLEAN _HK_ClientDriverApiSimulateKey(HANDLE hDriverHandle, HIDKEYCODE * pKeyCode);
BOOLEAN _HK_ClientDriverApiSimulateMouse(HANDLE hDriverHandle, HIDMOUSECODE * pMouseCode);
void _HK_ClientDriverApiCloseHandle(HANDLE hDriverHandle);



