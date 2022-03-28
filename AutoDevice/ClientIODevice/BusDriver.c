#include <ntddk.h>
#include <usb100.h>
#include <usb200.h>
#include <usbdi.h>
#include ".\..\common\common.h"

#define CHILDDEVICECOUNT 1
#define BUSDRIVERDEVICE 0
#define CHILDDRIVERDEVICE 1

#define CHILDDEVICEDEVICEID L"USB\\VID_1234&PID_5679"
#define CHILDDEVICEHARDWAREID L"USB\\VID_1234&PID_5679"
#define CHILDDEVICECOMPATIBLEID L"USB\\Composite"
#define CHILDDEVICEINSTANCEID1 L"0001"
#define CHILDDEVICECONTAINERID L"{ff0e874d-cd0b-11e6-9bd7-14dae9b57ca9}"
#define CHILDDEVICETEXT  L"USB Device"

#define CONFIGHANDLE 0xAAAAAAAA
#define INTERFACEHANDLE0 0x55555555
#define INTERFACEHANDLE1 0x55555556
#define PIPE0HANDLE 0x11111111
#define PIPE1HANDLE 0x22222222

#include "devioctl.h"
#include <initguid.h>

#define NOT_CONFIGURED 0
#define MAXIMUMLOOPBACKDATASIZE 0x1000

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
	char CurrentConfigValue;

	ULONG ConfigurationHandle;
	PUSBD_INTERFACE_INFORMATION pInterfaceInformation;

	KSPIN_LOCK	KeyboardLoopbackSpinLock;
	unsigned long SizeofHoldingKeyboard;
	unsigned char *pLoopBackDataBufferKeyboard;
	unsigned char *pLoopBackDataBufferKeyboardTemp;

	KSPIN_LOCK	MouseLoopbackSpinLock;
	unsigned long SizeofHoldingMouse;
	unsigned char *pLoopBackDataBufferMouse;
	unsigned char *pLoopBackDataBufferMouseTemp;

	KEVENT	* pKLEDUserEvent;
	unsigned char LED_Data;

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

typedef struct _QUEUE_ENTRY
{
	LIST_ENTRY	Entry;
	PIRP		Irp;
}QUEUE_ENTRY, *PQUEUE_ENTRY;

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

#define KEYBOARD_TYPE	(0)
#define MOUSE_TYPE		(1)

//#define MAXIMUMLOOPBACKDATASIZE 0x1000
/*
KSPIN_LOCK	KeyboardLoopbackSpinLock;
unsigned long SizeofHoldingKeyboard;
unsigned char *pLoopBackDataBufferKeyboard;
unsigned char *pLoopBackDataBufferKeyboardTemp;
*/

int GetLoopBackDataSize(PDEVICE_EXTENSION deviceExtension, int Type)
{
	KIRQL OldIrql;
	int Size = 0;

	if (Type == KEYBOARD_TYPE)
		KeAcquireSpinLock(&deviceExtension->KeyboardLoopbackSpinLock, &OldIrql);
	else if (Type == MOUSE_TYPE)
		KeAcquireSpinLock(&deviceExtension->MouseLoopbackSpinLock, &OldIrql);
	else
		return Size;

	if (Type == KEYBOARD_TYPE)
		Size = deviceExtension->SizeofHoldingKeyboard;
	else if (Type == MOUSE_TYPE)
		Size = deviceExtension->SizeofHoldingMouse;

	if (Type == KEYBOARD_TYPE)
		KeReleaseSpinLock(&deviceExtension->KeyboardLoopbackSpinLock, OldIrql);
	else if (Type == MOUSE_TYPE)
		KeReleaseSpinLock(&deviceExtension->MouseLoopbackSpinLock, OldIrql);

	return Size;
}

int InsertLoopBackData(PDEVICE_EXTENSION deviceExtension, int Type, unsigned char * pData, int Size)
{
	KIRQL OldIrql;
	int RemainSize = 0;
	int MaxSize = 0;
	int UsefulSize = 0;
	int Pos = 0;

	if( Size == 0 )
		return Size;

	if (Type == KEYBOARD_TYPE)
		KeAcquireSpinLock(&deviceExtension->KeyboardLoopbackSpinLock, &OldIrql);
	else if (Type == MOUSE_TYPE)
		KeAcquireSpinLock(&deviceExtension->MouseLoopbackSpinLock, &OldIrql);
	else
		return Size;

	MaxSize = MAXIMUMLOOPBACKDATASIZE;

	if (Type == KEYBOARD_TYPE)
		RemainSize = deviceExtension->SizeofHoldingKeyboard;
	else if (Type == MOUSE_TYPE)
		RemainSize = deviceExtension->SizeofHoldingMouse;

	UsefulSize = MaxSize - RemainSize;

	Size = (Size > UsefulSize) ? UsefulSize : Size;
	if (Size == 0)
		goto exit;

	Pos = RemainSize;

	if (Type == KEYBOARD_TYPE)
		memcpy(deviceExtension->pLoopBackDataBufferKeyboard + Pos, pData, Size);
	else if (Type == MOUSE_TYPE)
		memcpy(deviceExtension->pLoopBackDataBufferMouse + Pos, pData, Size);

	if (Type == KEYBOARD_TYPE)
		deviceExtension->SizeofHoldingKeyboard += Size;
	else if (Type == MOUSE_TYPE)
		deviceExtension->SizeofHoldingMouse += Size;

exit:
	if (Type == KEYBOARD_TYPE)
		KeReleaseSpinLock(&deviceExtension->KeyboardLoopbackSpinLock, OldIrql);
	else if (Type == MOUSE_TYPE)
		KeReleaseSpinLock(&deviceExtension->MouseLoopbackSpinLock, OldIrql);

	return Size;
}

