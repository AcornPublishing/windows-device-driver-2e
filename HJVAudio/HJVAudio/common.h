#ifndef _VAUDIO_COMMON_H_
#define _VAUDIO_COMMON_H_

//=============================================================================
// Defines
//=============================================================================

DEFINE_GUID(IID_IAdapterCommon,
0x7eda2950, 0xbf9f, 0x11d0, 0x87, 0x1f, 0x0, 0xa0, 0xc9, 0x11, 0xb5, 0x44);

// Global Function


//=============================================================================
// Function Prototypes
//=============================================================================
NTSTATUS
NewAdapterCommon
( 
    OUT PUNKNOWN *              Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN                UnknownOuter OPTIONAL,
    IN  POOL_TYPE               PoolType 
);

PWAVEFORMATEX                   GetWaveFormatEx
(
    IN  PKSDATAFORMAT           pDataFormat
);

NTSTATUS                        PropertyHandler_BasicSupport
(
    IN  PPCPROPERTY_REQUEST     PropertyRequest,
    IN  ULONG                   Flags,
    IN  DWORD                   PropTypeSetId
);

NTSTATUS                        ValidatePropertyParams
(
    IN PPCPROPERTY_REQUEST      PropertyRequest, 
    IN ULONG                    cbValueSize,
    IN ULONG                    cbInstanceSize = 0 
);

NTSTATUS
PropertyHandler_Wave
( 
    IN PPCPROPERTY_REQUEST      PropertyRequest 
);

NTSTATUS
PropertyHandler_Topology
( 
    IN PPCPROPERTY_REQUEST      PropertyRequest 
);

NTSTATUS
CreateMiniportWaveCyclicVAUDIO
( 
    OUT PUNKNOWN *,
    IN  REFCLSID,
    IN  PUNKNOWN,
    IN  POOL_TYPE
);

NTSTATUS
CreateMiniportTopologyVAUDIO
( 
    OUT PUNKNOWN *,
    IN  REFCLSID,
    IN  PUNKNOWN,
    IN  POOL_TYPE
);

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
);

//=============================================================================
// Interfaces
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// IAdapterCommon
//
DECLARE_INTERFACE_(IAdapterCommon, IUnknown)
{
    STDMETHOD_(NTSTATUS,        Init) 
    ( 
        THIS_
        IN  PDEVICE_OBJECT      DeviceObject 
    ) PURE;

    STDMETHOD_(VOID,            SetWaveServiceGroup) 
    ( 
        THIS_
        IN PSERVICEGROUP        ServiceGroup 
    ) PURE;

    STDMETHOD_(PUNKNOWN *,      WavePortDriverDest) 
    ( 
        THIS 
    ) PURE;
};
typedef IAdapterCommon *PADAPTERCOMMON;

class CAdapterCommon : 
    public IAdapterCommon,
    public IAdapterPowerManagement,
    public CUnknown    
{
    private:
        PPORTWAVECYCLIC         m_pPortWave;    // Port interface
        PSERVICEGROUP           m_pServiceGroupWave;
        PDEVICE_OBJECT          m_pDeviceObject;      
        DEVICE_POWER_STATE      m_PowerState;        

    public:
        //=====================================================================
        // Default CUnknown
        DECLARE_STD_UNKNOWN();
        DEFINE_STD_CONSTRUCTOR(CAdapterCommon);
        ~CAdapterCommon();

        //=====================================================================
        // Default IAdapterPowerManagement
        IMP_IAdapterPowerManagement;

        //=====================================================================
        // IAdapterCommon methods                                               
        STDMETHODIMP_(NTSTATUS) Init
        (   
            IN  PDEVICE_OBJECT  DeviceObject
        );

        STDMETHODIMP_(PUNKNOWN *)       WavePortDriverDest(void);

        STDMETHODIMP_(void)     SetWaveServiceGroup
        (   
            IN  PSERVICEGROUP   ServiceGroup
        );

};

