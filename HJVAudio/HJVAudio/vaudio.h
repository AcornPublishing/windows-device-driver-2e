#ifndef _VAUDIO_
#define _VAUDIO_

#include <portcls.h>
#include <stdunk.h>
#include <ksdebug.h>

#define DMA_BUFFER_SIZE             0x16000
#define VAUDIO_POOLTAG               'HJVD'  
#define KSPROPERTY_TYPE_ALL         KSPROPERTY_TYPE_BASICSUPPORT | \
                                    KSPROPERTY_TYPE_GET | \
                                    KSPROPERTY_TYPE_SET
                                    
//=============================================================================
// Externs
//=============================================================================

#define MAX_MINIPORTS 2

// Pin properties.
#define MAX_INPUT_STREAMS           1       // Number of render streams.
#define MAX_TOTAL_STREAMS           MAX_INPUT_STREAMS//MAX_OUTPUT_STREAMS// + MAX_INPUT_STREAMS                      

// PCM Info
#define MIN_CHANNELS                1       // Min Channels.
#define MAX_CHANNELS_PCM            2       // Max Channels.
#define MIN_BITS_PER_SAMPLE_PCM     16       // Min Bits Per Sample
#define MAX_BITS_PER_SAMPLE_PCM     32      // Max Bits Per Sample
#define MIN_SAMPLE_RATE             8000	// Min Sample Rate
#define MAX_SAMPLE_RATE             384000   // Max Sample Rate

// Wave pins
enum 
{
    KSPIN_WAVE_RENDER_SINK = 0, 
    KSPIN_WAVE_RENDER_SOURCE
};

// topology pins.
enum
{
    KSPIN_TOPO_WAVEOUT_SOURCE = 0,
    KSPIN_TOPO_LINEOUT_DEST
};

NTSTATUS
VAUDIO_AddDevice
	(
	IN PDRIVER_OBJECT DriverObject,         
	IN PDEVICE_OBJECT PhysicalDeviceObject  
	);

NTSTATUS
VAUDIO_StartDevice
( 
    IN  PDEVICE_OBJECT,      
    IN  PIRP,                
    IN  PRESOURCELIST        
); 
#endif //_VAUDIO_
