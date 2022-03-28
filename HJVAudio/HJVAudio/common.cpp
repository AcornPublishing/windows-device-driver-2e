#pragma warning (disable : 4127)

#define PUT_GUIDS_HERE

#include "vaudio.h"
#include "common.h"

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

//=============================================================================
#pragma code_seg("PAGE")

//=============================================================================
CAdapterCommon::~CAdapterCommon
( 
    void 
)
{
    PAGED_CODE();

    if (m_pPortWave)
    {
        m_pPortWave->Release();
    }

    if (m_pServiceGroupWave)
    {
        m_pServiceGroupWave->Release();
    }
} // ~CAdapterCommon  

//=============================================================================
NTSTATUS
CAdapterCommon::Init
( 
    IN  PDEVICE_OBJECT          DeviceObject 
)
{
    NTSTATUS                    ntStatus = STATUS_SUCCESS;

	DbgPrint("CAdapterCommon::Init++\n");

    m_pDeviceObject = DeviceObject;
    m_PowerState    = PowerDeviceD0;

    // Initialize HW.
    // 
	DbgPrint("CAdapterCommon::Init--\n");

    return ntStatus;
} // Init

//=============================================================================
STDMETHODIMP
CAdapterCommon::NonDelegatingQueryInterface
( 
    REFIID                      Interface,
    PVOID *                     Object 
)
{
    PAGED_CODE();

	DbgPrint("CAdapterCommon::NonDelegatingQueryInterface++\n");

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PADAPTERCOMMON(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IAdapterCommon))
    {
        *Object = PVOID(PADAPTERCOMMON(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IAdapterPowerManagement))
    {
        *Object = PVOID(PADAPTERPOWERMANAGEMENT(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        PUNKNOWN(*Object)->AddRef();

		DbgPrint("CAdapterCommon::NonDelegatingQueryInterface--\n");
        return STATUS_SUCCESS;
    }

	DbgPrint("CAdapterCommon::NonDelegatingQueryInterface--\n");

    return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

//=============================================================================
STDMETHODIMP_(void)
CAdapterCommon::SetWaveServiceGroup
( 
    IN PSERVICEGROUP            ServiceGroup 
)
{
    PAGED_CODE();
    
	DbgPrint("CAdapterCommon::SetWaveServiceGroup++\n");

    if (m_pServiceGroupWave)
    {
        m_pServiceGroupWave->Release();
    }

    m_pServiceGroupWave = ServiceGroup;

    if (m_pServiceGroupWave)
    {
        m_pServiceGroupWave->AddRef();
    }

	DbgPrint("CAdapterCommon::SetWaveServiceGroup--\n");

} // SetWaveServiceGroup

STDMETHODIMP_(PUNKNOWN *)
CAdapterCommon::WavePortDriverDest
( 
    void 
)
{
	DbgPrint("CAdapterCommon::WavePortDriverDest\n");

    return (PUNKNOWN *)&m_pPortWave;
} // WavePortDriverDest
#pragma code_seg()

//=============================================================================
STDMETHODIMP_(void)
CAdapterCommon::PowerChangeState
( 
    IN  POWER_STATE             NewState 
)
{
    // is this actually a state change??
    //
    if (NewState.DeviceState != m_PowerState)
    {
        // switch on new state
        //
        switch (NewState.DeviceState)
        {
            case PowerDeviceD0:
            case PowerDeviceD1:
            case PowerDeviceD2:
            case PowerDeviceD3:
                m_PowerState = NewState.DeviceState;
	            break;
            default:
                break;
        }
    }
} // PowerStateChange

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CAdapterCommon::QueryDeviceCapabilities
( 
    IN  PDEVICE_CAPABILITIES    PowerDeviceCaps 
)
{
    UNREFERENCED_PARAMETER(PowerDeviceCaps);

	DbgPrint("CAdapterCommon::QueryDeviceCapabilities\n");

    return (STATUS_SUCCESS);
} // QueryDeviceCapabilities

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CAdapterCommon::QueryPowerChangeState
( 
    IN  POWER_STATE             NewStateQuery 
)
{
    UNREFERENCED_PARAMETER(NewStateQuery);

	DbgPrint("CAdapterCommon::QueryPowerChangeState\n");

    return STATUS_SUCCESS;
} // QueryPowerChangeState

//=============================================================================
CMiniportWaveCyclicVAUDIO::CMiniportWaveCyclicVAUDIO(PUNKNOWN pUnknownOuter) : CUnknown(pUnknownOuter)
{
    PAGED_CODE();

    // Initialize members.
    //

	DbgPrint("CMiniportWaveCyclicVAUDIO::CMiniportWaveCyclicVAUDIO\n");
    
	m_AdapterCommon = NULL;
    m_Port = NULL;
    m_FilterDescriptor = NULL;

    m_NotificationInterval = 0;
    m_SamplingFrequency = 0;

    m_ServiceGroup = NULL;
    m_MaxDmaBufferSize = DMA_BUFFER_SIZE;

//    m_MaxOutputStreams = 0;
    m_MaxInputStreams = 0;
    m_MaxTotalStreams = 0;

    m_MinChannels = 0;
    m_MaxChannelsPcm = 0;
    m_MinBitsPerSamplePcm = 0;
    m_MaxBitsPerSamplePcm = 0;
    m_MinSampleRatePcm = 0;
    m_MaxSampleRatePcm = 0;
} // CMiniportWaveCyclicVAUDIO

//=============================================================================
CMiniportWaveCyclicVAUDIO::~CMiniportWaveCyclicVAUDIO
(
    void
)
{
    PAGED_CODE();

    if (m_Port)
    {
        m_Port->Release();
    }

    if (m_ServiceGroup)
    {
        m_ServiceGroup->Release();
    }

    if (m_AdapterCommon)
    {
        m_AdapterCommon->Release();
    }
} // ~CMiniportWaveCyclicVAUDIO

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicVAUDIO::GetDescription
(
    OUT PPCFILTER_DESCRIPTOR * OutFilterDescriptor
)
{
    *OutFilterDescriptor = m_FilterDescriptor;

	DbgPrint("CMiniportWaveCyclicVAUDIO::GetDescription\n");

    return (STATUS_SUCCESS);
} // GetDescription

//=============================================================================
NTSTATUS
CMiniportWaveCyclicVAUDIO::PropertyHandlerCpuResources
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	DbgPrint("CMiniportWaveCyclicVAUDIO::PropertyHandlerCpuResources++\n");

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(LONG), 0);
        if (NT_SUCCESS(ntStatus))
        {
            *(PLONG(PropertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
            PropertyRequest->ValueSize = sizeof(LONG);
            ntStatus = STATUS_SUCCESS;
        }
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus =
            PropertyHandler_BasicSupport
            (
                PropertyRequest,
                KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
                VT_I4
            );
    }

	DbgPrint("CMiniportWaveCyclicVAUDIO::PropertyHandlerCpuResources--\n");
    return ntStatus;
} // PropertyHandlerCpuResources

//=============================================================================
NTSTATUS
CMiniportWaveCyclicVAUDIO::PropertyHandlerGeneric
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	DbgPrint("CMiniportWaveCyclicVAUDIO::PropertyHandlerGeneric++\n");

    switch (PropertyRequest->PropertyItem->Id)
    {
        case KSPROPERTY_AUDIO_CHANNEL_CONFIG:
            // This miniport will handle channel config property
            // requests.
            ntStatus = PropertyHandlerChannelConfig(PropertyRequest);
            break;
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = PropertyHandlerCpuResources(PropertyRequest);
            break;
        default:
            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    }

	DbgPrint("CMiniportWaveCyclicVAUDIO::PropertyHandlerGeneric--\n");

    return ntStatus;
} // PropertyHandlerGeneric

//-----------------------------------------------------------------------------
NTSTATUS
CMiniportWaveCyclicVAUDIO::ValidatePcm
(
    IN  PWAVEFORMATEX           pWfx
)
{
    PAGED_CODE();

	DbgPrint("CMiniportWaveCyclicVAUDIO::ValidatePcm\n");

    if
    (
        pWfx                                                &&
        (pWfx->cbSize == 0)                                 &&
        (pWfx->nChannels >= m_MinChannels)                  &&
        (pWfx->nChannels <= m_MaxChannelsPcm)               &&
        (pWfx->nSamplesPerSec >= m_MinSampleRatePcm)        &&
        (pWfx->nSamplesPerSec <= m_MaxSampleRatePcm)        &&
        (pWfx->wBitsPerSample >= m_MinBitsPerSamplePcm)     &&
        (pWfx->wBitsPerSample <= m_MaxBitsPerSamplePcm)
    )
    {
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
} // ValidatePcm

//=============================================================================
// CMiniportWaveCyclicStreamVAUDIO
//=============================================================================

CMiniportWaveCyclicStreamVAUDIO::CMiniportWaveCyclicStreamVAUDIO(PUNKNOWN pUnknownOuter) : CUnknown(pUnknownOuter)
{
    PAGED_CODE();

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::CMiniportWaveCyclicStreamVAUDIO\n");

    m_pMiniport = NULL;
    m_fCapture = FALSE;
    m_fFormat16Bit = FALSE;
    m_usBlockAlign = 0;
    m_ksState = KSSTATE_STOP;
    m_ulPin = (ULONG)-1;

    m_pDpc = NULL;
    m_pTimer = NULL;

    m_fDmaActive = FALSE;
    m_ulDmaPosition = 0;
    m_ullElapsedTimeCarryForward = 0;
    m_ulByteDisplacementCarryForward = 0;
    m_pvDmaBuffer = NULL;
    m_ulDmaBufferSize = 0;
    m_ulDmaMovementRate = 0;
    m_ullDmaTimeStamp = 0;
}

//=============================================================================
CMiniportWaveCyclicStreamVAUDIO::~CMiniportWaveCyclicStreamVAUDIO
(
    void
)
{
    PAGED_CODE();

    if (m_pTimer)
    {
        KeCancelTimer(m_pTimer);
        ExFreePoolWithTag(m_pTimer, VAUDIO_POOLTAG);
    }

    if (m_pDpc)
    {
        ExFreePoolWithTag( m_pDpc, VAUDIO_POOLTAG );
    }

    // Free the DMA buffer
    //
    FreeBuffer();

    if (NULL != m_pMiniport)
    {
        if (!m_fCapture)
        {
            m_pMiniport->m_fRenderAllocated = FALSE;
        }
    }

} // ~CMiniportWaveCyclicStreamVAUDIO

//=============================================================================
NTSTATUS
CMiniportWaveCyclicStreamVAUDIO::Init
(
    IN  PCMiniportWaveCyclicVAUDIO Miniport_,
    IN  ULONG                   Pin_,
    IN  BOOLEAN                 Capture_,
    IN  PKSDATAFORMAT           DataFormat_
)
{
    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    PWAVEFORMATEX               pWfx;

//    m_pMiniportLocal = Miniport_;
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::Init++\n");


    pWfx = GetWaveFormatEx(DataFormat_);
    if (!pWfx)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(ntStatus))
    {
        m_pMiniport = Miniport_;

        m_ulPin                           = Pin_;
        m_fCapture                        = Capture_;
        m_usBlockAlign                    = pWfx->nBlockAlign;
        m_fFormat16Bit                    = (pWfx->wBitsPerSample == 16);
        m_ksState                         = KSSTATE_STOP;
        m_ulDmaPosition                   = 0;
        m_ullElapsedTimeCarryForward      = 0;
        m_ulByteDisplacementCarryForward  = 0;
        m_fDmaActive                      = FALSE;
        m_pDpc                            = NULL;
        m_pTimer                          = NULL;
        m_pvDmaBuffer                     = NULL;
    }

    // Allocate DMA buffer for this stream.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = AllocateBuffer(m_pMiniport->m_MaxDmaBufferSize, NULL);
    }

    // Set sample frequency. Note that m_SampleRateSync access should
    // be syncronized.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus =
            KeWaitForSingleObject
            (
                &m_pMiniport->m_SampleRateSync,
                Executive,
                KernelMode,
                FALSE,
                NULL
            );
        if (STATUS_SUCCESS == ntStatus)
        {
            m_pMiniport->m_SamplingFrequency = pWfx->nSamplesPerSec;
            KeReleaseMutex(&m_pMiniport->m_SampleRateSync, FALSE);
        }
        else
        {
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = SetFormat(DataFormat_);
    }

    if (NT_SUCCESS(ntStatus))
    {
        m_pDpc = (PRKDPC)
            ExAllocatePoolWithTag
            (
                NonPagedPool,
                sizeof(KDPC),
                VAUDIO_POOLTAG
            );
        if (!m_pDpc)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        m_pTimer = (PKTIMER)
            ExAllocatePoolWithTag
            (
                NonPagedPool,
                sizeof(KTIMER),
                VAUDIO_POOLTAG
            );
        if (!m_pTimer)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        KeInitializeDpc(m_pDpc, TimerNotify, m_pMiniport);
        KeInitializeTimerEx(m_pTimer, NotificationTimer);
    }

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::Init--\n");

    return ntStatus;
} // Init

