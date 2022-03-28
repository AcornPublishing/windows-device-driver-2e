// scsipassthrough.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stddef.h>

typedef struct _SCSI_PASS_THROUGH {
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    ULONG * DataBufferOffset;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
}SCSI_PASS_THROUGH, *PSCSI_PASS_THROUGH;

typedef struct _SCSI_PASS_THROUGH_DIRECT {
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    PVOID DataBuffer;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
}SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

#define SPTWB_SENSE_LENGTH  32
#define SPTWB_DATA_LENGTH   512
#define NAME_COUNT  25
#define CDB6GENERIC_LENGTH                   6
#define CDB10GENERIC_LENGTH                  10
#define CDB12GENERIC_LENGTH                  12
#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define MODE_SENSE_RETURN_ALL           0x3f

#define SCSIOP_TEST_UNIT_READY          0x00
#define SCSIOP_MODE_SENSE               0x1A
#define SCSIOP_WRITE_DATA_BUFF          0x3B
#define SCSIOP_READ_DATA_BUFF           0x3C

typedef struct _SCSI_PASS_THROUGH_WITH_BUFFERS {
    SCSI_PASS_THROUGH   spt;
    UCHAR				ucSenseBuf[SPTWB_SENSE_LENGTH];
    UCHAR				ucDataBuf[SPTWB_DATA_LENGTH];
} SCSI_PASS_THROUGH_WITH_BUFFERS, *PSCSI_PASS_THROUGH_WITH_BUFFERS;

typedef struct _SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER {
    SCSI_PASS_THROUGH_DIRECT    sptd;
    UCHAR						ucSenseBuf[SPTWB_SENSE_LENGTH];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

#define SPT_SENSE_LENGTH 32

#define IOCTL_SCSI_BASE                 FILE_DEVICE_CONTROLLER
#define FILE_DEVICE_SCSI                0x0000001b
#define IOCTL_SCSI_PASS_THROUGH         CTL_CODE(IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#include <conio.h>

VOID
PrintDataBuffer(PUCHAR DataBuffer, ULONG BufferLength)
{
    ULONG Cnt;

    printf("      00  01  02  03  04  05  06  07   08  09  0A  0B  0C  0D  0E  0F\n");
    printf("      ---------------------------------------------------------------\n");
    for (Cnt = 0; Cnt < BufferLength; Cnt++) {
       if ((Cnt) % 16 == 0) {
          printf(" %03X  ",Cnt);
          }
       printf("%02X  ", DataBuffer[Cnt]);
       if ((Cnt+1) % 8 == 0) {
          printf(" ");
          }
       if ((Cnt+1) % 16 == 0) {
          printf("\n");
          }
       }
    printf("\n\n");
}

VOID
PrintError(ULONG ErrorCode)
{
    CHAR errorBuffer[80];
    ULONG count;

    count = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  ErrorCode,
                  0,
                  errorBuffer,
                  sizeof(errorBuffer),
                  NULL
                  );

    if (count != 0) {
        printf("%s\n", errorBuffer);
    } else {
        printf("Format message failed.  Error: %d\n", GetLastError());
    }
}

VOID
PrintSenseInfo(PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb)
{
    UCHAR i;

    printf("Scsi status: %02Xh\n\n",psptwb->spt.ScsiStatus);
    if (psptwb->spt.SenseInfoLength == 0) {
       return;
       }
    printf("Sense Info -- consult SCSI spec for details\n");
    printf("-------------------------------------------------------------\n");
    for (i=0; i < psptwb->spt.SenseInfoLength; i++) {
       printf("%02X ",psptwb->ucSenseBuf[i]);
       }
    printf("\n\n");
}

