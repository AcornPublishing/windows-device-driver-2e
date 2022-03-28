#include <ntddk.h>
#include <usb100.h>
#include <usb200.h>
#include <usbdi.h>

#define CHILDDEVICECOUNT 1
#define BUSDRIVERDEVICE 0
#define CHILDDRIVERDEVICE 1
#define CHILDDEVICEDEVICEID L"USB\\VID_1234&PID_5678"
#define CHILDDEVICEHARDWAREID L"USB\\VID_1234&PID_5678"
#define CHILDDEVICECOMPATIBLEID L"USB\\HAJESOFTVIRTUALUSBDEVICE"
#define CHILDDEVICEINSTANCEID1 L"0001"
#define CHILDDEVICETEXT  L"USB Device"

#define CONFIGHANDLE 0xAAAAAAAA
#define INTERFACEHANDLE 0x55555555
#define PIPE0HANDLE 0x11111111
#define PIPE1HANDLE 0x22222222

#include "devioctl.h"
#include <initguid.h>

DEFINE_GUID(GUID_SAMPLE, 
0x5665dec0, 0xa40a, 0x11d1, 0xb9, 0x84, 0x0, 0x20, 0xaf, 0xd7, 0x97, 0x78);
// 현재 우리가 사용될 DeviceStack에 대한 Interface GUID를 정의합니다

// 응용프로그램과 주고받을 ControlCode를 정의합니다
#define IOCTL_BUSDRIVER_ADDDEVICE		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_BUSDRIVER_REMOVEDEVICE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0801, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct
{
	int DeviceObjectType;
	PDEVICE_OBJECT 	NextLayerDeviceObject;
	PDEVICE_OBJECT ChildDeviceObject[CHILDDEVICECOUNT];
	PDEVICE_OBJECT ParentDeviceObject;
	PDEVICE_OBJECT PhysicalDeviceObject;

	// USB IO
	PUSB_DEVICE_DESCRIPTOR pDeviceDescriptor;
	PUSB_CONFIGURATION_DESCRIPTOR pConfigurationDescriptor;
#define NOT_CONFIGURED 0
	char CurrentConfigValue;

	ULONG ConfigurationHandle;
	PUSBD_INTERFACE_INFORMATION pInterfaceInformation;

#define MAXIMUMLOOPBACKDATASIZE 0x100000
	unsigned long SizeofHolding;
	unsigned char *pLoopBackDataBuffer;

	LIST_ENTRY Pipe0IrpListHead;
	LIST_ENTRY Pipe1IrpListHead;

	KTIMER Pipe0Timer;
	KTIMER Pipe1Timer;

	KDPC Pipe0Dpc;
	KDPC Pipe1Dpc;

	BOOLEAN Removed;
	BOOLEAN HasChild;

	// Symbolic Link
	UNICODE_STRING UnicodeString; 
	// 사용자에게 허용될 DeviceStack에 대한 이름을 보관하는곳

}DEVICE_EXTENSION, *PDEVICE_EXTENSION;

typedef struct
{
	PIRP Irp;
	PIO_WORKITEM pIoWorkItem;
}WORKITEM_CONTEXT, *PWORKITEM_CONTEXT;

// Prototype
VOID
Pipe0TimerRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

VOID
Pipe1TimerRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

void
BUSDRIVER_WorkItemRoutine
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PVOID Context
	);

NTSTATUS
BUSDRIVER_DeviceControlDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	);

NTSTATUS
BUSDRIVER_InternalDeviceControlDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	);

NTSTATUS
USBIO
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	);

NTSTATUS
BUSDRIVER_AddDevice
	(
	IN PDRIVER_OBJECT DriverObject,         
	IN PDEVICE_OBJECT PhysicalDeviceObject  
	)
{
	NTSTATUS returnStatus = STATUS_SUCCESS;         
	PDEVICE_OBJECT DeviceObject = NULL;             
	PDEVICE_EXTENSION deviceExtension = NULL;

	returnStatus = IoCreateDevice                                       
			(                                               
				DriverObject,                               
				sizeof(DEVICE_EXTENSION),                
				NULL,                   
				FILE_DEVICE_UNKNOWN,
				0,                                          
				FALSE,                                      
				&DeviceObject       
			);

	deviceExtension = DeviceObject->DeviceExtension;
	deviceExtension->PhysicalDeviceObject = PhysicalDeviceObject;
	deviceExtension->DeviceObjectType = BUSDRIVERDEVICE;
	deviceExtension->HasChild = FALSE;

	deviceExtension->NextLayerDeviceObject =
		IoAttachDeviceToDeviceStack (
					DeviceObject,
					PhysicalDeviceObject	
	);

	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING; 
	DeviceObject->Flags |= deviceExtension->NextLayerDeviceObject->Flags & 
							( DO_POWER_PAGABLE  | DO_POWER_INRUSH); // DeviceObject의 Flag를 변경합니다

	IoRegisterDeviceInterface( PhysicalDeviceObject, (CONST GUID  *)&GUID_SAMPLE, NULL, &deviceExtension->UnicodeString );

	return returnStatus;
}

NTSTATUS
MyCompletionRoutine
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp,
	IN PVOID Context
	)
{
	PKEVENT pEvent = NULL;
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	pEvent = (PKEVENT)Context;
	KeSetEvent( pEvent, 0, FALSE );
	return STATUS_MORE_PROCESSING_REQUIRED;
}