#pragma code_seg()

//=============================================================================
// CMiniportWaveCyclicStreamVAUDIO IMiniportWaveCyclicStream
//=============================================================================

STDMETHODIMP
CMiniportWaveCyclicStreamVAUDIO::GetPosition
(
    OUT PULONG                  Position
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::GetPosition++\n");

    if (m_fDmaActive)
    {
        // Get the current time
        //
        ULONGLONG CurrentTime = KeQueryInterruptTime();

        ULONG TimeElapsedInMS =
            ( (ULONG) (CurrentTime - m_ullDmaTimeStamp + m_ullElapsedTimeCarryForward) ) / 10000;

        m_ullElapsedTimeCarryForward = 
            (CurrentTime - m_ullDmaTimeStamp + m_ullElapsedTimeCarryForward) % 10000;

        ULONG ByteDisplacement =
            ((m_ulDmaMovementRate * TimeElapsedInMS) + m_ulByteDisplacementCarryForward) / 1000;

        m_ulByteDisplacementCarryForward = (
            (m_ulDmaMovementRate * TimeElapsedInMS) + m_ulByteDisplacementCarryForward) % 1000;

        m_ulDmaPosition =
            (m_ulDmaPosition + ByteDisplacement) % m_ulDmaBufferSize;

        *Position = m_ulDmaPosition;

        m_ullDmaTimeStamp = CurrentTime;
    }
    else
    {
        *Position = m_ulDmaPosition;
    }

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::GetPosition--\n");

    return STATUS_SUCCESS;
} // GetPosition

//=============================================================================
STDMETHODIMP
CMiniportWaveCyclicStreamVAUDIO::NormalizePhysicalPosition
(
    IN OUT PLONGLONG            PhysicalPosition
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::NormalizePhysicalPosition\n");

    *PhysicalPosition =
        ( _100NS_UNITS_PER_SECOND / m_usBlockAlign * *PhysicalPosition ) /
        m_pMiniport->m_SamplingFrequency;

    return STATUS_SUCCESS;
} // NormalizePhysicalPosition

#pragma code_seg("PAGE")
//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicStreamVAUDIO::SetFormat
(
    IN  PKSDATAFORMAT           Format
)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    PWAVEFORMATEX               pWfx;

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::SetFormat++\n");

    if (m_ksState != KSSTATE_RUN)
    {
        // VAUDIO does not validate the format.
        //
        pWfx = GetWaveFormatEx(Format);
        if (pWfx)
        {
            ntStatus =
                KeWaitForSingleObject
                (
                    &m_pMiniport->m_SampleRateSync,
                    Executive,
                    KernelMode,
                    FALSE,
                    NULL
                );
            if (STATUS_SUCCESS == ntStatus)
            {
                m_usBlockAlign = pWfx->nBlockAlign;
                m_fFormat16Bit = (pWfx->wBitsPerSample == 16);
                m_pMiniport->m_SamplingFrequency =
                    pWfx->nSamplesPerSec;
                m_ulDmaMovementRate = pWfx->nAvgBytesPerSec;

            }

            KeReleaseMutex(&m_pMiniport->m_SampleRateSync, FALSE);
        }
    }

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::SetFormat--\n");

    return ntStatus;
} // SetFormat

