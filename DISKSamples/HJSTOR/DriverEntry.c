#include "HJSTOR.h"

ULONG
DriverEntry(
	PVOID	pDriverObject,
	PVOID	pArg
	)
{
	VIRTUAL_HW_INITIALIZATION_DATA	hwInitializationData	= { 0, };
	ULONG					ulStatus				= 0;
	ULONG					adapterCount			= 0;

	if ( pDriverObject == NULL )
	{
		return (ULONG)STATUS_UNSUCCESSFUL;
	}

#ifdef DBGOUT
    DbgPrint("[HJSTOR]DriverEntry\n");
#endif

	// Set Size
	hwInitializationData.HwInitializationDataSize	= sizeof(VIRTUAL_HW_INITIALIZATION_DATA);

	// Entry Point Routines
	hwInitializationData.HwInitialize = HJSTOR_HwInitialize;
	hwInitializationData.HwStartIo = HJSTOR_HwStartIo;
	hwInitializationData.HwInterrupt = HJSTOR_HwInterrupt;
	hwInitializationData.HwFindAdapter = HJSTOR_HwFindAdapter;
	hwInitializationData.HwResetBus = HJSTOR_HwResetBus;
	hwInitializationData.HwAdapterControl = HJSTOR_HwAdapterControl;

	hwInitializationData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);
	hwInitializationData.SpecificLuExtensionSize = 0;
	hwInitializationData.SrbExtensionSize = sizeof(SRB_EXTENSION);

	hwInitializationData.AdapterInterfaceType = Internal;

	hwInitializationData.NumberOfAccessRanges = 256;
	hwInitializationData.MapBuffers = STOR_MAP_NON_READ_WRITE_BUFFERS;
	hwInitializationData.NeedPhysicalAddresses = TRUE;
	hwInitializationData.TaggedQueuing = TRUE;
	hwInitializationData.AutoRequestSense = TRUE;
	hwInitializationData.MultipleRequestPerLu = TRUE;
	hwInitializationData.ReceiveEvent = FALSE;

	// call StorPort to register our HW init data
	ulStatus = StorPortInitialize( pDriverObject, pArg, &hwInitializationData, &adapterCount );

#ifdef DBGOUT
    DbgPrint("[HJSTOR]DriverEntry, return = 0x%8X\n", ulStatus);
#endif

	return ulStatus;
}


