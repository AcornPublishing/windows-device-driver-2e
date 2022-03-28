#include <ntddk.h>
#include <ntdef.h>
#include <stdio.h>
#include <initguid.h>

#include "devioctl.h"

#define SET_MYDEVICE_POWER_STATE(PowerLevel) \
{ \
	pDevExt->DevicePowerState = PowerLevel; \
	{ \
	POWER_STATE NewState; \
	NewState.DeviceState = PowerLevel; \
	PoSetPowerState( pFDO, DevicePowerState, NewState ); \
	} \
}\

#if defined(_WIN64)
#define POINTER_ALIGNMENT DECLSPEC_ALIGN(8)
#else
#define POINTER_ALIGNMENT
#endif

typedef struct _PCISAMPLE_INTERRUPT_INFO {
#if defined(NT_PROCESSOR_GROUPS)
	USHORT Level;
	USHORT Group;
#else
	ULONG Level;
#endif
	ULONG Vector;
	KAFFINITY Affinity;
	KINTERRUPT_MODE Flags;
} PCISAMPLE_INTERRUPT_INFO, *PPCISAMPLE_INTERRUPT_INFO;

typedef struct _PCISAMPLE_MAPPING_MEMORY
{
	ULONGLONG				pListEntry;
	ULONGLONG				pMdl;
	ULONGLONG				PhysicalAddress;
	ULONGLONG				KernelAddress;
	ULONGLONG				UserAddress;
	ULONGLONG				Size;
}PCISAMPLE_MAPPING_MEMORY, *PPCISAMPLE_MAPPING_MEMORY;

typedef struct {
	USHORT  VendorID;
	USHORT  DeviceID;
	USHORT  Command;
	USHORT  Status;
	UCHAR   RevisionID;
	UCHAR   ProgIf;
	UCHAR   SubClass;
	UCHAR   BaseClass;
	UCHAR   CacheLineSize;
	UCHAR   LatencyTimer;
	UCHAR   HeaderType;
	UCHAR   BIST;

	union {
		struct _PCIDEF_HEADER_TYPE_0 {
			ULONG   BaseAddresses[6];
			ULONG   CIS;
			USHORT  SubVendorID;
			USHORT  SubSystemID;
			ULONG   ROMBaseAddress;
			UCHAR   CapabilitiesPtr;
			UCHAR   Reserved1[3];
			ULONG   Reserved2;
			UCHAR   InterruptLine;
			UCHAR   InterruptPin;
			UCHAR   MinimumGrant;
			UCHAR   MaximumLatency;
		} type0;
	} u;
} PCIDEF_COMMON_HEADER, *PPCIDEF_COMMON_HEADER;

#define IOCTL_PCISAMPLE_REGISTER_SHARED_EVENT				CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA00, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_MAPPING_MEMORY						CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA01, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_UNMAPPING_MEMORY					CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA02, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_ALLOCATE_CONTIGUOUS_MEMORY_FOR_DMA	CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA03, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_FREE_CONTIGUOUS_MEMORY_FOR_DMA		CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA04, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_PCISAMPLE_GET_CONFIGURATION_REGISTER			CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA05, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _PCISAMPLE_DEVICE_EXTENSION
{
	PDEVICE_OBJECT		pNextLowerDeviceObject;
	PDEVICE_OBJECT		pFDO;
	PDEVICE_OBJECT		pPDO;
	UNICODE_STRING		UnicodeString;

	int					EnterCount;
	LIST_ENTRY			MappingManagerListHead;

	PKINTERRUPT			InterruptObject;
	KEVENT *			pInterruptEventObject;

	PHYSICAL_ADDRESS	PhysicalBaseAddress;
	void *				pKernelBaseAddress;
	SIZE_T				Size;

	DEVICE_POWER_STATE	CurrentDevicePowerState;
	SYSTEM_POWER_STATE	DevicePowerState;

	PCISAMPLE_INTERRUPT_INFO		PCISAMPLE_INTERRUPT_INFO;

} PCISAMPLE_DEVICE_EXTENSION, *PPCISAMPLE_DEVICE_EXTENSION;

typedef struct _PCISAMPLE_MAPPED_LIST
{
	LIST_ENTRY		Entry;
	BOOLEAN			IsDMA;
	PCISAMPLE_MAPPING_MEMORY	MappedMemory;
} PCISAMPLE_MAPPED_LIST, *PPCISAMPLE_MAPPED_LIST;

NTSTATUS DriverEntry(
	PDRIVER_OBJECT		pDriverObject,
	PUNICODE_STRING		pRegistryPath
);