class CMiniportTopologyVAUDIO : 
    public IMiniportTopology,
    public CUnknown
{
  protected:
    PADAPTERCOMMON              m_AdapterCommon;    // Adapter common object.
    PPCFILTER_DESCRIPTOR        m_FilterDescriptor; // Filter descriptor.

  public:
    DECLARE_STD_UNKNOWN();
    IMP_IMiniportTopology;
    DEFINE_STD_CONSTRUCTOR(CMiniportTopologyVAUDIO);

    ~CMiniportTopologyVAUDIO();

    // PropertyHandlers.
	// 아래부분의 함수들은 꼭 필요한 부분은 아니다.
	//
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
};

typedef CMiniportTopologyVAUDIO *PCMiniportTopologyVAUDIO;


//=============================================================================
// Referenced Forward
//=============================================================================
KDEFERRED_ROUTINE TimerNotify;

class CMiniportWaveCyclicStreamVAUDIO;
typedef CMiniportWaveCyclicStreamVAUDIO *PCMiniportWaveCyclicStreamVAUDIO;

//=============================================================================
// Classes
//=============================================================================

///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveCyclicVAUDIO
//   This is the common base class for all VAUDIO samples. It implements basic
//   functionality.

class CMiniportWaveCyclicVAUDIO : 
    public IMiniportWaveCyclic,
    public CUnknown
{
private:
    BOOL                        m_fRenderAllocated;
    KSAUDIO_CHANNEL_CONFIG      m_ChannelConfig;

protected:
    PADAPTERCOMMON              m_AdapterCommon;    // Adapter common object
    PPORTWAVECYCLIC             m_Port;             // Callback interface
    PPCFILTER_DESCRIPTOR        m_FilterDescriptor; // Filter descriptor

    ULONG                       m_NotificationInterval; // milliseconds.
    ULONG                       m_SamplingFrequency;    // Frames per second.

    PSERVICEGROUP               m_ServiceGroup;     // For notification.
    KMUTEX                      m_SampleRateSync;   // Sync for sample rate 

    ULONG                       m_MaxDmaBufferSize; // Dma buffer size.

    ULONG                       m_MaxInputStreams;
    ULONG                       m_MaxTotalStreams;

    ULONG                       m_MinChannels;      // Format caps
    ULONG                       m_MaxChannelsPcm;
    ULONG                       m_MinBitsPerSamplePcm;
    ULONG                       m_MaxBitsPerSamplePcm;
    ULONG                       m_MinSampleRatePcm;
    ULONG                       m_MaxSampleRatePcm;

protected:
    NTSTATUS                    ValidateFormat
    ( 
        IN	PKSDATAFORMAT       pDataFormat 
    );

    NTSTATUS                    ValidateWfxExt
    (
        IN  PWAVEFORMATEXTENSIBLE   pWfxExt
    );

    NTSTATUS                    ValidatePcm
    (
        IN  PWAVEFORMATEX       pWfx
    );

public:
    DECLARE_STD_UNKNOWN();
//    DEFINE_STD_CONSTRUCTOR(CMiniportWaveCyclicVAUDIO);
    ~CMiniportWaveCyclicVAUDIO();

    IMP_IMiniportWaveCyclic;
	CMiniportWaveCyclicVAUDIO(PUNKNOWN pUnknownOuter);

    NTSTATUS                    PropertyHandlerCpuResources
    ( 
        IN  PPCPROPERTY_REQUEST PropertyRequest 
    );

    NTSTATUS                    PropertyHandlerGeneric
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    NTSTATUS                    PropertyHandlerChannelConfig
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    // Friends
    friend class                CMiniportWaveCyclicStreamVAUDIO;
    friend class                CMiniportTopologyVAUDIO;
    friend void                 TimerNotify
    ( 
        IN  PKDPC               Dpc, 
        IN  PVOID               DeferredContext, 
        IN  PVOID               SA1, 
        IN  PVOID               SA2 
    );
};
typedef CMiniportWaveCyclicVAUDIO *PCMiniportWaveCyclicVAUDIO;

