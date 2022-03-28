#include "tablehooker.h"

// --------------- 전역변수들을 정의한다 ----------------------
PDRIVER_OBJECT		gpTargetDriverObject = NULL;
PDRIVER_DISPATCH	gfnOrgDriverDispatch[IRP_MJ_MAXIMUM_FUNCTION + 1] = { 0, };
PDRIVER_OBJECT		gMainDriverObject = NULL;
KSPIN_LOCK			gLogLock; // 로그작업시 사용되는 스핀락을 정의한다
int					gLogCount = 0; // 현재 기억되어있는 로그엔트리의 수
int					gSequenceNumber = 0; // 로그의 순서를 담는 변수
LIST_ENTRY			gLogList; // 로그를 기억하는 리스트헤드

// ---------------- 내부 함수들 ------------------------
// 로그를 모두 지우는 내부 함수
VOID
internal_EmptyLog(
	VOID
)
{
	PRECORD_LIST pRecordList;
	KIRQL oldIrql;

	KeAcquireSpinLock(&gLogLock, &oldIrql);

	while (!IsListEmpty(&gLogList)) {

		pRecordList = (PRECORD_LIST)RemoveHeadList(&gLogList);
		KeReleaseSpinLock(&gLogLock, oldIrql);

		// ....

		ExFreePool(pRecordList);

		KeAcquireSpinLock(&gLogLock, &oldIrql);
	}

	gLogCount = 0;
	gSequenceNumber = 0;

	KeReleaseSpinLock(&gLogLock, oldIrql);
}

// ---------------- Utility 함수들 ------------------------

PDRIVER_OBJECT SearchDriverObject(PUNICODE_STRING pUni)
{
	NTSTATUS st;
	PDRIVER_OBJECT Object;
	POBJECT_TYPE TypeObjectType = ObGetObjectType(gMainDriverObject);

	st = ObReferenceObjectByName(pUni, OBJ_CASE_INSENSITIVE, NULL, 0, TypeObjectType, KernelMode, NULL, &Object);

	if (st != STATUS_SUCCESS)
		return (PDRIVER_OBJECT)0;

	ObDereferenceObject(Object);
	return Object;
}

// 로그항목을 하나 보관하는 함수
BOOLEAN
InsertLog(
	PIRP	Irp,
	BOOLEAN bIsStatus)
{
	PRECORD_LIST pRecordList = NULL;
	KIRQL	oldIrql;
	ULONG	Length = 0;
	BOOLEAN	bRet = FALSE;
	PIO_STACK_LOCATION	pStack = NULL;
	void *	pTargetBuffer = NULL;
	
	if (gLogCount >= MAX_LOG_COUNT)
		goto exit;

	pStack = IoGetCurrentIrpStackLocation(Irp);

	Length = sizeof(RECORD_LIST);

	pRecordList = (PRECORD_LIST)ExAllocatePool(NonPagedPool, Length);
	if (pRecordList == NULL)
		goto exit;

	memset(pRecordList, 0, Length);

	InitializeListHead(&pRecordList->List);

	pRecordList->ListData.Length = Length - sizeof(LIST_ENTRY);
	pRecordList->ListData.bIsStatus = bIsStatus;

	pRecordList->ListData.MajorFunction = pStack->MajorFunction;

	switch (pStack->MajorFunction)
	{
	case IRP_MJ_READ:
		if (bIsStatus == FALSE)
		{
			pRecordList->ListData.extra.ReadRequest.Length = pStack->Parameters.Read.Length;
			pRecordList->ListData.extra.ReadRequest.Offset = pStack->Parameters.Read.ByteOffset.QuadPart;
		}
		else
		{
			pRecordList->ListData.extra.ReadStatus.ntStatus = Irp->IoStatus.Status;
			pRecordList->ListData.extra.ReadStatus.Information = Irp->IoStatus.Information;
			if (NT_SUCCESS(Irp->IoStatus.Status))
			{
				pRecordList->ListData.extra.ReadStatus.DataLength = (ULONG)Irp->IoStatus.Information;
				if (pRecordList->ListData.extra.ReadStatus.DataLength)
				{
					if (Irp->AssociatedIrp.SystemBuffer)
					{
						memcpy(pRecordList->ListData.extra.ReadStatus.DataBuffer, 
							Irp->AssociatedIrp.SystemBuffer, 
							pRecordList->ListData.extra.ReadStatus.DataLength);
					}
					else if (Irp->MdlAddress)
					{
						pTargetBuffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);
						if (pTargetBuffer)
						{
							memcpy(pRecordList->ListData.extra.ReadStatus.DataBuffer,
								pTargetBuffer,
								pRecordList->ListData.extra.ReadStatus.DataLength);
						}
					}
				}
			}
			pRecordList->ListData.extra.ReadStatus.ntStatus = Irp->IoStatus.Status;
		}
		break;
	}

	pRecordList->ListData.SequenceNumber = gSequenceNumber;
	gSequenceNumber++;

	KeAcquireSpinLock(&gLogLock, &oldIrql);

	InsertTailList(&gLogList, &pRecordList->List);
	gLogCount++;

	KeReleaseSpinLock(&gLogLock, oldIrql);

	bRet = TRUE;

