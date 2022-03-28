#include "wdfsimple.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, SIMPLEEvtDeviceAdd)
#pragma alloc_text (PAGE, SIMPLEEvtIoRead)
#pragma alloc_text (PAGE, SIMPLEEvtIoWrite)
#pragma alloc_text (PAGE, SIMPLEEvtIoDeviceControl)
#endif

// 드라이버가 메모리에 상주할때 호출됩니다
NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS            status = STATUS_SUCCESS;
    WDF_DRIVER_CONFIG   config;

    WDF_DRIVER_CONFIG_INIT(
        &config,
        SIMPLEEvtDeviceAdd
        );

// WDFDEVICE 오브젝트를 생성합니다
    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES, // Driver Attributes
        &config,          // Driver Config Info
        WDF_NO_HANDLE
        );

    if (!NT_SUCCESS(status)) {
    }

    return status;
}

// 디바이스스택을 구성할때 호출됩니다
NTSTATUS
SIMPLEEvtDeviceAdd(
    IN WDFDRIVER       Driver,
    IN PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS               status = STATUS_SUCCESS;
    PFDO_DATA              fdoData;
    WDF_IO_QUEUE_CONFIG    queueConfig;
    WDF_OBJECT_ATTRIBUTES  fdoAttributes;
    WDFDEVICE              hDevice;
    WDFQUEUE               queue;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&fdoAttributes, FDO_DATA);

// WDFDEVICE 오브젝트를 생성합니다
    status = WdfDeviceCreate(&DeviceInit, &fdoAttributes, &hDevice);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    fdoData = SIMPLEFdoGetData(hDevice);

// 응용프로그램이 연결될 수 있도록 합니다. CreateFile() API
    status = WdfDeviceCreateDeviceInterface(
                 hDevice,
                 (LPGUID) &GUID_SIMPLE,
                 NULL 
             );

    if (!NT_SUCCESS (status)) {
        return status;
    }

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,  WdfIoQueueDispatchParallel);

/*
	ReadFile() API, WriteFile API, DeviceIoControl() API 함수와 연결될 이벤트함수를 등록합니다
*/
    queueConfig.EvtIoRead = SIMPLEEvtIoRead;
    queueConfig.EvtIoWrite = SIMPLEEvtIoWrite;
    queueConfig.EvtIoDeviceControl = SIMPLEEvtIoDeviceControl;

// WDFQUEUE 오브젝트를 생성합니다
    status = WdfIoQueueCreate(
        hDevice,
        &queueConfig,
        WDF_NO_OBJECT_ATTRIBUTES,
        &queue
        );

    if (!NT_SUCCESS (status)) {

        return status;
    }

    return status;
}

// ReadFile() API함수와 연결됩니다
VOID
SIMPLEEvtIoRead (
    WDFQUEUE      Queue,
    WDFREQUEST    Request,
    size_t        Length
    )
{
    NTSTATUS    status = STATUS_SUCCESS;
    ULONG_PTR bytesCopied =0;
    WDFMEMORY memory;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Length);

    PAGED_CODE();

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if(NT_SUCCESS(status) ) {
    }

    WdfRequestCompleteWithInformation(Request, status, bytesCopied);
}

// WriteFile() API함수와 연결됩니다
VOID
SIMPLEEvtIoWrite (
    WDFQUEUE      Queue,
    WDFREQUEST    Request,
    size_t        Length
    )
{
    NTSTATUS    status = STATUS_SUCCESS;
    ULONG_PTR bytesWritten =0;
    WDFMEMORY memory;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(Length);

    PAGED_CODE();

    status = WdfRequestRetrieveInputMemory(Request, &memory);
    if(NT_SUCCESS(status) ) {
        bytesWritten = Length;
    }

    WdfRequestCompleteWithInformation(Request, status, bytesWritten);
}


// DeviceIoControl() API함수와 연결됩니다
VOID
SIMPLEEvtIoDeviceControl(
    IN WDFQUEUE     Queue,
    IN WDFREQUEST   Request,
    IN size_t       OutputBufferLength,
    IN size_t       InputBufferLength,
    IN ULONG        IoControlCode
    )
{
    NTSTATUS  status= STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(Queue);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    PAGED_CODE();

    switch (IoControlCode) {
		case IOCTL_SIMPLE_CONTROL: // 응용프로그램이 사용한 IOControlCode를 확인합니다
			break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
    }
    WdfRequestCompleteWithInformation(Request, status, (ULONG_PTR) 0);
}


