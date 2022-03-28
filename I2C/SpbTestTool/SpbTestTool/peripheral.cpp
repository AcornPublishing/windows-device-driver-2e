/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

Module Name: 

    peripheral.cpp

Abstract:

    This module contains the function for interaction 
    with the SPB API.

Environment:

    kernel-mode only

Revision History:

--*/

#include "internal.h"
#include "peripheral.h"

#include "peripheral.tmh"

NTSTATUS
SpbPeripheralOpen(
    _In_  PDEVICE_CONTEXT  pDevice
    )
/*++
 
  Routine Description:

    This routine opens a handle to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context

  Return Value:

    Status

--*/
{
    WDF_IO_TARGET_OPEN_PARAMS  openParams;
    NTSTATUS status;

    //
    // Create the device path using the connection ID.
    //

    DECLARE_UNICODE_STRING_SIZE(DevicePath, RESOURCE_HUB_PATH_SIZE);

    RESOURCE_HUB_CREATE_PATH_FROM_ID(
        &DevicePath,
        pDevice->PeripheralId.LowPart,
        pDevice->PeripheralId.HighPart);

    //
    // Open a handle to the SPB controller.
    //

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
        &openParams,
        &DevicePath,
        (GENERIC_READ | GENERIC_WRITE));
    
    openParams.ShareAccess = 0;
    openParams.CreateDisposition = FILE_OPEN;
    openParams.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    
    status = WdfIoTargetOpen(
        pDevice->SpbController,
        &openParams);
     
    if (!NT_SUCCESS(status)) 
    {
    }

    return status;
}

NTSTATUS
SpbPeripheralClose(
    _In_  PDEVICE_CONTEXT  pDevice
    )
/*++
 
  Routine Description:

    This routine closes a handle to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context

  Return Value:

    Status

--*/
{
    WdfIoTargetClose(pDevice->SpbController);

    return STATUS_SUCCESS;
}