void
CreateChildDevice
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	PDEVICE_OBJECT NextLayerDeviceObject = NULL;
	unsigned short wTotalLength;
	PUSB_CONFIGURATION_DESCRIPTOR pConfigurationDescriptor = NULL;
	PUSB_INTERFACE_DESCRIPTOR pInterfaceDescriptor = NULL;
	PUSB_ENDPOINT_DESCRIPTOR pEndpointDescriptor = NULL;
	PUSBD_INTERFACE_INFORMATION pInterfaceInformation = NULL;
	PDEVICE_EXTENSION childDeviceExtension = NULL;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;
	NextLayerDeviceObject = deviceExtension->NextLayerDeviceObject;

	// New Child Device 열거
	IoCreateDevice                                       
	(                                               
		DeviceObject->DriverObject,                               
		sizeof(DEVICE_EXTENSION),                
		NULL,                   
		FILE_DEVICE_UNKNOWN,
		FILE_AUTOGENERATED_DEVICE_NAME,                                          
		FALSE,                                      
		&deviceExtension->ChildDeviceObject[0]       
	);

	deviceExtension->ChildDeviceObject[0]->Flags |= DO_POWER_PAGABLE;
	deviceExtension->ChildDeviceObject[0]->Flags &= ~DO_DEVICE_INITIALIZING;

	childDeviceExtension = (PDEVICE_EXTENSION)deviceExtension->ChildDeviceObject[0]->DeviceExtension;
	childDeviceExtension->DeviceObjectType = CHILDDRIVERDEVICE;
	childDeviceExtension->ParentDeviceObject = DeviceObject;
	childDeviceExtension->Removed = FALSE;

	// USB Descriptor IO
	childDeviceExtension->pDeviceDescriptor = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePool( 
		NonPagedPool, 
		sizeof(USB_DEVICE_DESCRIPTOR) 
		);

	wTotalLength = sizeof(USB_CONFIGURATION_DESCRIPTOR) + sizeof(USB_INTERFACE_DESCRIPTOR) + sizeof(USB_ENDPOINT_DESCRIPTOR) * 2;

	childDeviceExtension->pConfigurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePool( 
		NonPagedPool, 
		wTotalLength );

	childDeviceExtension->CurrentConfigValue = NOT_CONFIGURED;

	// Descriptor Setup

	// DeviceDescriptor
	childDeviceExtension->pDeviceDescriptor->bLength = sizeof(USB_DEVICE_DESCRIPTOR);
	childDeviceExtension->pDeviceDescriptor->bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
	childDeviceExtension->pDeviceDescriptor->bcdUSB = 0x0200;
	childDeviceExtension->pDeviceDescriptor->bDeviceClass = 00;
	childDeviceExtension->pDeviceDescriptor->bDeviceSubClass = 00;
	childDeviceExtension->pDeviceDescriptor->bDeviceProtocol = 00;
	childDeviceExtension->pDeviceDescriptor->bMaxPacketSize0 = 0x40; // 64
	childDeviceExtension->pDeviceDescriptor->idVendor = 0x1234;
	childDeviceExtension->pDeviceDescriptor->idProduct = 0x5678;
	childDeviceExtension->pDeviceDescriptor->bcdDevice = 0;
	childDeviceExtension->pDeviceDescriptor->iManufacturer = 0;
	childDeviceExtension->pDeviceDescriptor->iProduct = 0;
	childDeviceExtension->pDeviceDescriptor->iSerialNumber = 0;
	childDeviceExtension->pDeviceDescriptor->bNumConfigurations = 1;

	// ConfigurationDescriptor
	pConfigurationDescriptor = childDeviceExtension->pConfigurationDescriptor;
	pConfigurationDescriptor->bLength = sizeof(USB_CONFIGURATION_DESCRIPTOR);
	pConfigurationDescriptor->bDescriptorType = USB_CONFIGURATION_DESCRIPTOR_TYPE;
	pConfigurationDescriptor->wTotalLength = wTotalLength;
	pConfigurationDescriptor->bNumInterfaces = 1;
	pConfigurationDescriptor->bConfigurationValue = 1;
	pConfigurationDescriptor->iConfiguration = 0;
	pConfigurationDescriptor->bmAttributes = 0x80; // BusPowered
	pConfigurationDescriptor->MaxPower = 0xFA; // 500mA

	// InterfaceDescriptor
	pInterfaceDescriptor = (PUSB_INTERFACE_DESCRIPTOR)(pConfigurationDescriptor+1);
	pInterfaceDescriptor->bLength = sizeof(USB_INTERFACE_DESCRIPTOR);
	pInterfaceDescriptor->bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE;
	pInterfaceDescriptor->bInterfaceNumber = 0;
	pInterfaceDescriptor->bAlternateSetting = 0;
	pInterfaceDescriptor->bNumEndpoints = 2;
	pInterfaceDescriptor->bInterfaceClass = 0xFF;
	pInterfaceDescriptor->bInterfaceSubClass = 0;
	pInterfaceDescriptor->bInterfaceProtocol = 0xFE;
	pInterfaceDescriptor->iInterface = 0;

	// EndpointDescriptor
	pEndpointDescriptor = (PUSB_ENDPOINT_DESCRIPTOR)(pInterfaceDescriptor+1);
	pEndpointDescriptor->bLength = sizeof(USB_ENDPOINT_DESCRIPTOR);
	pEndpointDescriptor->bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
	pEndpointDescriptor->bEndpointAddress = 0x81;
	pEndpointDescriptor->bmAttributes = USB_ENDPOINT_TYPE_BULK;
	pEndpointDescriptor->wMaxPacketSize = 0x200; // 512
	pEndpointDescriptor->bInterval = 0;

	pEndpointDescriptor++;
	pEndpointDescriptor->bLength = sizeof(USB_ENDPOINT_DESCRIPTOR);
	pEndpointDescriptor->bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
	pEndpointDescriptor->bEndpointAddress = 0x02;
	pEndpointDescriptor->bmAttributes = USB_ENDPOINT_TYPE_BULK;
	pEndpointDescriptor->wMaxPacketSize = 0x200; // 512
	pEndpointDescriptor->bInterval = 0;

	// USB Pipe준비
	childDeviceExtension->ConfigurationHandle = CONFIGHANDLE;
	childDeviceExtension->pInterfaceInformation = (PUSBD_INTERFACE_INFORMATION)ExAllocatePool( 
		NonPagedPool, 
		sizeof(USBD_INTERFACE_INFORMATION) + sizeof(USBD_PIPE_INFORMATION) );
	
	pInterfaceInformation = childDeviceExtension->pInterfaceInformation;
	RtlZeroMemory( pInterfaceInformation, sizeof(USBD_INTERFACE_INFORMATION) + sizeof(USBD_PIPE_INFORMATION) );
	pInterfaceInformation->Length = sizeof(USBD_INTERFACE_INFORMATION) + sizeof(USBD_PIPE_INFORMATION);
	pInterfaceInformation->InterfaceNumber = 0;
	pInterfaceInformation->AlternateSetting = 0;
	pInterfaceInformation->Class = 0xFF;
	pInterfaceInformation->SubClass = 0;
	pInterfaceInformation->Protocol = 0xFE;
	pInterfaceInformation->InterfaceHandle = (PVOID)INTERFACEHANDLE;
	pInterfaceInformation->NumberOfPipes = 2;
	pInterfaceInformation->Pipes[0].MaximumPacketSize = 0x200;
	pInterfaceInformation->Pipes[0].EndpointAddress = 0x81;
	pInterfaceInformation->Pipes[0].Interval = 0;
	pInterfaceInformation->Pipes[0].PipeType = USB_ENDPOINT_TYPE_BULK;
	pInterfaceInformation->Pipes[0].PipeHandle = (PVOID)PIPE0HANDLE;
	pInterfaceInformation->Pipes[1].MaximumPacketSize = 0x200;
	pInterfaceInformation->Pipes[1].EndpointAddress = 0x02;
	pInterfaceInformation->Pipes[1].Interval = 0;
	pInterfaceInformation->Pipes[1].PipeType = USB_ENDPOINT_TYPE_BULK;
	pInterfaceInformation->Pipes[1].PipeHandle = (PVOID)PIPE1HANDLE;

	InitializeListHead( &childDeviceExtension->Pipe0IrpListHead );
	InitializeListHead( &childDeviceExtension->Pipe1IrpListHead );

	KeInitializeTimer( &childDeviceExtension->Pipe0Timer );
	KeInitializeTimer( &childDeviceExtension->Pipe1Timer );

	KeInitializeDpc( &childDeviceExtension->Pipe0Dpc, Pipe0TimerRoutine, deviceExtension->ChildDeviceObject[0] );
	KeInitializeDpc( &childDeviceExtension->Pipe1Dpc, Pipe1TimerRoutine, deviceExtension->ChildDeviceObject[0] );

	KeSetTimerEx( &childDeviceExtension->Pipe0Timer, RtlConvertLongToLargeInteger( -1 * 100000), 1, &childDeviceExtension->Pipe0Dpc );
	KeSetTimerEx( &childDeviceExtension->Pipe1Timer, RtlConvertLongToLargeInteger( -1 * 100000), 1, &childDeviceExtension->Pipe1Dpc );

	// LoopBackBuffer 준비
	childDeviceExtension->pLoopBackDataBuffer = (unsigned char *)ExAllocatePool( NonPagedPool, MAXIMUMLOOPBACKDATASIZE+1 );
	childDeviceExtension->SizeofHolding = 0;
}