///////////////////////////////////////////////////////////////////////////////
// CMiniportWaveCyclicStreamVAUDIO
//   This is the common base class for all VAUDIO samples. It implements basic
//   functionality for wavecyclic streams.

class CMiniportWaveCyclicStreamVAUDIO : 
    public IMiniportWaveCyclicStream,
    public IDmaChannel,
    public CUnknown
{
protected:
    PCMiniportWaveCyclicVAUDIO   m_pMiniport;                        // Miniport that created us
    BOOLEAN                     m_fCapture;                         // Capture or render.
    BOOLEAN                     m_fFormat16Bit;                     // 16- or 8-bit samples.
    USHORT                      m_usBlockAlign;                     // Block alignment of current format.
    KSSTATE                     m_ksState;                          // Stop, pause, run.
    ULONG                       m_ulPin;                            // Pin Id.

    PRKDPC                      m_pDpc;                             // Deferred procedure call object
    PKTIMER                     m_pTimer;                           // Timer object

    BOOLEAN                     m_fDmaActive;                       // Dma currently active? 
    ULONG                       m_ulDmaPosition;                    // Position in Dma
    PVOID                       m_pvDmaBuffer;                      // Dma buffer pointer
    ULONG                       m_ulDmaBufferSize;                  // Size of dma buffer
    ULONG                       m_ulDmaMovementRate;                // Rate of transfer specific to system
    ULONGLONG                   m_ullDmaTimeStamp;                  // Dma time elasped 
    ULONGLONG                   m_ullElapsedTimeCarryForward;       // Time to carry forward in position calc.
    ULONG                       m_ulByteDisplacementCarryForward;   // Bytes to carry forward to next calc.

public:
    DECLARE_STD_UNKNOWN();
    //DEFINE_STD_CONSTRUCTOR(CMiniportWaveCyclicStreamVAUDIO);
    ~CMiniportWaveCyclicStreamVAUDIO();
	CMiniportWaveCyclicStreamVAUDIO(PUNKNOWN pUnknownOuter);

    IMP_IMiniportWaveCyclicStream;
    IMP_IDmaChannel;

    NTSTATUS                    Init
    ( 
        IN  PCMiniportWaveCyclicVAUDIO  Miniport,
        IN  ULONG               Pin,
        IN  BOOLEAN             Capture,
        IN  PKSDATAFORMAT       DataFormat
    );

    // Friends
    friend class CMiniportWaveCyclicStream;
    friend class CMiniportWaveCyclicVAUDIO;
};
typedef CMiniportWaveCyclicStreamVAUDIO *PCMiniportWaveCyclicStreamVAUDIO;

// Wave Topology nodes.
enum 
{
    KSNODE_WAVE_DAC = 0
};

// topology nodes.
enum
{
    KSNODE_TOPO_WAVEOUT_VOLUME = 0,
    KSNODE_TOPO_WAVEOUT_MUTE
};


//=============================================================================
static
KSDATARANGE PinDataRangesBridge[] =
{
 {
   sizeof(KSDATARANGE),
   0,
   0,
   0,
   STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
   STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
   STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
 }
};

//=============================================================================
static
PKSDATARANGE PinDataRangePointersBridge[] =
{
  &PinDataRangesBridge[0]
};

