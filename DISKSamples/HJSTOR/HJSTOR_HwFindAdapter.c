#include "HJSTOR.h"

int
HJSTOR_HwFindAdapter(
	IN PVOID  pContext,
	IN PVOID  HwContext,
	IN PVOID  BusInformation,
	IN PVOID  LowerDevice,
	IN PCHAR  ArgumentString,
	IN OUT PPORT_CONFIGURATION_INFORMATION  pConfigInfo,
	OUT PBOOLEAN  Again
)
{
	PHW_DEVICE_EXTENSION	pDeviceExtension = NULL;

	UNREFERENCED_PARAMETER(ArgumentString);
	UNREFERENCED_PARAMETER(LowerDevice);
	UNREFERENCED_PARAMETER(BusInformation);
	UNREFERENCED_PARAMETER(HwContext);

	if ((pContext == NULL) || (pConfigInfo == NULL))
	{
		return SP_RETURN_NOT_FOUND;
	}

	*Again = FALSE;

#ifdef DBGOUT
	DbgPrint("HJSTOR_HwFindAdapter\n");
#endif

	// Context
	pDeviceExtension = (PHW_DEVICE_EXTENSION)pContext;

	pConfigInfo->AdapterInterfaceType = Internal;
	pConfigInfo->VirtualDevice = TRUE;                        // Inidicate no real hardware.

	pConfigInfo->InterruptMode = Latched;
	pConfigInfo->MaximumTransferLength = MAX_DMA_TRANSFER_SIZE;
	pConfigInfo->NumberOfPhysicalBreaks = SCSI_MAXIMUM_PHYSICAL_BREAKS;
	pConfigInfo->DmaChannel = SP_UNINITIALIZED_VALUE;
	pConfigInfo->DmaPort = SP_UNINITIALIZED_VALUE;
	pConfigInfo->DmaWidth = MaximumDmaWidth; //Width32Bits;
	pConfigInfo->DmaSpeed = MaximumDmaSpeed; //Compatible;
	pConfigInfo->AlignmentMask = 3;	// DWORD Alignment	// scatter & gather option
	pConfigInfo->NumberOfBuses = 1;
	pConfigInfo->ScatterGather = TRUE;
	pConfigInfo->Master = TRUE;
	pConfigInfo->CachesData = FALSE;
	pConfigInfo->AdapterScansDown = FALSE;
	pConfigInfo->Dma32BitAddresses = TRUE;
	pConfigInfo->MaximumNumberOfTargets = 1;
	pConfigInfo->MaximumNumberOfLogicalUnits = 1;
	pConfigInfo->WmiDataProvider = FALSE;

	pConfigInfo->ResetTargetSupported = TRUE;
	pConfigInfo->SynchronizationModel = StorSynchronizeFullDuplex;
	pConfigInfo->MapBuffers = STOR_MAP_ALL_BUFFERS_INCLUDING_READ_WRITE;// STOR_MAP_NON_READ_WRITE_BUFFERS;

	return SP_RETURN_FOUND;
}