NTSTATUS
BUSDRIVER_CreateClose
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	if( deviceExtension->DeviceObjectType == BUSDRIVERDEVICE )
	{ // BusDriverDevice
		Irp->IoStatus.Information = 0;
		status = STATUS_SUCCESS;
	}
	Irp->IoStatus.Status = status;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return status;
}

NTSTATUS
BusDriverDevice_PnpDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	PDEVICE_OBJECT NextLayerDeviceObject = NULL;
	NTSTATUS returnStatus;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;
	NextLayerDeviceObject = deviceExtension->NextLayerDeviceObject;

	switch ( pStack->MinorFunction )
	{
		case IRP_MN_START_DEVICE :
		{
			{
				KEVENT Event;

				KeInitializeEvent( &Event, NotificationEvent, FALSE );

				IoCopyCurrentIrpStackLocationToNext( Irp );
				IoSetCompletionRoutine( 
					Irp, 
					MyCompletionRoutine, 
					&Event, 
					TRUE, 
					TRUE, 
					TRUE );
				returnStatus = IoCallDriver( NextLayerDeviceObject, Irp );
				if( returnStatus == STATUS_PENDING )
				{
					KeWaitForSingleObject( 
						&Event, 
						Executive, 
						KernelMode, 
						FALSE, 
						NULL );
					returnStatus = Irp->IoStatus.Status;
				}
				if( NT_SUCCESS( returnStatus ) )
				{
					IoSetDeviceInterfaceState( &deviceExtension->UnicodeString, TRUE ); 
					// 사용자가 우리의 DeviceStack에 접근하는 것을 허용
				}

				IoCompleteRequest( Irp, IO_NO_INCREMENT );
				return returnStatus;
			}			
		}
		break;			

		case IRP_MN_REMOVE_DEVICE :
		{ 
			IoSetDeviceInterfaceState( &deviceExtension->UnicodeString, FALSE ); 
			// 사용자가 우리의 DeviceStack에 접근하는 것을 허용

			RtlFreeUnicodeString( &deviceExtension->UnicodeString );
			// 사용자가 접근하는 이름을 메모리에서 해제한다

			IoDetachDevice ( NextLayerDeviceObject );								
			IoDeleteDevice ( DeviceObject );									
			Irp->IoStatus.Status = STATUS_SUCCESS;
		}
		break;

		case IRP_MN_QUERY_DEVICE_RELATIONS :
		{ 
			{
				PDEVICE_RELATIONS pRelations;
			
				if( pStack->Parameters.QueryDeviceRelations.Type == BusRelations )
				{
					if( deviceExtension->HasChild == TRUE)
					{
						pRelations = (PDEVICE_RELATIONS)ExAllocatePool( 
							PagedPool, 
							sizeof(ULONG) + sizeof(PDEVICE_OBJECT) * (CHILDDEVICECOUNT + 1) );
						pRelations->Count = CHILDDEVICECOUNT;
						pRelations->Objects[0] = deviceExtension->ChildDeviceObject[0];
						pRelations->Objects[1] = 0;

						ObReferenceObject( pRelations->Objects[0] );
					}
					else
					{
						pRelations = (PDEVICE_RELATIONS)ExAllocatePool( 
							PagedPool, 
							sizeof(ULONG) + sizeof(PDEVICE_OBJECT) );
						pRelations->Count = 0;
						pRelations->Objects[0] = 0;
					}
					Irp->IoStatus.Information = (ULONG_PTR)pRelations;				
					Irp->IoStatus.Status = STATUS_SUCCESS;
				}
			}
		}
		break;
	} 

	IoSkipCurrentIrpStackLocation( Irp ); 
	returnStatus = IoCallDriver(NextLayerDeviceObject, Irp);

	return returnStatus;
}

NTSTATUS
QueryDeviceCaps( IN  PIRP   Irp )
{
	
    PIO_STACK_LOCATION      pStack = NULL;
    PDEVICE_CAPABILITIES    deviceCapabilities = NULL;
    
    pStack = IoGetCurrentIrpStackLocation (Irp);
	
    deviceCapabilities = pStack->Parameters.DeviceCapabilities.Capabilities;
	
    if(deviceCapabilities->Version != 1 || 
		deviceCapabilities->Size < sizeof(DEVICE_CAPABILITIES))
    {
		return STATUS_UNSUCCESSFUL; 
    }
    
    deviceCapabilities->DeviceState[PowerSystemWorking] = PowerDeviceD0;
    deviceCapabilities->DeviceState[PowerSystemSleeping1] = PowerDeviceD2;
    deviceCapabilities->DeviceState[PowerSystemSleeping2] = PowerDeviceD2;
    deviceCapabilities->DeviceState[PowerSystemSleeping3] = PowerDeviceD2;
    deviceCapabilities->DeviceState[PowerSystemHibernate] = PowerDeviceD2;
    deviceCapabilities->DeviceState[PowerSystemShutdown] = PowerDeviceD3;
    deviceCapabilities->SystemWake = PowerSystemHibernate;
    deviceCapabilities->DeviceWake = PowerDeviceD2;
    deviceCapabilities->DeviceD1 = TRUE;
    deviceCapabilities->DeviceD2 = TRUE;
    deviceCapabilities->WakeFromD0 = TRUE;
    deviceCapabilities->WakeFromD1 = TRUE;
    deviceCapabilities->WakeFromD2 = TRUE;
    deviceCapabilities->WakeFromD3 = FALSE;
    deviceCapabilities->D1Latency = 0;
    deviceCapabilities->D2Latency = 0;
    deviceCapabilities->D3Latency = 0;
    deviceCapabilities->EjectSupported = FALSE;
    deviceCapabilities->HardwareDisabled = FALSE;
    deviceCapabilities->Removable = TRUE;
	deviceCapabilities->SurpriseRemovalOK = FALSE;
    deviceCapabilities->UniqueID = TRUE;
    deviceCapabilities->SilentInstall = FALSE;
    return STATUS_SUCCESS;
}

