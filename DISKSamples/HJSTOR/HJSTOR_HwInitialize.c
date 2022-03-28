
#include "HJSTOR.h"


BOOLEAN
HJSTOR_HwInitialize(
	PVOID pContext
	)
{
	PHW_DEVICE_EXTENSION	pHwDeviceExtension = NULL;
		
	if ( pContext == NULL )
	{
		return FALSE;
	}
#ifdef DBGOUT
	DbgPrint("HJSTOR_HwInitialize\n");
#endif

	pHwDeviceExtension = pContext;
	pHwDeviceExtension->Base = ExAllocatePool(NonPagedPool, DEFAULT_DISK_SIZE);
	if (pHwDeviceExtension->Base == NULL)
		return FALSE;

	pHwDeviceExtension->TotalBlocks = NUM_TOTAL_BLOCKS;

	return TRUE;
}