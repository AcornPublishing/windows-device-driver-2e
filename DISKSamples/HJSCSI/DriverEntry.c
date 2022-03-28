
#include "HJSCSI.h"

BOOLEAN 
KeyBoardTrick()
{
#define I8042_PHYSICAL_BASE           0x60
#define I8042_DATA_REGISTER_OFFSET		 0
#define I8042_STATUS_REGISTER_OFFSET	 4

#define OUTPUT_BUFFER_FULL       	  0x01
#define INPUT_BUFFER_FULL        	  0x02	

#define KEYBOARD_F12				  0xD8
#define KEYBOARD_F12_2				  0x58
#define KEYBOARD_ENTER				  0x1C
	
	ULONG Port = I8042_PHYSICAL_BASE + I8042_STATUS_REGISTER_OFFSET;
	ULONG Data = I8042_PHYSICAL_BASE + I8042_DATA_REGISTER_OFFSET;
	ULONG i = 0;
	LARGE_INTEGER li;
	UCHAR StatusValue;
	UCHAR DataValue;
	KIRQL        Irql;

	li.QuadPart = -10000; 

	KeRaiseIrql(  HIGH_LEVEL , &Irql );		

	for( i = 0 ; i < 1000000 ; i++ )
	{
		StatusValue = READ_PORT_UCHAR( (PVOID)Port );

		if(  StatusValue & OUTPUT_BUFFER_FULL )
		{
			DataValue = READ_PORT_UCHAR( (PVOID)Data );		
			if( DataValue == KEYBOARD_F12 )
			{
				KeLowerIrql(Irql);
				return TRUE;
			}
			if( DataValue == KEYBOARD_F12_2 )
			{
				KeLowerIrql(Irql);
				return TRUE;
			}
		}		
	}	
	KeLowerIrql(Irql);

	return FALSE;
}


/***************************************************************************
함수명 : DriverEntry
인  자 : IN PVOID DriverObject,IN PVOID Arg
리턴값 : ULONG
***************************************************************************/
ULONG DriverEntry(IN PVOID DriverObject,IN PVOID Arg)
{

	UCHAR					VendorId[] = {'8','0','8','6'};
	UCHAR					DeviceId[] = {'1','C','2','0'};

	ULONG					statusToReturn, newStatus;
	ULONG					adapterCount;
	int						i;

	HW_INITIALIZATION_DATA	hwInitializationData;

   	if( KeyBoardTrick() == TRUE )
	{ 
		return (ULONG)STATUS_UNSUCCESSFUL;
    }

	
	statusToReturn = 0xffffffff;

#ifdef DBGOUT
    DbgPrint("[HJSCSI]DriverEntry\n");
#endif

	/* Zero out structure. */
    for (i = 0; i < sizeof(HW_INITIALIZATION_DATA); i++) {
        ((PUCHAR) & hwInitializationData)[i] = 0;
    }

	hwInitializationData.HwInitializationDataSize	= sizeof(HW_INITIALIZATION_DATA);

	// Entry Point Set
	hwInitializationData.HwFindAdapter				= HJSCSI_HwFindAdapter;
	hwInitializationData.HwInitialize				= HJSCSI_HwInitialize;
	hwInitializationData.HwStartIo					= HJSCSI_HwStartIo;
	hwInitializationData.HwInterrupt				= HJSCSI_HwInterrupt;		
	hwInitializationData.HwResetBus					= HJSCSI_HwResetBus;

	hwInitializationData.HwAdapterControl			= HJSCSI_HwAdapterControl;


	hwInitializationData.DeviceExtensionSize		= sizeof(HW_DEVICE_EXTENSION);
	hwInitializationData.SpecificLuExtensionSize	= 0;
	hwInitializationData.SrbExtensionSize			= 0;

	hwInitializationData.AdapterInterfaceType =		PCIBus;

	hwInitializationData.NumberOfAccessRanges		= 256;
	hwInitializationData.MapBuffers					= TRUE;
	hwInitializationData.NeedPhysicalAddresses		= TRUE;
	hwInitializationData.TaggedQueuing				= FALSE;
	hwInitializationData.AutoRequestSense			= TRUE;					
	hwInitializationData.MultipleRequestPerLu		= TRUE;					
	hwInitializationData.ReceiveEvent				= FALSE;

	hwInitializationData.VendorIdLength				= 4;
	hwInitializationData.VendorId					= VendorId;
	hwInitializationData.DeviceIdLength				= 4; 
	hwInitializationData.DeviceId					= DeviceId;

	adapterCount									= 0;


	newStatus = ScsiPortInitialize(	DriverObject,
									Arg,
									&hwInitializationData,
									&adapterCount);

	if (newStatus < statusToReturn)
		statusToReturn = newStatus;

#ifdef DBGOUT
	DbgPrint("[HJSCSI]statusToReturn = 0x%8X\n", statusToReturn);
#endif
	return statusToReturn;
}