NTSTATUS
ChildDriverDevice_PnpDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	KIRQL OldIrql;
	LIST_ENTRY Dequeued0IrpListHead, Dequeued1IrpListHead;
	PLIST_ENTRY pListEntry = NULL;
	PIRP CancelIrp = NULL;

	NTSTATUS returnStatus = Irp->IoStatus.Status;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	switch ( pStack->MinorFunction )
	{
		case IRP_MN_QUERY_CAPABILITIES:
		{
			returnStatus = QueryDeviceCaps( Irp );
			break;
		}

		case IRP_MN_START_DEVICE :
		case IRP_MN_QUERY_STOP_DEVICE:
		case IRP_MN_QUERY_REMOVE_DEVICE:
		{ 
			returnStatus = STATUS_SUCCESS;
			break;
		}

		case IRP_MN_REMOVE_DEVICE :
		{ 
			if( deviceExtension->Removed == TRUE )
			{
				returnStatus = STATUS_SUCCESS;
				break;
			}
			deviceExtension->Removed = TRUE;

			{
				PDEVICE_OBJECT ParentDeviceObject;
				PDEVICE_EXTENSION ParentDeviceExtension;
				ParentDeviceObject = deviceExtension->ParentDeviceObject;
				ParentDeviceExtension = (PDEVICE_EXTENSION)ParentDeviceObject->DeviceExtension;

				if( ParentDeviceExtension->HasChild == TRUE )
				{
					ParentDeviceExtension->HasChild = FALSE;
				}
			}

			InitializeListHead( &Dequeued0IrpListHead );
			InitializeListHead( &Dequeued1IrpListHead );

			IoAcquireCancelSpinLock( &OldIrql );
			KeCancelTimer( &deviceExtension->Pipe0Timer );
			KeCancelTimer( &deviceExtension->Pipe1Timer );

			while( !IsListEmpty( &deviceExtension->Pipe0IrpListHead ) )
			{
				pListEntry = RemoveHeadList(&deviceExtension->Pipe0IrpListHead);
				InsertTailList( &Dequeued0IrpListHead, pListEntry );
			}

			while( !IsListEmpty( &deviceExtension->Pipe1IrpListHead ) )
			{
				pListEntry = RemoveHeadList(&deviceExtension->Pipe1IrpListHead);
				InsertTailList( &Dequeued1IrpListHead, pListEntry );
			}

			IoReleaseCancelSpinLock( OldIrql );

			while( !IsListEmpty( &Dequeued0IrpListHead ) )
			{
				pListEntry = RemoveHeadList(&Dequeued0IrpListHead);
				CancelIrp = CONTAINING_RECORD( pListEntry, IRP, Tail.Overlay.ListEntry );
				CancelIrp->IoStatus.Status = STATUS_CANCELLED;
				CancelIrp->IoStatus.Information = 0;
				IoCompleteRequest( CancelIrp, IO_NO_INCREMENT );
			}

			while( !IsListEmpty( &Dequeued1IrpListHead ) )
			{
				pListEntry = RemoveHeadList(&Dequeued1IrpListHead);
				CancelIrp = CONTAINING_RECORD( pListEntry, IRP, Tail.Overlay.ListEntry );
				CancelIrp->IoStatus.Status = STATUS_CANCELLED;
				CancelIrp->IoStatus.Information = 0;
				IoCompleteRequest( CancelIrp, IO_NO_INCREMENT );
			}

			if( deviceExtension->pDeviceDescriptor )
			{
				ExFreePool( deviceExtension->pDeviceDescriptor );
				deviceExtension->pDeviceDescriptor = NULL;
			}
			if( deviceExtension->pConfigurationDescriptor )
			{
				ExFreePool( deviceExtension->pConfigurationDescriptor );
				deviceExtension->pConfigurationDescriptor = NULL;
			}
			if( deviceExtension->pInterfaceInformation )
			{
				ExFreePool( deviceExtension->pInterfaceInformation );
				deviceExtension->pInterfaceInformation = NULL;
			}
			if( deviceExtension->pLoopBackDataBuffer )
			{
				ExFreePool( deviceExtension->pLoopBackDataBuffer );
				deviceExtension->pLoopBackDataBuffer = NULL;
				deviceExtension->SizeofHolding = 0;
			}			

			IoDeleteDevice ( DeviceObject );
			returnStatus = STATUS_SUCCESS;
			break;
		}

		case IRP_MN_QUERY_BUS_INFORMATION:
		{
			Irp->IoStatus.Information = 0;
			returnStatus = STATUS_NOT_SUPPORTED;
			break;
		}

		case IRP_MN_QUERY_DEVICE_RELATIONS :
		{ 
			PDEVICE_RELATIONS pRelations = NULL;

			if( pStack->Parameters.QueryDeviceRelations.Type == TargetDeviceRelation )
			{
				pRelations = (PDEVICE_RELATIONS)ExAllocatePool( 
					PagedPool, 
					sizeof(ULONG) + sizeof(PDEVICE_OBJECT) * 2 );
				pRelations->Count = 1;
				pRelations->Objects[0] = DeviceObject;
				pRelations->Objects[1] = 0;

				ObReferenceObject( pRelations->Objects[0] );

				Irp->IoStatus.Information = (ULONG_PTR)pRelations;				
				returnStatus = STATUS_SUCCESS;
			}
			break;
		}

	case IRP_MN_QUERY_DEVICE_TEXT:
		{
			PWCHAR string=0;
			PDEVICE_EXTENSION ParentDeviceExtension = NULL;

			ParentDeviceExtension = (PDEVICE_EXTENSION)deviceExtension->ParentDeviceObject->DeviceExtension;
			
			if( pStack->Parameters.QueryDeviceText.DeviceTextType != DeviceTextDescription  )
				break;

			if( ParentDeviceExtension->ChildDeviceObject[0] == DeviceObject )				
			{
				string = (WCHAR *)ExAllocatePool( PagedPool, sizeof(CHILDDEVICETEXT)+2 );
				RtlZeroMemory( string, sizeof(CHILDDEVICETEXT)+2 );
				RtlMoveMemory( string, CHILDDEVICETEXT, sizeof(CHILDDEVICETEXT) );
				returnStatus = STATUS_SUCCESS;
			}
			else
				break;

			Irp->IoStatus.Information = (ULONG_PTR)string;				
			returnStatus = STATUS_SUCCESS;
			break;
		}

		case IRP_MN_QUERY_ID :
		{ 
			WCHAR * pID = NULL;
			PDEVICE_EXTENSION ParentDeviceExtension = NULL;

			ParentDeviceExtension = (PDEVICE_EXTENSION)deviceExtension->ParentDeviceObject->DeviceExtension;

			switch( pStack->Parameters.QueryId.IdType )
			{
				case BusQueryDeviceID:
					if( ParentDeviceExtension->ChildDeviceObject[0] == DeviceObject )
					{
						pID = (WCHAR *)ExAllocatePool( PagedPool, sizeof(CHILDDEVICEDEVICEID)+2 );
						RtlZeroMemory( pID, sizeof(CHILDDEVICEDEVICEID)+2 );
						RtlMoveMemory( pID, CHILDDEVICEDEVICEID, sizeof(CHILDDEVICEDEVICEID) );
						returnStatus = STATUS_SUCCESS;
					}
					break;

				case BusQueryHardwareIDs:
					if( ParentDeviceExtension->ChildDeviceObject[0] == DeviceObject )
					{
						pID = (WCHAR *)ExAllocatePool( PagedPool, sizeof(CHILDDEVICEHARDWAREID)+4 );
						RtlZeroMemory( pID, sizeof(CHILDDEVICEHARDWAREID)+4 );
						RtlMoveMemory( pID, CHILDDEVICEHARDWAREID, sizeof(CHILDDEVICEHARDWAREID) );
						returnStatus = STATUS_SUCCESS;
					}
					break;

				case BusQueryCompatibleIDs:
					if( ParentDeviceExtension->ChildDeviceObject[0] == DeviceObject )
					{
						pID = (WCHAR *)ExAllocatePool( PagedPool, sizeof(CHILDDEVICECOMPATIBLEID)+4 );
						RtlZeroMemory( pID, sizeof(CHILDDEVICECOMPATIBLEID)+4 );
						RtlMoveMemory( pID, CHILDDEVICECOMPATIBLEID, sizeof(CHILDDEVICECOMPATIBLEID) );
						returnStatus = STATUS_SUCCESS;
					}
					break;
				case BusQueryInstanceID:
					if( ParentDeviceExtension->ChildDeviceObject[0] == DeviceObject )
					{
						pID = (WCHAR *)ExAllocatePool( PagedPool, sizeof(CHILDDEVICEINSTANCEID1)+2 );
						RtlZeroMemory( pID, sizeof(CHILDDEVICEINSTANCEID1)+2 );
						RtlMoveMemory( pID, CHILDDEVICEINSTANCEID1, sizeof(CHILDDEVICEINSTANCEID1) );
						returnStatus = STATUS_SUCCESS;
					}
					break;
			}
			Irp->IoStatus.Information = (ULONG_PTR)pID;				
			break;
		}
	} 

	Irp->IoStatus.Status = returnStatus;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );

	return returnStatus;
}