exit:
	return bRet;
}

// 로그항목을 하나 가져오면서, 지우는 함수
ULONG
GetAndRemoveLog(
	void * OutputBuffer,
	ULONG  OutputBufferLength)
{
	PRECORD_LIST pRecordList = NULL;
	KIRQL	oldIrql;
	ULONG	Length = 0;

	if (OutputBuffer == NULL)
		goto exit;

	if (OutputBufferLength == 0)
		goto exit;

	KeAcquireSpinLock(&gLogLock, &oldIrql);

	if (gLogCount == 0)
	{
		KeReleaseSpinLock(&gLogLock, oldIrql);
		goto exit;
	}

	pRecordList = (PRECORD_LIST)RemoveHeadList(&gLogList);
	gLogCount--;

	KeReleaseSpinLock(&gLogLock, oldIrql);

	Length = pRecordList->ListData.Length;
	Length = (OutputBufferLength > Length) ? Length : OutputBufferLength;

	memcpy(OutputBuffer, &pRecordList->ListData, Length);

	ExFreePool(pRecordList);

exit:
	return Length;
}

// 모든 로그를 삭제하는 함수
void
SpyClearLog()
{
	internal_EmptyLog();
}

/*
doPreparePostProcessingIrp() 함수는 후처리작업을 위해서 완료루틴(CompletionRoutine)을 설정하는 작업을 수행하는 함수이다
Hooker 함수가 호출되는 시기는 엄격하게 말해서, 가로채기수준에서 호출되었기 때문에, Hooker가 사용할만한 개인적인 IO_STACK_LOCATION 이 없다
이것은 정상적인 CompletionRoutine설정작업이 금지된다는 의미이기도 하기 때문에, 트릭(Trick)을 사용해야 한다
*/
typedef struct _COMPLETION_CONTEXT
{
	IO_STACK_LOCATION	Stack;					// 원래 Stack
}COMPLETION_CONTEXT, *PCOMPLETION_CONTEXT;