//=============================================================================
STDMETHODIMP_(ULONG)
CMiniportWaveCyclicStreamVAUDIO::SetNotificationFreq
(
    IN  ULONG                   Interval,
    OUT PULONG                  FramingSize
)
{
    PAGED_CODE();

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::SetNotificationFreq\n");

    m_pMiniport->m_NotificationInterval = Interval;

    *FramingSize =
        m_usBlockAlign *
        m_pMiniport->m_SamplingFrequency *
        Interval / 1000;

    return m_pMiniport->m_NotificationInterval;
} // SetNotificationFreq

//=============================================================================
STDMETHODIMP
CMiniportWaveCyclicStreamVAUDIO::SetState
(
    IN  KSSTATE                 NewState
)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_SUCCESS;

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::SetState++\n");

    // The acquire state is not distinguishable from the stop state for our
    // purposes.
    //
    if (NewState == KSSTATE_ACQUIRE)
    {
        NewState = KSSTATE_STOP;
    }

    if (m_ksState != NewState)
    {
        switch(NewState)
        {
            case KSSTATE_PAUSE:
            {
				DbgPrint("KSSTATE_PAUSE\n");
                m_fDmaActive = FALSE;
            }
            break;

            case KSSTATE_RUN:
            {
                 LARGE_INTEGER   delay;

                // Set the timer for DPC.
                //
				DbgPrint("KSSTATE_RUN\n");

                m_ullDmaTimeStamp             = KeQueryInterruptTime();
                m_ullElapsedTimeCarryForward  = 0;
                m_fDmaActive                  = TRUE;
                delay.HighPart                = 0;
                delay.LowPart                 = m_pMiniport->m_NotificationInterval;

                KeSetTimerEx
                (
                    m_pTimer,
                    delay,
                    m_pMiniport->m_NotificationInterval,
                    m_pDpc
                );
            }
            break;

        case KSSTATE_STOP:
			DbgPrint("KSSTATE_STOP\n");


            m_fDmaActive                      = FALSE;
            m_ulDmaPosition                   = 0;
            m_ullElapsedTimeCarryForward      = 0;
            m_ulByteDisplacementCarryForward  = 0;

            KeCancelTimer( m_pTimer );
            break;
        }

        m_ksState = NewState;
    }

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::SetState--\n");

    return ntStatus;
} // SetState

#pragma code_seg()

//=============================================================================
STDMETHODIMP_(void)
CMiniportWaveCyclicStreamVAUDIO::Silence
(
    __out_bcount(ByteCount) PVOID Buffer,
    IN ULONG                    ByteCount
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::Silence\n");

    RtlFillMemory(Buffer, ByteCount, m_fFormat16Bit ? 0 : 0x80);
} // Silence

STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicStreamVAUDIO::NonDelegatingQueryInterface
( 
    IN  REFIID  Interface,
    OUT PVOID * Object 
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::NonDelegatingQueryInterface\n");

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLICSTREAM(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclicStream))
    {
        *Object = PVOID(PMINIPORTWAVECYCLICSTREAM(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IDmaChannel))
    {
        *Object = PVOID(PDMACHANNEL(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface


// ----------------------------------------------------------
/*
CMiniportTopologyVAUDIO 클래스가 가져야 하는 함수들은 다음과 같다
[COM 필수함수]
    NTSTATUS                    Init
    ( 
        IN  PUNKNOWN            UnknownAdapter,
        IN	PRESOURCELIST		ResourceList,
        IN  PPORTTOPOLOGY       Port_ 
    );

    NTSTATUS                    GetDescription
    (   
        OUT PPCFILTER_DESCRIPTOR *  Description
    );

    NTSTATUS                    DataRangeIntersection
    (   
        IN  ULONG               PinId,
        IN  PKSDATARANGE        ClientDataRange,
        IN  PKSDATARANGE        MyDataRange,
        IN  ULONG               OutputBufferLength,
        OUT PVOID               ResultantFormat OPTIONAL,
        OUT PULONG              ResultantFormatLength
    );

[선택함수]
    CMiniportTopologyVAUDIO();
    ~CMiniportTopologyVAUDIO();
    NTSTATUS                    PropertyHandlerBasicSupportVolume
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );
    
    NTSTATUS                    PropertyHandlerCpuResources
    ( 
        IN  PPCPROPERTY_REQUEST PropertyRequest 
    );

    NTSTATUS                    PropertyHandlerGeneric
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerMute
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerVolume
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );
*/

// 선택함수
/*
CMiniportTopologyVAUDIO::CMiniportTopologyVAUDIO
(
    void
)
{
    m_AdapterCommon = NULL;
    m_FilterDescriptor = NULL;
} // CMiniportTopologyVAUDIO
*/

// 선택함수
CMiniportTopologyVAUDIO::~CMiniportTopologyVAUDIO
(
    void
)
{
    if (m_AdapterCommon)
    {
        m_AdapterCommon->Release();
    }
} // ~CMiniportTopologyVAUDIO

// COM 필수함수
// 현재샘플에서는 지원하지 않도록 리턴값을 사용한다
NTSTATUS
CMiniportTopologyVAUDIO::DataRangeIntersection
( 
    IN  ULONG                   PinId,
    IN  PKSDATARANGE            ClientDataRange,
    IN  PKSDATARANGE            MyDataRange,
    IN  ULONG                   OutputBufferLength,
    OUT PVOID                   ResultantFormat     OPTIONAL,
    OUT PULONG                  ResultantFormatLength 
)
{
    UNREFERENCED_PARAMETER(PinId);
    UNREFERENCED_PARAMETER(ClientDataRange);
    UNREFERENCED_PARAMETER(MyDataRange);
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(ResultantFormat);
    UNREFERENCED_PARAMETER(ResultantFormatLength);

	DbgPrint("CMiniportTopologyVAUDIO::DataRangeIntersection\n");

    return (STATUS_NOT_IMPLEMENTED);
} // DataRangeIntersection

// COM 필수함수
NTSTATUS
CMiniportTopologyVAUDIO::GetDescription
( 
    OUT PPCFILTER_DESCRIPTOR *  OutFilterDescriptor 
)
{
    *OutFilterDescriptor = m_FilterDescriptor;

	DbgPrint("CMiniportTopologyVAUDIO::GetDescription\n");

    return (STATUS_SUCCESS);
} // GetDescription

// COM 필수함수
NTSTATUS
CMiniportTopologyVAUDIO::Init
( 
    IN  PUNKNOWN                UnknownAdapter_,
    IN	PRESOURCELIST			ResourceList,
    IN  PPORTTOPOLOGY           Port_ 
)
{
    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(Port_);

    NTSTATUS                    ntStatus;

	DbgPrint("CMiniportTopologyVAUDIO::Init ++\n");

    ntStatus = 
        UnknownAdapter_->QueryInterface
        ( 
            IID_IAdapterCommon,
            (PVOID *) &m_AdapterCommon
        );
    if (NT_SUCCESS(ntStatus))
    {
        m_FilterDescriptor = &MiniportFilterDescriptor;
    }

    if (!NT_SUCCESS(ntStatus))
    {
        // clean up AdapterCommon
        if (m_AdapterCommon)
        {
            m_AdapterCommon->Release();
            m_AdapterCommon = NULL;
        }
    }

	DbgPrint("CMiniportTopologyVAUDIO::Init --\n");

    return ntStatus;
} // Init

// 선택함수
// Volume Node를 향한 BasicSupport Property Request를 처리하는 함수
NTSTATUS
CMiniportTopologyVAUDIO::PropertyHandlerBasicSupportVolume
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
{
    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    ULONG                       cbFullProperty = 
        sizeof(KSPROPERTY_DESCRIPTION) +
        sizeof(KSPROPERTY_MEMBERSHEADER) +
        sizeof(KSPROPERTY_STEPPING_LONG);

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerBasicSupportVolume ++\n");

    if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
    {
        PKSPROPERTY_DESCRIPTION PropDesc = 
            PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

        PropDesc->AccessFlags       = KSPROPERTY_TYPE_ALL;
        PropDesc->DescriptionSize   = cbFullProperty;
        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
        PropDesc->PropTypeSet.Id    = VT_I4;
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 1;
        PropDesc->Reserved          = 0;

        // if return buffer can also hold a range description, return it too
        if(PropertyRequest->ValueSize >= cbFullProperty)
        {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER Members = 
                PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

            Members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            Members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            Members->MembersCount   = 1;
            Members->Flags          = KSPROPERTY_MEMBER_FLAG_BASICSUPPORT_MULTICHANNEL;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG Range = 
                PKSPROPERTY_STEPPING_LONG(Members + 1);

            Range->Bounds.SignedMaximum = 0x00000000;      //   0 dB
            Range->Bounds.SignedMinimum = -96 * 0x10000;   // -96 dB
            Range->SteppingDelta        = 0x08000;         //  .5 dB
            Range->Reserved             = 0;

            // set the return value size
            PropertyRequest->ValueSize = cbFullProperty;
        } 
        else
        {
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        }
    } 
    else if(PropertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        PULONG AccessFlags = PULONG(PropertyRequest->Value);

        PropertyRequest->ValueSize = sizeof(ULONG);
        *AccessFlags = KSPROPERTY_TYPE_ALL;
    }
    else
    {
        PropertyRequest->ValueSize = 0;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerBasicSupportVolume --\n");

    return ntStatus;
} // PropertyHandlerBasicSupportVolume

// 선택함수
// 모든 노드로 전달되는 Property Request는 이곳에서 분기된다
NTSTATUS                            
CMiniportTopologyVAUDIO::PropertyHandlerGeneric
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
{
    NTSTATUS                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerGeneric ++\n");

    switch (PropertyRequest->PropertyItem->Id)
    {
        case KSPROPERTY_AUDIO_VOLUMELEVEL:
            ntStatus = PropertyHandlerVolume(PropertyRequest);
            break;
        
        case KSPROPERTY_AUDIO_CPU_RESOURCES:
            ntStatus = PropertyHandlerCpuResources(PropertyRequest);
            break;

        case KSPROPERTY_AUDIO_MUTE:
            ntStatus = PropertyHandlerMute(PropertyRequest);
            break;

        default:
            break;
    }

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerGeneric --\n");

    return ntStatus;
} // PropertyHandlerGeneric

// 선택함수
// 모든 노드로 전달되는 Property Request중에서 CpuResource관련 명령어를 일반적인 방법으로 처리하는 함수
NTSTATUS
CMiniportTopologyVAUDIO::PropertyHandlerCpuResources
( 
    IN  PPCPROPERTY_REQUEST     PropertyRequest 
)
{
    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerCpuResources ++\n");

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(ULONG));
        if (NT_SUCCESS(ntStatus))
        {
            *(PLONG(PropertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
            PropertyRequest->ValueSize = sizeof(LONG);
        }
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = 
            PropertyHandler_BasicSupport
            ( 
                PropertyRequest, 
                KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
                VT_ILLEGAL
            );
    }

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerCpuResources --\n");
    return ntStatus;
} // PropertyHandlerCpuResources

// 선택함수
// Mute노드로 전달되는 Property Request를 처리하는 함수
NTSTATUS                            
CMiniportTopologyVAUDIO::PropertyHandlerMute
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
{
    NTSTATUS                    ntStatus;
    LONG                        lChannel;
    PBOOL                       pfMute;

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerMute ++\n");

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = 
            PropertyHandler_BasicSupport
            (
                PropertyRequest,
                KSPROPERTY_TYPE_ALL,
                VT_BOOL
            );
    }
    else
    {
        ntStatus = 
            ValidatePropertyParams
            (   
                PropertyRequest, 
                sizeof(BOOL), 
                sizeof(LONG)
            );
        if (NT_SUCCESS(ntStatus))
        {
            lChannel = * PLONG (PropertyRequest->Instance);
            pfMute   = PBOOL (PropertyRequest->Value);

            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *pfMute = 0; // 임의로..
				DbgPrint("[GET] mute = 0x%8X\n", *pfMute );

                PropertyRequest->ValueSize = sizeof(BOOL);
            }
            else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
				DbgPrint("[SET] mute = 0x%8X\n", *pfMute );
                // *pfMute 을 적용한다
            }
        }
        else
        {
        }
    }

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerMute --\n");

    return ntStatus;
} // PropertyHandlerMute

// 선택함수
// Volume노드로 전달되는 Property Request를 처리하는 함수
NTSTATUS
CMiniportTopologyVAUDIO::PropertyHandlerVolume
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest     
)
{
    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    LONG     lChannel;
    PULONG   pulVolume;

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerVolume ++\n");

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
    {
        ntStatus = PropertyHandlerBasicSupportVolume(PropertyRequest);
    }
    else
    {
        ntStatus = 
            ValidatePropertyParams
            (
                PropertyRequest, 
                sizeof(ULONG),  // volume value is a ULONG
                sizeof(LONG)    // instance is the channel number
            );
        if (NT_SUCCESS(ntStatus))
        {
            lChannel = * (PLONG (PropertyRequest->Instance));
            pulVolume = PULONG (PropertyRequest->Value);

            if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
            {
                *pulVolume = 0; // 임의로 최대값을 줘보자
				DbgPrint("[GET] volume = 0x%8X\n", *pulVolume );
                PropertyRequest->ValueSize = sizeof(ULONG);                
            }
            else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
            {
				DbgPrint("[SET] volume = 0x%8X\n", *pulVolume );
				// lChannel, *pulVolume 를 사용한다
            }
        }
        else
        {
        }
    }

	DbgPrint("CMiniportTopologyVAUDIO::PropertyHandlerVolume --\n");

    return ntStatus;
} // PropertyHandlerVolume

//=============================================================================

// COM 인터페이스약속상 필요한 함수
STDMETHODIMP
CMiniportTopologyVAUDIO::NonDelegatingQueryInterface
( 
    IN  REFIID                  Interface,
    OUT PVOID                   * Object 
)
{
    PAGED_CODE();

    ASSERT(Object);

	DbgPrint("CMiniportTopologyVAUDIO::NonDelegatingQueryInterface\n");

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportTopology))
    {
        *Object = PVOID(PMINIPORTTOPOLOGY(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        // We reference the interface for the caller.
        PUNKNOWN(*Object)->AddRef();
        return(STATUS_SUCCESS);
    }

    return(STATUS_INVALID_PARAMETER);
} // NonDelegatingQueryInterface

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicVAUDIO::DataRangeIntersection
( 
    IN  ULONG                       PinId,
    IN  PKSDATARANGE                ClientDataRange,
    IN  PKSDATARANGE                MyDataRange,
    IN  ULONG                       OutputBufferLength,
    OUT PVOID                       ResultantFormat,
    OUT PULONG                      ResultantFormatLength 
)
{
    UNREFERENCED_PARAMETER(PinId);

    PAGED_CODE();

	DbgPrint("CMiniportWaveCyclicVAUDIO::DataRangeIntersection++\n");

    // Check the size of output buffer. Note that we are returning
    // WAVEFORMATPCMEX.
    //
    if (!OutputBufferLength) 
    {
        *ResultantFormatLength = sizeof(KSDATAFORMAT) + sizeof(WAVEFORMATPCMEX);
		DbgPrint("CMiniportWaveCyclicVAUDIO::DataRangeIntersection--\n");

        return STATUS_BUFFER_OVERFLOW;
    } 
    
    if (OutputBufferLength < (sizeof(KSDATAFORMAT) + sizeof(WAVEFORMATPCMEX))) 
    {
		DbgPrint("CMiniportWaveCyclicVAUDIO::DataRangeIntersection--\n");
        return STATUS_BUFFER_TOO_SMALL;
    }

    // Fill in the structure the datarange structure.
    //
    RtlCopyMemory(ResultantFormat, MyDataRange, sizeof(KSDATAFORMAT));

    // Modify the size of the data format structure to fit the WAVEFORMATPCMEX
    // structure.
    //
    ((PKSDATAFORMAT)ResultantFormat)->FormatSize =
        sizeof(KSDATAFORMAT) + sizeof(WAVEFORMATPCMEX);

    // Append the WAVEFORMATPCMEX structure
    //
    PWAVEFORMATPCMEX pWfxExt = 
        (PWAVEFORMATPCMEX)((PKSDATAFORMAT)ResultantFormat + 1);

    // Ensure that the returned channel count falls within our range of
    // supported channel counts.
    pWfxExt->Format.nChannels = 
        (WORD)min(((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumChannels, 
                  ((PKSDATARANGE_AUDIO) MyDataRange)->MaximumChannels);


    // Ensure that the returned sample rate falls within the supported range
    // of sample rates from our data range.
    if((((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumSampleFrequency <
        ((PKSDATARANGE_AUDIO) MyDataRange)->MinimumSampleFrequency) ||
       (((PKSDATARANGE_AUDIO) ClientDataRange)->MinimumSampleFrequency >
        ((PKSDATARANGE_AUDIO) MyDataRange)->MaximumSampleFrequency))
    {
		DbgPrint("CMiniportWaveCyclicVAUDIO::DataRangeIntersection--\n");
        return STATUS_NO_MATCH;
    }
    pWfxExt->Format.nSamplesPerSec = 
        min(((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumSampleFrequency,
            ((PKSDATARANGE_AUDIO) MyDataRange)->MaximumSampleFrequency);

    // Ensure that the returned bits per sample is in the supported
    // range of bit depths from our data range.
    if((((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumBitsPerSample <
        ((PKSDATARANGE_AUDIO) MyDataRange)->MinimumBitsPerSample) ||
       (((PKSDATARANGE_AUDIO) ClientDataRange)->MinimumBitsPerSample >
        ((PKSDATARANGE_AUDIO) MyDataRange)->MaximumBitsPerSample))
    {
		DbgPrint("CMiniportWaveCyclicVAUDIO::DataRangeIntersection--\n");
        return STATUS_NO_MATCH;
    }
    pWfxExt->Format.wBitsPerSample = 
        (WORD)min(((PKSDATARANGE_AUDIO) ClientDataRange)->MaximumBitsPerSample,
                  ((PKSDATARANGE_AUDIO) MyDataRange)->MaximumBitsPerSample);

    // Fill in the rest of the format
    pWfxExt->Format.nBlockAlign = 
        (pWfxExt->Format.wBitsPerSample * pWfxExt->Format.nChannels) / 8;
    pWfxExt->Format.nAvgBytesPerSec = 
        pWfxExt->Format.nSamplesPerSec * pWfxExt->Format.nBlockAlign;
    pWfxExt->Format.cbSize = 22;
    pWfxExt->Samples.wValidBitsPerSample = pWfxExt->Format.wBitsPerSample;
    pWfxExt->SubFormat = KSDATAFORMAT_SUBTYPE_PCM; //KSDATAFORMAT_SUBTYPE_PCM;
    pWfxExt->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;

    // Determine the appropriate channel config to use.
    switch(pWfxExt->Format.nChannels)
    {
        case 1:
            pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_MONO;
            break;
        case 2:
            pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_STEREO;
            break;
        case 4:
            // Since there are two 4-channel speaker configs, make sure
            // the appropriate one is used.  If neither is set, default
            // to KSAUDIO_SPEAKER_QUAD.
            if(m_ChannelConfig.ActiveSpeakerPositions == KSAUDIO_SPEAKER_SURROUND)
            {
                pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_SURROUND;
            }
            else
            {
                pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_QUAD;
            }
            break;
        case 6:
            // Since there are two 6-channel speaker configs, make sure
            // the appropriate one is used.  If neither is set, default
            // to KSAUDIO_SPEAKER_5PIONT1_SURROUND.
            if(m_ChannelConfig.ActiveSpeakerPositions == KSAUDIO_SPEAKER_5POINT1)
            {
                pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_5POINT1;
            }
            else
            {
                pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_5POINT1_SURROUND;
            }
            break;
        case 8:
            // Since there are two 8-channel speaker configs, make sure
            // the appropriate one is used.  If neither is set, default
            // to KSAUDIO_SPEAKER_7POINT1_SURROUND.
            if(m_ChannelConfig.ActiveSpeakerPositions == KSAUDIO_SPEAKER_7POINT1)
            {
                pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_7POINT1;
            }
            else
            {
                pWfxExt->dwChannelMask = KSAUDIO_SPEAKER_7POINT1_SURROUND;
            }
            break;
        default:
            // Unsupported channel count.
			DbgPrint("CMiniportWaveCyclicVAUDIO::DataRangeIntersection--\n");
            return STATUS_NO_MATCH;
    }
    
    // Now overwrite also the sample size in the ksdataformat structure.
    ((PKSDATAFORMAT)ResultantFormat)->SampleSize = pWfxExt->Format.nBlockAlign;
    
    // That we will return.
    //
    *ResultantFormatLength = sizeof(KSDATAFORMAT) + sizeof(WAVEFORMATPCMEX);
    
	DbgPrint("CMiniportWaveCyclicVAUDIO::DataRangeIntersection--\n");
    return STATUS_SUCCESS;
} // DataRangeIntersection

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicVAUDIO::Init
( 
    IN  PUNKNOWN                UnknownAdapter_,
    IN  PRESOURCELIST           ResourceList_,
    IN  PPORTWAVECYCLIC         Port_ 
)
{
    UNREFERENCED_PARAMETER(ResourceList_);
    PAGED_CODE();

    NTSTATUS                    ntStatus;

	DbgPrint("CMiniportWaveCyclicVAUDIO::Init++\n");

//    m_MaxOutputStreams                      = MAX_OUTPUT_STREAMS;
    m_MaxInputStreams                       = MAX_INPUT_STREAMS;
    m_MaxTotalStreams                       = MAX_TOTAL_STREAMS;

    m_MinChannels                           = MIN_CHANNELS;
    m_MaxChannelsPcm                        = MAX_CHANNELS_PCM;

    m_MinBitsPerSamplePcm                   = MIN_BITS_PER_SAMPLE_PCM;
    m_MaxBitsPerSamplePcm                   = MAX_BITS_PER_SAMPLE_PCM;
    m_MinSampleRatePcm                      = MIN_SAMPLE_RATE;
    m_MaxSampleRatePcm                      = MAX_SAMPLE_RATE;

    m_ChannelConfig.ActiveSpeakerPositions  = KSAUDIO_SPEAKER_STEREO;

    m_Port = Port_;
    m_Port->AddRef();

    ntStatus =
        UnknownAdapter_->QueryInterface
        (
            IID_IAdapterCommon,
            (PVOID *) &m_AdapterCommon
        );

    if (NT_SUCCESS(ntStatus))
    {
        KeInitializeMutex(&m_SampleRateSync, 1);
        ntStatus = PcNewServiceGroup(&m_ServiceGroup, NULL);

        if (NT_SUCCESS(ntStatus))
        {
            m_AdapterCommon->SetWaveServiceGroup(m_ServiceGroup);
        }
    }

    if (!NT_SUCCESS(ntStatus))
    {
        // clean up AdapterCommon
        //
        if (m_AdapterCommon)
        {
            // clean up the service group
            //
            if (m_ServiceGroup)
            {
                m_AdapterCommon->SetWaveServiceGroup(NULL);
                m_ServiceGroup->Release();
                m_ServiceGroup = NULL;
            }

            m_AdapterCommon->Release();
            m_AdapterCommon = NULL;
        }

        // release the port
        //
        m_Port->Release();
        m_Port = NULL;
    }

    if (NT_SUCCESS(ntStatus))
    {
        // Set filter descriptor.
        m_FilterDescriptor = &WavMiniportFilterDescriptor;
        m_fRenderAllocated = FALSE;
    }

	DbgPrint("CMiniportWaveCyclicVAUDIO::Init--\n");

    return ntStatus;
} // Init

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicVAUDIO::NewStream
( 
    OUT PMINIPORTWAVECYCLICSTREAM  * OutStream,
    IN  PUNKNOWN                OuterUnknown,
    IN  POOL_TYPE               PoolType,
    IN  ULONG                   Pin,
    IN  BOOLEAN                 Capture,
    IN  PKSDATAFORMAT           DataFormat,
    OUT PDMACHANNEL *           OutDmaChannel,
    OUT PSERVICEGROUP *         OutServiceGroup 
)
{
    UNREFERENCED_PARAMETER(PoolType);

    NTSTATUS                    ntStatus = STATUS_SUCCESS;
    PCMiniportWaveCyclicStreamVAUDIO  stream = NULL;

	DbgPrint("CMiniportWaveCyclicVAUDIO::NewStream++\n");

    // Check if we have enough streams.
    if (Capture)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        if (m_fRenderAllocated)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    // Determine if the format is valid.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = ValidateFormat(DataFormat);
    }

    // Instantiate a stream. Stream must be in
    // NonPagedPool because of file saving.
    //
    if (NT_SUCCESS(ntStatus))
    {
        stream = new (NonPagedPool, VAUDIO_POOLTAG) 
            CMiniportWaveCyclicStreamVAUDIO(OuterUnknown);

        if (stream)
        {
            stream->AddRef();

            ntStatus = 
                stream->Init
                ( 
                    this,
                    Pin,
                    Capture,
                    DataFormat
                );
        }
        else
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
        if (Capture)
        {
            ntStatus = STATUS_INSUFFICIENT_RESOURCES;
	        stream->Release();
			return ntStatus;
        }
        else
        {
            m_fRenderAllocated = TRUE;
        }

        *OutStream = PMINIPORTWAVECYCLICSTREAM(stream);
        (*OutStream)->AddRef();
        
        *OutDmaChannel = PDMACHANNEL(stream);
        (*OutDmaChannel)->AddRef();

        *OutServiceGroup = m_ServiceGroup;
        (*OutServiceGroup)->AddRef();
    }

    if (stream)
    {
        stream->Release();
    }

   	DbgPrint("CMiniportWaveCyclicVAUDIO::NewStream--\n");
 
    return ntStatus;
} // NewStream

//=============================================================================
STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicVAUDIO::NonDelegatingQueryInterface
( 
    IN  REFIID  Interface,
    OUT PVOID * Object 
)
{
    PAGED_CODE();

	DbgPrint("CMiniportWaveCyclicVAUDIO::NonDelegatingQueryInterface\n");

    if (IsEqualGUIDAligned(Interface, IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(PMINIPORTWAVECYCLIC(this)));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else if (IsEqualGUIDAligned(Interface, IID_IMiniportWaveCyclic))
    {
        *Object = PVOID(PMINIPORTWAVECYCLIC(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        // We reference the interface for the caller.

        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
} // NonDelegatingQueryInterface

//=============================================================================
NTSTATUS                    
CMiniportWaveCyclicVAUDIO::PropertyHandlerChannelConfig
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest
)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_INVALID_DEVICE_REQUEST;

	DbgPrint("CMiniportWaveCyclicVAUDIO::PropertyHandlerChannelConfig++\n");

    // Validate the property request structure.
    ntStatus = ValidatePropertyParams(PropertyRequest, sizeof(KSAUDIO_CHANNEL_CONFIG), 0);
    if(!NT_SUCCESS(ntStatus))
    {
		DbgPrint("CMiniportWaveCyclicVAUDIO::PropertyHandlerChannelConfig--\n");
        return ntStatus;
    }

    // Get the KSAUDIO_CHANNEL_CONFIG structure.
    KSAUDIO_CHANNEL_CONFIG *value = static_cast<KSAUDIO_CHANNEL_CONFIG*>(PropertyRequest->Value);

    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        // Store the current channel config in the return structure.
        *value = m_ChannelConfig;
        PropertyRequest->ValueSize = sizeof(KSAUDIO_CHANNEL_CONFIG);
    }
    else if (PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
    {
        // Limit the channel mask based on the maximum supported channel
        // count.
        //
        switch(value->ActiveSpeakerPositions)
        {
            case KSAUDIO_SPEAKER_MONO:
                break;
            case KSAUDIO_SPEAKER_STEREO:
                if (m_MaxChannelsPcm >= 2)
                {
                    value->ActiveSpeakerPositions = KSAUDIO_SPEAKER_STEREO;
                    break;
                }
                return STATUS_NOT_SUPPORTED;
            case KSAUDIO_SPEAKER_QUAD:
                if (m_MaxChannelsPcm >= 4)
                {
                    value->ActiveSpeakerPositions = KSAUDIO_SPEAKER_QUAD;
                    break;
                }
                return STATUS_NOT_SUPPORTED;
            case KSAUDIO_SPEAKER_SURROUND:
                if (m_MaxChannelsPcm >= 4)
                {
                    value->ActiveSpeakerPositions = KSAUDIO_SPEAKER_SURROUND;
                    break;
                }
                return STATUS_NOT_SUPPORTED;
            case KSAUDIO_SPEAKER_5POINT1:
                if (m_MaxChannelsPcm >= 6)
                {
                    value->ActiveSpeakerPositions = KSAUDIO_SPEAKER_5POINT1;
                    break;
                }
                return STATUS_NOT_SUPPORTED;
            case KSAUDIO_SPEAKER_5POINT1_SURROUND:
                if (m_MaxChannelsPcm >= 6)
                {
                    value->ActiveSpeakerPositions = KSAUDIO_SPEAKER_5POINT1_SURROUND;
                    break;
                }
                return STATUS_NOT_SUPPORTED;
            case KSAUDIO_SPEAKER_7POINT1_SURROUND:
                if (m_MaxChannelsPcm >= 8)
                {
                    value->ActiveSpeakerPositions = KSAUDIO_SPEAKER_7POINT1_SURROUND;
                    break;
                }
                return STATUS_NOT_SUPPORTED;
            case KSAUDIO_SPEAKER_7POINT1:
                if (m_MaxChannelsPcm >= 8)
                {
                    value->ActiveSpeakerPositions = KSAUDIO_SPEAKER_7POINT1;
                    break;
                }
                return STATUS_NOT_SUPPORTED;
            default:
                return STATUS_NOT_SUPPORTED;
        }

        // Store the new channel mask.
        m_ChannelConfig = *value;
    }
	DbgPrint("CMiniportWaveCyclicVAUDIO::PropertyHandlerChannelConfig--\n");
    
    return ntStatus;
} // PropertyHandlerChannelConfig

//=============================================================================
NTSTATUS
CMiniportWaveCyclicVAUDIO::ValidateFormat
( 
    IN  PKSDATAFORMAT           pDataFormat 
)
{
    PAGED_CODE();

    NTSTATUS                    ntStatus=STATUS_NOT_SUPPORTED;
    PWAVEFORMATEX               pwfx;

    // Let the default Validator handle the request.
    //
	DbgPrint("CMiniportWaveCyclicVAUDIO::ValidateFormat++\n");

    pwfx = GetWaveFormatEx(pDataFormat);
    if (pwfx)
    {
        if (IS_VALID_WAVEFORMATEX_GUID(&pDataFormat->SubFormat))
        {
            USHORT wfxID = EXTRACT_WAVEFORMATEX_ID(&pDataFormat->SubFormat);

            switch (wfxID)
            {
                case WAVE_FORMAT_PCM:
                {
                    switch (pwfx->wFormatTag)
                    {
                        case WAVE_FORMAT_PCM:
                        {
                            ntStatus = ValidatePcm(pwfx);
                            break;
                        }
                    }
                    break;
                }

                default:
                    break;
            }
        }
        else
        {
        }
    }

    if (NT_SUCCESS(ntStatus))
    {
		DbgPrint("CMiniportWaveCyclicVAUDIO::ValidateFormat--\n");
        return ntStatus;
    }

    // If the format is not known check for WAVEFORMATEXTENSIBLE.
    //
    pwfx = GetWaveFormatEx(pDataFormat);
    if (pwfx)
    {
        if (IS_VALID_WAVEFORMATEX_GUID(&pDataFormat->SubFormat))
        {
            USHORT wfxID = EXTRACT_WAVEFORMATEX_ID(&pDataFormat->SubFormat);

            switch (wfxID)
            {
                // This is for WAVE_FORMAT_EXTENSIBLE support.
                //
                case WAVE_FORMAT_PCM:
                {
                    switch (pwfx->wFormatTag)
                    {
                        case WAVE_FORMAT_EXTENSIBLE:
                        {
                            PWAVEFORMATEXTENSIBLE   pwfxExt = 
                                (PWAVEFORMATEXTENSIBLE) pwfx;
                            ntStatus = ValidateWfxExt(pwfxExt);
                            break;
                        }
                    }
                    break;
                }
                

                default:
                    break;
            }
        }
        else
        {
        }
    }

	DbgPrint("CMiniportWaveCyclicVAUDIO::ValidateFormat--\n");
    return ntStatus;
} // ValidateFormat

//=============================================================================
NTSTATUS
CMiniportWaveCyclicVAUDIO::ValidateWfxExt
(
    IN  PWAVEFORMATEXTENSIBLE   pWfxExt
)
{
    PAGED_CODE();

    // First verify that the subformat is OK
    //
	DbgPrint("CMiniportWaveCyclicVAUDIO::ValidateWfxExt++\n");

    if (pWfxExt)
    {
        if(IsEqualGUIDAligned(pWfxExt->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))//KSDATAFORMAT_SUBTYPE_PCM))
        {
            PWAVEFORMATEX           pWfx = (PWAVEFORMATEX) pWfxExt;

            // Then verify that the format parameters are supported.
            if
            (
                pWfx                                                &&
                (pWfx->cbSize == sizeof(WAVEFORMATEXTENSIBLE) - 
                    sizeof(WAVEFORMATEX))                           &&
                (pWfx->nChannels >= m_MinChannels)                  &&
                (pWfx->nChannels <= m_MaxChannelsPcm)               &&
                (pWfx->nSamplesPerSec >= m_MinSampleRatePcm)        &&
                (pWfx->nSamplesPerSec <= m_MaxSampleRatePcm)        &&
                (pWfx->wBitsPerSample >= m_MinBitsPerSamplePcm)     &&
                (pWfx->wBitsPerSample <= m_MaxBitsPerSamplePcm)
            )
            {
				DbgPrint("CMiniportWaveCyclicVAUDIO::ValidateWfxExt--\n");
                return STATUS_SUCCESS;
            }
        }
    }

	DbgPrint("CMiniportWaveCyclicVAUDIO::ValidateWfxExt--\n");
    return STATUS_INVALID_PARAMETER;
} // ValidateWfxExtPcm

STDMETHODIMP_(NTSTATUS)
CMiniportWaveCyclicStreamVAUDIO::AllocateBuffer
(
    IN ULONG                    BufferSize,
    IN PPHYSICAL_ADDRESS        PhysicalAddressConstraint OPTIONAL
)
{
    UNREFERENCED_PARAMETER(PhysicalAddressConstraint);

    PAGED_CODE();

    NTSTATUS                    ntStatus = STATUS_SUCCESS;


	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::AllocateBuffer++\n");
    //
    // Adjust this cap as needed...
    ASSERT (BufferSize <= DMA_BUFFER_SIZE);

    m_pvDmaBuffer = (PVOID)
        ExAllocatePoolWithTag
        (
            NonPagedPool,
            BufferSize,
            VAUDIO_POOLTAG
        );
    if (!m_pvDmaBuffer)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    else
    {
        m_ulDmaBufferSize = BufferSize;
    }

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::AllocateBuffer--\n");

    return ntStatus;
} // AllocateBuffer
#pragma code_seg()

//=============================================================================
STDMETHODIMP_(ULONG)
CMiniportWaveCyclicStreamVAUDIO::AllocatedBufferSize
(
    void
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::AllocatedBufferSize\n");

    return m_ulDmaBufferSize;
} // AllocatedBufferSize

//=============================================================================
STDMETHODIMP_(ULONG)
CMiniportWaveCyclicStreamVAUDIO::BufferSize
(
    void
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::BufferSize\n");
    return m_ulDmaBufferSize;
} // BufferSize

//=============================================================================
STDMETHODIMP_(void)
CMiniportWaveCyclicStreamVAUDIO::CopyFrom
(
    IN  PVOID                   Destination,
    IN  PVOID                   Source,
    IN  ULONG                   ByteCount
)
{
    UNREFERENCED_PARAMETER(Destination);
    UNREFERENCED_PARAMETER(Source);
    UNREFERENCED_PARAMETER(ByteCount);
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::CopyFrom\n");
} // CopyFrom

//=============================================================================
STDMETHODIMP_(void)
CMiniportWaveCyclicStreamVAUDIO::CopyTo
(
    IN  PVOID                   Destination,
    IN  PVOID                   Source,
    IN  ULONG                   ByteCount
)
{
    UNREFERENCED_PARAMETER(Destination);
    UNREFERENCED_PARAMETER(Source);
    UNREFERENCED_PARAMETER(ByteCount);
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::CopyTo\n");
} // CopyTo

//=============================================================================
#pragma code_seg("PAGE")
STDMETHODIMP_(void)
CMiniportWaveCyclicStreamVAUDIO::FreeBuffer
(
    void
)
{
    PAGED_CODE();

	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::FreeBuffer\n");
    if ( m_pvDmaBuffer )
    {
        ExFreePoolWithTag( m_pvDmaBuffer, VAUDIO_POOLTAG );
        m_ulDmaBufferSize = 0;
    }
} // FreeBuffer
#pragma code_seg()

//=============================================================================
STDMETHODIMP_(PADAPTER_OBJECT)
CMiniportWaveCyclicStreamVAUDIO::GetAdapterObject
(
    void
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::GetAdapterObject\n");
    return NULL;
} // GetAdapterObject

//=============================================================================
STDMETHODIMP_(ULONG)
CMiniportWaveCyclicStreamVAUDIO::MaximumBufferSize
(
    void
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::MaximumBufferSize\n");
    return m_pMiniport->m_MaxDmaBufferSize;
} // MaximumBufferSize

//=============================================================================
STDMETHODIMP_(PHYSICAL_ADDRESS)
CMiniportWaveCyclicStreamVAUDIO::PhysicalAddress
(
    void
)
{
    PHYSICAL_ADDRESS            pAddress;

    pAddress.QuadPart = (LONGLONG) m_pvDmaBuffer;
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::PhysicalAddress\n");

    return pAddress;
} // PhysicalAddress

//=============================================================================
STDMETHODIMP_(void)
CMiniportWaveCyclicStreamVAUDIO::SetBufferSize
(
    IN ULONG                    BufferSize
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::SetBufferSize\n");
    if ( BufferSize <= m_ulDmaBufferSize )
    {
        m_ulDmaBufferSize = BufferSize;
    }
    else
    {
    }
} // SetBufferSize

//=============================================================================
STDMETHODIMP_(PVOID)
CMiniportWaveCyclicStreamVAUDIO::SystemAddress
(
    void
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::SystemAddress\n");
    return m_pvDmaBuffer;
} // SystemAddress

//=============================================================================
STDMETHODIMP_(ULONG)
CMiniportWaveCyclicStreamVAUDIO::TransferCount
(
    void
)
{
	DbgPrint("CMiniportWaveCyclicStreamVAUDIO::TransferCount\n");
    return m_ulDmaBufferSize;
}






//=============================================================================
// ----------------------------------------------------------------------------
// Global Utility Functions
//=============================================================================
NTSTATUS
PropertyHandler_Wave
( 
    IN PPCPROPERTY_REQUEST      PropertyRequest 
)
{
	NTSTATUS ntStatus;

    PAGED_CODE();

	DbgPrint("PropertyHandler_Wave++\n");

    ntStatus = ((PCMiniportWaveCyclicVAUDIO)
        (PropertyRequest->MajorTarget))->PropertyHandlerGeneric
        (
            PropertyRequest
        );

	DbgPrint("PropertyHandler_Wave--\n");

	return ntStatus;
} // PropertyHandler_Wave


// PortClass와 연결되어 처음호출되는 Topology를 위한 Property Request 처리기
NTSTATUS
PropertyHandler_Topology
( 
    IN PPCPROPERTY_REQUEST      PropertyRequest 
)
{
	DbgPrint("PropertyHandler_Topology\n");
    return ((PCMiniportTopologyVAUDIO)
        (PropertyRequest->MajorTarget))->PropertyHandlerGeneric
        (
            PropertyRequest
        );
} // PropertyHandler_Topology



NTSTATUS
CreateMiniportWaveCyclicVAUDIO
( 
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    IN  POOL_TYPE               PoolType 
)
{
    PAGED_CODE();
	DbgPrint("CreateMiniportWaveCyclicVAUDIO\n");

    STD_CREATE_BODY(CMiniportWaveCyclicVAUDIO, Unknown, UnknownOuter, PoolType);
}


// 이 함수는 글로벌함수로서, COM 객체를 생성하는 일을 돕는다
NTSTATUS
CreateMiniportTopologyVAUDIO
( 
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    IN  POOL_TYPE               PoolType 
)
{
	DbgPrint("CreateMiniportTopologyVAUDIO\n");
    STD_CREATE_BODY(CMiniportTopologyVAUDIO, Unknown, UnknownOuter, PoolType);
} // CreateMiniportTopologyVAUDIO

NTSTATUS
NewAdapterCommon
( 
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    IN  POOL_TYPE               PoolType 
)
{
    PAGED_CODE();

    ASSERT(Unknown);
	DbgPrint("NewAdapterCommon\n");

    STD_CREATE_BODY_
    ( 
        CAdapterCommon, 
        Unknown, 
        UnknownOuter, 
        PoolType,      
        PADAPTERCOMMON 
    );

} // NewAdapterCommon

//=============================================================================
void
TimerNotify
(
    IN  PKDPC                   Dpc,
    IN  PVOID                   DeferredContext,
    IN  PVOID                   SA1,
    IN  PVOID                   SA2
)
{
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SA1);
    UNREFERENCED_PARAMETER(SA2);

    PCMiniportWaveCyclicVAUDIO pMiniport =
        (PCMiniportWaveCyclicVAUDIO) DeferredContext;

    if (pMiniport && pMiniport->m_Port)
    {
		DbgPrint("TimerNotify++\n");

        pMiniport->m_Port->Notify(pMiniport->m_ServiceGroup);

		DbgPrint("TimerNotify--\n");

    }
} // TimerNotify

//-----------------------------------------------------------------------------
PWAVEFORMATEX                   
GetWaveFormatEx
(
    IN  PKSDATAFORMAT           pDataFormat
)
{
    PWAVEFORMATEX           pWfx = NULL;
    
	DbgPrint("GetWaveFormatEx++\n");

    // If this is a known dataformat extract the waveformat info.
    //
    if
    ( 
        pDataFormat &&
        ( IsEqualGUIDAligned(pDataFormat->MajorFormat, 
                KSDATAFORMAT_TYPE_AUDIO)             &&
          ( IsEqualGUIDAligned(pDataFormat->Specifier, 
                KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) ||
            IsEqualGUIDAligned(pDataFormat->Specifier, 
                KSDATAFORMAT_SPECIFIER_DSOUND) ) )
    )
    {
        pWfx = PWAVEFORMATEX(pDataFormat + 1);

        if (IsEqualGUIDAligned(pDataFormat->Specifier, 
                KSDATAFORMAT_SPECIFIER_DSOUND))
        {
            PKSDSOUND_BUFFERDESC    pwfxds;

            pwfxds = PKSDSOUND_BUFFERDESC(pDataFormat + 1);
            pWfx = &pwfxds->WaveFormatEx;
        }
    }

	DbgPrint("GetWaveFormatEx--\n");

    return pWfx;
} // GetWaveFormatEx

//-----------------------------------------------------------------------------
NTSTATUS                        
PropertyHandler_BasicSupport
(
    IN PPCPROPERTY_REQUEST         PropertyRequest,
    IN ULONG                       Flags,
    IN DWORD                       PropTypeSetId
)
{
    NTSTATUS                    ntStatus = STATUS_INVALID_PARAMETER;

	DbgPrint("PropertyHandler_BasicSupport++\n");

    if (PropertyRequest->ValueSize >= sizeof(KSPROPERTY_DESCRIPTION))
    {
        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
        //
        PKSPROPERTY_DESCRIPTION PropDesc = 
            PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

        PropDesc->AccessFlags       = Flags;
        PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
        if  (VT_ILLEGAL != PropTypeSetId)
        {
            PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
            PropDesc->PropTypeSet.Id    = PropTypeSetId;
        }
        else
        {
            PropDesc->PropTypeSet.Set   = GUID_NULL;
            PropDesc->PropTypeSet.Id    = 0;
        }
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 0;
        PropDesc->Reserved          = 0;

        PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        ntStatus = STATUS_SUCCESS;
    } 
    else if (PropertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        //
        *(PULONG(PropertyRequest->Value)) = Flags;

        PropertyRequest->ValueSize = sizeof(ULONG);
        ntStatus = STATUS_SUCCESS;                    
    }
    else
    {
        PropertyRequest->ValueSize = 0;
        ntStatus = STATUS_BUFFER_TOO_SMALL;
    }

	DbgPrint("PropertyHandler_BasicSupport--\n");

    return ntStatus;
} // PropertyHandler_BasicSupport

//-----------------------------------------------------------------------------
NTSTATUS 
ValidatePropertyParams
(
    IN PPCPROPERTY_REQUEST      PropertyRequest, 
    IN ULONG                    cbSize,
    IN ULONG                    cbInstanceSize /* = 0  */
)
{
    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;

	DbgPrint("ValidatePropertyParams++\n");

    if (PropertyRequest && cbSize)
    {
        // If the caller is asking for ValueSize.
        //
        if (0 == PropertyRequest->ValueSize) 
        {
            PropertyRequest->ValueSize = cbSize;
            ntStatus = STATUS_BUFFER_OVERFLOW;
        }
        // If the caller passed an invalid ValueSize.
        //
        else if (PropertyRequest->ValueSize < cbSize)
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        else if (PropertyRequest->InstanceSize < cbInstanceSize)
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
        // If all parameters are OK.
        // 
        else if (PropertyRequest->ValueSize == cbSize)
        {
            if (PropertyRequest->Value)
            {
                ntStatus = STATUS_SUCCESS;
                //
                // Caller should set ValueSize, if the property 
                // call is successful.
                //
            }
        }
    }
    else
    {
        ntStatus = STATUS_INVALID_PARAMETER;
    }
    
    // Clear the ValueSize if unsuccessful.
    //
    if (PropertyRequest &&
        STATUS_SUCCESS != ntStatus &&
        STATUS_BUFFER_OVERFLOW != ntStatus)
    {
        PropertyRequest->ValueSize = 0;
    }

	DbgPrint("ValidatePropertyParams--\n");

    return ntStatus;
} // ValidatePropertyParams

NTSTATUS
InstallSubdevice
( 
    __in        PDEVICE_OBJECT          DeviceObject,
    __in        PIRP                    Irp,
    __in        PWSTR                   Name,
    __in        REFGUID                 PortClassId,
    __in        REFGUID                 MiniportClassId,
    __in_opt    PFNCREATEINSTANCE       MiniportCreate,
    __in_opt    PUNKNOWN                UnknownAdapter,
    __in_opt    PRESOURCELIST           ResourceList,
    __in        REFGUID                 PortInterfaceId,
    __out_opt   PUNKNOWN *              OutPortInterface,
    __out_opt   PUNKNOWN *              OutPortUnknown
)
{

    NTSTATUS                    ntStatus;
    PPORT                       port = NULL;
    PUNKNOWN                    miniport = NULL;

	DbgPrint("InstallSubdevice++\n");
	
    // Create the port driver object
    //
    ntStatus = PcNewPort(&port, PortClassId);

    // Create the miniport object
    //
    if (NT_SUCCESS(ntStatus))
    {
        if (MiniportCreate)
        {
            ntStatus = 
                MiniportCreate
                ( 
                    &miniport,
                    MiniportClassId,
                    NULL,
                    NonPagedPool 
                );
        }
        else
        {
            ntStatus = 
                PcNewMiniport
                (
                    (PMINIPORT *) &miniport, 
                    MiniportClassId
                );
        }
    }

    // Init the port driver and miniport in one go.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = 
            port->Init
            ( 
                DeviceObject,
                Irp,
                miniport,
                UnknownAdapter,
                ResourceList 
            );

        if (NT_SUCCESS(ntStatus))
        {
            // Register the subdevice (port/miniport combination).
            //
            ntStatus = 
                PcRegisterSubdevice
                ( 
                    DeviceObject,
                    Name,
                    port 
                );
        }

        // We don't need the miniport any more.  Either the port has it,
        // or we've failed, and it should be deleted.
        //
        miniport->Release();
    }

    // Deposit the port interfaces if it's needed.
    //
    if (NT_SUCCESS(ntStatus))
    {
        if (OutPortUnknown)
        {
            ntStatus = 
                port->QueryInterface
                ( 
                    IID_IUnknown,
                    (PVOID *)OutPortUnknown 
                );
        }

        if (OutPortInterface)
        {
            ntStatus = 
                port->QueryInterface
                ( 
                    PortInterfaceId,
                    (PVOID *) OutPortInterface 
                );
        }
    }

    if (port)
    {
        port->Release();
    }

	DbgPrint("InstallSubdevice--\n");

    return ntStatus;
} // InstallSubDevice