void PCISAMPLE_Unload(
	PDRIVER_OBJECT	pDriverObject
);

NTSTATUS PCISAMPLE_CommonCompletionRoutine(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp,
	IN PVOID			pContext
);

NTSTATUS
PCISAMPLE_Cleanup(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp
);

NTSTATUS
PCISAMPLE_CreateClose(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp
);

NTSTATUS
PCISAMPLE_DeviceIOControl(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp
);

NTSTATUS PCISAMPLE_AddDevice(
	PDRIVER_OBJECT pDriverObject,
	PDEVICE_OBJECT pPDO
);

NTSTATUS PCISAMPLE_PnPDispatch(
	PDEVICE_OBJECT	pFDO,
	PIRP			pIrp
);

NTSTATUS
PCISAMPLE_PowerDispatch(
	PDEVICE_OBJECT		pDeviceObject,
	PIRP				pIrp
);

BOOLEAN
PCISAMPLE_InterruptServiceRoutine(
	IN struct _KINTERRUPT	*Interrupt,
	IN PVOID				ServiceContext
);

void
PCISAMPLE_DPCRoutine(
	PKDPC					pDpc,
	PDEVICE_OBJECT			pDeviceObject,
	PIRP					pIrp,
	PVOID					pContext
);

DEFINE_GUID(GUID_DEVINTERFACE_PCISAMPLE,
	0x946A7B1A, 0xEBBC, 0x422A, 0xA8, 0x1F, 0xF0, 0x7C, 0x8E, 0x41, 0xD1, 0x11);

// --------------- ISR & DPC ----------------------

void
PCISAMPLE_DPCRoutine(
	PKDPC pDpc,
	PDEVICE_OBJECT  pDeviceObject,
	PIRP pIrp,
	PVOID pDpcContext
)
{
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;

	UNREFERENCED_PARAMETER(pDpc);
	UNREFERENCED_PARAMETER(pIrp);
	UNREFERENCED_PARAMETER(pDpcContext);

	if (pDeviceObject == NULL)
	{
		return;
	}
	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;
	if (pDevExt == NULL)
	{
		return;
	}

	KeSetEvent((PKEVENT)pDevExt->pInterruptEventObject, 0, FALSE);

	return;
}

BOOLEAN
PCISAMPLE_InterruptServiceRoutine(
	IN struct _KINTERRUPT*Interrupt,
	IN PVOID ServiceContext
)
{
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;
	PDEVICE_OBJECT pFDO = NULL;
	BOOLEAN IsOurISR = FALSE;

	UNREFERENCED_PARAMETER(Interrupt);

	if (ServiceContext == NULL)
	{
		goto exit;
	}

	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)ServiceContext;
	pFDO = pDevExt->pFDO;
	if (pFDO == NULL)
	{
		goto exit;
	}

	// 본인의 카드가 발생한 인터럽트의 경우만 DPC를 구동한다
	IsOurISR = FALSE;

	if (IsOurISR == FALSE)
	{
		goto exit;
	}

	IoRequestDpc(pFDO, NULL, NULL);

exit:
	return IsOurISR;
}

// --------------- DriverEntry ----------------------

NTSTATUS
DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegistryPath
)
{
	UNREFERENCED_PARAMETER(pRegistryPath);

	pDriverObject->DriverUnload = PCISAMPLE_Unload;
	pDriverObject->DriverExtension->AddDevice = PCISAMPLE_AddDevice;

	pDriverObject->MajorFunction[IRP_MJ_PNP] = PCISAMPLE_PnPDispatch;
	pDriverObject->MajorFunction[IRP_MJ_POWER] = PCISAMPLE_PowerDispatch;

	pDriverObject->MajorFunction[IRP_MJ_CREATE] = PCISAMPLE_CreateClose;
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = PCISAMPLE_CreateClose;
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = PCISAMPLE_Cleanup;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PCISAMPLE_DeviceIOControl;

	return STATUS_SUCCESS;
}

// --------------- PCISAMPLE_Unload ----------------------

void
PCISAMPLE_Unload(
	PDRIVER_OBJECT pDriverObject
)
{
	UNREFERENCED_PARAMETER(pDriverObject);

	return;
}

// --------------- PCISAMPLE_AddDevice ----------------------