NTSTATUS HookerCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID pContext)
{
	PCOMPLETION_CONTEXT	pNewCompletionContext = NULL;
	IO_STACK_LOCATION	TempStack;

	UCHAR OrgControl = 0;
	PIO_COMPLETION_ROUTINE OrgCompletionRoutine = NULL;
	PVOID OrgContext = NULL;

	BOOLEAN bMustCallOrgCompletionRoutine = FALSE;
	PIO_STACK_LOCATION	pLowerStackLocation = NULL;

	pNewCompletionContext = (PCOMPLETION_CONTEXT)pContext;
	if (pNewCompletionContext == NULL)
	{
		goto exit;
	}

	memcpy(&TempStack, &pNewCompletionContext->Stack, sizeof(IO_STACK_LOCATION));
	OrgControl = TempStack.Control;
	OrgCompletionRoutine = TempStack.CompletionRoutine;
	OrgContext = TempStack.Context;

	// 후처리 로그를 남긴다
	// 이때, 주의할점은 현재 StackLocation 위치가 Hooker의 위 계층의 소유라는 점이다
	pLowerStackLocation = IoGetNextIrpStackLocation(Irp);
	memcpy(pLowerStackLocation, &TempStack, sizeof(IO_STACK_LOCATION));
	IoSetNextIrpStackLocation(Irp);
	InsertLog(Irp, TRUE);
	IoSkipCurrentIrpStackLocation(Irp);
	//

exit:
	if (pNewCompletionContext)
	{
		ExFreePool(pNewCompletionContext);
	}

	// Hooker 이전에 호출자의 Completion Routine 이 설정되어있으면서, 호출될 조건이 맞는지를 확인한다
	if ((OrgControl & SL_INVOKE_ON_SUCCESS) && (Irp->IoStatus.Status == STATUS_SUCCESS))
	{
		bMustCallOrgCompletionRoutine = TRUE;
	}
	else if ((OrgControl & SL_INVOKE_ON_CANCEL) && (Irp->IoStatus.Status == STATUS_CANCELLED))
	{
		bMustCallOrgCompletionRoutine = TRUE;
	}
	else if ((OrgControl & SL_INVOKE_ON_ERROR) && (!NT_SUCCESS(Irp->IoStatus.Status)))
	{
		bMustCallOrgCompletionRoutine = TRUE;
	}

	if ((bMustCallOrgCompletionRoutine == TRUE) && OrgCompletionRoutine)
	{
		return OrgCompletionRoutine(DeviceObject, Irp, OrgContext);
	}

	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);

	return Irp->IoStatus.Status;
}

void doPreparePostProcessingIrp(PIRP Irp)
{
	PCOMPLETION_CONTEXT	pNewCompletionContext = NULL;
	PIO_STACK_LOCATION	pLowerStackLocation = NULL;

	pNewCompletionContext = (PCOMPLETION_CONTEXT)ExAllocatePool(NonPagedPool, sizeof(COMPLETION_CONTEXT));
	if (pNewCompletionContext == NULL)
		goto exit;

	memset(pNewCompletionContext, 0, sizeof(COMPLETION_CONTEXT));

	// 현재까지 Irp->Tail.Overlay.CurrentIrpStackLocation 은 Hooker의 아래계층의 소유이다
	pLowerStackLocation = IoGetCurrentIrpStackLocation(Irp);

	// 원래 Hooker의 아래계층이 보아야하는 내용의 문맥정보를 백업한다
	memcpy(&pNewCompletionContext->Stack, pLowerStackLocation, sizeof(IO_STACK_LOCATION));

	// 새로운 Completion Routine을 설정한다
	pLowerStackLocation->CompletionRoutine = HookerCompletionRoutine;
	pLowerStackLocation->Context = pNewCompletionContext;
	pLowerStackLocation->Control = (SL_INVOKE_ON_SUCCESS| SL_INVOKE_ON_ERROR | SL_INVOKE_ON_CANCEL);

exit:
	return;
}

////////////////////////////////////////////////////
// 가로채기함수

// 발생되는 정보를 로그로 남기는 작업만 수행한다
NTSTATUS NewHookerDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS			ntStatus = STATUS_NOT_SUPPORTED;
	PIO_STACK_LOCATION	pStack = NULL;
	PDRIVER_DISPATCH	pOrgDispatchFunction = NULL;
	BOOLEAN				bInserted = FALSE;
	
	pStack = IoGetCurrentIrpStackLocation(Irp); // IRP의 파라미터를 확인한다

	pOrgDispatchFunction = gfnOrgDriverDispatch[pStack->MajorFunction];

	if (pOrgDispatchFunction == NULL)
	{
		ntStatus = STATUS_NOT_SUPPORTED;
		goto exit;
	}

	switch (pStack->MajorFunction)
	{
	case IRP_MJ_READ:
		bInserted = InsertLog(Irp, FALSE);
		break;
	}

	if (bInserted == TRUE)
	{
		// Completion Routine을 설정해서, 후처리 로그도 생성해야 한다
		doPreparePostProcessingIrp(Irp);
	}

	goto bypass;

bypass:
	ntStatus = pOrgDispatchFunction(DeviceObject, Irp);
	return ntStatus;

exit:
	Irp->IoStatus.Status = ntStatus;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return ntStatus;
}