NTSTATUS
BUSDRIVER_PowerDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	if( deviceExtension->DeviceObjectType == BUSDRIVERDEVICE )
	{ // BusDriverDevice
		PoStartNextPowerIrp( Irp );
		IoSkipCurrentIrpStackLocation( Irp );
		return PoCallDriver( deviceExtension->NextLayerDeviceObject, Irp );
	}
	else
	{ // ChildDriverDevice
		PoStartNextPowerIrp( Irp );
		Irp->IoStatus.Status = STATUS_SUCCESS;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return STATUS_SUCCESS;
	}			
}

NTSTATUS
BUSDRIVER_PnpDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	if( deviceExtension->DeviceObjectType == BUSDRIVERDEVICE )
	{ // BusDriverDevice
		return BusDriverDevice_PnpDispatch( DeviceObject, Irp );
	}
	else
	{ // ChildDriverDevice
		return ChildDriverDevice_PnpDispatch( DeviceObject, Irp );
	}			
}

NTSTATUS
BUSDRIVER_DeviceControlDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	ULONG Information=0;
	NTSTATUS returnStatus=STATUS_INVALID_DEVICE_REQUEST;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	if( deviceExtension->DeviceObjectType == BUSDRIVERDEVICE )
	{ // BusDriverDevice
		switch( pStack->Parameters.DeviceIoControl.IoControlCode )
		// 응용프로그램이 전달하는 ControlCode를 확인합니다. 
		// 이곳에서는 파라미터의 유효성을 확인합니다.
		{
			case IOCTL_BUSDRIVER_ADDDEVICE:
				if( deviceExtension->HasChild == FALSE )
				{
					deviceExtension->HasChild = TRUE;
					CreateChildDevice( DeviceObject, Irp );
					IoInvalidateDeviceRelations( deviceExtension->PhysicalDeviceObject, BusRelations );
					returnStatus = STATUS_SUCCESS;
				}
				break;
			case IOCTL_BUSDRIVER_REMOVEDEVICE:
				if( deviceExtension->HasChild == TRUE )
				{
					deviceExtension->HasChild = FALSE;
					IoInvalidateDeviceRelations( deviceExtension->PhysicalDeviceObject, BusRelations );
					returnStatus = STATUS_SUCCESS;
				}
				break;
		}
	}

	Irp->IoStatus.Status = returnStatus;
	Irp->IoStatus.Information = Information;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return returnStatus;
}

