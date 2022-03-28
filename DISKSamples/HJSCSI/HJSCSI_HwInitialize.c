
#include "HJSCSI.h"

/***********************************************************************************
함수명 : HJSCSI_HwInitialize
인  자 : 
			HwDeviceExtension
리턴값 : BOOLEAN
설  명 : 하드웨어를 초기화합니다. 
***********************************************************************************/

BOOLEAN HJSCSI_HwInitialize(IN PVOID HwDeviceExtension)
{
	UNREFERENCED_PARAMETER(HwDeviceExtension);

#ifdef DBGOUT
	DbgPrint("HJSCSI_HwInitialize\n");
#endif
	return TRUE;
}