void HookCallback(unsigned char MajorFunction)
{
	if (gpTargetDriverObject == NULL)
		goto exit;

	if(gfnOrgDriverDispatch[MajorFunction]) // 이미 가로채기주소가 설정된 상태라면,
		goto exit;

	gfnOrgDriverDispatch[MajorFunction] = gpTargetDriverObject->MajorFunction[MajorFunction];
	gpTargetDriverObject->MajorFunction[MajorFunction] = NewHookerDispatch;

exit:
	return;
}

void UnhookCallback(unsigned char MajorFunction)
{
	if (gpTargetDriverObject == NULL)
		goto exit;

	if (gfnOrgDriverDispatch[MajorFunction] == NULL) // 가로채기주소가 설정되지 않은 상태라면,
		goto exit;

	gpTargetDriverObject->MajorFunction[MajorFunction] = gfnOrgDriverDispatch[MajorFunction];
	gfnOrgDriverDispatch[MajorFunction] = NULL;

exit:
	return;
}

// --------------- DriverEntry ----------------------

NTSTATUS
DriverEntry(
	PDRIVER_OBJECT pDriverObject,
	PUNICODE_STRING pRegistryPath
)
{
	UNICODE_STRING		sddlString;
	UNICODE_STRING		DeviceName;
	UNICODE_STRING		Win32Name;
	PDEVICE_OBJECT		pCustomDeviceObject = NULL;
	UNICODE_STRING		TargetDriverUniName;
	NTSTATUS			ntStatus = STATUS_UNSUCCESSFUL;
	TABLEHOOKER_DEVICE_EXTENSION	* pDeviceExtension = NULL;

	UNREFERENCED_PARAMETER(pRegistryPath);

	gMainDriverObject = pDriverObject;

	// 가로채기작업을 수행한다
	RtlInitUnicodeString(&TargetDriverUniName, TARGET_DRIVER_NAME);

	gpTargetDriverObject = SearchDriverObject(&TargetDriverUniName);
	if (gpTargetDriverObject == NULL)
	{
		goto exit;// 대상이름이 발견되지 않으면 그냥 종료한다
	}

	RtlInitUnicodeString(&DeviceName, CUSTOM_DEVICE_NAME);
	RtlInitUnicodeString(&Win32Name, CUSTOM_WIN32_NAME);
	RtlInitUnicodeString(&sddlString, L"D:P(A;;GA;;;BA)(A;;GA;;;SY)");

	// Custom 드라이버를 위한 Custom Device Object를 생성한다
	ntStatus = IoCreateDeviceSecure(pDriverObject, sizeof(TABLEHOOKER_DEVICE_EXTENSION),
		&DeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, 
		FALSE, &sddlString, 0, &pCustomDeviceObject);

	if (!NT_SUCCESS(ntStatus))
	{
		goto exit; // DEVICE_OBJECT를 생성할 수 없다면, 종료한다
	}

	// 전용응용 프로그램과 통신하는 채널을 위해 심볼링링크를 설정한다
	ntStatus = IoCreateSymbolicLink(&Win32Name, &DeviceName);
	if (!NT_SUCCESS(ntStatus))
	{ // 심볼링링크를 설정할 수 없다면, 종료한다
		IoDeleteDevice(pCustomDeviceObject);
		goto exit;
	}
									
	pDeviceExtension = (TABLEHOOKER_DEVICE_EXTENSION *)pCustomDeviceObject->DeviceExtension;
	memset(pDeviceExtension, 0, sizeof(TABLEHOOKER_DEVICE_EXTENSION));

	pDeviceExtension->CustomSignature = 'HAJE'; // 시그니쳐를 등록한다
	/*
	지금은 TableHooker 드라이버가 수행하는 작업이 특정드라이버의 동작을 감시하는 목적이므로, 시그니쳐가 꼭 필요한것은 아니다.
	시그니쳐는 현재 드라이버가 미니포트드라이버모델을 사용하는 경우를 위해서 정의해둔다
	*/

	// Custom 드라이버를 위한 콜백함수를 등록한다
	pDriverObject->DriverUnload = TABLEHOOKER_Unload;
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = TABLEHOOKER_Create; // for Win32 API CreateFile()
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = TABLEHOOKER_Close; // for Win32 API CloseHandle()
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = TABLEHOOKER_Cleanup; // for Win32 API CloseHandle()
	// Win32 API CloseHandle() -> IRP_MJ_CLEANUP -> IRP_MJ_CLOSE

	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = TABLEHOOKER_DeviceIoControl; // for Win32 API DeviceIoControl()

	KeInitializeSpinLock(&gLogLock);
	InitializeListHead(&gLogList);

	pCustomDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING; // 응용프로그램으로 부터 접근을 허용한다

	ntStatus = STATUS_SUCCESS;

exit:
	return ntStatus;
}