NTSTATUS
PCISAMPLE_AddDevice(
	PDRIVER_OBJECT pDriverObject,
	PDEVICE_OBJECT pPDO
)
{
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT pFDO = NULL;
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;

	POWER_STATE NewPowerState;

	ntStatus = IoCreateDevice(
		pDriverObject,
		sizeof(PCISAMPLE_DEVICE_EXTENSION),
		NULL,
		FILE_DEVICE_UNKNOWN,
		FILE_AUTOGENERATED_DEVICE_NAME,
		FALSE,
		&pFDO
	);

	if (!NT_SUCCESS(ntStatus))
	{
		goto exit;
	}

	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)pFDO->DeviceExtension;
	if (pDevExt == NULL)
	{
		goto exit;
	}

	RtlZeroMemory(pDevExt, sizeof(PCISAMPLE_DEVICE_EXTENSION));

	InitializeListHead(&pDevExt->MappingManagerListHead);

	IoInitializeDpcRequest(pFDO, PCISAMPLE_DPCRoutine);

	NewPowerState.DeviceState = PowerDeviceD3;
	pDevExt->CurrentDevicePowerState = PowerDeviceD3;
	PoSetPowerState(pFDO, DevicePowerState, NewPowerState);

	pDevExt->pNextLowerDeviceObject = IoAttachDeviceToDeviceStack(pFDO, pPDO);

	pDevExt->pPDO = pPDO;
	pDevExt->pFDO = pFDO;

	ntStatus = IoRegisterDeviceInterface(pPDO, (GUID *)&GUID_DEVINTERFACE_PCISAMPLE, NULL, &pDevExt->UnicodeString);
	if (!NT_SUCCESS(ntStatus))
	{
		goto exit;
	}

	pFDO->Flags |= DO_BUFFERED_IO;
	pFDO->Flags |= (pDevExt->pNextLowerDeviceObject->Flags & (DO_POWER_PAGABLE | DO_POWER_INRUSH));
	pFDO->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;

exit:
	if ((pDevExt != NULL) && (pDevExt->pNextLowerDeviceObject != NULL))
	{
		IoDetachDevice(pDevExt->pNextLowerDeviceObject);
	}

	if (pFDO != NULL)
	{
		IoDeleteDevice(pFDO);
		pFDO = NULL;
	}

	return STATUS_UNSUCCESSFUL;
}

// --------------- PCISAMPLE_PnPDispatch ----------------------