VOID
PrintStatusResults(
    BOOL status,DWORD returned,PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb,
    ULONG length)
{
    ULONG errorCode;

    if (!status ) {
       printf( "Error: %d  ",
          errorCode = GetLastError() );
       PrintError(errorCode);
       return;
       }
    if (psptwb->spt.ScsiStatus) {
       PrintSenseInfo(psptwb);
       return;
       }
    else {
       printf("Scsi status: %02Xh, Bytes returned: %Xh, ",
          psptwb->spt.ScsiStatus,returned);
       printf("Data buffer length: %Xh\n\n\n",
          psptwb->spt.DataTransferLength);
       PrintDataBuffer((PUCHAR)psptwb,length);
       }
}

int main(int argc, char* argv[])
{
    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptdwb;
    BOOL status = 0;
    DWORD accessMode = 0, shareMode = 0, returned;
    ULONG length = 0,
          errorCode = 0,
          sectorSize = 512;
    HANDLE fileHandle = NULL;
    CHAR string[NAME_COUNT];
    PUCHAR dataBuffer = NULL;
	char ch;
	char drive[256];

    shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;  // default
    accessMode = GENERIC_WRITE | GENERIC_READ;       // default

    printf("Drive 이름을 입력하세요. 콜론을 함께 입력하세요(예, C: )\n");
	scanf( "%s", drive );
    printf("입력한 Drive 이름(%s)이 맞나요?(Y/N)\n", drive);
	ch = _getch();
	if(( ch != 'Y' ) && ( ch != 'y' ))
		return 0;

    sprintf(string, "\\\\.\\%s", drive);

    fileHandle = CreateFile(string,
       accessMode,
       shareMode,
       NULL,
       OPEN_EXISTING,
       0,
       NULL);

	if( fileHandle == (HANDLE)-1 )
	{
		printf("Invalid Parameter(ex, SCSIPASSTHROUGH C:\n");
		_getch();
		return 0;
	}

    dataBuffer = (PUCHAR)malloc(sectorSize);

	// ----------------- Mode Sense Test
	printf("Mode Sense Command Test\n");
    ZeroMemory(&sptwb,sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));
    sptwb.spt.Length = sizeof(SCSI_PASS_THROUGH);
    sptwb.spt.PathId = 0;
    sptwb.spt.TargetId = 1;
    sptwb.spt.Lun = 0;
    sptwb.spt.CdbLength = CDB6GENERIC_LENGTH;
    sptwb.spt.SenseInfoLength = SPT_SENSE_LENGTH;
    sptwb.spt.DataIn = SCSI_IOCTL_DATA_IN;
    sptwb.spt.DataTransferLength = 192;
    sptwb.spt.TimeOutValue = 2;
    sptwb.spt.DataBufferOffset =(ULONG *)
       offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf);
    sptwb.spt.SenseInfoOffset =
       offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucSenseBuf);
    sptwb.spt.Cdb[0] = SCSIOP_MODE_SENSE;
    sptwb.spt.Cdb[2] = MODE_SENSE_RETURN_ALL;
    sptwb.spt.Cdb[4] = 192;
    length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf) +
       sptwb.spt.DataTransferLength;

    status = DeviceIoControl(fileHandle,
                             IOCTL_SCSI_PASS_THROUGH,
                             &sptwb,
                             sizeof(SCSI_PASS_THROUGH),
                             &sptwb,
                             length,
                             &returned,
                             FALSE);
    PrintStatusResults(status,returned,&sptwb,length);

	// ----------------- Test Unit Ready Test
	printf("TestUnitReady Command Test\n");
    ZeroMemory(&sptwb,sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));
    sptwb.spt.Length = sizeof(SCSI_PASS_THROUGH);
    sptwb.spt.PathId = 0;
    sptwb.spt.TargetId = 1;
    sptwb.spt.Lun = 0;
    sptwb.spt.CdbLength = CDB6GENERIC_LENGTH;
    sptwb.spt.SenseInfoLength = SPT_SENSE_LENGTH;
    sptwb.spt.DataIn = SCSI_IOCTL_DATA_IN;
    sptwb.spt.DataTransferLength = 0;
    sptwb.spt.TimeOutValue = 2;
    sptwb.spt.DataBufferOffset = 0;
    sptwb.spt.SenseInfoOffset =
       offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucSenseBuf);
    sptwb.spt.Cdb[0] = SCSIOP_TEST_UNIT_READY;
    length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf);

    status = DeviceIoControl(fileHandle,
                             IOCTL_SCSI_PASS_THROUGH,
                             &sptwb,
                             sizeof(SCSI_PASS_THROUGH),
                             &sptwb,
                             length,
                             &returned,
                             FALSE);

    PrintStatusResults(status,returned,&sptwb,length);

	// ----------------- Write_Data_Buff Test
	printf("Write Data Buff Command Test\n");
	{
		// 512바이트의 데이타샘플을 준비합니다
		memset( dataBuffer, 0x55, sectorSize );
	}
    ZeroMemory(&sptdwb, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));
    sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptdwb.sptd.PathId = 0;
    sptdwb.sptd.TargetId = 1;
    sptdwb.sptd.Lun = 0;
    sptdwb.sptd.CdbLength = CDB10GENERIC_LENGTH;
    sptdwb.sptd.SenseInfoLength = SPT_SENSE_LENGTH;
    sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
    sptdwb.sptd.DataTransferLength = sectorSize;
    sptdwb.sptd.TimeOutValue = 2;
    sptdwb.sptd.DataBuffer = dataBuffer;
    sptdwb.sptd.SenseInfoOffset =
       offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER,ucSenseBuf);
    sptdwb.sptd.Cdb[0] = SCSIOP_WRITE_DATA_BUFF;
    sptdwb.sptd.Cdb[1] = 2;                         // Data mode
    sptdwb.sptd.Cdb[7] = (UCHAR)(sectorSize >> 8);  // Parameter List length
    sptdwb.sptd.Cdb[8] = 0;
    length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);

    status = DeviceIoControl(fileHandle,
                             IOCTL_SCSI_PASS_THROUGH_DIRECT,
                             &sptdwb,
                             length,
                             &sptdwb,
                             length,
                             &returned,
                             FALSE);
    PrintStatusResults(status,returned,&sptwb,length);
	
    if ((sptdwb.sptd.ScsiStatus == 0) && (status != 0)) {
       PrintDataBuffer(dataBuffer,sptdwb.sptd.DataTransferLength);
    }

	// ----------------- Read_Data_Buff Test
	printf("Read Data Buff Command Test\n");
    ZeroMemory(&sptdwb, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));
    sptdwb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptdwb.sptd.PathId = 0;
    sptdwb.sptd.TargetId = 1;
    sptdwb.sptd.Lun = 0;
    sptdwb.sptd.CdbLength = CDB10GENERIC_LENGTH;
    sptdwb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    sptdwb.sptd.SenseInfoLength = SPT_SENSE_LENGTH;
    sptdwb.sptd.DataTransferLength = sectorSize;
    sptdwb.sptd.TimeOutValue = 2;
    sptdwb.sptd.DataBuffer = dataBuffer;
    sptdwb.sptd.SenseInfoOffset =
       offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER,ucSenseBuf);
    sptdwb.sptd.Cdb[0] = SCSIOP_READ_DATA_BUFF;
    sptdwb.sptd.Cdb[1] = 2;                         // Data mode
    sptdwb.sptd.Cdb[7] = (UCHAR)(sectorSize >> 8);  // Parameter List length
    sptdwb.sptd.Cdb[8] = 0;
    length = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);

    status = DeviceIoControl(fileHandle,
                             IOCTL_SCSI_PASS_THROUGH_DIRECT,
                             &sptdwb,
                             length,
                             &sptdwb,
                             length,
                             &returned,
                             FALSE);
    PrintStatusResults(status,returned,&sptwb,length);
    
    if ((sptdwb.sptd.ScsiStatus == 0) && (status != 0)) {
       PrintDataBuffer(dataBuffer,sptdwb.sptd.DataTransferLength);
    }



	
	
	CloseHandle( fileHandle );
	free(dataBuffer);
	_getch();

	return 0;
}
