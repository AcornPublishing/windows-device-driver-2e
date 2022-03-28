
#ifndef _HJSCSI_H_
#define _HJSCSI_H_

#include <ntddk.h>               /*  from NT DDK */
#include "scsi.h"               /*  from NT DDK */

//#define USE_INTERRUPT_MODE	1

#define	SIZE_LOGICAL_BLOCK				(0x200UL)  // 512
//#define PAGE_SIZE						(0x1000UL) // PAGE_SIZE == 4K		

#define MAX_DMA_BLOCK_SIZE 				(0x80000UL)

#define MAX_PHYSICAL_PAGES				(0x400UL )//0x1000UL //
#define MAX_DMA_TRANSFER_SIZE			(MAX_PHYSICAL_PAGES * PAGE_SIZE)		
#define DEFAULT_DISK_SIZE				(((ULONGLONG)1024*1024*16)) // 총 바이트수
#define NUM_TOTAL_BLOCKS				(((ULONG)2*1024*16)) // 총 섹터수

// End..



//HW_DEVICE_EXTENSION 구조체 선언 
typedef struct _HW_DEVICE_EXTENSION
{
    PSCSI_REQUEST_BLOCK CurrentSrb;   
	ULONGLONG			DiskLength;
	ULONG				TotalBlocks;
	PUCHAR				Base;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

#pragma pack()

ULONG HJSCSI_HwFindAdapter(
    IN PVOID HwDeviceExtension,
    IN PVOID Context,
    IN PVOID BusInformation,
    IN PCHAR ArgumentString,
    IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
    OUT PBOOLEAN Again
    );

BOOLEAN	HJSCSI_HwInitialize(
	IN PVOID HwDeviceExtension
	);

BOOLEAN HJSCSI_HwStartIo(
	IN PVOID HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb
	);

BOOLEAN HJSCSI_HwInterrupt(
	IN PVOID HwDeviceExtension
	);

BOOLEAN HJSCSI_HwResetBus(
	IN PVOID HwDeviceExtension,
	IN ULONG PathId
	);

SCSI_ADAPTER_CONTROL_STATUS HJSCSI_HwAdapterControl(
    IN PVOID HwDeviceExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE ControlType,
    IN PVOID Parameters
    );

#endif // _HJSCSI_H_


/////////////////////////////////////////////////////////////////////////////////////////