//=============================================================================
static
PCPIN_DESCRIPTOR MiniportPins[] =
{
  // KSPIN_TOPO_WAVEOUT_SOURCE
  {
    0,
    0,
    0,                                              // InstanceCount
    NULL,                                           // AutomationTable
    {                                               // KsPinDescriptor
      0,                                            // InterfacesCount
      NULL,                                         // Interfaces
      0,                                            // MediumsCount
      NULL,                                         // Mediums
      SIZEOF_ARRAY(PinDataRangePointersBridge),     // DataRangesCount
      PinDataRangePointersBridge,                   // DataRanges
      KSPIN_DATAFLOW_IN,                            // DataFlow
      KSPIN_COMMUNICATION_NONE,                     // Communication
      &KSCATEGORY_AUDIO,                            // Category
      NULL,                                         // Name
      0                                             // Reserved
    }
  },

  // KSPIN_TOPO_LINEOUT_DEST
  {
    0,
    0,
    0,                                              // InstanceCount
    NULL,                                           // AutomationTable
    {                                               // KsPinDescriptor
      0,                                            // InterfacesCount
      NULL,                                         // Interfaces
      0,                                            // MediumsCount
      NULL,                                         // Mediums
      SIZEOF_ARRAY(PinDataRangePointersBridge),     // DataRangesCount
      PinDataRangePointersBridge,                   // DataRanges
      KSPIN_DATAFLOW_OUT,                           // DataFlow
      KSPIN_COMMUNICATION_NONE,                     // Communication
      &KSNODETYPE_SPEAKER,                          // Category, KSNODETYPE_SPDIF_INTERFACE, KSNODETYPE_SPEAKER, KSNODETYPE_DESKTOP_SPEAKER
      NULL,                                         // Name
      0                                             // Reserved
    }
  }

};

//=============================================================================
static
PCPROPERTY_ITEM PropertiesVolume[] =
{
    {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_VOLUMELEVEL,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
    },
    {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_CPU_RESOURCES,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationVolume, PropertiesVolume);

//=============================================================================
static
PCPROPERTY_ITEM PropertiesMute[] =
{
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_MUTE,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  },
  {
    &KSPROPSETID_Audio,
    KSPROPERTY_AUDIO_CPU_RESOURCES,
    KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_BASICSUPPORT,
    PropertyHandler_Topology
  }
};

DEFINE_PCAUTOMATION_TABLE_PROP(AutomationMute, PropertiesMute);

//=============================================================================
static
PCNODE_DESCRIPTOR TopologyNodes[] =
{
  // KSNODE_TOPO_WAVEOUT_VOLUME
  {
    0,                      // Flags
    &AutomationVolume,      // AutomationTable
    &KSNODETYPE_VOLUME,     // Type
    &KSAUDFNAME_WAVE_VOLUME // Name
  },

  // KSNODE_TOPO_WAVEOUT_MUTE
  {
    0,                      // Flags
    &AutomationMute,        // AutomationTable
    &KSNODETYPE_MUTE,       // Type
    &KSAUDFNAME_WAVE_MUTE   // Name
  },
};

//=============================================================================
static
PCCONNECTION_DESCRIPTOR MiniportConnections[] =
{
  //  FromNode,                     FromPin,                        ToNode,                      ToPin
  {   PCFILTER_NODE,                KSPIN_TOPO_WAVEOUT_SOURCE,      KSNODE_TOPO_WAVEOUT_VOLUME,  1 },
  {   KSNODE_TOPO_WAVEOUT_VOLUME,   0,                              KSNODE_TOPO_WAVEOUT_MUTE,    1 },
  {   KSNODE_TOPO_WAVEOUT_MUTE,     0,                              PCFILTER_NODE,     KSPIN_TOPO_LINEOUT_DEST }
};

//=============================================================================
static
PCFILTER_DESCRIPTOR MiniportFilterDescriptor =
{
  0,                                  // Version
  NULL,                               // AutomationTable
  sizeof(PCPIN_DESCRIPTOR),           // PinSize
  SIZEOF_ARRAY(MiniportPins),         // PinCount
  MiniportPins,                       // Pins
  sizeof(PCNODE_DESCRIPTOR),          // NodeSize
  SIZEOF_ARRAY(TopologyNodes),        // NodeCount
  TopologyNodes,                      // Nodes
  SIZEOF_ARRAY(MiniportConnections),  // ConnectionCount
  MiniportConnections,                // Connections
  0,                                  // CategoryCount
  NULL                                // Categories
};


// -----------
static
KSDATARANGE_AUDIO WavPinDataRangesStream[] =
{
    {
        {
            sizeof(KSDATARANGE_AUDIO),
            0,
            0,
            0,
            STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
            STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM), // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, KSDATAFORMAT_SUBTYPE_PCM
            STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX) // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, KSDATAFORMAT_SPECIFIER_WAVEFORMATEX
        },
        MAX_CHANNELS_PCM,           
        MIN_BITS_PER_SAMPLE_PCM,    
        MAX_BITS_PER_SAMPLE_PCM,    
        MIN_SAMPLE_RATE,            
        MAX_SAMPLE_RATE             
    },
};

