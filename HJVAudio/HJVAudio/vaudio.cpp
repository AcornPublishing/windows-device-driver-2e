/*++

Copyright (c) 2013-2100  Hajesoft Corporation All Rights Reserved

Module Name:

    vaudio.cpp

--*/

#include "VAUDIO.h"    
#include "common.h"    

typedef struct _PHYSICALCONNECTIONTABLE
{
    ULONG       ulTopologyIn;
    ULONG       ulWaveOut;
} PHYSICALCONNECTIONTABLE, *PPHYSICALCONNECTIONTABLE;

PHYSICALCONNECTIONTABLE TopologyPhysicalConnections =
{
    KSPIN_TOPO_WAVEOUT_SOURCE,  // TopologyIn
    KSPIN_WAVE_RENDER_SOURCE    // WaveOut
};

extern "C" NTSTATUS                                                            
DriverEntry                                                         
	(                                                               
	IN PDRIVER_OBJECT DriverObject,             /* pointer to the device object */
	IN PUNICODE_STRING RegistryPath             /* pointer to a unicode string representing the path */
												/*    to the drivers specific key in the registry    */
	)
{
    NTSTATUS                    ntStatus;
	
	DbgPrint("DriverEntry++\n");
    ntStatus =  
        PcInitializeAdapterDriver
        ( 
            DriverObject,
            RegistryPath,
            (PDRIVER_ADD_DEVICE)VAUDIO_AddDevice
        );

	DbgPrint("DriverEntry--\n");
    return ntStatus;
}

NTSTATUS
VAUDIO_AddDevice
	(
	IN PDRIVER_OBJECT DriverObject,         
	IN PDEVICE_OBJECT PhysicalDeviceObject  
	)
{
    NTSTATUS                    ntStatus;
	DbgPrint("VAUDIO_AddDevice++\n");

    ntStatus = 
        PcAddAdapterDevice
        ( 
            DriverObject,
            PhysicalDeviceObject,
            PCPFNSTARTDEVICE(VAUDIO_StartDevice),
            MAX_MINIPORTS,
            0
        );

	DbgPrint("VAUDIO_AddDevice--\n");
    return ntStatus;
}

NTSTATUS
VAUDIO_StartDevice
( 
    IN  PDEVICE_OBJECT          DeviceObject,     
    IN  PIRP                    Irp,              
    IN  PRESOURCELIST           ResourceList      
)  
{
    UNREFERENCED_PARAMETER(ResourceList);

    NTSTATUS                    ntStatus        = STATUS_SUCCESS;
    PUNKNOWN                    unknownTopology = NULL;
    PUNKNOWN                    unknownWave     = NULL;
    PADAPTERCOMMON              pAdapterCommon  = NULL;
    PUNKNOWN                    pUnknownCommon  = NULL;

	DbgPrint("VAUDIO_StartDevice++\n");

    ntStatus = 
        NewAdapterCommon
        ( 
            &pUnknownCommon,
            IID_IAdapterCommon,
            NULL,
            NonPagedPool 
        );
	/*
	COM객체를 생성한다
	접근 Interface Pointer로서, pUnknownCommon 를 구한다
	*/

    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = 
            pUnknownCommon->QueryInterface
            ( 
                IID_IAdapterCommon,
                (PVOID *) &pAdapterCommon 
            );

		// pUnknownCommon 인터페이스를 사용해서, 접근할 IID_IAdapterCommon 를 구한다

        if (NT_SUCCESS(ntStatus))
        {
            ntStatus = 
                pAdapterCommon->Init(DeviceObject);

            if (NT_SUCCESS(ntStatus))
            {
                ntStatus = 
                    PcRegisterAdapterPowerManagement
                    ( 
                        PUNKNOWN(pAdapterCommon),
                        DeviceObject 
                    );
            }
        }
    }

    // install VAUDIO topology miniport.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = 
            InstallSubdevice
            ( 
                DeviceObject,
                Irp,
                L"Topology",
                CLSID_PortTopology,
                CLSID_PortTopology, 
                CreateMiniportTopologyVAUDIO,
                pAdapterCommon,
                NULL,
                IID_IPortTopology,
                NULL,
                &unknownTopology 
            );
    }

    // install VAUDIO wavecyclic miniport.
    //
    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = 
            InstallSubdevice
            ( 
                DeviceObject,
                Irp,
                L"Wave",
                CLSID_PortWaveCyclic,
                CLSID_PortWaveCyclic,   
                CreateMiniportWaveCyclicVAUDIO,
                pAdapterCommon,
                NULL,
                IID_IPortWaveCyclic,
                pAdapterCommon->WavePortDriverDest(),
                &unknownWave 
            );
    }

    if (unknownTopology && unknownWave)
    {
        // register wave <=> topology connections
        // This will connect bridge pins of wavecyclic and topology
        // miniports.
        //
        if (NT_SUCCESS(ntStatus))
        {
            if ((TopologyPhysicalConnections.ulWaveOut != (ULONG)-1) &&
                (TopologyPhysicalConnections.ulTopologyIn != (ULONG)-1))
            {
                ntStatus =
                    PcRegisterPhysicalConnection
                    ( 
                        DeviceObject,
                        unknownWave,
                        TopologyPhysicalConnections.ulWaveOut,
                        unknownTopology,
                        TopologyPhysicalConnections.ulTopologyIn
                    );
            }
        }
    }

    // Release the adapter common object.  It either has other references,
    // or we need to delete it anyway.
    //
    if (pAdapterCommon)
    {
        pAdapterCommon->Release();
    }

    if (pUnknownCommon)
    {
        pUnknownCommon->Release();
    }
    
    if (unknownTopology)
    {
        unknownTopology->Release();
    }

    if (unknownWave)
    {
        unknownWave->Release();
    }

	DbgPrint("VAUDIO_StartDevice--\n");

    return ntStatus;
} // StartDevice