int RemoveLoopBackData(PDEVICE_EXTENSION deviceExtension, int Type, unsigned char * pData, int Size)
{
	KIRQL OldIrql;
	int RemainSize = 0;
	int MaxSize = 0;
	int UsefulSize = 0;

	if (Size == 0)
		return Size;

	if (Type == KEYBOARD_TYPE)
		KeAcquireSpinLock(&deviceExtension->KeyboardLoopbackSpinLock, &OldIrql);
	else if (Type == MOUSE_TYPE)
		KeAcquireSpinLock(&deviceExtension->MouseLoopbackSpinLock, &OldIrql);
	else
		return Size;

	MaxSize = MAXIMUMLOOPBACKDATASIZE;

	if (Type == KEYBOARD_TYPE)
		RemainSize = deviceExtension->SizeofHoldingKeyboard;
	else if (Type == MOUSE_TYPE)
		RemainSize = deviceExtension->SizeofHoldingMouse;

	UsefulSize = RemainSize;

	Size = (Size > UsefulSize) ? UsefulSize : Size;
	if (Size == 0)
		goto exit;

	if (Type == KEYBOARD_TYPE)
		memcpy(deviceExtension->pLoopBackDataBufferKeyboardTemp, deviceExtension->pLoopBackDataBufferKeyboard, MaxSize);
	else if (Type == MOUSE_TYPE)
		memcpy(deviceExtension->pLoopBackDataBufferMouseTemp, deviceExtension->pLoopBackDataBufferMouse, MaxSize);

	if (Type == KEYBOARD_TYPE)
		memcpy(pData, deviceExtension->pLoopBackDataBufferKeyboard, Size);
	else if (Type == MOUSE_TYPE)
		memcpy(pData, deviceExtension->pLoopBackDataBufferMouse, Size);

	RemainSize -= Size;

	if (RemainSize)
	{
		if (Type == KEYBOARD_TYPE)
			memcpy(deviceExtension->pLoopBackDataBufferKeyboard, deviceExtension->pLoopBackDataBufferKeyboardTemp + Size, RemainSize);
		else if (Type == MOUSE_TYPE)
			memcpy(deviceExtension->pLoopBackDataBufferMouse, deviceExtension->pLoopBackDataBufferMouseTemp + Size, RemainSize);
	}

	if (Type == KEYBOARD_TYPE)
		deviceExtension->SizeofHoldingKeyboard -= Size;
	else if (Type == MOUSE_TYPE)
		deviceExtension->SizeofHoldingMouse -= Size;

exit:
	if (Type == KEYBOARD_TYPE)
		KeReleaseSpinLock(&deviceExtension->KeyboardLoopbackSpinLock, OldIrql);
	else if (Type == MOUSE_TYPE)
		KeReleaseSpinLock(&deviceExtension->MouseLoopbackSpinLock, OldIrql);

	return Size;
}


NTSTATUS
BUSDRIVER_AddDevice
	(
	IN PDRIVER_OBJECT DriverObject,         
	IN PDEVICE_OBJECT PhysicalDeviceObject  
	)
{
	NTSTATUS returnStatus = STATUS_SUCCESS;         
	PDEVICE_OBJECT DeviceObject = NULL;             
	PDEVICE_EXTENSION deviceExtension;

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

	IoRegisterDeviceInterface( PhysicalDeviceObject, &GUID_VUSBBUS, NULL, &deviceExtension->UnicodeString );

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
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	PKEVENT pEvent;
	pEvent = (PKEVENT)Context;
	KeSetEvent( pEvent, 0, FALSE );
	return STATUS_MORE_PROCESSING_REQUIRED;
}

unsigned char KbdReportDescriptor[0x36] = {
	// Kbd
	0x05, 0x01,
	0x09, 0x06,
	0xA1, 0x01,
	0x05, 0x08,
	0x19, 0x01,
	0x29, 0x03,
	0x15, 0x00,
	0x25, 0x01,
	0x75, 0x01,
	0x95, 0x03,
	0x91, 0x02,
	0x95, 0x05,
	0x91, 0x01,
	0x05, 0x07,
	0x19, 0xE0,
	0x29, 0xE7,
	0x95, 0x08,
	0x81, 0x02, // input modifier
	0x75, 0x08,
	0x95, 0x01,
	0x81, 0x01, // input reserved
	0x19, 0x00,
	0x29, 0x91,
	0x26, 0xFF, 0x00,
	0x95, 0x06,
	0x81, 0x00, // key[6]
	0xC0
};

unsigned char MouseReportDescriptor[0x3E] = {
	// Mouse
	0x05, 0x01,
	0x09, 0x02,
	0xA1, 0x01,
	0x05, 0x09,
	0x19, 0x01,
	0x29, 0x03,
	0x15, 0x00,
	0x25, 0x01,
	0x95, 0x03,
	0x75, 0x01,
	0x81, 0x02,
	0x95, 0x01,
	0x75, 0x05,
	0x81, 0x03,
	0x05, 0x01,
	0x09, 0x01,
	0xA1, 0x00,
	0x09, 0x30,
	0x09, 0x31,
	0x15, 0x81,
	0x25, 0x7F,
	0x75, 0x08,
	0x95, 0x02,
	0x81, 0x06,
	0xC0,
	0x09, 0x38,
	0x15, 0x81,
	0x25, 0x7F,
	0x75, 0x08,
	0x95, 0x01,
	0x81, 0x06,
	0xC0
};