VOID                                                                
BUSDRIVER_Unload                                                       
	(                                                               
	IN PDRIVER_OBJECT DriverObject 
	)
{
	UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS                                                            
DriverEntry                                                         
	(                                                               
	IN PDRIVER_OBJECT DriverObject,             
	IN PUNICODE_STRING RegistryPath             
							
	)
{
	NTSTATUS returnStatus = STATUS_SUCCESS;    

	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = BUSDRIVER_Unload;
	DriverObject->DriverExtension->AddDevice = BUSDRIVER_AddDevice;
	DriverObject->MajorFunction[IRP_MJ_PNP] = BUSDRIVER_PnpDispatch;
	DriverObject->MajorFunction[IRP_MJ_POWER] = BUSDRIVER_PowerDispatch;

	// APPIO
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = BUSDRIVER_DeviceControlDispatch;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = BUSDRIVER_CreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP] = BUSDRIVER_CreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = BUSDRIVER_CreateClose;

	// USBIO
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = BUSDRIVER_InternalDeviceControlDispatch;
	return returnStatus;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// USB IO
//
VOID
Pipe0TimerRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{ // BulkIn
	NTSTATUS status;
	KIRQL OldIrql;
	PDEVICE_OBJECT DeviceObject = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	PLIST_ENTRY pListEntry=NULL;
	PIRP Irp=NULL;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	DeviceObject = (PDEVICE_OBJECT)DeferredContext;
	deviceExtension = DeviceObject->DeviceExtension;

	IoAcquireCancelSpinLock( &OldIrql );

	if( !IsListEmpty( &deviceExtension->Pipe0IrpListHead ) )
	{
		pListEntry = RemoveHeadList(&deviceExtension->Pipe0IrpListHead);
		Irp = CONTAINING_RECORD( pListEntry, IRP, Tail.Overlay.ListEntry );
	}
	IoReleaseCancelSpinLock( OldIrql );

	if( !Irp )
		return;

	// Handling
	status = USBIO( DeviceObject, Irp );
	if( status == STATUS_PENDING )
	{
		if( deviceExtension->Removed != TRUE )
		{
			IoAcquireCancelSpinLock( &OldIrql );
			InsertHeadList(&deviceExtension->Pipe0IrpListHead, pListEntry);
			IoReleaseCancelSpinLock( OldIrql );
		}
	}
}

VOID
Pipe1TimerRoutine(
    IN PKDPC Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    )
{ // BulkOut
	KIRQL OldIrql;
	PDEVICE_OBJECT DeviceObject = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	PLIST_ENTRY pListEntry=NULL;
	PIRP Irp=NULL;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	DeviceObject = (PDEVICE_OBJECT)DeferredContext;
	deviceExtension = DeviceObject->DeviceExtension;

	IoAcquireCancelSpinLock( &OldIrql );

	if( !IsListEmpty( &deviceExtension->Pipe1IrpListHead ) )
	{
		pListEntry = RemoveHeadList(&deviceExtension->Pipe1IrpListHead);
		Irp = CONTAINING_RECORD( pListEntry, IRP, Tail.Overlay.ListEntry );
	}
	IoReleaseCancelSpinLock( OldIrql );

	if( !Irp )
		return;

	// Handling
	USBIO( DeviceObject, Irp );
}

NTSTATUS USBComplete( PIRP Irp, PURB pUrb, NTSTATUS IrpStatus, NTSTATUS UsbStatus )
{
	IoSetCancelRoutine( Irp, NULL );
	if( pUrb )
		pUrb->UrbHeader.Status = UsbStatus;
	Irp->IoStatus.Status = IrpStatus;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return IrpStatus;
}

NTSTATUS
checkURB
	(
	PURB pUrb
	)
{
	if( !pUrb )
		return STATUS_INVALID_PARAMETER;

	if( !pUrb->UrbHeader.Length )
		return STATUS_INVALID_PARAMETER;

	switch( pUrb->UrbHeader.Function )
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
	case URB_FUNCTION_ABORT_PIPE:
	case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
	case URB_FUNCTION_RESET_PIPE:
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
	case URB_FUNCTION_GET_CONFIGURATION:
		break;
	default:
		return STATUS_NOT_SUPPORTED;
	}

	return STATUS_SUCCESS;
}

NTSTATUS 
USBIO_Select_Configuration( 
	PDEVICE_OBJECT DeviceObject, 
	PIRP Irp, 
	PURB pUrb 
	)
{
	PDEVICE_EXTENSION deviceExtension = NULL;
	deviceExtension = DeviceObject->DeviceExtension;

	if( pUrb->UrbSelectConfiguration.Hdr.Length != (sizeof(struct _URB_SELECT_CONFIGURATION) + sizeof( USBD_PIPE_INFORMATION) ) )
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( !pUrb->UrbSelectConfiguration.ConfigurationDescriptor )
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbSelectConfiguration.ConfigurationDescriptor->wTotalLength != deviceExtension->pConfigurationDescriptor->wTotalLength )
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	deviceExtension->pInterfaceInformation->Pipes[0].MaximumTransferSize = pUrb->UrbSelectConfiguration.Interface.Pipes[0].MaximumTransferSize;
	deviceExtension->pInterfaceInformation->Pipes[1].MaximumTransferSize = pUrb->UrbSelectConfiguration.Interface.Pipes[1].MaximumTransferSize;

	pUrb->UrbSelectConfiguration.ConfigurationHandle = (PVOID)deviceExtension->ConfigurationHandle;
	RtlMoveMemory( &(pUrb->UrbSelectConfiguration.Interface), deviceExtension->pInterfaceInformation, sizeof( USBD_INTERFACE_INFORMATION ) + sizeof( USBD_PIPE_INFORMATION) );

	return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
}

NTSTATUS 
USBIO_ResetPipe( 
	PDEVICE_OBJECT DeviceObject, 
	PIRP Irp, 
	PURB pUrb 
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
}

NTSTATUS 
USBIO_AbortPipe( 
	PDEVICE_OBJECT DeviceObject, 
	PIRP Irp, 
	PURB pUrb 
	)
{
	PDEVICE_EXTENSION deviceExtension = NULL;
	LIST_ENTRY DequeuedIrpListHead;
	KIRQL OldIrql;
	PLIST_ENTRY pListEntry = NULL;
	PIRP CancelIrp = NULL;

	deviceExtension = DeviceObject->DeviceExtension;

	InitializeListHead( &DequeuedIrpListHead );

	if( pUrb->UrbPipeRequest.PipeHandle == (PVOID)PIPE0HANDLE )
	{
		IoAcquireCancelSpinLock( &OldIrql );

		while( !IsListEmpty( &deviceExtension->Pipe0IrpListHead ) )
		{
			pListEntry = RemoveHeadList(&deviceExtension->Pipe0IrpListHead);
			InsertTailList( &DequeuedIrpListHead, pListEntry );
		}

		IoReleaseCancelSpinLock( OldIrql );
	}
	else
	if( pUrb->UrbPipeRequest.PipeHandle == (PVOID)PIPE1HANDLE )
	{
		IoAcquireCancelSpinLock( &OldIrql );

		while( !IsListEmpty( &deviceExtension->Pipe1IrpListHead ) )
		{
			pListEntry = RemoveHeadList(&deviceExtension->Pipe1IrpListHead);
			InsertTailList( &DequeuedIrpListHead, pListEntry );
		}

		IoReleaseCancelSpinLock( OldIrql );
	}
	else
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	while( !IsListEmpty( &DequeuedIrpListHead ) )
	{
		pListEntry = RemoveHeadList(&DequeuedIrpListHead);
		CancelIrp = CONTAINING_RECORD( pListEntry, IRP, Tail.Overlay.ListEntry );
		CancelIrp->IoStatus.Status = STATUS_CANCELLED;
		CancelIrp->IoStatus.Information = 0;
		IoCompleteRequest( CancelIrp, IO_NO_INCREMENT );
	}

	return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
}