static
PKSDATARANGE WavPinDataRangePointersStream[] =
{
    PKSDATARANGE(&WavPinDataRangesStream[0])
};

//=============================================================================
static
KSDATARANGE WavPinDataRangesBridge[] =
{
    {
        sizeof(KSDATARANGE),
        0,
        0,
        0,
        STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
        STATICGUIDOF(KSDATAFORMAT_SUBTYPE_ANALOG),
        STATICGUIDOF(KSDATAFORMAT_SPECIFIER_NONE)
    }
};

static
PKSDATARANGE WavPinDataRangePointersBridge[] =
{
    &WavPinDataRangesBridge[0]
};

//=============================================================================
static
PCPIN_DESCRIPTOR WavMiniportPins[] =
{
    // Wave Out Streaming Pin
    {
        MAX_INPUT_STREAMS,
        MAX_INPUT_STREAMS, 
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(WavPinDataRangePointersStream),
            WavPinDataRangePointersStream,
            KSPIN_DATAFLOW_IN,
            KSPIN_COMMUNICATION_SINK,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
    // Wave Out Bridge Pin
    {
        0,
        0,
        0,
        NULL,
        {
            0,
            NULL,
            0,
            NULL,
            SIZEOF_ARRAY(WavPinDataRangePointersBridge),
            WavPinDataRangePointersBridge,
            KSPIN_DATAFLOW_OUT,
            KSPIN_COMMUNICATION_NONE,
            &KSCATEGORY_AUDIO,
            NULL,
            0
        }
    },
};

//=============================================================================
static PCPROPERTY_ITEM PropertiesDAC[] =
{
    // KSPROPERTY_AUDIO_CHANNEL_CONFIG
    {
        &KSPROPSETID_Audio,                                     // Property Set     
        KSPROPERTY_AUDIO_CHANNEL_CONFIG,                        // Property ID
        PCPROPERTY_ITEM_FLAG_GET | PCPROPERTY_ITEM_FLAG_SET,    // Flags
        PropertyHandler_Wave                                    // Handler
    }
};

//=============================================================================
DEFINE_PCAUTOMATION_TABLE_PROP(AutomationDAC, PropertiesDAC);

//=============================================================================
static
PCNODE_DESCRIPTOR WavMiniportNodes[] =
{
    // KSNODE_WAVE_DAC
    {
        0,                      // Flags
        &AutomationDAC,         // AutomationTable
        &KSNODETYPE_DAC,        // Type
        NULL                    // Name
    }
};


//=============================================================================
static
PCCONNECTION_DESCRIPTOR WavMiniportConnections[] =
{
    { PCFILTER_NODE,        KSPIN_WAVE_RENDER_SINK,     KSNODE_WAVE_DAC,     1 },    
    { KSNODE_WAVE_DAC,      0,                          PCFILTER_NODE,       KSPIN_WAVE_RENDER_SOURCE },    
};


static
PCFILTER_DESCRIPTOR WavMiniportFilterDescriptor =
{
    0,                                  // Version
    NULL,                               // AutomationTable
    sizeof(PCPIN_DESCRIPTOR),           // PinSize
    SIZEOF_ARRAY(WavMiniportPins),         // PinCount
    WavMiniportPins,                       // Pins
    sizeof(PCNODE_DESCRIPTOR),          // NodeSize
    SIZEOF_ARRAY(WavMiniportNodes),        // NodeCount
    WavMiniportNodes,                      // Nodes
    SIZEOF_ARRAY(WavMiniportConnections),  // ConnectionCount
    WavMiniportConnections,                // Connections
    0,                                  // CategoryCount
    NULL                                // Categories - NULL->use defaults (AUDIO RENDER CAPTURE)
};

#endif  //_COMMON_H_