NTSTATUS
PCISAMPLE_PnPDispatch(
	PDEVICE_OBJECT pFDO,
	PIRP pIrp
)
{
	KEVENT StartEvent = { 0, };
	KEVENT  Event = { 0, };
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PIO_STACK_LOCATION pIoStack = NULL;
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;
	PCM_RESOURCE_LIST pDeviceResource = NULL;
	int i = 0;
	int m = 0;
	PDEVICE_OBJECT    pNextDevice = NULL;

	if (pIrp == NULL)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (pFDO == NULL)
	{
		pIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return STATUS_UNSUCCESSFUL;
	}

	pIoStack = IoGetCurrentIrpStackLocation(pIrp);
	if (pIoStack == NULL)
	{
		pIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return STATUS_UNSUCCESSFUL;
	}

	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)pFDO->DeviceExtension;
	if (pDevExt == NULL)
	{
		pIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return STATUS_UNSUCCESSFUL;
	}

	switch (pIoStack->MinorFunction)
	{
	case IRP_MN_START_DEVICE:
	{
		KeInitializeEvent(&StartEvent, NotificationEvent, FALSE);

		IoCopyCurrentIrpStackLocationToNext(pIrp);
		IoSetCompletionRoutine(pIrp, PCISAMPLE_CommonCompletionRoutine, &StartEvent, TRUE, TRUE, TRUE);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		if (!NT_SUCCESS(ntStatus))
		{
			goto exit;
		}

		if (ntStatus == STATUS_PENDING)
		{
			ntStatus = KeWaitForSingleObject(&StartEvent, Executive, KernelMode, FALSE, NULL);
			if (!NT_SUCCESS(ntStatus))
			{
				goto exit;
			}
			ntStatus = pIrp->IoStatus.Status;
		}

		if (!NT_SUCCESS(ntStatus))
		{
			goto exit;
		}

		SET_MYDEVICE_POWER_STATE(PowerDeviceD0)

		pDeviceResource = pIoStack->Parameters.StartDevice.AllocatedResourcesTranslated;
		if (pDeviceResource == NULL)
		{
			ntStatus = STATUS_UNSUCCESSFUL;
			goto exit;
		}

		ntStatus = STATUS_SUCCESS;

		for (i = 0; i < (int)pDeviceResource->Count; i++)
		{
			if (pDeviceResource->List[i].InterfaceType == PCIBus)
			{
				for (m = 0; m < (int)pDeviceResource->List[i].PartialResourceList.Count; m++)
				{
					switch (pDeviceResource->List[i].PartialResourceList.PartialDescriptors[m].Type)
					{
					case CmResourceTypeMemory:
					{
						if (pDevExt->PhysicalBaseAddress.QuadPart == 0)
						{
							pDevExt->PhysicalBaseAddress = pDeviceResource->List[i].PartialResourceList.PartialDescriptors[m].u.Memory.Start;
							pDevExt->Size = pDeviceResource->List[i].PartialResourceList.PartialDescriptors[m].u.Memory.Length;
							pDevExt->pKernelBaseAddress = MmMapIoSpace(pDevExt->PhysicalBaseAddress, pDevExt->Size, MmNonCached);
						}
						break;
					}
					case CmResourceTypeInterrupt:
					{
						pDevExt->PCISAMPLE_INTERRUPT_INFO.Level = pDeviceResource->List[i].PartialResourceList.PartialDescriptors[m].u.Interrupt.Level;
						pDevExt->PCISAMPLE_INTERRUPT_INFO.Vector = pDeviceResource->List[i].PartialResourceList.PartialDescriptors[m].u.Interrupt.Vector;
						pDevExt->PCISAMPLE_INTERRUPT_INFO.Affinity = pDeviceResource->List[i].PartialResourceList.PartialDescriptors[m].u.Interrupt.Affinity;
						pDevExt->PCISAMPLE_INTERRUPT_INFO.Flags = pDeviceResource->List[i].PartialResourceList.PartialDescriptors[m].Flags;

						ntStatus = IoConnectInterrupt(&pDevExt->InterruptObject,
							(PKSERVICE_ROUTINE)PCISAMPLE_InterruptServiceRoutine,
							(PVOID)pDevExt,
							NULL,
							pDevExt->PCISAMPLE_INTERRUPT_INFO.Vector,
							(KIRQL)pDevExt->PCISAMPLE_INTERRUPT_INFO.Level,
							(KIRQL)pDevExt->PCISAMPLE_INTERRUPT_INFO.Level,
							(KINTERRUPT_MODE)pDevExt->PCISAMPLE_INTERRUPT_INFO.Flags,
							TRUE,
							pDevExt->PCISAMPLE_INTERRUPT_INFO.Affinity,
							FALSE);
						break;
					}
					} 
				} 
			} 
		} 

		if (NT_SUCCESS(ntStatus))
			IoSetDeviceInterfaceState(&pDevExt->UnicodeString, TRUE);

	exit:

		pIrp->IoStatus.Status = ntStatus;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		break;
	}

	case  IRP_MN_QUERY_PNP_DEVICE_STATE:
	{
		KeInitializeEvent(&Event, NotificationEvent, FALSE);

		IoCopyCurrentIrpStackLocationToNext(pIrp);
		IoSetCompletionRoutine(pIrp, PCISAMPLE_CommonCompletionRoutine, &Event, TRUE, TRUE, TRUE);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		if (ntStatus == STATUS_PENDING)
		{
			KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
			ntStatus = pIrp->IoStatus.Status;
		}

		ntStatus = pIrp->IoStatus.Status;
		pIrp->IoStatus.Information = 0;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		break;
	}

	case IRP_MN_STOP_DEVICE:
	{
		IoSetDeviceInterfaceState(&pDevExt->UnicodeString, FALSE);

		SET_MYDEVICE_POWER_STATE(PowerDeviceD3)

			pIrp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(pIrp);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		if (!NT_SUCCESS(ntStatus))
		{
			pIrp->IoStatus.Status = ntStatus;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			return ntStatus;
		}

		break;
	}

	case IRP_MN_QUERY_REMOVE_DEVICE:
	{
		pIrp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(pIrp);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		if (!NT_SUCCESS(ntStatus))
		{
			pIrp->IoStatus.Status = ntStatus;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			return ntStatus;
		}
		break;
	}

	case IRP_MN_CANCEL_REMOVE_DEVICE:
	{
		KeInitializeEvent(&StartEvent, NotificationEvent, FALSE);
		IoCopyCurrentIrpStackLocationToNext(pIrp);
		IoSetCompletionRoutine(pIrp, PCISAMPLE_CommonCompletionRoutine, &StartEvent, TRUE, TRUE, TRUE);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		if (!NT_SUCCESS(ntStatus))
		{
			pIrp->IoStatus.Status = ntStatus;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			return ntStatus;
		}

		if (ntStatus == STATUS_PENDING)
		{
			ntStatus = KeWaitForSingleObject(&StartEvent, Executive, KernelMode, FALSE, NULL);
			if (ntStatus != STATUS_SUCCESS)
			{
				pIrp->IoStatus.Status = ntStatus;
				IoCompleteRequest(pIrp, IO_NO_INCREMENT);
				return ntStatus;
			}
			ntStatus = pIrp->IoStatus.Status;
		}

		ntStatus = pIrp->IoStatus.Status;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		break;
	}

	case IRP_MN_REMOVE_DEVICE:
	{
		pNextDevice = pDevExt->pNextLowerDeviceObject;

		if (pDevExt->InterruptObject != NULL)
		{
			IoDisconnectInterrupt(pDevExt->InterruptObject);
			pDevExt->InterruptObject = NULL;
		}

		if (pDevExt->pKernelBaseAddress)
		{
			MmUnmapIoSpace(pDevExt->pKernelBaseAddress, pDevExt->Size);
		}
		IoSetDeviceInterfaceState(&pDevExt->UnicodeString, FALSE);
		RtlFreeUnicodeString(&pDevExt->UnicodeString);

		SET_MYDEVICE_POWER_STATE(PowerDeviceD3)

		IoDetachDevice(pNextDevice);
		IoDeleteDevice(pFDO);

		pIrp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(pIrp);
		ntStatus = IoCallDriver(pNextDevice, pIrp);
		if (!NT_SUCCESS(ntStatus))
		{
			pIrp->IoStatus.Status = ntStatus;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			return ntStatus;
		}
		break;
	}

	case IRP_MN_SURPRISE_REMOVAL:
	{
		IoSetDeviceInterfaceState(&pDevExt->UnicodeString, FALSE);

		pIrp->IoStatus.Status = STATUS_SUCCESS;
		IoSkipCurrentIrpStackLocation(pIrp);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		if (!NT_SUCCESS(ntStatus))
		{
			pIrp->IoStatus.Status = ntStatus;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			return ntStatus;
		}
		break;
	}

	default:
	{
		IoSkipCurrentIrpStackLocation(pIrp);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		break;
	}
	}

	return ntStatus;

}