NTSTATUS 
USBIO_BulkOrInterruptTransfer( 
	PDEVICE_OBJECT DeviceObject, 
	PIRP Irp, 
	PURB pUrb 
	)
{
	PVOID TargetBuffer = NULL;
	ULONG TargetBufferRequestSize;
	PDEVICE_EXTENSION deviceExtension = NULL;
	deviceExtension = DeviceObject->DeviceExtension;

	if( !pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength )
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( !pUrb->UrbBulkOrInterruptTransfer.TransferBuffer && !pUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL )
	{
		pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbBulkOrInterruptTransfer.TransferBuffer && pUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL )
	{
		pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbBulkOrInterruptTransfer.TransferBuffer )
	{
		TargetBuffer = pUrb->UrbBulkOrInterruptTransfer.TransferBuffer;
	}
	else if( pUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL )
	{
		TargetBuffer = MmGetSystemAddressForMdlSafe( pUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL, HighPagePriority);
	}
	
	TargetBufferRequestSize = pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
	
	if( pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (PVOID)PIPE0HANDLE )
	{ // Bulk IN
		TargetBufferRequestSize = (TargetBufferRequestSize >( deviceExtension->SizeofHolding ))?
			deviceExtension->SizeofHolding : TargetBufferRequestSize;

		if( !TargetBufferRequestSize )
		{
			IoMarkIrpPending( Irp );
			return STATUS_PENDING;
		}

		RtlMoveMemory( TargetBuffer, &deviceExtension->pLoopBackDataBuffer[0], TargetBufferRequestSize );
		deviceExtension->SizeofHolding -= TargetBufferRequestSize;

		RtlMoveMemory( &deviceExtension->pLoopBackDataBuffer[0], &deviceExtension->pLoopBackDataBuffer[TargetBufferRequestSize], deviceExtension->SizeofHolding );

		pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = TargetBufferRequestSize;
		return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
	}
	else
	if( pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (PVOID)PIPE1HANDLE )
	{ // Bulk OUT
		TargetBufferRequestSize = (TargetBufferRequestSize >( MAXIMUMLOOPBACKDATASIZE- deviceExtension->SizeofHolding ))?
			MAXIMUMLOOPBACKDATASIZE- deviceExtension->SizeofHolding : TargetBufferRequestSize;

		if( !TargetBufferRequestSize )
		{
			pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
			return USBComplete( Irp, pUrb, STATUS_INSUFFICIENT_RESOURCES, USBD_STATUS_BUFFER_OVERRUN );
		}

		RtlMoveMemory( &deviceExtension->pLoopBackDataBuffer[deviceExtension->SizeofHolding], TargetBuffer, TargetBufferRequestSize );
		pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = TargetBufferRequestSize;
		deviceExtension->SizeofHolding += TargetBufferRequestSize;
		return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
	}
	else
	{
		pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}
}

NTSTATUS 
USBIO_GetCurrentFrameNumber( 
	PDEVICE_OBJECT DeviceObject, 
	PIRP Irp, 
	PURB pUrb 
	)
{
	ULONGLONG dwwTime;

	UNREFERENCED_PARAMETER(DeviceObject);

	dwwTime = KeQueryInterruptTime();
	dwwTime = dwwTime / 10000;
	pUrb->UrbGetCurrentFrameNumber.FrameNumber = (ULONG)dwwTime;

	return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
}

NTSTATUS 
USBIO_GetDescriptorFromDevice( 
	PDEVICE_OBJECT DeviceObject, 
	PIRP Irp, 
	PURB pUrb 
	)
{
	PVOID TargetBuffer = NULL;
	ULONG TargetBufferRequestSize;
	PDEVICE_EXTENSION deviceExtension = NULL;
	deviceExtension = DeviceObject->DeviceExtension;

	if( !pUrb->UrbControlDescriptorRequest.TransferBufferLength )
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( !pUrb->UrbControlDescriptorRequest.TransferBuffer && !pUrb->UrbControlDescriptorRequest.TransferBufferMDL )
	{
		pUrb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbControlDescriptorRequest.TransferBuffer && pUrb->UrbControlDescriptorRequest.TransferBufferMDL )
	{
		pUrb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbControlDescriptorRequest.TransferBuffer )
	{
		TargetBuffer = pUrb->UrbControlDescriptorRequest.TransferBuffer;
	}
	else if( pUrb->UrbControlDescriptorRequest.TransferBufferMDL )
	{
		TargetBuffer = MmGetSystemAddressForMdlSafe(pUrb->UrbControlDescriptorRequest.TransferBufferMDL, HighPagePriority);
	}
	
	TargetBufferRequestSize = pUrb->UrbControlDescriptorRequest.TransferBufferLength;

	if( pUrb->UrbControlDescriptorRequest.DescriptorType == USB_DEVICE_DESCRIPTOR_TYPE )
	{
		TargetBufferRequestSize = ( TargetBufferRequestSize > deviceExtension->pDeviceDescriptor->bLength )? 
			deviceExtension->pDeviceDescriptor->bLength : TargetBufferRequestSize;

		RtlMoveMemory( TargetBuffer, deviceExtension->pDeviceDescriptor, TargetBufferRequestSize );

		pUrb->UrbControlDescriptorRequest.TransferBufferLength = TargetBufferRequestSize;
		return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
	}
	else if( pUrb->UrbControlDescriptorRequest.DescriptorType == USB_CONFIGURATION_DESCRIPTOR_TYPE )
	{
		if( pUrb->UrbControlDescriptorRequest.Index != 0 )
		{
			pUrb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
		}
		TargetBufferRequestSize = ( TargetBufferRequestSize > deviceExtension->pConfigurationDescriptor->wTotalLength )? 
			deviceExtension->pConfigurationDescriptor->wTotalLength : TargetBufferRequestSize;

		RtlMoveMemory( TargetBuffer, deviceExtension->pConfigurationDescriptor, TargetBufferRequestSize );

		pUrb->UrbControlDescriptorRequest.TransferBufferLength = TargetBufferRequestSize;
		return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
	}
	else
	{
		pUrb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}
}

