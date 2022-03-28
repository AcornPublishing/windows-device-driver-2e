#include <wdm.h>
#include <wdmsec.h>
#include <stdio.h>

#include "devioctl.h"

#include ".\\..\common\appinterface.h"

#ifndef _TABLEHOOKER_H_
#define _TABLEHOOKER_H_

// --------------- 상수를 정의한다 ----------------------
// 가로채기를 수행할 대상의 이름을 정의한다
#define TARGET_DRIVER_NAME	L"\\Driver\\KBDCLASS" // 키보드클래스드라이버를 가로채기해본다

// 응용프로그램과 연결되기 위해서 정의하는 이름
#define CUSTOM_DEVICE_NAME	L"\\Device\\" CUSTOM_NAME
#define CUSTOM_WIN32_NAME	L"\\DosDevices\\" CUSTOM_NAME

typedef struct _TABLEHOOKER_DEVICE_EXTENSION
{
	ULONG		CustomSignature; // 'HAJE'
} TABLEHOOKER_DEVICE_EXTENSION, *PTABLEHOOKER_DEVICE_EXTENSION;

NTSTATUS DriverEntry(
	PDRIVER_OBJECT		pDriverObject,
	PUNICODE_STRING		pRegistryPath
);

void TABLEHOOKER_Unload(
	PDRIVER_OBJECT	pDriverObject
);

NTSTATUS
TABLEHOOKER_Cleanup(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp
);

NTSTATUS
TABLEHOOKER_Create(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp
);

NTSTATUS
TABLEHOOKER_Close(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp
);

NTSTATUS
TABLEHOOKER_DeviceIoControl(
	IN PDEVICE_OBJECT	pFDO,
	IN PIRP				pIrp
);

// --------------- 각종 유틸리티함수를 정의한다 ----------------------
void HookCallback(unsigned char MajorFunction);
void UnhookCallback(unsigned char MajorFunction);
PDRIVER_OBJECT SearchDriverObject(PUNICODE_STRING pUni);
NTSTATUS NewHookerDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);

BOOLEAN
InsertLog(
	PIRP	Irp,
	BOOLEAN bIsStatus);

ULONG
GetAndRemoveLog(
	void * OutputBuffer,
	ULONG  OutputBufferLength);

void
SpyClearLog();



NTSTATUS NTAPI ObReferenceObjectByName(PUNICODE_STRING ObjectName,
	ULONG Attributes,
	PACCESS_STATE AccessState,
	ACCESS_MASK DesiredAccess,
	POBJECT_TYPE ObjectType,
	KPROCESSOR_MODE AccessMode,
	PVOID ParseContext OPTIONAL,
	PVOID* Object);

POBJECT_TYPE ObGetObjectType(PVOID Object);

typedef struct _RECORD_LIST {
	LIST_ENTRY						List;
	RECORD_LIST_APP					ListData;
} RECORD_LIST, *PRECORD_LIST;


#endif //_TABLEHOOKER_H_