#pragma pack(1)
typedef struct _USB_HID_DESCRIPTOR {
	UCHAR   bLength;
	UCHAR   bDescriptorType;
	USHORT  bcdHID;
	UCHAR   bCountryCode;
	UCHAR   bNumDescriptors;
	UCHAR   bDescriptorTypeReport;
	USHORT  wDescriptorLength;
} USB_HID_DESCRIPTOR, *PUSB_HID_DESCRIPTOR;
#pragma pack()
void
CreateChildDevice
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	PDEVICE_OBJECT NextLayerDeviceObject;
	//NTSTATUS returnStatus;
	unsigned short wTotalLength;
	PUSB_CONFIGURATION_DESCRIPTOR pConfigurationDescriptor;
	PUSB_INTERFACE_DESCRIPTOR pInterfaceDescriptor;
	PUSB_HID_DESCRIPTOR pHidDescriptor;
	PUSB_ENDPOINT_DESCRIPTOR pEndpointDescriptor;
	PUSBD_INTERFACE_INFORMATION pInterfaceInformation;
	PDEVICE_EXTENSION childDeviceExtension;

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
		FILE_DEVICE_SECURE_OPEN,
		FALSE,                                      
		&deviceExtension->ChildDeviceObject[0]       
	);

	deviceExtension->ChildDeviceObject[0]->Characteristics |= FILE_REMOVABLE_MEDIA;
	deviceExtension->ChildDeviceObject[0]->StackSize = 10;
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

	wTotalLength = sizeof(USB_CONFIGURATION_DESCRIPTOR) + 
		sizeof(USB_INTERFACE_DESCRIPTOR) +
		sizeof(USB_HID_DESCRIPTOR) +
		sizeof(USB_ENDPOINT_DESCRIPTOR) +
		sizeof(USB_INTERFACE_DESCRIPTOR) +
		sizeof(USB_HID_DESCRIPTOR) +
		sizeof(USB_ENDPOINT_DESCRIPTOR);

	childDeviceExtension->pConfigurationDescriptor = (PUSB_CONFIGURATION_DESCRIPTOR)ExAllocatePool( 
		NonPagedPool, 
		wTotalLength );

	childDeviceExtension->CurrentConfigValue = NOT_CONFIGURED;

	// Descriptor Setup

	// DeviceDescriptor
	childDeviceExtension->pDeviceDescriptor->bLength = sizeof(USB_DEVICE_DESCRIPTOR);
	childDeviceExtension->pDeviceDescriptor->bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
	childDeviceExtension->pDeviceDescriptor->bcdUSB = 0x0110;
	childDeviceExtension->pDeviceDescriptor->bDeviceClass = 00;
	childDeviceExtension->pDeviceDescriptor->bDeviceSubClass = 00;
	childDeviceExtension->pDeviceDescriptor->bDeviceProtocol = 00;
	childDeviceExtension->pDeviceDescriptor->bMaxPacketSize0 = 0x8; 
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
	pConfigurationDescriptor->bNumInterfaces = 2;
	pConfigurationDescriptor->bConfigurationValue = 1;
	pConfigurationDescriptor->iConfiguration = 0;
	pConfigurationDescriptor->bmAttributes = 0xA0;
	pConfigurationDescriptor->MaxPower = 0x32;

	// InterfaceDescriptor
	pInterfaceDescriptor = (PUSB_INTERFACE_DESCRIPTOR)(pConfigurationDescriptor+1);
	pInterfaceDescriptor->bLength = sizeof(USB_INTERFACE_DESCRIPTOR);
	pInterfaceDescriptor->bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE;
	pInterfaceDescriptor->bInterfaceNumber = 0;
	pInterfaceDescriptor->bAlternateSetting = 0;
	pInterfaceDescriptor->bNumEndpoints = 1;
	pInterfaceDescriptor->bInterfaceClass = 3;
	pInterfaceDescriptor->bInterfaceSubClass = 0;
	pInterfaceDescriptor->bInterfaceProtocol = 1; // Keyboard
	pInterfaceDescriptor->iInterface = 0;

	// HidDescriptor
	pHidDescriptor = (PUSB_HID_DESCRIPTOR)(pInterfaceDescriptor + 1);
	pHidDescriptor->bLength = sizeof(USB_HID_DESCRIPTOR);
	pHidDescriptor->bDescriptorType = 0x21;
	pHidDescriptor->bcdHID = 0x0110;
	pHidDescriptor->bCountryCode = 0;
	pHidDescriptor->bNumDescriptors = 1;
	pHidDescriptor->bDescriptorTypeReport = 0x22;
	pHidDescriptor->wDescriptorLength = sizeof(KbdReportDescriptor);

	// EndpointDescriptor
	pEndpointDescriptor = (PUSB_ENDPOINT_DESCRIPTOR)(pHidDescriptor+1);
	pEndpointDescriptor->bLength = sizeof(USB_ENDPOINT_DESCRIPTOR);
	pEndpointDescriptor->bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
	pEndpointDescriptor->bEndpointAddress = 0x81;
	pEndpointDescriptor->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;
	pEndpointDescriptor->wMaxPacketSize = 8;
	pEndpointDescriptor->bInterval = 0x0a;

	// mouse
	// InterfaceDescriptor
	pInterfaceDescriptor = (PUSB_INTERFACE_DESCRIPTOR)(pEndpointDescriptor + 1);
	pInterfaceDescriptor->bLength = sizeof(USB_INTERFACE_DESCRIPTOR);
	pInterfaceDescriptor->bDescriptorType = USB_INTERFACE_DESCRIPTOR_TYPE;
	pInterfaceDescriptor->bInterfaceNumber = 1;
	pInterfaceDescriptor->bAlternateSetting = 0;
	pInterfaceDescriptor->bNumEndpoints = 1;
	pInterfaceDescriptor->bInterfaceClass = 3;
	pInterfaceDescriptor->bInterfaceSubClass = 0;
	pInterfaceDescriptor->bInterfaceProtocol = 2; // Mouse
	pInterfaceDescriptor->iInterface = 0;

	// HidDescriptor
	pHidDescriptor = (PUSB_HID_DESCRIPTOR)(pInterfaceDescriptor + 1);
	pHidDescriptor->bLength = sizeof(USB_HID_DESCRIPTOR);
	pHidDescriptor->bDescriptorType = 0x21;
	pHidDescriptor->bcdHID = 0x0110;
	pHidDescriptor->bCountryCode = 0;
	pHidDescriptor->bNumDescriptors = 1;
	pHidDescriptor->bDescriptorTypeReport = 0x22;
	pHidDescriptor->wDescriptorLength = sizeof(MouseReportDescriptor);

	// EndpointDescriptor
	pEndpointDescriptor = (PUSB_ENDPOINT_DESCRIPTOR)(pHidDescriptor + 1);
	pEndpointDescriptor->bLength = sizeof(USB_ENDPOINT_DESCRIPTOR);
	pEndpointDescriptor->bDescriptorType = USB_ENDPOINT_DESCRIPTOR_TYPE;
	pEndpointDescriptor->bEndpointAddress = 0x82;
	pEndpointDescriptor->bmAttributes = USB_ENDPOINT_TYPE_INTERRUPT;
	pEndpointDescriptor->wMaxPacketSize = 4;
	pEndpointDescriptor->bInterval = 0x0a;

	// USB Pipe준비
	childDeviceExtension->ConfigurationHandle = CONFIGHANDLE;
	childDeviceExtension->pInterfaceInformation = (PUSBD_INTERFACE_INFORMATION)ExAllocatePool(
		NonPagedPool,
		sizeof(USBD_INTERFACE_INFORMATION) + sizeof(USBD_INTERFACE_INFORMATION));

	pInterfaceInformation = childDeviceExtension->pInterfaceInformation;
	RtlZeroMemory( pInterfaceInformation, sizeof(USBD_INTERFACE_INFORMATION) + sizeof(USBD_INTERFACE_INFORMATION));

	pInterfaceInformation->Length = sizeof(USBD_INTERFACE_INFORMATION);
	pInterfaceInformation->InterfaceNumber = 0;
	pInterfaceInformation->AlternateSetting = 0;
	pInterfaceInformation->Class = 3;
	pInterfaceInformation->SubClass = 0;
	pInterfaceInformation->Protocol = 1;
	pInterfaceInformation->InterfaceHandle = (PVOID)INTERFACEHANDLE0;
	pInterfaceInformation->NumberOfPipes = 1;
	pInterfaceInformation->Pipes[0].MaximumPacketSize = 8;
	pInterfaceInformation->Pipes[0].EndpointAddress = 0x81;
	pInterfaceInformation->Pipes[0].Interval = 0x0a;
	pInterfaceInformation->Pipes[0].PipeType = USB_ENDPOINT_TYPE_INTERRUPT;
	pInterfaceInformation->Pipes[0].PipeHandle = (PVOID)PIPE0HANDLE;

	pInterfaceInformation++;

	pInterfaceInformation->Length = sizeof(USBD_INTERFACE_INFORMATION);
	pInterfaceInformation->InterfaceNumber = 1;
	pInterfaceInformation->AlternateSetting = 0;
	pInterfaceInformation->Class = 3;
	pInterfaceInformation->SubClass = 0;
	pInterfaceInformation->Protocol = 2;
	pInterfaceInformation->InterfaceHandle = (PVOID)INTERFACEHANDLE1;
	pInterfaceInformation->NumberOfPipes = 1;
	pInterfaceInformation->Pipes[0].MaximumPacketSize = 4;
	pInterfaceInformation->Pipes[0].EndpointAddress = 0x82;
	pInterfaceInformation->Pipes[0].Interval = 0x0a;
	pInterfaceInformation->Pipes[0].PipeType = USB_ENDPOINT_TYPE_INTERRUPT;
	pInterfaceInformation->Pipes[0].PipeHandle = (PVOID)PIPE1HANDLE;

	InitializeListHead( &childDeviceExtension->Pipe0IrpListHead );
	InitializeListHead(&childDeviceExtension->Pipe1IrpListHead);

	KeInitializeTimer( &childDeviceExtension->Pipe0Timer );
	KeInitializeTimer(&childDeviceExtension->Pipe1Timer);

	KeInitializeDpc( &childDeviceExtension->Pipe0Dpc, Pipe0TimerRoutine, deviceExtension->ChildDeviceObject[0] );
	KeInitializeDpc(&childDeviceExtension->Pipe1Dpc, Pipe1TimerRoutine, deviceExtension->ChildDeviceObject[0]);

	KeSetTimerEx( &childDeviceExtension->Pipe0Timer, RtlConvertLongToLargeInteger( -1 * 100000), 1, &childDeviceExtension->Pipe0Dpc );
	KeSetTimerEx(&childDeviceExtension->Pipe1Timer, RtlConvertLongToLargeInteger(-1 * 100000), 1, &childDeviceExtension->Pipe1Dpc);

	// LoopBackBuffer 준비
	childDeviceExtension->pLoopBackDataBufferKeyboard = (unsigned char *)ExAllocatePool( NonPagedPool, MAXIMUMLOOPBACKDATASIZE+1 );
	childDeviceExtension->pLoopBackDataBufferKeyboardTemp = (unsigned char *)ExAllocatePool(NonPagedPool, MAXIMUMLOOPBACKDATASIZE + 1);
	childDeviceExtension->SizeofHoldingKeyboard = 0;
	KeInitializeSpinLock(&childDeviceExtension->KeyboardLoopbackSpinLock);

	childDeviceExtension->pLoopBackDataBufferMouse = (unsigned char *)ExAllocatePool(NonPagedPool, MAXIMUMLOOPBACKDATASIZE + 1);
	childDeviceExtension->pLoopBackDataBufferMouseTemp = (unsigned char *)ExAllocatePool(NonPagedPool, MAXIMUMLOOPBACKDATASIZE + 1);
	childDeviceExtension->SizeofHoldingMouse = 0;
	KeInitializeSpinLock(&childDeviceExtension->MouseLoopbackSpinLock);
}