NTSTATUS 
USBIO_GetConfiguration( 
	PDEVICE_OBJECT DeviceObject, 
	PIRP Irp, 
	PURB pUrb 
	)
{
	PDEVICE_EXTENSION deviceExtension = NULL;
	PVOID TargetBuffer = NULL;
	ULONG TargetBufferRequestSize;
	deviceExtension = DeviceObject->DeviceExtension;

	if( pUrb->UrbControlGetConfigurationRequest.TransferBufferLength != 1 )
	{
		pUrb->UrbControlGetConfigurationRequest.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( !pUrb->UrbControlGetConfigurationRequest.TransferBuffer && !pUrb->UrbControlGetConfigurationRequest.TransferBufferMDL )
	{
		pUrb->UrbControlGetConfigurationRequest.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbControlGetConfigurationRequest.TransferBuffer && pUrb->UrbControlGetConfigurationRequest.TransferBufferMDL )
	{
		pUrb->UrbControlGetConfigurationRequest.TransferBufferLength = 0;
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbControlGetConfigurationRequest.TransferBuffer )
	{
		TargetBuffer = pUrb->UrbControlGetConfigurationRequest.TransferBuffer;
	}
	else if( pUrb->UrbControlGetConfigurationRequest.TransferBufferMDL )
	{
		TargetBuffer = MmGetSystemAddressForMdlSafe(pUrb->UrbControlGetConfigurationRequest.TransferBufferMDL, HighPagePriority);
	}
	
	TargetBufferRequestSize = pUrb->UrbControlGetConfigurationRequest.TransferBufferLength;

	*(unsigned char *)TargetBuffer = deviceExtension->CurrentConfigValue;

	return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
}

NTSTATUS
USBIO
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	PURB pUrb = NULL;

	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	pUrb = (PURB)pStack->Parameters.Others.Argument1;
	
	switch( pUrb->UrbHeader.Function )
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		return USBIO_Select_Configuration( DeviceObject, Irp, pUrb );
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		return USBIO_BulkOrInterruptTransfer( DeviceObject, Irp, pUrb );
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		return USBIO_GetDescriptorFromDevice( DeviceObject, Irp, pUrb );
	case URB_FUNCTION_GET_CONFIGURATION:
		return USBIO_GetConfiguration( DeviceObject, Irp, pUrb );
	case URB_FUNCTION_RESET_PIPE:
		return USBIO_ResetPipe( DeviceObject, Irp, pUrb );
	}	
	return USBComplete( Irp, pUrb, STATUS_NOT_SUPPORTED, USBD_STATUS_INVALID_PARAMETER );
}

void
BUSDRIVER_WorkItemRoutine
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PVOID Context
	)
{
	PIRP Irp = NULL;

	PWORKITEM_CONTEXT pWorkItemContext = (PWORKITEM_CONTEXT)Context;

	Irp = pWorkItemContext->Irp;

	USBIO( DeviceObject, Irp );

	ExFreePool( pWorkItemContext->pIoWorkItem );
	ExFreePool( pWorkItemContext );
}

void
Pipe0CancelRoutine
	(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	RemoveEntryList( &Irp->Tail.Overlay.ListEntry );

	IoReleaseCancelSpinLock( Irp->CancelIrql );
	USBComplete( Irp, 0, STATUS_CANCELLED, 0 );
}

void
Pipe1CancelRoutine
	(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
	)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	RemoveEntryList( &Irp->Tail.Overlay.ListEntry );

	IoReleaseCancelSpinLock( Irp->CancelIrql );
	USBComplete( Irp, 0, STATUS_CANCELLED, 0 );
}

NTSTATUS
BUSDRIVER_InternalDeviceControlDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack = NULL;
	PDEVICE_EXTENSION deviceExtension = NULL;
	PURB pUrb = NULL;
	NTSTATUS status;
	PWORKITEM_CONTEXT pWorkItemContext = NULL;
	KIRQL OldIrql;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	if( deviceExtension->DeviceObjectType == BUSDRIVERDEVICE )
	{ // BusDriverDevice
		return USBComplete( Irp, 0, STATUS_INVALID_DEVICE_REQUEST, 0 );
	}
	else
	{ // ChildDriverDevice
		if( deviceExtension->Removed == TRUE )
			return USBComplete( Irp, 0, STATUS_DEVICE_REMOVED, 0 );

		switch( pStack->Parameters.DeviceIoControl.IoControlCode )
		{
			case IOCTL_INTERNAL_USB_SUBMIT_URB:
				// 현 문맥에서 처리할 수 있는 명령어를 구분한다
				pUrb = (PURB)pStack->Parameters.Others.Argument1;

				status = checkURB( pUrb );
				if( !NT_SUCCESS( status ) )
				{
					return USBComplete( Irp, pUrb, status, USBD_STATUS_INVALID_PARAMETER );
				}

				switch( pUrb->UrbHeader.Function )
				{
				case URB_FUNCTION_ABORT_PIPE:
					return USBIO_AbortPipe( DeviceObject, Irp, pUrb );
				case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
					return USBIO_GetCurrentFrameNumber( DeviceObject, Irp, pUrb );
				case URB_FUNCTION_RESET_PIPE:
				case URB_FUNCTION_SELECT_CONFIGURATION:
				case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
				case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
				case URB_FUNCTION_GET_CONFIGURATION:
					break;
				default:
					return USBComplete( Irp, pUrb, STATUS_NOT_SUPPORTED, USBD_STATUS_INVALID_PARAMETER );
				}	
				
				IoMarkIrpPending( Irp );

				// Timer를 이용해야 하는 요청을 구분한다
				if( pUrb->UrbHeader.Function == URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER )
				{
					IoAcquireCancelSpinLock( &OldIrql );

					if( pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (PVOID)PIPE0HANDLE )
					{
						InsertTailList( &deviceExtension->Pipe0IrpListHead, &Irp->Tail.Overlay.ListEntry );
						IoSetCancelRoutine( Irp, Pipe0CancelRoutine );
						IoReleaseCancelSpinLock( OldIrql );
					}
					else
					if( pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (PVOID)PIPE1HANDLE )
					{
						IoSetCancelRoutine( Irp, Pipe1CancelRoutine );
						InsertTailList( &deviceExtension->Pipe1IrpListHead, &Irp->Tail.Overlay.ListEntry );
						IoReleaseCancelSpinLock( OldIrql );
					}
					else
					{
						IoReleaseCancelSpinLock( OldIrql );
						return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
					}
				}
				else
				{ // 나머지는 WorkItem을 이용한다
					pWorkItemContext = (PWORKITEM_CONTEXT)ExAllocatePool( NonPagedPool, sizeof(WORKITEM_CONTEXT) );
					pWorkItemContext->Irp = Irp;
					pWorkItemContext->pIoWorkItem = IoAllocateWorkItem( DeviceObject );
					IoQueueWorkItem( pWorkItemContext->pIoWorkItem, BUSDRIVER_WorkItemRoutine, CriticalWorkQueue, pWorkItemContext );
				}

				return STATUS_PENDING;
		}
		return USBComplete( Irp, 0, STATUS_NOT_SUPPORTED, 0 );
	}			
}
