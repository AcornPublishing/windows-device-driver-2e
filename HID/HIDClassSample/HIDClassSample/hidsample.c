#include <ntddk.h>
#include <hidclass.h>
#include <hidport.h>

#define INPUT_REPORT_ID1	1
#define INPUT_REPORT_ID2	2
#define FEATURE_REPORT_ID	3

#define GET_NEXT_DEVICE_OBJECT(DO) \
    (((PHID_DEVICE_EXTENSION)(DO)->DeviceExtension)->NextDeviceObject)

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

HID_REPORT_DESCRIPTOR           G_DefaultReportDescriptor[] = {
    0x06,0x00, 0xFF,                // USAGE_PAGE (Vender Defined Usage Page)
    0x09,0x20,                      // USAGE (Vendor Usage 0x20)
    0xA1,0x01,                      // COLLECTION (Application)
    0x85,INPUT_REPORT_ID1,			// Report ID 
    0x19,0x01,                      //   USAGE MINIMUM 
    0x29,0x04,                      //   USAGE MAXIMUM 
    0x15,0x00,                      //   LOGICAL_MINIMUM(0)
    0x26,0xff, 0x00,                //   LOGICAL_MAXIMUM(255)
    0x75,0x08,                      //   REPORT_SIZE 
    0x95,0x04,                      //   REPORT_COUNT 
    0x81, 0x06,                     //   Input (Data, Variable, Relative, Preferred)
    0xC0,                            // END_COLLECTION

    0x06,0x00, 0xFF,                // USAGE_PAGE (Vender Defined Usage Page)
    0x09,0x30,                      // USAGE (Vendor Usage 0x30)
    0xA1,0x01,                      // COLLECTION (Application)
    0x85,INPUT_REPORT_ID2,			// Report ID 
    0x19,0x01,                      //   USAGE MINIMUM 
    0x29,0x08,                      //   USAGE MAXIMUM 
    0x15,0x00,                      //   LOGICAL_MINIMUM(0)
    0x26,0xff, 0x00,                //   LOGICAL_MAXIMUM(255)
    0x75,0x08,                      //   REPORT_SIZE 
    0x95,0x08,                      //   REPORT_COUNT 
    0x81, 0x06,                     //   Input (Data, Variable, Relative, Preferred)
    0xC0,                            // END_COLLECTION

    0x06,0x00, 0xFF,                // USAGE_PAGE (Vender Defined Usage Page)
    0x09,0x40,                      // USAGE (Vendor Usage 0x40)
    0xA1,0x01,                      // COLLECTION (Application)
    0x85,FEATURE_REPORT_ID,			// Report ID 
    0x19,0x01,                      //   USAGE MINIMUM 
    0x29,0x10,                      //   USAGE MAXIMUM 
    0x15,0x00,                      //   LOGICAL_MINIMUM(0)
    0x26,0xff, 0x00,                //   LOGICAL_MAXIMUM(255)
    0x75,0x08,                      //   REPORT_SIZE 
    0x95,0x10,                      //   REPORT_COUNT 
    0xB1,0x06,                      //   Feature (Data,Variable,Relative)
    0xC0                            // END_COLLECTION
};

CONST HID_DESCRIPTOR G_DefaultHidDescriptor = {
    0x09,   // length of HID descriptor
    0x21,   // descriptor type == HID  0x21
    0x0100, // hid spec release
    0x00,   // country code == Not Specified
    0x01,   // number of HID class descriptors
    { 0x22,   // descriptor type 
    sizeof(G_DefaultReportDescriptor) }  // total length of report descriptor
};

typedef struct _MINIDRIVER_EXTENSION_tag
{
	BOOLEAN g_Removed;
}MINIDRIVER_EXTENSION, *PMINIDRIVER_EXTENSION;

#define HARDWAREID L"HID\\VID_1234&PID_5678"