// --------------- PCISAMPLE_CommonCompletionRoutine ----------------------

NTSTATUS
PCISAMPLE_CommonCompletionRoutine(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp,
	IN PVOID pContext
)
{
	PKEVENT pEvent = NULL;

	UNREFERENCED_PARAMETER(pFDO);

	if ((pIrp == NULL) || (pContext == NULL))
	{
		return STATUS_UNSUCCESSFUL;
	}

	pEvent = (PKEVENT)pContext;
	KeSetEvent(pEvent, 1, FALSE);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

// --------------- PCISAMPLE_Cleanup ----------------------

NTSTATUS
PCISAMPLE_Cleanup(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;
	PPCISAMPLE_MAPPED_LIST pMappedList = NULL;

	UNREFERENCED_PARAMETER(pFDO);

	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)pFDO->DeviceExtension;
	if (pDevExt == NULL)
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		goto exit;
	}

	while (!IsListEmpty(&pDevExt->MappingManagerListHead))
	{
		pMappedList = (PPCISAMPLE_MAPPED_LIST)RemoveHeadList(&pDevExt->MappingManagerListHead);

		if (pMappedList->IsDMA == TRUE)
		{
			MmUnmapLockedPages((PVOID)pMappedList->MappedMemory.UserAddress, (PMDL)pMappedList->MappedMemory.pMdl);

			if (pMappedList->MappedMemory.pMdl)
			{
				IoFreeMdl((PMDL)pMappedList->MappedMemory.pMdl);
			}
			if (pMappedList->MappedMemory.KernelAddress)
			{
				MmFreeContiguousMemory((PVOID)pMappedList->MappedMemory.KernelAddress);
			}
		}
		else
		{
			MmUnmapLockedPages((PVOID)pMappedList->MappedMemory.UserAddress, (PMDL)pMappedList->MappedMemory.pMdl);

			if (pMappedList->MappedMemory.pMdl)
			{
				IoFreeMdl((PMDL)pMappedList->MappedMemory.pMdl);
			}
			if (pMappedList->MappedMemory.KernelAddress)
			{
				MmUnmapIoSpace((PVOID)pMappedList->MappedMemory.KernelAddress, pMappedList->MappedMemory.Size);
			}
		}

		ExFreePool(pMappedList);
	}

exit:
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

// --------------- PCISAMPLE_CreateClose ----------------------

