#include "HJSCSI.h"

/***********************************************************************************
함수명 : HJSCSI_HwInterrupt
인  자 : 
			HwDeviceExtension
리턴값 : BOOLEAN
설  명 : Interrupt routine
***********************************************************************************/

BOOLEAN HJSCSI_HwInterrupt(IN PVOID HwDeviceExtension)
{
	UNREFERENCED_PARAMETER(HwDeviceExtension);

	return FALSE;
/*
	실제 하드웨어가 있다면 인터럽트를 발생시킨 하드웨어를 조사해야 한다
	Srb명령어를 종료해야 한다
	DeviceExtension->CurrentSrb->SrbStatus = (UCHAR)SRB_STATUS_SUCCESS;    
	ScsiPortNotification(RequestComplete,   DeviceExtension, DeviceExtension->CurrentSrb);
    ScsiPortNotification(NextRequest,  DeviceExtension, NULL);
	DeviceExtension->CurrentSrb = NULL;		
*/
}