VOID
SpbPeripheralLock(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine sends a lock command to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    NTSTATUS status;

    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Initialize the SPB request for lock and send.
    //

    status = WdfIoTargetFormatRequestForIoctl(
        pDevice->SpbController,
        pDevice->SpbRequest,
        IOCTL_SPB_LOCK_CONTROLLER,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (NT_SUCCESS(status))
    {
        status = SpbPeripheralSendRequest(
            pDevice,
            pDevice->SpbRequest,
            FxRequest);
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }

}

VOID
SpbPeripheralUnlock(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine sends an unlock command to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    NTSTATUS status;

    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Initialize the SPB request for unlock and send.
    //

    status = WdfIoTargetFormatRequestForIoctl(
        pDevice->SpbController,
        pDevice->SpbRequest,
        IOCTL_SPB_UNLOCK_CONTROLLER,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (NT_SUCCESS(status))
    {
        status = SpbPeripheralSendRequest(
            pDevice,
            pDevice->SpbRequest,
            FxRequest);
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }
}

VOID
SpbPeripheralLockConnection(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine sends a lock connection command to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    NTSTATUS status;

      
    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Initialize the SPB request for lock and send.
    //

    status = WdfIoTargetFormatRequestForIoctl(
        pDevice->SpbController,
        pDevice->SpbRequest,
        IOCTL_SPB_LOCK_CONNECTION,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (NT_SUCCESS(status))
    {
        status = SpbPeripheralSendRequest(
            pDevice,
            pDevice->SpbRequest,
            FxRequest);
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }
}

VOID
SpbPeripheralUnlockConnection(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine sends an unlock connection command to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    NTSTATUS status;

    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Initialize the SPB request for unlock and send.
    //

    status = WdfIoTargetFormatRequestForIoctl(
        pDevice->SpbController,
        pDevice->SpbRequest,
        IOCTL_SPB_UNLOCK_CONNECTION,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (NT_SUCCESS(status))
    {
        status = SpbPeripheralSendRequest(
            pDevice,
            pDevice->SpbRequest,
            FxRequest);
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }
}

VOID
SpbPeripheralRead(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine reads from the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    WDFMEMORY memory = nullptr;
    NTSTATUS status;

       
    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Initialize the SPB request for read and send.
    //

    status = WdfRequestRetrieveOutputMemory(
        FxRequest,
        &memory);

    if (NT_SUCCESS(status))
    {
        status = WdfIoTargetFormatRequestForRead(
            pDevice->SpbController,
            pDevice->SpbRequest,
            memory,
            nullptr,
            nullptr);

        if (NT_SUCCESS(status))
        {
            status = SpbPeripheralSendRequest(
                pDevice,
                pDevice->SpbRequest,
                FxRequest);
        }
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }
}

VOID
SpbPeripheralWrite(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine writes to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    WDFMEMORY memory = nullptr;
    NTSTATUS status;

    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Initialize the SPB request for write and send.
    //

    status = WdfRequestRetrieveInputMemory(
        FxRequest,
        &memory);

    if (NT_SUCCESS(status))
    {
        status = WdfIoTargetFormatRequestForWrite(
            pDevice->SpbController,
            pDevice->SpbRequest,
            memory,
            nullptr,
            nullptr);

        if (NT_SUCCESS(status))
        {
            status = SpbPeripheralSendRequest(
                pDevice,
                pDevice->SpbRequest,
                FxRequest);
        }
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }
}

VOID
SpbPeripheralWriteRead(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine sends a write-read sequence to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    PVOID pInputBuffer = nullptr;
    PVOID pOutputBuffer = nullptr;
    size_t inputBufferLength = 0;
    size_t outputBufferLength = 0;
    WDFMEMORY inputMemory = nullptr;
    WDF_OBJECT_ATTRIBUTES attributes;
    PREQUEST_CONTEXT pRequest;
    NTSTATUS status;

    pRequest = GetRequestContext(pDevice->SpbRequest);

    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Get input and output buffers.
    //

    status = WdfRequestRetrieveInputBuffer(
        FxRequest,
        0,
        &pInputBuffer,
        &inputBufferLength);

    if (NT_SUCCESS(status))
    {
        status = WdfRequestRetrieveOutputBuffer(
            FxRequest,
            0,
            &pOutputBuffer,
            &outputBufferLength);
    }

    //
    // Build SPB sequence.
    //
    
    if (NT_SUCCESS(status))
    {
        const ULONG transfers = 2;

        SPB_TRANSFER_LIST_AND_ENTRIES(transfers) seq;
        SPB_TRANSFER_LIST_INIT(&(seq.List), transfers);

        {
            //
            // PreFAST cannot figure out the SPB_TRANSFER_LIST_ENTRY
            // "struct hack" size but using an index variable quiets 
            // the warning. This is a false positive from OACR.
            // 

            ULONG index = 0;
            seq.List.Transfers[index] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(
                SpbTransferDirectionToDevice,
                0,
                pInputBuffer,
                (ULONG)inputBufferLength);

            seq.List.Transfers[index + 1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(
                SpbTransferDirectionFromDevice,
                0,
                pOutputBuffer,
                (ULONG)outputBufferLength);
        }

        //
        // Create preallocated WDFMEMORY.
        //

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        
        status = WdfMemoryCreatePreallocated(
            &attributes,
            (PVOID)&seq,
            sizeof(seq),
            &inputMemory);

        if (NT_SUCCESS(status))
        {
        }
    }

    //
    // Send sequence IOCTL.
    //

    if (NT_SUCCESS(status))
    {
        //
        // Mark SPB request as sequence and save length.
        // These will be used in the completion callback
        // to complete the client request with the correct
        // number of bytes
        //

        pRequest->IsSpbSequenceRequest = TRUE;
        pRequest->SequenceWriteLength = (ULONG_PTR)inputBufferLength;

        //
        // Format and send the SPB sequence request.
        //

        status = WdfIoTargetFormatRequestForIoctl(
            pDevice->SpbController,
            pDevice->SpbRequest,
            IOCTL_SPB_EXECUTE_SEQUENCE,
            inputMemory,
            nullptr,
            nullptr,
            nullptr);

        if (NT_SUCCESS(status))
        {
            status = SpbPeripheralSendRequest(
                pDevice,
                pDevice->SpbRequest,
                FxRequest);
        }
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }
}

VOID
SpbPeripheralFullDuplex(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
 
  Routine Description:

    This routine sends a full duplex transfer to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    FxRequest - the framework request object

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxRequest);

    PVOID pInputBuffer = nullptr;
    PVOID pOutputBuffer = nullptr;
    size_t inputBufferLength = 0;
    size_t outputBufferLength = 0;
    WDFMEMORY inputMemory = nullptr;
    WDF_OBJECT_ATTRIBUTES attributes;
    PREQUEST_CONTEXT pRequest;
    NTSTATUS status;

    pRequest = GetRequestContext(pDevice->SpbRequest);

    //
    // Save the client request.
    //

    pDevice->ClientRequest = FxRequest;

    //
    // Get input and output buffers.
    //

    status = WdfRequestRetrieveInputBuffer(
        FxRequest,
        0,
        &pInputBuffer,
        &inputBufferLength);

    if (NT_SUCCESS(status))
    {
        status = WdfRequestRetrieveOutputBuffer(
            FxRequest,
            0,
            &pOutputBuffer,
            &outputBufferLength);
    }

    //
    // Build full duplex transfer using SPB transfer list.
    //
    
    if (NT_SUCCESS(status))
    {
        const ULONG transfers = 2;

        SPB_TRANSFER_LIST_AND_ENTRIES(transfers) seq;
        SPB_TRANSFER_LIST_INIT(&(seq.List), transfers);

        {
            //
            // PreFAST cannot figure out the SPB_TRANSFER_LIST_ENTRY
            // "struct hack" size but using an index variable quiets 
            // the warning. This is a false positive from OACR.
            // 

            ULONG index = 0;
            seq.List.Transfers[index] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(
                SpbTransferDirectionToDevice,
                0,
                pInputBuffer,
                (ULONG)inputBufferLength);

            seq.List.Transfers[index + 1] = SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(
                SpbTransferDirectionFromDevice,
                0,
                pOutputBuffer,
                (ULONG)outputBufferLength);
        }

        //
        // Create preallocated WDFMEMORY.
        //

        WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
        
        status = WdfMemoryCreatePreallocated(
            &attributes,
            (PVOID)&seq,
            sizeof(seq),
            &inputMemory);

        if (NT_SUCCESS(status))
        {
        }
    }

    //
    // Send full duplex IOCTL.
    //

    if (NT_SUCCESS(status))
    {
        //
        // Mark SPB request as full duplex (sequence format)
        // and save length. These will be used in the completion 
        // callback to complete the client request with the correct
        // number of bytes
        //

        pRequest->IsSpbSequenceRequest = TRUE;
        pRequest->SequenceWriteLength = (ULONG_PTR)inputBufferLength;

        //
        // Format and send the full duplex request.
        //

        status = WdfIoTargetFormatRequestForIoctl(
            pDevice->SpbController,
            pDevice->SpbRequest,
            IOCTL_SPB_FULL_DUPLEX,
            inputMemory,
            nullptr,
            nullptr,
            nullptr);

        if (NT_SUCCESS(status))
        {
            status = SpbPeripheralSendRequest(
                pDevice,
                pDevice->SpbRequest,
                FxRequest);
        }
    }

    if (!NT_SUCCESS(status))
    {
        SpbPeripheralCompleteRequestPair(
            pDevice,
            status,
            0);
    }
}

VOID
SpbPeripheralSignalInterrupt(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
Routine Description:

    This routine signals the interrupt service routine
    to continue.

Arguments:

    pDevice - the device context
    FxRequest - the framework request object

Return Value:

   None.

--*/
{
    NTSTATUS status = STATUS_SUCCESS;

    if (pDevice->Interrupt != nullptr)
    {
        //
        // Signal ISR to continue.
        //

        KeSetEvent(
            &pDevice->IsrWaitEvent,
            IO_NO_INCREMENT,
            FALSE);
    }
    else
    {
        status = STATUS_NOT_FOUND;
    }

    WdfRequestComplete(FxRequest, status);
}

VOID
SpbPeripheralWaitOnInterrupt(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       FxRequest
    )
/*++
Routine Description:

    This routine pends the WaitOnInterrupt request.

Arguments:

    pDevice - the device context
    FxRequest - the framework request object

Return Value:

   None.

--*/
{
    PREQUEST_CONTEXT pRequest = GetRequestContext(FxRequest);
    NTSTATUS status = STATUS_SUCCESS;

    if (pDevice->WaitOnInterruptRequest == nullptr)
    {
        //
        // Mark request cancellable.
        //

        pRequest->FxDevice = pDevice->FxDevice;

        status = WdfRequestMarkCancelableEx(
            FxRequest,
            SpbPeripheralOnWaitOnInterruptCancel);

        if (!NT_SUCCESS(status))
        {
        }

        //
        // Pend the WaitOnInterrupt request.
        //

        if (NT_SUCCESS(status))
        {
            pDevice->WaitOnInterruptRequest = FxRequest;
        }
    }
    else
    {
        status = STATUS_INVALID_DEVICE_STATE;
    }

    if (!NT_SUCCESS(status))
    {
        WdfRequestComplete(FxRequest, status);
    }
}

VOID
SpbPeripheralOnWaitOnInterruptCancel(
    _In_  WDFREQUEST  FxRequest
    )
/*++
Routine Description:

    This event is called when the WaitOnInterrupt request is cancelled.

Arguments:

    FxRequest - the framework request object

Return Value:

   VOID

--*/
{
    PREQUEST_CONTEXT pRequest;
    PDEVICE_CONTEXT pDevice;

    pRequest = GetRequestContext(FxRequest);
    pDevice = GetDeviceContext(pRequest->FxDevice);

    //
    // Complete the request as cancelled
    //

    if (FxRequest == pDevice->WaitOnInterruptRequest)
    {    
        pDevice->WaitOnInterruptRequest = nullptr;
        WdfRequestComplete(FxRequest, STATUS_CANCELLED);
    }
    else
    {
        WdfRequestComplete(FxRequest, STATUS_CANCELLED);
    }
}

BOOLEAN
SpbPeripheralInterruptNotify(
    _In_  PDEVICE_CONTEXT  pDevice
    )
/*++
Routine Description:

    This routine completes the pending WaitOnInterrupt request.

Arguments:

    pDevice - the device context

Return Value:

   TRUE if notification sent, false otherwise.

--*/
{
    WDFREQUEST request;
    BOOLEAN fNotificationSent = FALSE;
    NTSTATUS status;

    if (pDevice->WaitOnInterruptRequest != nullptr)
    {
        //
        // Complete the WaitOnInterrupt request.
        //

        request = pDevice->WaitOnInterruptRequest;
        pDevice->WaitOnInterruptRequest = nullptr;

        status = WdfRequestUnmarkCancelable(request);

        if (NT_SUCCESS(status))
        {
            WdfRequestComplete(request, STATUS_SUCCESS);
            fNotificationSent = TRUE;
        }
        else if (status == STATUS_CANCELLED)
        {
        }
        else
        {
        }
    }
    else
    {
    }

    return fNotificationSent;
}

NTSTATUS
SpbPeripheralSendRequest(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  WDFREQUEST       SpbRequest,
    _In_  WDFREQUEST       ClientRequest
    )
/*++
 
  Routine Description:

    This routine sends a write-read sequence to the SPB controller.

  Arguments:

    pDevice - a pointer to the device context
    SpbRequest - the SPB request object
    ClientRequest - the client request object

  Return Value:

    Status

--*/
{
    PREQUEST_CONTEXT pRequest = GetRequestContext(ClientRequest);
    NTSTATUS status = STATUS_SUCCESS;

    //
    // Init client request context.
    //

    pRequest->FxDevice = pDevice->FxDevice;
    pRequest->IsSpbSequenceRequest = FALSE;
    pRequest->SequenceWriteLength = 0;

    //
    // Mark the client request as cancellable.
    //

    if (NT_SUCCESS(status))
    {
        status = WdfRequestMarkCancelableEx(
            ClientRequest,
            SpbPeripheralOnCancel);
    }

    //
    // Send the SPB request.
    //

    if (NT_SUCCESS(status))
    {
        WdfRequestSetCompletionRoutine(
            SpbRequest,
            SpbPeripheralOnCompletion,
            GetRequestContext(SpbRequest));

        BOOLEAN fSent = WdfRequestSend(
            SpbRequest,
            pDevice->SpbController,
            WDF_NO_SEND_OPTIONS);

        if (!fSent)
        {
            status = WdfRequestGetStatus(SpbRequest);

            NTSTATUS cancelStatus;
            cancelStatus = WdfRequestUnmarkCancelable(ClientRequest);

            if (!NT_SUCCESS(cancelStatus))
            {
                NT_ASSERTMSG("WdfRequestUnmarkCancelable should only fail if request has already been cancelled",
                    cancelStatus == STATUS_CANCELLED);
            }
        }
    }

    return status;
}

VOID 
SpbPeripheralOnCompletion(
    _In_  WDFREQUEST                      FxRequest,
    _In_  WDFIOTARGET                     FxTarget,
    _In_  PWDF_REQUEST_COMPLETION_PARAMS  Params,
    _In_  WDFCONTEXT                      Context
    )
/*++
 
  Routine Description:

    This routine is called when a request completes.

  Arguments:

    FxRequest - the framework request object
    FxTarget - the framework IO target object
    Params - a pointer to the request completion parameters
    Context - the request context

  Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(FxTarget);
    UNREFERENCED_PARAMETER(Context);
    
    PREQUEST_CONTEXT pRequest;
    PDEVICE_CONTEXT pDevice;
    NTSTATUS status;
    NTSTATUS cancelStatus;
    ULONG_PTR bytesCompleted;

    pRequest = GetRequestContext(FxRequest);
    pDevice = GetDeviceContext(pRequest->FxDevice);

    status = Params->IoStatus.Status;

    //
    // Unmark the client request as cancellable
    //

    cancelStatus = WdfRequestUnmarkCancelable(pDevice->ClientRequest);

    if (!NT_SUCCESS(cancelStatus))
    {
        NT_ASSERTMSG("WdfRequestUnmarkCancelable should only fail if request has already been cancelled",
            cancelStatus == STATUS_CANCELLED);
    }

    //
    // Complete the request pair
    //

    if (pRequest->IsSpbSequenceRequest == TRUE)
    {
        //
        // The client DeviceIoControl should only be
        // completed with bytesReturned and not total
        // bytes transferred.  Here we infer the number
        // of bytes read by substracting the write
        // length from the total.
        //

        bytesCompleted = 
            Params->IoStatus.Information < pRequest->SequenceWriteLength ?
            0 : Params->IoStatus.Information - pRequest->SequenceWriteLength;
    }
    else
    {
        bytesCompleted = Params->IoStatus.Information;
    }

    SpbPeripheralCompleteRequestPair(
        pDevice,
        status,
        bytesCompleted);
   
}

VOID
SpbPeripheralOnCancel(
    _In_  WDFREQUEST  FxRequest
    )
/*++
Routine Description:

    This event is called when the client request is cancelled.

Arguments:

    FxRequest - the framework request object

Return Value:

   VOID

--*/
{
    PREQUEST_CONTEXT pRequest;
    PDEVICE_CONTEXT pDevice;

    pRequest = GetRequestContext(FxRequest);
    pDevice = GetDeviceContext(pRequest->FxDevice);

    //
    // Attempt to cancel the SPB request
    //
    
    WdfRequestCancelSentRequest(pDevice->SpbRequest);
}

VOID
SpbPeripheralCompleteRequestPair(
    _In_  PDEVICE_CONTEXT  pDevice,
    _In_  NTSTATUS         status,
    _In_  ULONG_PTR        bytesCompleted
    )
/*++
Routine Description:

    This routine marks the SpbRequest as reuse
    and completes the client request.

Arguments:

    pDevice - the device context
    status - the client completion status
    bytesCompleted - the number of bytes completed
        for the client request

Return Value:

   VOID

--*/
{
    PREQUEST_CONTEXT pRequest;
    pRequest = GetRequestContext(pDevice->SpbRequest);

    //
    // Mark the SPB request as reuse
    //

    pRequest->IsSpbSequenceRequest = FALSE;
    pRequest->SequenceWriteLength = 0;
    
    WDF_REQUEST_REUSE_PARAMS params;
    WDF_REQUEST_REUSE_PARAMS_INIT(
        &params,
        WDF_REQUEST_REUSE_NO_FLAGS,
        STATUS_SUCCESS);

    WdfRequestReuse(pDevice->SpbRequest, &params);

    //
    // Complete the client request
    //

    if (pDevice->ClientRequest != nullptr)
    {
        WDFREQUEST clientRequest = pDevice->ClientRequest;
        pDevice->ClientRequest = nullptr;

        //
        // In order to satisfy SDV, assume clientRequest
        // is equal to pDevice->ClientRequest. This suppresses
        // a warning in the driver's cancellation path. 
        //
        // Typically when WdfRequestUnmarkCancelable returns 
        // STATUS_CANCELLED a driver does not go on to complete 
        // the request in that context. This sample, however, 
        // driver has handled this condition appropriately by 
        // not completing the cancelled request in its 
        // EvtRequestCancel callback. Developers should be 
        // cautious when copying code from this sample, paying 
        // close attention to the cancellation logic.
        //
        _Analysis_assume_(clientRequest == pDevice->ClientRequest);
        
        WdfRequestCompleteWithInformation(
            clientRequest,
            status,
            bytesCompleted);
    }
}
