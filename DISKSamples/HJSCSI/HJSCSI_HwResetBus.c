#include "HJSCSI.h"

/***********************************************************************************
함수명 : HJSCSI_ResetController
인  자 :
			IN PVOID HwDeviceExtension,
			IN ULONG PathId
리턴값 : BOOLEAN
설  명 : Reset
***********************************************************************************/

BOOLEAN HJSCSI_HwResetBus(IN PVOID HwDeviceExtension,IN ULONG PathId)
{
	UNREFERENCED_PARAMETER(HwDeviceExtension);
	UNREFERENCED_PARAMETER(PathId);

    return TRUE;
}