NTSTATUS
BUSDRIVER_CreateClose
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	PDEVICE_OBJECT HIDDeviceObject;
	PDEVICE_EXTENSION HIDDeviceExtension;
	NTSTATUS status=STATUS_INVALID_DEVICE_REQUEST;
	
	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	if( deviceExtension->DeviceObjectType == BUSDRIVERDEVICE )
	{ // BusDriverDevice
		switch (pStack->MajorFunction)
		{
		case IRP_MJ_CREATE:
			break;
		case IRP_MJ_CLEANUP:
		case IRP_MJ_CLOSE:
			HIDDeviceObject = deviceExtension->ChildDeviceObject[0];
			if (HIDDeviceObject == NULL)
				break;

			HIDDeviceExtension = (PDEVICE_EXTENSION)HIDDeviceObject->DeviceExtension;

			if (HIDDeviceExtension->pKLEDUserEvent)
			{
				ObDereferenceObject(HIDDeviceExtension->pKLEDUserEvent);
				HIDDeviceExtension->pKLEDUserEvent = NULL;
			}
			break;
		}

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
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	PDEVICE_OBJECT NextLayerDeviceObject;
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
//				PDEVICE_EXTENSION childDeviceExtension;

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

					// 특별히 드라이버가 자동으로 자식장치를 열거한다
					{
						if (deviceExtension->HasChild == FALSE)
						{
							deviceExtension->HasChild = TRUE;
							CreateChildDevice(DeviceObject, Irp);
							IoInvalidateDeviceRelations(deviceExtension->PhysicalDeviceObject, BusRelations);
							returnStatus = STATUS_SUCCESS;
						}
					}
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
	
    PIO_STACK_LOCATION      pStack;
    PDEVICE_CAPABILITIES    deviceCapabilities;
//    NTSTATUS                status;
    
    pStack = IoGetCurrentIrpStackLocation (Irp);
	
    deviceCapabilities = pStack->Parameters.DeviceCapabilities.Capabilities;
	
    if(deviceCapabilities->Version != 1 || 
		deviceCapabilities->Size < sizeof(DEVICE_CAPABILITIES))
    {
		return STATUS_UNSUCCESSFUL; 
    }
    
    deviceCapabilities->DeviceState[PowerSystemWorking] = PowerDeviceD0;
    deviceCapabilities->DeviceState[PowerSystemSleeping1] = PowerDeviceD3;
    deviceCapabilities->DeviceState[PowerSystemSleeping2] = PowerDeviceD3;
    deviceCapabilities->DeviceState[PowerSystemSleeping3] = PowerDeviceD3;
    deviceCapabilities->SystemWake = PowerSystemUnspecified;
    deviceCapabilities->DeviceWake = PowerDeviceUnspecified;
    deviceCapabilities->DeviceD1 = FALSE;
    deviceCapabilities->DeviceD2 = FALSE;
    deviceCapabilities->WakeFromD0 = FALSE;
    deviceCapabilities->WakeFromD1 = FALSE;
    deviceCapabilities->WakeFromD2 = FALSE;
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

#include <initguid.h>

DEFINE_GUID(BusGuid, 0xeeaf37d0, 0x1963, 0x47c4, 0xaa, 0x48, 0x72, 0x47, 0x6d, 0xb7, 0xcf, 0x49);

NTSTATUS
ChildDriverDevice_PnpDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	KIRQL OldIrql;
	LIST_ENTRY Dequeued0IrpListHead, Dequeued1IrpListHead;
	PIRP CancelIrp;
	PQUEUE_ENTRY	pQueueListEntry = NULL;

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
			InitializeListHead(&Dequeued1IrpListHead);

			IoAcquireCancelSpinLock( &OldIrql );
			KeCancelTimer( &deviceExtension->Pipe0Timer );
			KeCancelTimer(&deviceExtension->Pipe1Timer);

			while( !IsListEmpty( &deviceExtension->Pipe0IrpListHead ) )
			{
				pQueueListEntry = (QUEUE_ENTRY *)RemoveHeadList(&deviceExtension->Pipe0IrpListHead);
				
				InsertTailList( &Dequeued0IrpListHead, &pQueueListEntry->Entry);
				CancelIrp = pQueueListEntry->Irp;
				IoSetCancelRoutine(CancelIrp, NULL);
			}
			while (!IsListEmpty(&deviceExtension->Pipe1IrpListHead))
			{
				pQueueListEntry = (QUEUE_ENTRY *)RemoveHeadList(&deviceExtension->Pipe1IrpListHead);
				InsertTailList(&Dequeued1IrpListHead, &pQueueListEntry->Entry);
				CancelIrp = pQueueListEntry->Irp;
				IoSetCancelRoutine(CancelIrp, NULL);
			}

			IoReleaseCancelSpinLock( OldIrql );

			while( !IsListEmpty( &Dequeued0IrpListHead ) )
			{
				pQueueListEntry = (QUEUE_ENTRY *)RemoveHeadList(&Dequeued0IrpListHead);
				CancelIrp = pQueueListEntry->Irp;
				CancelIrp->IoStatus.Status = STATUS_CANCELLED;
				CancelIrp->IoStatus.Information = 0;
				IoCompleteRequest( CancelIrp, IO_NO_INCREMENT );
				ExFreePool(pQueueListEntry);
			}
			while (!IsListEmpty(&Dequeued1IrpListHead))
			{
				pQueueListEntry = (QUEUE_ENTRY *)RemoveHeadList(&Dequeued1IrpListHead);
				CancelIrp = pQueueListEntry->Irp;
				CancelIrp->IoStatus.Status = STATUS_CANCELLED;
				CancelIrp->IoStatus.Information = 0;
				IoCompleteRequest(CancelIrp, IO_NO_INCREMENT);
				ExFreePool(pQueueListEntry);
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
			if( deviceExtension->pLoopBackDataBufferKeyboard )
			{
				ExFreePool( deviceExtension->pLoopBackDataBufferKeyboard);
				ExFreePool(deviceExtension->pLoopBackDataBufferKeyboardTemp);
				deviceExtension->pLoopBackDataBufferKeyboard = NULL;
				deviceExtension->pLoopBackDataBufferKeyboardTemp = NULL;
				deviceExtension->SizeofHoldingKeyboard = 0;
			}			
			if (deviceExtension->pLoopBackDataBufferMouse)
			{
				ExFreePool(deviceExtension->pLoopBackDataBufferMouse);
				ExFreePool(deviceExtension->pLoopBackDataBufferMouseTemp);
				deviceExtension->pLoopBackDataBufferMouse = NULL;
				deviceExtension->pLoopBackDataBufferMouseTemp = NULL;
				deviceExtension->SizeofHoldingMouse = 0;
			}
			if (deviceExtension->pKLEDUserEvent)
			{
				ObDereferenceObject(deviceExtension->pKLEDUserEvent);
				deviceExtension->pKLEDUserEvent = NULL;
			}

			IoDeleteDevice ( DeviceObject );

			returnStatus = STATUS_SUCCESS;
			break;
		}

		case IRP_MN_QUERY_INTERFACE:
		{
			{
				int i;
				i = 0;
			}
			Irp->IoStatus.Information = 0;
			returnStatus = STATUS_NOT_SUPPORTED;
			break;
		}

		case IRP_MN_QUERY_BUS_INFORMATION:
		{ 
			{
				PPNP_BUS_INFORMATION pInformation = NULL;
				pInformation = (PPNP_BUS_INFORMATION)ExAllocatePool(NonPagedPool, sizeof(PNP_BUS_INFORMATION));
				pInformation->BusNumber = 0;
				pInformation->LegacyBusType = PNPBus;
				memcpy(&pInformation->BusTypeGuid, &BusGuid, 16);				
				Irp->IoStatus.Information = (ULONG_PTR)pInformation;
			}
			returnStatus = STATUS_SUCCESS;
			break;
		}

		case IRP_MN_DEVICE_ENUMERATED:
		{
			Irp->IoStatus.Information = 0;
			returnStatus = STATUS_SUCCESS;
			break;
		}

		case IRP_MN_QUERY_DEVICE_RELATIONS :
		{ 
			PDEVICE_RELATIONS pRelations;

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
//			ULONG dwSize;
			PDEVICE_EXTENSION ParentDeviceExtension;

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
			PDEVICE_EXTENSION ParentDeviceExtension;

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
						pID = (WCHAR *)ExAllocatePool(PagedPool, 
							sizeof(CHILDDEVICEHARDWAREID) + 4);
						RtlZeroMemory( pID, sizeof(CHILDDEVICEHARDWAREID) + 4);
						wcscpy( pID, CHILDDEVICEHARDWAREID );
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
				case BusQueryContainerID:
					if (ParentDeviceExtension->ChildDeviceObject[0] == DeviceObject)
					{
						pID = (WCHAR *)ExAllocatePool(PagedPool, sizeof(CHILDDEVICECONTAINERID) + 2);
						RtlZeroMemory(pID, sizeof(CHILDDEVICECONTAINERID) + 2);
						RtlMoveMemory(pID, CHILDDEVICECONTAINERID, sizeof(CHILDDEVICECONTAINERID));
						returnStatus = STATUS_SUCCESS;
					}
					break;
			}
			Irp->IoStatus.Information = (ULONG_PTR)pID;
			break;
		}
		default:
			returnStatus = STATUS_NOT_SUPPORTED;
			break;
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
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	
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
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	
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
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension = NULL;
	PDEVICE_EXTENSION HIDDeviceExtension = NULL;
	PDEVICE_OBJECT HIDDeviceObject = NULL;
	ULONG Information=0;
	NTSTATUS returnStatus=STATUS_INVALID_DEVICE_REQUEST;
	unsigned char * pInputData = NULL;
	int	UserDataLength = 0;
	int	InputDataLength = 0;
	HANDLE * pUserEventHandle = NULL;
	unsigned char * pUserLEDData = NULL;

	pStack = IoGetCurrentIrpStackLocation ( Irp );
	deviceExtension = DeviceObject->DeviceExtension;

	HIDDeviceObject = deviceExtension->ChildDeviceObject[0];
	HIDDeviceExtension = (PDEVICE_EXTENSION)HIDDeviceObject->DeviceExtension;

	if (HIDDeviceObject == 0)
		goto exit;

	if( deviceExtension->DeviceObjectType == BUSDRIVERDEVICE )
	{ // BusDriverDevice
		switch( pStack->Parameters.DeviceIoControl.IoControlCode )
		// 응용프로그램이 전달하는 ControlCode를 확인합니다. 
		// 이곳에서는 파라미터의 유효성을 확인합니다.
		{
			case IOCTL_BUSDRIVER_GETKEYBOARD_LED:
				if (deviceExtension->HasChild == TRUE)
				{
					if (pStack->Parameters.DeviceIoControl.OutputBufferLength != 1)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}
					pUserLEDData = (unsigned char *)Irp->AssociatedIrp.SystemBuffer;
					if (pUserLEDData == NULL)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}
					*pUserLEDData = HIDDeviceExtension->LED_Data;
					Information = sizeof(unsigned char);
					returnStatus = STATUS_SUCCESS;
				}
				break;
		case IOCTL_BUSDRIVER_SENDKEYBOARD_LED_EVENT:
				if (deviceExtension->HasChild == TRUE)
				{
					if (HIDDeviceExtension->pKLEDUserEvent)
					{
						ObDereferenceObject(HIDDeviceExtension->pKLEDUserEvent);
						HIDDeviceExtension->pKLEDUserEvent = NULL;
					}

					if (pStack->Parameters.DeviceIoControl.InputBufferLength != sizeof(HANDLE))
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					pUserEventHandle = (HANDLE *)Irp->AssociatedIrp.SystemBuffer;
					if (pUserEventHandle == NULL)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					returnStatus = ObReferenceObjectByHandle(
						*pUserEventHandle,
						THREAD_ALL_ACCESS,
						NULL,
						KernelMode,
						&HIDDeviceExtension->pKLEDUserEvent,
						NULL);
					if (!NT_SUCCESS(returnStatus))
					{
						goto exit;
					}
					Information = 0;
					returnStatus = STATUS_SUCCESS;
				}
				break;

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

			case IOCTL_BUSDRIVER_SENDKEYBOARD: // 8bytes
				if (deviceExtension->HasChild == TRUE)
				{
					pInputData = (unsigned char *)Irp->AssociatedIrp.SystemBuffer;
					UserDataLength = pStack->Parameters.DeviceIoControl.InputBufferLength;
					
					UserDataLength = UserDataLength / 8;
					UserDataLength = UserDataLength * 8;

					if(pInputData == NULL)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					if (UserDataLength == 0)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					InputDataLength = GetLoopBackDataSize(HIDDeviceExtension, KEYBOARD_TYPE);
					InputDataLength = MAXIMUMLOOPBACKDATASIZE - InputDataLength;

					UserDataLength = (UserDataLength > InputDataLength) ? InputDataLength : UserDataLength;
					if (UserDataLength == 0)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					if(UserDataLength < 8)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					InsertLoopBackData(HIDDeviceExtension, KEYBOARD_TYPE, pInputData, UserDataLength);
					returnStatus = STATUS_SUCCESS;
					Information = UserDataLength;
				}
				break;

			case IOCTL_BUSDRIVER_SENDMOUSE: // 4bytes
				if (deviceExtension->HasChild == TRUE)
				{
					pInputData = (unsigned char *)Irp->AssociatedIrp.SystemBuffer;
					UserDataLength = pStack->Parameters.DeviceIoControl.InputBufferLength;

					UserDataLength = UserDataLength / 4;
					UserDataLength = UserDataLength * 4;

					if (pInputData == NULL)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					if (UserDataLength == 0)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					InputDataLength = GetLoopBackDataSize(HIDDeviceExtension, MOUSE_TYPE);
					InputDataLength = MAXIMUMLOOPBACKDATASIZE - InputDataLength;

					UserDataLength = (UserDataLength > InputDataLength) ? InputDataLength : UserDataLength;
					if (UserDataLength == 0)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					if (UserDataLength < 4)
					{
						returnStatus = STATUS_INVALID_PARAMETER;
						goto exit;
					}

					InsertLoopBackData(HIDDeviceExtension, MOUSE_TYPE, pInputData, UserDataLength);
					Information = UserDataLength;
					returnStatus = STATUS_SUCCESS;
				}
				break;
		}
	}