NTSTATUS
PCISAMPLE_CreateClose(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION pStack = NULL;
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;

	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)pFDO->DeviceExtension;
	if (pDevExt == NULL)
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		goto exit;
	}

	pStack = IoGetCurrentIrpStackLocation(pIrp);
	if (pStack == NULL)
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		goto exit;
	}

	if (pStack->MajorFunction == IRP_MJ_CREATE)
	{
		if (pDevExt->EnterCount)
		{
			ntStatus = STATUS_UNSUCCESSFUL;
			goto exit;
		}

		pDevExt->EnterCount++;
	}
	else
	{
		pDevExt->EnterCount--;
	}
	ntStatus = STATUS_SUCCESS;

exit:
	pIrp->IoStatus.Status = ntStatus;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

// --------------- PCISAMPLE_DeviceIOControl ----------------------

NTSTATUS
PCISAMPLE_DeviceIOControl(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION pStack = NULL;
	ULONG IoctlCode = 0;
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;
	PPCISAMPLE_MAPPING_MEMORY pMapMemory = NULL;
	ULONGLONG RequestSize = 0;
	PHYSICAL_ADDRESS PhysicalAddress;
	int OutputBufferLength = 0;
	int InputBufferLength = 0;
	PVOID pTempMemory = NULL;
	KEVENT ConfigEvent = { 0, };
	PIO_STACK_LOCATION pConfigStack = NULL;
	ULONG Size;
	PPCISAMPLE_MAPPED_LIST pMappedList = NULL;
	PHANDLE pEvent = NULL;
	ULONGLONG *pSize = NULL;
	PHYSICAL_ADDRESS LowestPhysicalAddress;
	PHYSICAL_ADDRESS HighestPhysicalAddress;
	PHYSICAL_ADDRESS Boundary;

	if ((pFDO == NULL) || (pIrp == NULL))
	{
		return STATUS_UNSUCCESSFUL;
	}

	pStack = IoGetCurrentIrpStackLocation(pIrp);
	if (pStack == NULL)
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		goto exit;
	}

	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)pFDO->DeviceExtension;
	if (pDevExt == NULL)
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		goto exit;
	}

	IoctlCode = pStack->Parameters.DeviceIoControl.IoControlCode;
	switch (IoctlCode)
	{
	case IOCTL_PCISAMPLE_MAPPING_MEMORY:
	{
		OutputBufferLength = pStack->Parameters.DeviceIoControl.OutputBufferLength;
		InputBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;

		if (OutputBufferLength != sizeof(PCISAMPLE_MAPPING_MEMORY))
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		if (InputBufferLength != sizeof(PCISAMPLE_MAPPING_MEMORY))
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		pMapMemory = (PPCISAMPLE_MAPPING_MEMORY)pIrp->AssociatedIrp.SystemBuffer;
		RequestSize = pMapMemory->Size;
		PhysicalAddress.QuadPart = pMapMemory->PhysicalAddress;

		pTempMemory = MmMapIoSpace(PhysicalAddress, RequestSize, MmNonCached);
		if (pTempMemory == NULL) 
		{
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		pMapMemory->KernelAddress = (ULONGLONG)pTempMemory;
		pMapMemory->pMdl = (ULONGLONG)IoAllocateMdl(pTempMemory, (ULONG)RequestSize, FALSE, FALSE, NULL);
		MmBuildMdlForNonPagedPool((PMDL)pMapMemory->pMdl);
		pMapMemory->UserAddress = (ULONGLONG)MmMapLockedPages((PMDL)pMapMemory->pMdl, UserMode);

		pIrp->IoStatus.Information = OutputBufferLength;
		{
			pMappedList = (PPCISAMPLE_MAPPED_LIST)ExAllocatePool(NonPagedPool, sizeof(PCISAMPLE_MAPPED_LIST));
			memset(pMappedList, 0, sizeof(PCISAMPLE_MAPPED_LIST));
			InsertTailList(&pDevExt->MappingManagerListHead, &pMappedList->Entry);
			pMappedList->MappedMemory.KernelAddress = pMapMemory->KernelAddress;
			pMappedList->MappedMemory.pMdl = pMapMemory->pMdl;
			pMappedList->MappedMemory.UserAddress = pMapMemory->UserAddress;
			pMappedList->MappedMemory.Size = RequestSize;
			pMappedList->IsDMA = FALSE;
			pMapMemory->pListEntry = (ULONGLONG)&pMappedList->Entry;
		}
	}
	break;

	case IOCTL_PCISAMPLE_UNMAPPING_MEMORY:
	{
		InputBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;

		if (InputBufferLength != sizeof(PCISAMPLE_MAPPING_MEMORY))
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		pMapMemory = (PPCISAMPLE_MAPPING_MEMORY)pIrp->AssociatedIrp.SystemBuffer;

		if (MmIsAddressValid((PVOID)pMapMemory->KernelAddress) == FALSE)
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		if (MmIsAddressValid(&pMapMemory->UserAddress) == FALSE)
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		MmUnmapLockedPages((PVOID)pMapMemory->UserAddress, (PMDL)pMapMemory->pMdl);

		if (pMapMemory->pMdl)
		{
			IoFreeMdl((PMDL)pMapMemory->pMdl);
		}
		if (pMapMemory->KernelAddress)
		{
			MmUnmapIoSpace((PVOID)pMapMemory->KernelAddress, pMapMemory->Size);
		}
		{
			PLIST_ENTRY pListEntry = (PLIST_ENTRY)pMapMemory->pListEntry;
			RemoveEntryList(pListEntry);
			ExFreePool(pListEntry);
		}
	}
	break;

	case IOCTL_PCISAMPLE_GET_CONFIGURATION_REGISTER:
	{
		Size = pStack->Parameters.DeviceIoControl.OutputBufferLength;
		Size = (Size > sizeof(PCIDEF_COMMON_HEADER)) ? sizeof(PCIDEF_COMMON_HEADER) : Size;

		pConfigStack = IoGetNextIrpStackLocation(pIrp);
		pConfigStack->MajorFunction = IRP_MJ_PNP;
		pConfigStack->MinorFunction = IRP_MN_READ_CONFIG;
		pConfigStack->Parameters.ReadWriteConfig.WhichSpace = PCI_WHICHSPACE_CONFIG;
		pConfigStack->Parameters.ReadWriteConfig.Buffer = pIrp->AssociatedIrp.SystemBuffer;
		pConfigStack->Parameters.ReadWriteConfig.Offset = 0;
		pConfigStack->Parameters.ReadWriteConfig.Length = Size;

		KeInitializeEvent(&ConfigEvent, NotificationEvent, FALSE);

		IoSetCompletionRoutine(pIrp, PCISAMPLE_CommonCompletionRoutine, &ConfigEvent, TRUE, TRUE, TRUE);
		ntStatus = IoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
		if (!NT_SUCCESS(ntStatus))
		{
			goto exit;
		}

		if (ntStatus == STATUS_PENDING)
		{
			ntStatus = KeWaitForSingleObject(&ConfigEvent, Executive, KernelMode, FALSE, NULL);
			if (!NT_SUCCESS(ntStatus))
			{
				goto exit;
			}
			ntStatus = pIrp->IoStatus.Status;
		}
		if (NT_SUCCESS(ntStatus))
		{
			pIrp->IoStatus.Information = pConfigStack->Parameters.ReadWriteConfig.Length;
		}
		break;
	}

	case IOCTL_PCISAMPLE_REGISTER_SHARED_EVENT:
	{
		pEvent = (PHANDLE)pIrp->AssociatedIrp.SystemBuffer;
		if (pEvent == NULL)
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			pIrp->IoStatus.Information = 0;
			break;
		}

		if (pDevExt->pInterruptEventObject)
		{
			KeSetEvent((PKEVENT)pDevExt->pInterruptEventObject, 0, FALSE);
			ObDereferenceObject((PKEVENT)pDevExt->pInterruptEventObject);
			pDevExt->pInterruptEventObject = 0;
		}

		ntStatus = ObReferenceObjectByHandle(*pEvent, EVENT_ALL_ACCESS, *ExEventObjectType, UserMode, (PVOID *)&(pDevExt->pInterruptEventObject), NULL);
		if (!NT_SUCCESS(ntStatus))
		{
			break;
		}

		pIrp->IoStatus.Information = sizeof(HANDLE);
		break;
	}

	case IOCTL_PCISAMPLE_ALLOCATE_CONTIGUOUS_MEMORY_FOR_DMA:
	{
		OutputBufferLength = pStack->Parameters.DeviceIoControl.OutputBufferLength;
		InputBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;

		if (OutputBufferLength != sizeof(PCISAMPLE_MAPPING_MEMORY))
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		if (InputBufferLength != sizeof(ULONGLONG))
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		pSize = (ULONGLONG *)pIrp->AssociatedIrp.SystemBuffer;
		RequestSize = *pSize;
		LowestPhysicalAddress.QuadPart = 0;
		HighestPhysicalAddress.QuadPart = 0xFFFFFFF0;
		HighestPhysicalAddress.QuadPart = 0xFFFFFFF0;
		Boundary.QuadPart = 0x100000; // 1MB

		pMapMemory = (PPCISAMPLE_MAPPING_MEMORY)pIrp->AssociatedIrp.SystemBuffer;

		pTempMemory = MmAllocateContiguousMemorySpecifyCache(RequestSize, LowestPhysicalAddress, HighestPhysicalAddress, Boundary, MmNonCached);
		if (pTempMemory == NULL) 
		{
			ntStatus = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		pMapMemory->KernelAddress = (ULONGLONG)pTempMemory;
		memset(pTempMemory, 0, RequestSize);

		pMapMemory->pMdl = (ULONGLONG)IoAllocateMdl(pTempMemory, (ULONG)RequestSize, FALSE, FALSE, NULL);
		MmBuildMdlForNonPagedPool((PMDL)pMapMemory->pMdl);
		pMapMemory->UserAddress = (ULONGLONG)MmMapLockedPages((PMDL)pMapMemory->pMdl, UserMode);
		Boundary = MmGetPhysicalAddress(pTempMemory);
		pMapMemory->PhysicalAddress = Boundary.QuadPart;
		pMapMemory->Size = RequestSize;

		pIrp->IoStatus.Information = OutputBufferLength;
		{
			pMappedList = (PPCISAMPLE_MAPPED_LIST)ExAllocatePool(NonPagedPool, sizeof(PCISAMPLE_MAPPED_LIST));
			memset(pMappedList, 0, sizeof(PCISAMPLE_MAPPED_LIST));
			InsertTailList(&pDevExt->MappingManagerListHead, &pMappedList->Entry);
			pMappedList->MappedMemory.KernelAddress = pMapMemory->KernelAddress;
			pMappedList->MappedMemory.pMdl = pMapMemory->pMdl;
			pMappedList->MappedMemory.UserAddress = pMapMemory->UserAddress;
			pMappedList->IsDMA = TRUE;
			pMapMemory->pListEntry = (ULONGLONG)&pMappedList->Entry;
		}
	}
	break;

	case IOCTL_PCISAMPLE_FREE_CONTIGUOUS_MEMORY_FOR_DMA:
	{
		InputBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;

		if (InputBufferLength != sizeof(PCISAMPLE_MAPPING_MEMORY))
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		pMapMemory = (PPCISAMPLE_MAPPING_MEMORY)pIrp->AssociatedIrp.SystemBuffer;

		if (MmIsAddressValid((PVOID)pMapMemory->KernelAddress) == FALSE)
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		if (MmIsAddressValid((PVOID)pMapMemory->UserAddress) == FALSE)
		{
			ntStatus = STATUS_INVALID_PARAMETER;
			break;
		}

		MmUnmapLockedPages((PVOID)pMapMemory->UserAddress, (PMDL)pMapMemory->pMdl);

		if (pMapMemory->pMdl)
		{
			IoFreeMdl((PMDL)pMapMemory->pMdl);
		}
		if (pMapMemory->KernelAddress)
		{
			MmFreeContiguousMemory((PVOID)pMapMemory->KernelAddress);
		}
		{
			PLIST_ENTRY pListEntry = (PLIST_ENTRY)pMapMemory->pListEntry;
			RemoveEntryList(pListEntry);
			ExFreePool(pListEntry);
		}
	}
	break;

	default:
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		break;
	}
	}


exit:
	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

// --------------- PCISAMPLE_PowerDispatch ----------------------

NTSTATUS
PCISAMPLE_PowerDispatch(
	PDEVICE_OBJECT pDeviceObject,
	PIRP pIrp
)
{
	NTSTATUS NtStatus = STATUS_SUCCESS;
	PPCISAMPLE_DEVICE_EXTENSION pDevExt = NULL;

	if (pIrp == NULL)
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (pDeviceObject == NULL)
	{
		NtStatus = STATUS_UNSUCCESSFUL;
		pIrp->IoStatus.Status = NtStatus;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return NtStatus;
	}

	pDevExt = (PPCISAMPLE_DEVICE_EXTENSION)pDeviceObject->DeviceExtension;
	if (pDevExt == NULL)
	{
		NtStatus = STATUS_UNSUCCESSFUL;
		pIrp->IoStatus.Status = NtStatus;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return NtStatus;
	}

	IoCopyCurrentIrpStackLocationToNext(pIrp);
	PoStartNextPowerIrp(pIrp);
	NtStatus = PoCallDriver(pDevExt->pNextLowerDeviceObject, pIrp);
	return NtStatus;
}