NTSTATUS
HIDSAMPLE_AddDevice
	(
	IN PDRIVER_OBJECT DriverObject,         
	IN PDEVICE_OBJECT FunctionalDeviceObject  
	)
{
	PHID_DEVICE_EXTENSION	pDeviceExtension;
	PMINIDRIVER_EXTENSION	pMiniExtension;

	UNREFERENCED_PARAMETER(DriverObject);

	pDeviceExtension = (PHID_DEVICE_EXTENSION)FunctionalDeviceObject->DeviceExtension;
	pMiniExtension = (PMINIDRIVER_EXTENSION)pDeviceExtension->MiniDeviceExtension;
	pMiniExtension->g_Removed = FALSE;

    FunctionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

// --- Hid processing
NTSTATUS
GetHidDescriptor(
    PDEVICE_OBJECT DeviceObject,
    PIRP	Irp
    )
{
    size_t              bytesToCopy = 0;
    void *	            buffer;
	PIO_STACK_LOCATION pStack; 
	
	UNREFERENCED_PARAMETER(DeviceObject);

	pStack = IoGetCurrentIrpStackLocation ( Irp );

	buffer = Irp->UserBuffer;
	if( !buffer )
		return STATUS_INVALID_PARAMETER;

	bytesToCopy = (pStack->Parameters.DeviceIoControl.OutputBufferLength > G_DefaultHidDescriptor.bLength)?
		G_DefaultHidDescriptor.bLength:pStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (bytesToCopy == 0) {
        return STATUS_INVALID_PARAMETER;        
    }
    
	RtlMoveMemory( buffer, &G_DefaultHidDescriptor, bytesToCopy );
	Irp->IoStatus.Information = bytesToCopy;
    return STATUS_SUCCESS;
}

NTSTATUS
GetReportDescriptor(
    PDEVICE_OBJECT DeviceObject,
    PIRP	Irp
    )
{
    size_t              bytesToCopy = 0;
    void *	            buffer;
	PIO_STACK_LOCATION pStack; 
	
	UNREFERENCED_PARAMETER(DeviceObject);

	pStack = IoGetCurrentIrpStackLocation ( Irp );

	buffer = Irp->UserBuffer;
	if( !buffer )
		return STATUS_INVALID_PARAMETER;

	bytesToCopy = (pStack->Parameters.DeviceIoControl.OutputBufferLength > G_DefaultHidDescriptor.DescriptorList[0].wReportLength)?
		G_DefaultHidDescriptor.DescriptorList[0].wReportLength:pStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (bytesToCopy == 0) {
        return STATUS_INVALID_PARAMETER;        
    }
    
	RtlMoveMemory( buffer, G_DefaultReportDescriptor, bytesToCopy );
	Irp->IoStatus.Information = bytesToCopy;
    return STATUS_SUCCESS;
}


NTSTATUS
GetDeviceAttributes(
    PDEVICE_OBJECT DeviceObject,
    PIRP	Irp
    )
{
    size_t              bytesToCopy = 0;
	PIO_STACK_LOCATION pStack; 
    PHID_DEVICE_ATTRIBUTES   deviceAttributes = NULL;
	
	UNREFERENCED_PARAMETER(DeviceObject);

	pStack = IoGetCurrentIrpStackLocation ( Irp );

	deviceAttributes = Irp->UserBuffer;
	if( !deviceAttributes )
		return STATUS_INVALID_PARAMETER;

	bytesToCopy = pStack->Parameters.DeviceIoControl.OutputBufferLength;

    if (bytesToCopy == 0) {
        return STATUS_INVALID_PARAMETER;        
    }
    
    deviceAttributes->Size = sizeof (HID_DEVICE_ATTRIBUTES);
    deviceAttributes->VendorID = 0x1234;
    deviceAttributes->ProductID = 0x5678;
    deviceAttributes->VersionNumber = 0x55aa;

	Irp->IoStatus.Information = bytesToCopy;
    return STATUS_SUCCESS;
}

NTSTATUS
GetFeature(
    PDEVICE_OBJECT DeviceObject,
	PIRP		Irp
    )
{
	/*
	응용프로그램으로 부터 요청되는 Feature Report의 ReportId는 3이된다. 이것을 확인한다
	Test로 사용되므로 첫번째 Usage에만 값을 넣어준다
	*/
	unsigned char * pAppBuffer;
    PHID_XFER_PACKET             transferPacket = NULL;
	PIO_STACK_LOCATION pStack; 
	size_t	bytesToCopy;
	
	UNREFERENCED_PARAMETER(DeviceObject);

	pStack = IoGetCurrentIrpStackLocation ( Irp );

    if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_XFER_PACKET)) {
        return STATUS_BUFFER_TOO_SMALL;
    }
	bytesToCopy = sizeof(HID_XFER_PACKET);

    transferPacket = (PHID_XFER_PACKET)Irp->UserBuffer;
    if (transferPacket == NULL) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (transferPacket->reportBufferLen == 0){
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (transferPacket->reportBufferLen < sizeof(UCHAR)){
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (transferPacket->reportId != FEATURE_REPORT_ID){ // 0x03
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    
	pAppBuffer = (unsigned char *)transferPacket->reportBuffer;
    if (pAppBuffer == NULL) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

	pAppBuffer[1] = 0x33; // 결과를 넣는다
	Irp->IoStatus.Information = bytesToCopy;
    return STATUS_SUCCESS;
}

NTSTATUS
SetFeature(
    PDEVICE_OBJECT DeviceObject,
	PIRP		Irp
    )
{
    PHID_XFER_PACKET             transferPacket = NULL;
	PIO_STACK_LOCATION pStack; 
	size_t	bytesToCopy;
	
	UNREFERENCED_PARAMETER(DeviceObject);

	pStack = IoGetCurrentIrpStackLocation ( Irp );

    if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(HID_XFER_PACKET)) {
        return STATUS_BUFFER_TOO_SMALL;
    }
	bytesToCopy = sizeof(HID_XFER_PACKET);

    transferPacket = (PHID_XFER_PACKET)Irp->UserBuffer;
    if (transferPacket == NULL) {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    if (transferPacket->reportBufferLen == 0){
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (transferPacket->reportBufferLen < sizeof(UCHAR)){
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (transferPacket->reportId != FEATURE_REPORT_ID){ // 0x03
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    
	Irp->IoStatus.Information = bytesToCopy;
    return STATUS_SUCCESS;
}

NTSTATUS
ReadReport(
    PDEVICE_OBJECT DeviceObject,
	PIRP		Irp
    )
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
WriteReport(
    PDEVICE_OBJECT DeviceObject,
	PIRP		Irp
    )
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);
	
	return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS
InputReport(
    PDEVICE_OBJECT DeviceObject,
	PIRP		Irp
    )
{
	/*
	응용프로그램으로 부터 요청되는 Input Report의 ReportId는 1또는 2가된다. 이것을 확인한다
	Test로 사용되므로 첫번째 Usage에만 값을 넣어준다
	*/
	unsigned char * pAppBuffer;
	NTSTATUS returnStatus;

	UNREFERENCED_PARAMETER(DeviceObject);

	pAppBuffer = (unsigned char *)MmGetSystemAddressForMdlSafe( Irp->MdlAddress, HighPagePriority);

	switch( pAppBuffer[0] ) // ReportId
	{
		case INPUT_REPORT_ID1: // 0x01
			pAppBuffer[1] = 0x55;
			returnStatus = STATUS_SUCCESS;
			break;
		case INPUT_REPORT_ID2: // 0x02
			pAppBuffer[1] = 0xAA;
			returnStatus = STATUS_SUCCESS;
			break;
		default:
			returnStatus = STATUS_NOT_SUPPORTED;
			break;
	}
    return returnStatus;
}

NTSTATUS
OutputReport(
    PDEVICE_OBJECT DeviceObject,
	PIRP		Irp
    )
{
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Irp);

	return STATUS_INVALID_DEVICE_REQUEST;
}

// ---

NTSTATUS
HIDSAMPLE_InternalDeviceControl
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PHID_DEVICE_EXTENSION	pDeviceExtension;
	PMINIDRIVER_EXTENSION	pMiniExtension;
	PIO_STACK_LOCATION pStack; 
	NTSTATUS returnStatus = STATUS_SUCCESS;
	ULONG IoControlCode;
	
	pDeviceExtension = (PHID_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	pMiniExtension = (PMINIDRIVER_EXTENSION)pDeviceExtension->MiniDeviceExtension;

	if( pMiniExtension->g_Removed == TRUE )
	{
		Irp->IoStatus.Status = STATUS_DEVICE_REMOVED;
		IoCompleteRequest( Irp, IO_NO_INCREMENT );
		return STATUS_DEVICE_REMOVED;
	}

	pStack = IoGetCurrentIrpStackLocation ( Irp );
			
	IoControlCode = pStack->Parameters.DeviceIoControl.IoControlCode;

    switch(IoControlCode) {

    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        returnStatus = GetHidDescriptor(DeviceObject, Irp);
        break;

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        returnStatus = GetDeviceAttributes(DeviceObject, Irp);
        break;

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        returnStatus = GetReportDescriptor(DeviceObject, Irp);
        break;

    case IOCTL_HID_READ_REPORT:
        returnStatus = ReadReport(DeviceObject, Irp);
		break;

    case IOCTL_HID_WRITE_REPORT:
        returnStatus = WriteReport(DeviceObject, Irp);
        break;

    case IOCTL_HID_GET_FEATURE:
        returnStatus = GetFeature(DeviceObject, Irp);
        break;

    case IOCTL_HID_SET_FEATURE:
        returnStatus = SetFeature(DeviceObject, Irp);
		break;
      
    case IOCTL_HID_GET_INPUT_REPORT:
        returnStatus = InputReport(DeviceObject, Irp);
		break;

    case IOCTL_HID_SET_OUTPUT_REPORT:
        returnStatus = OutputReport(DeviceObject, Irp);
        break;

    case IOCTL_HID_GET_STRING:
        returnStatus = STATUS_SUCCESS;
        break;

    case IOCTL_HID_ACTIVATE_DEVICE:
		{
			int a;
			a = 1;
		}
        returnStatus = STATUS_SUCCESS;
        break;

    case IOCTL_HID_DEACTIVATE_DEVICE:
		{
			int b;
			b = 2;
		}
        returnStatus = STATUS_SUCCESS;
        break;

    default:
        returnStatus = STATUS_NOT_SUPPORTED;
        break;
    }

	if( returnStatus == STATUS_PENDING )
	{
		return returnStatus;
	}

	Irp->IoStatus.Status = returnStatus;
	IoCompleteRequest( Irp, IO_NO_INCREMENT );
	return returnStatus;
}

NTSTATUS
HIDSAMPLE_PnpDispatch
	(
	IN PDEVICE_OBJECT DeviceObject,   
	IN PIRP Irp                       
	)
{
	PHID_DEVICE_EXTENSION	pDeviceExtension;
	PMINIDRIVER_EXTENSION	pMiniExtension;
	PIO_STACK_LOCATION pStack;

	pDeviceExtension = (PHID_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	pMiniExtension = (PMINIDRIVER_EXTENSION)pDeviceExtension->MiniDeviceExtension;

	pStack = IoGetCurrentIrpStackLocation( Irp );
	switch( pStack->MinorFunction )
	{
		case IRP_MN_START_DEVICE:
			pMiniExtension->g_Removed = FALSE;
			break;
		case IRP_MN_QUERY_REMOVE_DEVICE:
		case IRP_MN_SURPRISE_REMOVAL:
		case IRP_MN_REMOVE_DEVICE:
			pMiniExtension->g_Removed = TRUE;
			break;
	}
    IoCopyCurrentIrpStackLocationToNext(Irp);
    return IoCallDriver(GET_NEXT_DEVICE_OBJECT(DeviceObject), Irp);
}

VOID                                                                
HIDSAMPLE_Unload                                                       
	(                                                               
	IN PDRIVER_OBJECT DriverObject 
	)
{
	UNREFERENCED_PARAMETER(DriverObject);
}

NTSTATUS
HIDSAMPLE_PowerPassThrough(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PoStartNextPowerIrp(Irp);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    return PoCallDriver(GET_NEXT_DEVICE_OBJECT(DeviceObject), Irp);
}

NTSTATUS                                                            
DriverEntry                                                         
	(                                                               
	IN PDRIVER_OBJECT DriverObject,             
	IN PUNICODE_STRING RegistryPath             
							
	)
{
    HID_MINIDRIVER_REGISTRATION hidMinidriverRegistration;
	NTSTATUS returnStatus = STATUS_SUCCESS;    

	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = HIDSAMPLE_InternalDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_PNP] = HIDSAMPLE_PnpDispatch;
    DriverObject->MajorFunction[IRP_MJ_POWER] = HIDSAMPLE_PowerPassThrough;
	DriverObject->DriverUnload = HIDSAMPLE_Unload;
	DriverObject->DriverExtension->AddDevice = HIDSAMPLE_AddDevice;

    RtlZeroMemory(&hidMinidriverRegistration,
                  sizeof(hidMinidriverRegistration));

    hidMinidriverRegistration.Revision            = HID_REVISION;
    hidMinidriverRegistration.DriverObject        = DriverObject;
    hidMinidriverRegistration.RegistryPath        = RegistryPath;
    hidMinidriverRegistration.DeviceExtensionSize = sizeof(MINIDRIVER_EXTENSION);
    hidMinidriverRegistration.DevicesArePolled = FALSE;
    returnStatus = HidRegisterMinidriver(&hidMinidriverRegistration);
    if (!NT_SUCCESS(returnStatus) ){
    }

	return returnStatus;
}