exit:
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
FilterPass(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{
	NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
	PIO_STACK_LOCATION pStack;

	UNREFERENCED_PARAMETER(DeviceObject);

	pStack = IoGetCurrentIrpStackLocation(Irp);

	Irp->IoStatus.Status = ntStatus;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return ntStatus;
}

NTSTATUS                                                            
DriverEntry                                                         
	(                                                               
	IN PDRIVER_OBJECT DriverObject,             
	IN PUNICODE_STRING RegistryPath             
							
	)
{
	NTSTATUS returnStatus = STATUS_SUCCESS;    
	ULONG               ulIndex;
	PDRIVER_DISPATCH  * dispatch;

	UNREFERENCED_PARAMETER(RegistryPath);

	for (ulIndex = 0, dispatch = DriverObject->MajorFunction;
		ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
		ulIndex++, dispatch++) {

		*dispatch = FilterPass;
	}

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
{ // BulkIn. Keyboard
	NTSTATUS status;
	KIRQL OldIrql;
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_EXTENSION deviceExtension; 
	PIRP Irp=NULL;
	QUEUE_ENTRY * pQueueListEntry = NULL;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	DeviceObject = (PDEVICE_OBJECT)DeferredContext;
	deviceExtension = DeviceObject->DeviceExtension;

	IoAcquireCancelSpinLock( &OldIrql );

	if( !IsListEmpty( &deviceExtension->Pipe0IrpListHead ) )
	{
		pQueueListEntry = (QUEUE_ENTRY *)RemoveHeadList(&deviceExtension->Pipe0IrpListHead);
		Irp = pQueueListEntry->Irp;
		ExFreePool(pQueueListEntry);
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
			pQueueListEntry = (QUEUE_ENTRY *)ExAllocatePool(NonPagedPool, sizeof(QUEUE_ENTRY));
			pQueueListEntry->Irp = Irp;
			InsertHeadList(&deviceExtension->Pipe0IrpListHead, &pQueueListEntry->Entry);
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
{ // BulkOut. Mouse
	NTSTATUS status;
	KIRQL OldIrql;
	PDEVICE_OBJECT DeviceObject;
	PDEVICE_EXTENSION deviceExtension;
	PIRP Irp = NULL;
	QUEUE_ENTRY * pQueueListEntry = NULL;

	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(SystemArgument1);
	UNREFERENCED_PARAMETER(SystemArgument2);

	DeviceObject = (PDEVICE_OBJECT)DeferredContext;
	deviceExtension = DeviceObject->DeviceExtension;

	IoAcquireCancelSpinLock(&OldIrql);

	if (!IsListEmpty(&deviceExtension->Pipe1IrpListHead))
	{
		pQueueListEntry = (QUEUE_ENTRY *)RemoveHeadList(&deviceExtension->Pipe1IrpListHead);
		Irp = pQueueListEntry->Irp;
		ExFreePool(pQueueListEntry);
	}
	IoReleaseCancelSpinLock(OldIrql);

	if (!Irp)
		return;

	// Handling
	status = USBIO(DeviceObject, Irp);

	if (status == STATUS_PENDING)
	{
		if (deviceExtension->Removed != TRUE)
		{
			IoAcquireCancelSpinLock(&OldIrql);
			pQueueListEntry = (QUEUE_ENTRY *)ExAllocatePool(NonPagedPool, sizeof(QUEUE_ENTRY));
			pQueueListEntry->Irp = Irp;
			InsertHeadList(&deviceExtension->Pipe1IrpListHead, &pQueueListEntry->Entry);
			IoReleaseCancelSpinLock(OldIrql);
		}
	}
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
	int bIndex = 0;

	if( !pUrb )
		return STATUS_INVALID_PARAMETER;

	if( !pUrb->UrbHeader.Length )
		return STATUS_INVALID_PARAMETER;

	switch( pUrb->UrbHeader.Function )
	{
	case URB_FUNCTION_SELECT_CONFIGURATION:
		break;
	case URB_FUNCTION_ABORT_PIPE:
		if (pUrb->UrbPipeRequest.PipeHandle == (USBD_PIPE_HANDLE)PIPE1HANDLE)
		{
			bIndex = 1;
		}
		break;
	case URB_FUNCTION_GET_CURRENT_FRAME_NUMBER:
		break;
	case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
		if (pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (USBD_PIPE_HANDLE)PIPE1HANDLE)
		{
			bIndex = 1;
		}
		break;
	case URB_FUNCTION_RESET_PIPE:
		if (pUrb->UrbPipeRequest.PipeHandle == (USBD_PIPE_HANDLE)PIPE1HANDLE)
		{
			bIndex = 1;
		}
		break;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
		break;
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		if (pUrb->UrbControlDescriptorRequest.Index)
		{
			bIndex = 1;
		}
		break;
	case URB_FUNCTION_GET_CONFIGURATION:
		break;
	case URB_FUNCTION_CLASS_INTERFACE:
		if (pUrb->UrbControlVendorClassRequest.Index)
		{
			bIndex = 1;
		}
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
	USBD_INTERFACE_INFORMATION * pInformation = NULL;
	USBD_INTERFACE_INFORMATION * pDEInformation = NULL;
	PDEVICE_EXTENSION deviceExtension;
	USHORT dwSize=0;
	deviceExtension = DeviceObject->DeviceExtension;

	dwSize = pUrb->UrbSelectConfiguration.Hdr.Length;

	if( !pUrb->UrbSelectConfiguration.ConfigurationDescriptor )
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	if( pUrb->UrbSelectConfiguration.ConfigurationDescriptor->wTotalLength != deviceExtension->pConfigurationDescriptor->wTotalLength )
	{
		return USBComplete( Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER );
	}

	pDEInformation = (USBD_INTERFACE_INFORMATION *)deviceExtension->pInterfaceInformation;

	pInformation = (USBD_INTERFACE_INFORMATION *)&pUrb->UrbSelectConfiguration.Interface;

	if (dwSize > sizeof(struct _URB_SELECT_CONFIGURATION))
	{
		// Composite
		pInformation->NumberOfPipes = 1;
		pInformation->Pipes[0].MaximumTransferSize = 8;
		pDEInformation->Pipes[0].MaximumTransferSize = pInformation->Pipes[0].MaximumTransferSize;
		RtlMoveMemory(pInformation, pDEInformation, sizeof(USBD_INTERFACE_INFORMATION));

		pInformation++;
		pDEInformation++;

		pInformation->NumberOfPipes = 1;
		pInformation->Pipes[0].MaximumTransferSize = 4;
		pDEInformation->Pipes[0].MaximumTransferSize = pInformation->Pipes[0].MaximumTransferSize;
		RtlMoveMemory(pInformation, pDEInformation, sizeof(USBD_INTERFACE_INFORMATION));
	}
	else
	{
		if (pInformation->InterfaceNumber == 0)
		{
			pInformation->NumberOfPipes = 1;
			pInformation->Pipes[0].MaximumTransferSize = 8;
			pDEInformation->Pipes[0].MaximumTransferSize = pInformation->Pipes[0].MaximumTransferSize;
			RtlMoveMemory(pInformation, pDEInformation, sizeof(USBD_INTERFACE_INFORMATION));
		}
		else
		{
			pInformation->NumberOfPipes = 1;
			pInformation->Pipes[0].MaximumTransferSize = 4;
			pDEInformation->Pipes[0].MaximumTransferSize = pInformation->Pipes[0].MaximumTransferSize;
			RtlMoveMemory(pInformation, pDEInformation, sizeof(USBD_INTERFACE_INFORMATION));
		}
	}
	pUrb->UrbSelectConfiguration.ConfigurationHandle = (PVOID)deviceExtension->ConfigurationHandle;
	return USBComplete( Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS );
}

NTSTATUS
USBIO_ClassInterface(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PURB pUrb
) // 현재는 키보드의 경우만 처리한다. 키보드의 LED를 조작하라는 메시지에 대해서만 처리한다
{
	PDEVICE_EXTENSION deviceExtension;
	unsigned char LED_Data = 0;

	deviceExtension = DeviceObject->DeviceExtension;

	if (pUrb->UrbControlVendorClassRequest.Index != 0)
	{
		return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
	}

	// SetReport LED
	if(pUrb->UrbControlVendorClassRequest.Request != 9)
	{
		return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
	}
	if(pUrb->UrbControlVendorClassRequest.TransferBufferLength != 1)
	{
		return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
	}
	if(pUrb->UrbControlVendorClassRequest.TransferBuffer == NULL)
	{
		return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
	}

	LED_Data = *((unsigned char *)pUrb->UrbControlVendorClassRequest.TransferBuffer);
	deviceExtension->LED_Data = LED_Data;

	// Set Event
	if(deviceExtension->pKLEDUserEvent)
		KeSetEvent(deviceExtension->pKLEDUserEvent, 0, FALSE);

	return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
}

NTSTATUS
USBIO_GetDescriptorFromInterface(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PURB pUrb
)
{
	PVOID TargetBuffer = NULL;
	ULONG TargetBufferRequestSize;
	PDEVICE_EXTENSION deviceExtension;
	deviceExtension = DeviceObject->DeviceExtension;

	if (pUrb->UrbControlDescriptorRequest.Hdr.Function != URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE)
	{
		return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
	}

	// Report Resciptor
	if (!pUrb->UrbControlDescriptorRequest.TransferBufferLength)
	{
		return USBComplete(Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER);
	}

	if (!pUrb->UrbControlDescriptorRequest.TransferBuffer && !pUrb->UrbControlDescriptorRequest.TransferBufferMDL)
	{
		pUrb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		return USBComplete(Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER);
	}

	if (pUrb->UrbControlDescriptorRequest.TransferBuffer && pUrb->UrbControlDescriptorRequest.TransferBufferMDL)
	{
		pUrb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		return USBComplete(Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER);
	}

	if (pUrb->UrbControlDescriptorRequest.TransferBuffer)
	{
		TargetBuffer = pUrb->UrbControlDescriptorRequest.TransferBuffer;
	}
	else if (pUrb->UrbControlDescriptorRequest.TransferBufferMDL)
	{
		TargetBuffer = MmGetSystemAddressForMdlSafe(pUrb->UrbControlDescriptorRequest.TransferBufferMDL, HighPagePriority);
	}

	TargetBufferRequestSize = pUrb->UrbControlDescriptorRequest.TransferBufferLength;

	if (pUrb->UrbControlDescriptorRequest.DescriptorType == 0x22) // report
	{
		if (pUrb->UrbControlDescriptorRequest.LanguageId == 0)
		{
			TargetBufferRequestSize = (TargetBufferRequestSize > sizeof(KbdReportDescriptor)) ?
				sizeof(KbdReportDescriptor) : TargetBufferRequestSize;

			RtlMoveMemory(TargetBuffer, KbdReportDescriptor, TargetBufferRequestSize);
		}
		else
		{
			TargetBufferRequestSize = (TargetBufferRequestSize > sizeof(MouseReportDescriptor)) ?
				sizeof(MouseReportDescriptor) : TargetBufferRequestSize;

			RtlMoveMemory(TargetBuffer, MouseReportDescriptor, TargetBufferRequestSize);
		}
		pUrb->UrbControlDescriptorRequest.TransferBufferLength = TargetBufferRequestSize;
		return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
	}
	else
	{
		pUrb->UrbControlDescriptorRequest.TransferBufferLength = 0;
		return USBComplete(Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER);
	}
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
	PDEVICE_EXTENSION deviceExtension; 
//	PVOID TargetBuffer;
//	ULONG TargetBufferRequestSize;
	LIST_ENTRY DequeuedIrpListHead;
	KIRQL OldIrql;
	PLIST_ENTRY pListEntry;
	PIRP CancelIrp;

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
	ULONG TargetBufferRequestSize = 0;
	PDEVICE_EXTENSION deviceExtension = NULL; 
	int RequestSize;
	int RemainSize;

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
		TargetBuffer = MmGetSystemAddressForMdlSafe(pUrb->UrbBulkOrInterruptTransfer.TransferBufferMDL, HighPagePriority);
	}

	if(TargetBuffer == NULL)
	{
		pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = 0;
		return USBComplete(Irp, pUrb, STATUS_INVALID_PARAMETER, USBD_STATUS_INVALID_PARAMETER);
	}

	TargetBufferRequestSize = pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength;
	
	if( pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (PVOID)PIPE0HANDLE )
	{ // Interrupt IN. Keyboard
		RequestSize = TargetBufferRequestSize;
		RemainSize = GetLoopBackDataSize( deviceExtension, KEYBOARD_TYPE);

		RequestSize = (RequestSize > RemainSize) ? RemainSize : RequestSize;
		if (RequestSize)
		{
			RequestSize = RemoveLoopBackData(deviceExtension, KEYBOARD_TYPE, TargetBuffer, RequestSize);
			if (RequestSize)
			{
				pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = RequestSize;
				return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
			}
		}
		IoMarkIrpPending( Irp );
		return STATUS_PENDING;
	}
	else if( pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (PVOID)PIPE1HANDLE )
	{ // Interrupt IN. Mouse
		RequestSize = TargetBufferRequestSize;
		RemainSize = GetLoopBackDataSize(deviceExtension, MOUSE_TYPE);

		RequestSize = (RequestSize > RemainSize) ? RemainSize : RequestSize;
		if (RequestSize)
		{
			RequestSize = RemoveLoopBackData(deviceExtension, MOUSE_TYPE, TargetBuffer, RequestSize);
			if (RequestSize)
			{
				pUrb->UrbBulkOrInterruptTransfer.TransferBufferLength = RequestSize;
				return USBComplete(Irp, pUrb, STATUS_SUCCESS, USBD_STATUS_SUCCESS);
			}
		}
		IoMarkIrpPending(Irp);
		return STATUS_PENDING;
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
	PDEVICE_EXTENSION deviceExtension; 
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
	PDEVICE_EXTENSION deviceExtension; 
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
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	PURB pUrb;

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

	// add
	case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
		return USBIO_GetDescriptorFromInterface(DeviceObject, Irp, pUrb);

	case URB_FUNCTION_CLASS_INTERFACE:
		return USBIO_ClassInterface(DeviceObject, Irp, pUrb);

	}	
	return USBComplete( Irp, pUrb, STATUS_NOT_SUPPORTED, USBD_STATUS_INVALID_PARAMETER );
}

// Interrupt요청외에 모든 요청을 이곳에서 처리한다
void
BUSDRIVER_WorkItemRoutine
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PVOID Context
	)
{
	PIRP Irp;

	PWORKITEM_CONTEXT pWorkItemContext = (PWORKITEM_CONTEXT)Context;

	Irp = pWorkItemContext->Irp;

	USBIO( DeviceObject, Irp );

	ExFreePool( pWorkItemContext->pIoWorkItem );
	ExFreePool( pWorkItemContext );
}

QUEUE_ENTRY * FindEntryList(PLIST_ENTRY ListHead, PIRP Irp)
{
	QUEUE_ENTRY * pQueueListEntry = NULL;
	PLIST_ENTRY Start;

	Start = ListHead->Flink;

	while (Start != ListHead)
	{
		pQueueListEntry = (QUEUE_ENTRY *)Start;
		if (pQueueListEntry->Irp == Irp)
		{
			RemoveEntryList(Start);
			break;
		}
		Start = Start->Flink;
	}
	return pQueueListEntry;
}

void
Pipe0CancelRoutine
	(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
	)
{
	PDEVICE_EXTENSION deviceExtension;
	QUEUE_ENTRY * pQueueListEntry = NULL;

	deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	pQueueListEntry = (QUEUE_ENTRY *)FindEntryList( &deviceExtension->Pipe0IrpListHead, Irp );
	if (pQueueListEntry)
	{
		ExFreePool(pQueueListEntry);
	}
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
	PDEVICE_EXTENSION deviceExtension;
	QUEUE_ENTRY * pQueueListEntry = NULL;

	deviceExtension = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	pQueueListEntry = (QUEUE_ENTRY *)FindEntryList(&deviceExtension->Pipe1IrpListHead, Irp);
	if (pQueueListEntry)
	{
		ExFreePool(pQueueListEntry);
	}
	IoReleaseCancelSpinLock(Irp->CancelIrql);
	USBComplete(Irp, 0, STATUS_CANCELLED, 0);
}

NTSTATUS
BUSDRIVER_InternalDeviceControlDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PIO_STACK_LOCATION pStack; 
	PDEVICE_EXTENSION deviceExtension; 
	PURB pUrb;
	NTSTATUS status;
	PWORKITEM_CONTEXT pWorkItemContext;
	KIRQL OldIrql;
	QUEUE_ENTRY * pQueueListEntry = NULL;

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
				case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
				case URB_FUNCTION_GET_CONFIGURATION:
				case URB_FUNCTION_CLASS_INTERFACE:
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
						pQueueListEntry = (QUEUE_ENTRY *)ExAllocatePool(NonPagedPool, sizeof(QUEUE_ENTRY));
						pQueueListEntry->Irp = Irp;
						InsertTailList( &deviceExtension->Pipe0IrpListHead, &pQueueListEntry->Entry);
						IoSetCancelRoutine( Irp, Pipe0CancelRoutine );
						IoReleaseCancelSpinLock( OldIrql );
					}
					else if (pUrb->UrbBulkOrInterruptTransfer.PipeHandle == (PVOID)PIPE1HANDLE)
					{
						pQueueListEntry = (QUEUE_ENTRY *)ExAllocatePool(NonPagedPool, sizeof(QUEUE_ENTRY));
						pQueueListEntry->Irp = Irp;
						InsertTailList(&deviceExtension->Pipe1IrpListHead, &pQueueListEntry->Entry);
						IoSetCancelRoutine(Irp, Pipe1CancelRoutine);
						IoReleaseCancelSpinLock(OldIrql);
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