// --------------- TABLEHOOKER_Unload ----------------------

void
TABLEHOOKER_Unload(
	PDRIVER_OBJECT pDriverObject
)
{
	UNICODE_STRING		Win32Name;

	UNREFERENCED_PARAMETER(pDriverObject);

	// 생성되어있는 모든 로그를 지운다
	SpyClearLog();

	// 심볼릭링크를 해제한다
	RtlInitUnicodeString(&Win32Name, CUSTOM_WIN32_NAME);
	IoDeleteSymbolicLink(&Win32Name);

	// Custom Device Object를 제거한다
	IoDeleteDevice(pDriverObject->DeviceObject); 
	// Custom Driver가 생성한 DEVICE_OBJECT는 현재 Custom Device Object 밖에 없으므로,

	return;
}

// --------------- TABLEHOOKER_Create ----------------------

NTSTATUS
TABLEHOOKER_Create(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(pFDO);

	//////////////////////////////////////////////////////////////
	// 감시할 대상 드라이버의 주요 콜백함수를 가로채기한다
	HookCallback(IRP_MJ_READ);

	goto exit;

exit:
	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

// --------------- TABLEHOOKER_Cleanup ----------------------

NTSTATUS
TABLEHOOKER_Cleanup(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(pFDO);

	goto exit;

exit:
	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

// --------------- TABLEHOOKER_Close ----------------------

NTSTATUS
TABLEHOOKER_Close(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	UNREFERENCED_PARAMETER(pFDO);

	// 이 시기에 가로채기를 복원하는 것이 좋다
	UnhookCallback(IRP_MJ_READ);

	goto exit;

exit:
	pIrp->IoStatus.Status = ntStatus;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}

// --------------- TABLEHOOKER_DeviceIoControl ----------------------

NTSTATUS
TABLEHOOKER_DeviceIoControl(
	IN PDEVICE_OBJECT pFDO,
	IN PIRP pIrp
)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PIO_STACK_LOCATION pStack = NULL;
	ULONG IoctlCode = 0;
	int OutputBufferLength = 0;
	int InputBufferLength = 0;
	ULONG_PTR Information = 0;

	UNREFERENCED_PARAMETER(pFDO);

	pStack = IoGetCurrentIrpStackLocation(pIrp);
	if (pStack == NULL)
	{
		ntStatus = STATUS_UNSUCCESSFUL;
		goto exit;
	}

	OutputBufferLength = pStack->Parameters.DeviceIoControl.OutputBufferLength;
	InputBufferLength = pStack->Parameters.DeviceIoControl.InputBufferLength;

	IoctlCode = pStack->Parameters.DeviceIoControl.IoControlCode;

	switch (IoctlCode)
	{
		case IOCTL_TABLEHOOKER_GET_LOG:
		{
			if (OutputBufferLength < sizeof(RECORD_LIST_APP))
			{
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}
			if (pIrp->AssociatedIrp.SystemBuffer == NULL)
			{
				ntStatus = STATUS_INVALID_PARAMETER;
				break;
			}
			Information = (ULONG_PTR)GetAndRemoveLog(pIrp->AssociatedIrp.SystemBuffer, OutputBufferLength);
			ntStatus = STATUS_SUCCESS;
			break;
		}
		case IOCTL_TABLEHOOKER_CLR_LOG:
		{
			SpyClearLog();
			Information = 0;
			ntStatus = STATUS_SUCCESS;
			break;
		}
		default:
		{
			ntStatus = STATUS_UNSUCCESSFUL;
			break;
		}
	}

exit:
	pIrp->IoStatus.Status = ntStatus;
	pIrp->IoStatus.Information = Information;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return ntStatus;
}
