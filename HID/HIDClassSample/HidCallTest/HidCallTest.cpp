// HidCallTest.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <Windows.h>
#include <initguid.h>
#include <SetupAPI.h>
extern "C"
{
#include <hidsdi.h>
}

#include <stdio.h>
#include <conio.h>

#pragma comment(lib,"SetupApi")
#pragma comment(lib,"hid")

#define MAXDEVICENUMBER 10 // 같은 Interface GUID를 사용하는 디바이스는 최대 10개까지 검색하려는 의도를 가집니다
#define MYDEVICE	_T("HID\\VID_1234&PID_5678")

/*
GetDeviceStackNameCount()함수와 GetDeviceStackName()함수는 제가 임의로 작성한 지역함수입니다.
이곳에서는 어떤 방법으로 InterfaceGUID를 사용하여 디바이스스택의 이름을 구하는지를 알 수 있습니다
*/

// 현재 제공되는 GUID를 지원하는 DeviceStack의 개수를 알려줍니다
int GetDeviceStackNameCount(struct _GUID * pGuid)
{
	SP_DEVINFO_DATA dinfo;
	SP_INTERFACE_DEVICE_DATA interfaceData;
	int index = 0;
	int count = 0;
	HDEVINFO Info = SetupDiGetClassDevs(pGuid, 0, 0, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

	if (Info == (HDEVINFO)-1)
		return 0; // 시스템은 이런 명령을 지원하지 못한다

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

	while (1)
	{
		BOOLEAN bl;
		DWORD ret;
		DWORD PropertyRegDataType;
		WCHAR PropertyData[1024];

		bl = SetupDiEnumDeviceInterfaces(Info, 0, pGuid, index, &interfaceData);
		if (bl == FALSE)
			break;

		dinfo.cbSize = sizeof(dinfo);
		bl = SetupDiEnumDeviceInfo(Info, index, &dinfo);
		if (bl == FALSE)
		{
			index++;
			continue;
		}
		bl = SetupDiGetDeviceRegistryProperty(Info, &dinfo, SPDRP_HARDWAREID, &PropertyRegDataType, (BYTE *)PropertyData, sizeof(WCHAR) * 1024, &ret);
		if (bl == FALSE)
		{
			index++;
			continue;
		}
		if (wcsncmp(PropertyData, MYDEVICE, wcslen(MYDEVICE)) == 0)
		{
			count++;
		}
		index++;
	}

	SetupDiDestroyDeviceInfoList(Info);

	return count;
}

// 현재 제공되는 GUID를 지원하는 DeviceStack의 이름들을 알려줍니다
BOOLEAN GetDeviceStackName(struct _GUID * pGuid, WCHAR ** ppDeviceName, WCHAR ** ppHardwareId, int deviceindex)
// index : 같은 Interface GUID를 가진 장치가 여러개인경우, 가장 먼저 검색되는 장치를 선택하려면 이곳에 0을 기록합니다
{
	SP_DEVINFO_DATA dinfo;
	DWORD size;
	SP_INTERFACE_DEVICE_DATA interfaceData;
	PSP_INTERFACE_DEVICE_DETAIL_DATA pData;
	HDEVINFO Info = SetupDiGetClassDevs(pGuid, 0, 0, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
	WCHAR *pDeviceName;
	WCHAR *pHardwareId;
	int index = 0;
	int count = 0;

	*ppDeviceName = (WCHAR *)0;

	if (Info == (HANDLE)-1)
		return FALSE;

	interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

	while (1)
	{
		BOOLEAN bl;
		DWORD ret;
		DWORD PropertyRegDataType;
		WCHAR PropertyData[1024];

		bl = SetupDiEnumDeviceInterfaces(Info, 0, pGuid, index, &interfaceData);
		if (bl == FALSE)
			break;

		dinfo.cbSize = sizeof(dinfo);
		bl = SetupDiEnumDeviceInfo(Info, index, &dinfo);
		if (bl == FALSE)
		{
			index++;
			continue;
		}
		bl = SetupDiGetDeviceRegistryProperty(Info, &dinfo, SPDRP_HARDWAREID, &PropertyRegDataType, (BYTE *)PropertyData, sizeof(WCHAR) * 1024, &ret);
		if (bl == FALSE)
		{
			index++;
			continue;
		}
		if (wcsncmp(PropertyData, MYDEVICE, wcslen(MYDEVICE)) == 0)
		{
			if (deviceindex == count)
			{
				SetupDiGetDeviceInterfaceDetail(Info, &interfaceData, 0, 0, &size, 0);
				pData = (PSP_INTERFACE_DEVICE_DETAIL_DATA)malloc(size);
				pData->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA);
				SetupDiGetDeviceInterfaceDetail(Info, &interfaceData, pData, size, 0, 0);

				pDeviceName = (WCHAR *)malloc((wcslen(pData->DevicePath) + 1) * sizeof(WCHAR));
				memset(pDeviceName, 0, (wcslen(pData->DevicePath) + 1) * sizeof(WCHAR));
				wcscpy(pDeviceName, pData->DevicePath);
				pHardwareId = (WCHAR *)malloc((wcslen(PropertyData) + 1) * sizeof(WCHAR));
				memset(pHardwareId, 0, (wcslen(PropertyData) + 1) * sizeof(WCHAR));
				wcscpy(pHardwareId, PropertyData);

				free(pData);

				SetupDiDestroyDeviceInfoList(Info);
				*ppDeviceName = pDeviceName;
				*ppHardwareId = pHardwareId;
				return TRUE;
			}

			count++;
		}
		index++;
	}

	SetupDiDestroyDeviceInfoList(Info);
	return FALSE;
}

void dumpDeviceInfo(int index, WCHAR *pHardwareId, WCHAR *pDeviceName, HIDP_CAPS * pCaps, PHIDP_PREPARSED_DATA	Ppd, HANDLE handle)
{
	USHORT					numValues;
	USHORT                  numCaps;
	PHIDP_BUTTON_CAPS       buttonCaps;
	PHIDP_VALUE_CAPS        valueCaps;

	buttonCaps = (PHIDP_BUTTON_CAPS)malloc(sizeof(HIDP_BUTTON_CAPS));
	valueCaps = (PHIDP_VALUE_CAPS)malloc(sizeof(HIDP_VALUE_CAPS));

	wprintf(_T("Device Index = %d"), index);
	wprintf(_T("HardwareId : %ws\n"), pHardwareId);
	wprintf(_T("DeviceName : %ws\n"), pDeviceName);

	wprintf(_T("Usage				: 0x%4x\n"), pCaps->Usage);
	wprintf(_T("UsagePage			: 0x%4x\n"), pCaps->UsagePage);
	if (pCaps->InputReportByteLength)
		wprintf(_T("InputReportByteLength		: 0x%4x\n"), pCaps->InputReportByteLength);
	if (pCaps->OutputReportByteLength)
		wprintf(_T("OutputReportByteLength		: 0x%4x\n"), pCaps->OutputReportByteLength);
	if (pCaps->FeatureReportByteLength)
		wprintf(_T("FeatureReportByteLength		: 0x%4x\n"), pCaps->FeatureReportByteLength);

	if (pCaps->NumberInputButtonCaps)
	{
		wprintf(_T("pCaps->NumberInputButtonCaps	: 0x%4x\n"), pCaps->NumberInputButtonCaps);
		numCaps = pCaps->NumberInputButtonCaps;
		HidP_GetButtonCaps(HidP_Input,
			buttonCaps,
			&numCaps,
			Ppd);
		if (numCaps)
		{
			if (buttonCaps->ReportID)
				wprintf(_T("ReportId		: 0x%4x\n"), buttonCaps->ReportID);

			wprintf(_T("	UsagePage	: 0x%4x\n"), buttonCaps->UsagePage);
			if (buttonCaps->IsRange)
			{
				for (USHORT usage = buttonCaps->Range.UsageMin; usage <= buttonCaps->Range.UsageMax; usage++)
				{
					wprintf(_T("	Usage		: 0x%4x\n"), usage);
				}
			}
			else
			{
				wprintf(_T("	Usage		: 0x%4x\n"), buttonCaps->NotRange.Usage);
			}
		}
		wprintf(_T("\n"));
	}
	if (pCaps->NumberInputValueCaps)
	{
		wprintf(_T("pCaps->NumberInputValueCaps	: 0x%4x\n"), pCaps->NumberInputValueCaps);
		numValues = pCaps->NumberInputValueCaps;
		HidP_GetValueCaps(HidP_Input,
			valueCaps,
			&numValues,
			Ppd);
		if (numValues)
		{
			if (valueCaps->ReportID)
				wprintf(_T("ReportId		: 0x%4x\n"), valueCaps->ReportID);

			wprintf(_T("	UsagePage	: 0x%4x\n"), valueCaps->UsagePage);
			if (valueCaps->IsRange)
			{
				for (USHORT usage = valueCaps->Range.UsageMin; usage <= valueCaps->Range.UsageMax; usage++)
				{
					wprintf(_T("	Usage		: 0x%4x\n"), usage);
				}
			}
			else
			{
				wprintf(_T("	Usage		: 0x%4x\n"), valueCaps->NotRange.Usage);
			}
		}
		wprintf(_T("\n"));
	}

	if (pCaps->NumberOutputButtonCaps)
	{
		wprintf(_T("pCaps->NumberOutputButtonCaps	: 0x%4x\n"), pCaps->NumberOutputButtonCaps);
		numCaps = pCaps->NumberOutputButtonCaps;
		HidP_GetButtonCaps(HidP_Output,
			buttonCaps,
			&numCaps,
			Ppd);
		if (numCaps)
		{
			if (buttonCaps->ReportID)
				wprintf(_T("ReportId		: 0x%4x\n"), buttonCaps->ReportID);

			wprintf(_T("	UsagePage	: 0x%4x\n"), buttonCaps->UsagePage);
			if (buttonCaps->IsRange)
			{
				for (USHORT usage = buttonCaps->Range.UsageMin; usage <= buttonCaps->Range.UsageMax; usage++)
				{
					wprintf(_T("	Usage		: 0x%4x\n"), usage);
				}
			}
			else
			{
				wprintf(_T("	Usage		: 0x%4x\n"), buttonCaps->NotRange.Usage);
			}
		}
		wprintf(_T("\n"));
	}
	if (pCaps->NumberOutputValueCaps)
	{
		wprintf(_T("pCaps->NumberOutputValueCaps	: 0x%4x\n"), pCaps->NumberOutputValueCaps);
		numValues = pCaps->NumberOutputValueCaps;
		HidP_GetValueCaps(HidP_Output,
			valueCaps,
			&numValues,
			Ppd);
		if (numValues)
		{
			if (valueCaps->ReportID)
				wprintf(_T("ReportId		: 0x%4x\n"), valueCaps->ReportID);

			wprintf(_T("	UsagePage	: 0x%4x\n"), valueCaps->UsagePage);
			if (valueCaps->IsRange)
			{
				for (USHORT usage = valueCaps->Range.UsageMin; usage <= valueCaps->Range.UsageMax; usage++)
				{
					wprintf(_T("	Usage		: 0x%4x\n"), usage);
				}
			}
			else
			{
				wprintf(_T("	Usage		: 0x%4x\n"), valueCaps->NotRange.Usage);
			}
		}
		wprintf(_T("\n"));
	}

	if (pCaps->NumberFeatureButtonCaps)
	{
		wprintf(_T("pCaps->NumberFeatureButtonCaps	: 0x%4x\n"), pCaps->NumberFeatureButtonCaps);
		numCaps = pCaps->NumberFeatureButtonCaps;
		HidP_GetButtonCaps(HidP_Feature,
			buttonCaps,
			&numCaps,
			Ppd);
		if (numCaps)
		{
			if (buttonCaps->ReportID)
				wprintf(_T("ReportId		: 0x%4x\n"), buttonCaps->ReportID);

			wprintf(_T("	UsagePage	: 0x%4x\n"), buttonCaps->UsagePage);
			if (buttonCaps->IsRange)
			{
				for (USHORT usage = buttonCaps->Range.UsageMin; usage <= buttonCaps->Range.UsageMax; usage++)
				{
					wprintf(_T("	Usage		: 0x%4x\n"), usage);
				}
			}
			else
			{
				wprintf(_T("	Usage		: 0x%4x\n"), buttonCaps->NotRange.Usage);
			}
		}
		wprintf(_T("\n"));
	}
	if (pCaps->NumberFeatureValueCaps)
	{
		wprintf(_T("pCaps->NumberFeatureValueCaps	: 0x%4x\n"), pCaps->NumberFeatureValueCaps);
		numValues = pCaps->NumberFeatureValueCaps;
		HidP_GetValueCaps(HidP_Feature,
			valueCaps,
			&numValues,
			Ppd);
		if (numValues)
		{
			if (valueCaps->ReportID)
				wprintf(_T("ReportId		: 0x%4x\n"), valueCaps->ReportID);

			wprintf(_T("	UsagePage	: 0x%4x\n"), valueCaps->UsagePage);
			if (valueCaps->IsRange)
			{
				for (USHORT usage = valueCaps->Range.UsageMin; usage <= valueCaps->Range.UsageMax; usage++)
				{
					wprintf(_T("	Usage		: 0x%4x\n"), usage);
				}
			}
			else
			{
				wprintf(_T("	Usage		: 0x%4x\n"), valueCaps->NotRange.Usage);
			}
		}
		wprintf(_T("\n"));
	}

	free(buttonCaps);
	free(valueCaps);
}

int main()
{
	SP_DEVINFO_DATA			DevInfoData;
	GUID                    hidGuid;
	HANDLE					handle;
	int						count, index;
	WCHAR					*pDeviceName;
	WCHAR					*pHardwareId;
	BOOLEAN					bl;
	PHIDP_PREPARSED_DATA	Ppd;
	HIDD_ATTRIBUTES			Attributes;
	HIDP_CAPS				Caps;

	memset(&DevInfoData, 0, sizeof(SP_DEVINFO_DATA));
	DevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	HidD_GetHidGuid(&hidGuid);

	count = GetDeviceStackNameCount((struct _GUID *)&hidGuid);
	if (count == 0)
		return 0; // 시스템은 SampleGuid를 지원하는 장치가 설치되지 않았습니다

	for (index = 0; index < count; index++)
	{
		bl = GetDeviceStackName((struct _GUID *)&hidGuid, &pDeviceName, &pHardwareId, index);

		if (bl == FALSE)
			continue;

		// pDeviceName은 디바이스스택에 접근하기 위한 이름이 구해집니다
		handle = CreateFile(pDeviceName, GENERIC_READ | GENERIC_WRITE
			, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL
			, 0);
		// 이때 드라이버측으로 IRP_MJ_CREATE명령어가 전달됩니다
		if (handle == (HANDLE)-1)
		{
			free(pHardwareId);
			free(pDeviceName);
			continue;
		}

		//MessageBox(NULL, _T("다음작업을 시작합니다"), _T("HID Call Test"), MB_OK );

		// Hid Report관련정보를 구한다
		if (!HidD_GetPreparsedData(handle, &Ppd)) {
			free(pHardwareId);
			free(pDeviceName);
			continue;
		}

		if (!HidD_GetAttributes(handle, &Attributes)) {
			HidD_FreePreparsedData(Ppd);
			free(pHardwareId);
			free(pDeviceName);
			continue;
		}

		if (!HidP_GetCaps(Ppd, &Caps)) {
			HidD_FreePreparsedData(Ppd);
			free(pHardwareId);
			free(pDeviceName);
			continue;
		}

		// 열린장치에 대한 장치정보를 출력한다
		dumpDeviceInfo(index, pHardwareId, pDeviceName, &Caps, Ppd, handle);

		//////////////////////////////////////////////////////////////////////////////////////////////////////
		// 테스트를 해본다.
		// 먼저 ReadFile()함수를 사용해서 Input Report를 읽어본다. 그다음, HidP_GetXXX()함수를 사용한다.
		// 아래코드는 드라이버의 상태의존적인 코드이므로, 드라이버가 수정되면 반드시 함께 수정되어져야 한다
		// 테스트장치는 각각의 Collection 마다 데이타를 읽어오는 테스트이다
		// Input Report와 Feature Report가 가진 현재 데이타를 읽어오는 과정을 수행한다
		// 예재 드라이버는 Input Report, Feature Report 모두 적절한 ReportId 가 사용됨을 유의하자
		// dumpDeviceInfo()함수에 의해서 출력된 내용과 일치해야 한다
		// Col01's ReportId = 0x01 (Input Report)
		// Col02's ReportId = 0x02 (Input Report)
		// Col03's ReportId = 0x03 (Feature Report)
		// 따라서, 각각의 Collection이 위와 같은 데이타를 가진것을 알고있다고 가정한다
		BYTE *reportDataBuffer = NULL;
		int reportSize;
		NTSTATUS status;
		ULONG usageData;

		// ReadFile() Test
#if 0
		reportSize = Caps.InputReportByteLength;
		reportDataBuffer = (BYTE *)malloc(reportSize);
		ReadFile(handle, reportDataBuffer, reportSize, &ret, NULL);
		ReadFile(handle, reportDataBuffer, reportSize, &ret, NULL);
		free(reportDataBuffer);
#endif 
		// HidP_GetXXX() Test
		switch (index)
		{
		case 0: // Col01
			reportSize = Caps.InputReportByteLength;
			reportDataBuffer = (BYTE *)malloc(reportSize);
			reportDataBuffer[0] = 0x01; // 첫번째 바이트는 ReportId를 넣어주어야 한다

			HidD_GetInputReport(handle, reportDataBuffer, reportSize);
			// 읽은 데이타를 Usage를 적용하여 해석하려면 아래와 같은 작업이 추가로 필요하다
			// 필요한 파라미터는 dumpDeviceInfo()함수에서 열거된다
			// 여기서는 강제지정하도록 한다
			// 현재 드라이버는 UsageMin부터 UsageMax까지를 가지므로, 여기서는 0x0001 만 분석해보자
			status = HidP_GetUsageValue(HidP_Input,
				0xFF00, // UsagePage
				0,
				0x0001, // Usage
				&usageData,
				Ppd,
				(PCHAR)reportDataBuffer,
				reportSize);

			// debug
			/*
			wprintf( _T("0x%8x\n"), HIDP_STATUS_SUCCESS);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_NULL);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_INVALID_PREPARSED_DATA);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_INVALID_REPORT_TYPE);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_INVALID_REPORT_LENGTH);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_USAGE_NOT_FOUND);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_VALUE_OUT_OF_RANGE);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_BAD_LOG_PHY_VALUES);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_BUFFER_TOO_SMALL);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_INTERNAL_ERROR);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_INCOMPATIBLE_REPORT_ID);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_NOT_VALUE_ARRAY);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_IS_VALUE_ARRAY);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_DATA_INDEX_NOT_FOUND);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_DATA_INDEX_OUT_OF_RANGE);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_BUTTON_NOT_PRESSED);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_REPORT_DOES_NOT_EXIST);
			wprintf( _T("0x%8x\n"), HIDP_STATUS_NOT_IMPLEMENTED);
			*/
			if (status == HIDP_STATUS_SUCCESS)
			{
				// 읽은 값을 출력한다
				wprintf(_T("		* Usage 0x0001 value = 0x%4x\n\n"), usageData);
			}
			free(reportDataBuffer);

			break;
		case 1: // Col02
			reportSize = Caps.InputReportByteLength;
			reportDataBuffer = (BYTE *)malloc(reportSize);
			reportDataBuffer[0] = 0x02; // 첫번째 바이트는 ReportId를 넣어주어야 한다

			HidD_GetInputReport(handle, reportDataBuffer, reportSize);
			// 읽은 데이타를 Usage를 적용하여 해석하려면 아래와 같은 작업이 추가로 필요하다
			// 필요한 파라미터는 dumpDeviceInfo()함수에서 열거된다
			// 여기서는 강제지정하도록 한다
			// 현재 드라이버는 UsageMin부터 UsageMax까지를 가지므로, 여기서는 0x0001 만 분석해보자
			status = HidP_GetUsageValue(HidP_Input,
				0xFF00, // UsagePage
				0,
				0x0001, // Usage
				&usageData,
				Ppd,
				(PCHAR)reportDataBuffer,
				reportSize);
			if (status == HIDP_STATUS_SUCCESS)
			{
				// 읽은 값을 출력한다
				wprintf(_T("		* Usage 0x0001 value = 0x%4x\n\n"), usageData);
			}
			free(reportDataBuffer);

			break;
		case 2: // Col03
			reportSize = Caps.FeatureReportByteLength;
			reportDataBuffer = (BYTE *)malloc(reportSize);
			reportDataBuffer[0] = 0x03; // 첫번째 바이트는 ReportId를 넣어주어야 한다

			HidD_GetFeature(handle, reportDataBuffer, reportSize);

			// 읽은 데이타를 Usage를 적용하여 해석하려면 아래와 같은 작업이 추가로 필요하다
			// 필요한 파라미터는 dumpDeviceInfo()함수에서 열거된다
			// 여기서는 강제지정하도록 한다
			// 현재 드라이버는 UsageMin부터 UsageMax까지를 가지므로, 여기서는 0x0001 만 분석해보자
			status = HidP_GetUsageValue(HidP_Feature,
				0xFF00, // UsagePage
				0,
				0x0001, // Usage
				&usageData,
				Ppd,
				(PCHAR)reportDataBuffer,
				reportSize);

			if (status == HIDP_STATUS_SUCCESS)
			{
				// 읽은 값을 출력한다
				wprintf(_T("		* Usage 0x0001 value = 0x%4x\n\n"), usageData);
			}
			free(reportDataBuffer);

			break;
		}

		//
		//////////////////////////////////////////////////////////////////////////////////////////////////////

		CloseHandle(handle);

		free(pHardwareId);
		free(pDeviceName);
		// pDeviceName는 GetDeviceStackName()함수내에서 할당되는 메모리이므로, 사용이 끝나면 반드시 해제하여야 합니다
		wprintf(_T("-----------------------------------------------------------------\n"));

	}
	MessageBox(NULL, _T("작업이 끝났습니다"), _T("HID Call Test"), MB_OK);
	return 0;
}

