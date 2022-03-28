#include "HJSTOR.h"

/*
Scsi Miniport와 다른점중 가장 중요한것은!!!
사용자의 버퍼가 전달되는 방식이다. 
Stor Miniport에서는 사용자의 버퍼가 Scatter Gather 방식의 물리주소형태로 제공된다.
램디스크를 구현하기 때문에, 우리는 이 Scatter Gather 주소를 구해온뒤, 이것을 가상주소로 변환해서 사용해야 한다
*/
BOOLEAN
HJSTOR_Hw_BuildIo(
	PVOID					pContext,
	PSCSI_REQUEST_BLOCK		srb
	)
{
	PSRB_EXTENSION			pSrbExtension		= NULL;
	PHW_DEVICE_EXTENSION	DeviceExtension	= NULL;
	PCDB					cdb					= NULL;
	ULONG					diskSectors;
	ULONG_PTR				SrbBuffer;
	LARGE_INTEGER			blk_offset;
	ULONG					blk_length;
	PSTOR_SCATTER_GATHER_LIST	pScatterGatherList	= NULL;
	UCHAR					status			= SRB_STATUS_SUCCESS;

	if ( (pContext == NULL) || (srb == NULL) )
	{
		return FALSE;
	}

	if ( srb->Function != SRB_FUNCTION_EXECUTE_SCSI )
	{
		return TRUE; // StartIo로 명령을 보내라는 의미
	}

	cdb = (PCDB) srb->Cdb;

	// READ, WRITE 명령어만 BuildIo에서 처리하는 모습을 취해서 StartIo와 연동되는 방법을 보여준다
	// 나머지 명령은 모두 다 StartIO 로 보내서 처리한다.	
	if ( cdb->CDB6GENERIC.OperationCode != SCSIOP_READ 
		&& cdb->CDB6GENERIC.OperationCode != SCSIOP_READ6 
		&& cdb->CDB6GENERIC.OperationCode != SCSIOP_READ12 
		&& cdb->CDB6GENERIC.OperationCode != SCSIOP_READ16 
		&& cdb->CDB6GENERIC.OperationCode != SCSIOP_WRITE 
		&& cdb->CDB6GENERIC.OperationCode != SCSIOP_WRITE6 
		&& cdb->CDB6GENERIC.OperationCode != SCSIOP_WRITE12 
		&& cdb->CDB6GENERIC.OperationCode != SCSIOP_WRITE16 
		)
	{
		return TRUE; // STOR 에게 StartIo로 명령어를 전송할것을 요청한다
	}

	DeviceExtension	= (PHW_DEVICE_EXTENSION) pContext;
	pSrbExtension		= (PSRB_EXTENSION) srb->SrbExtension;

/*
	pScatterGatherList = StorPortGetScatterGatherList( DeviceExtension, srb );
	if( pScatterGatherList == NULL )
	{
		return TRUE; // StartIo로 명령을 보내라는 의미
	}
*/

	diskSectors	= DeviceExtension->TotalBlocks;

	cdb = (PCDB)srb->Cdb;

	if (cdb->CDB6GENERIC.OperationCode == SCSIOP_READ)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_READ\n");
#endif

		blk_offset.u.HighPart = 0;
		blk_offset.u.LowPart = (ULONG)(cdb->CDB10.LogicalBlockByte3 |
						cdb->CDB10.LogicalBlockByte2 << 8 |
						cdb->CDB10.LogicalBlockByte1 << 16 |
						cdb->CDB10.LogicalBlockByte0 << 24);
	
		blk_length = (ULONG)(cdb->CDB10.TransferBlocksLsb |
						cdb->CDB10.TransferBlocksMsb << 8);

		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/

#ifdef DBGOUT
		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory( (PVOID)SrbBuffer, (PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );
        status = SRB_STATUS_SUCCESS;
	}
	else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_WRITE\n");
#endif
		blk_offset.u.HighPart = 0;
		blk_offset.u.LowPart = (ULONG)(cdb->CDB10.LogicalBlockByte3 |
						cdb->CDB10.LogicalBlockByte2 << 8 |
						cdb->CDB10.LogicalBlockByte1 << 16 |
						cdb->CDB10.LogicalBlockByte0 << 24);
	
		blk_length = (ULONG)(cdb->CDB10.TransferBlocksLsb |
						cdb->CDB10.TransferBlocksMsb << 8);

		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/

#ifdef DBGOUT
		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory((PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (PVOID)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

        status = SRB_STATUS_SUCCESS;
	}
	else if(cdb->CDB6GENERIC.OperationCode == SCSIOP_READ16)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_READ16\n");
#endif
		blk_offset.u.HighPart = (ULONG)(cdb->CDB16.LogicalBlock[3] |
						cdb->CDB16.LogicalBlock[2] << 8 |
						cdb->CDB16.LogicalBlock[1] << 16 |
						cdb->CDB16.LogicalBlock[0] << 24);
		
		blk_offset.u.LowPart = (ULONG)(cdb->CDB16.LogicalBlock[7] |
						cdb->CDB16.LogicalBlock[6] << 8 |
						cdb->CDB16.LogicalBlock[5] << 16 |
						cdb->CDB16.LogicalBlock[4] << 24);
	
		blk_length = (ULONG)(cdb->CDB16.TransferLength[3] |
						cdb->CDB16.TransferLength[2] << 8 |
						cdb->CDB16.TransferLength[1] << 16 |
						cdb->CDB16.TransferLength[0] << 24);
		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/

#ifdef DBGOUT
		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory((PVOID)SrbBuffer, (PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );

        status = SRB_STATUS_SUCCESS;
	}
	else if(cdb->CDB6GENERIC.OperationCode == SCSIOP_READ12)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_READ12\n");
#endif
		blk_offset.u.HighPart = 0;

		blk_offset.u.LowPart = (ULONG)(cdb->CDB12.LogicalBlock[3] |
						cdb->CDB12.LogicalBlock[2] << 8 |
						cdb->CDB12.LogicalBlock[1] << 16 |
						cdb->CDB12.LogicalBlock[0] << 24);
	
		blk_length = (ULONG)(cdb->CDB12.TransferLength[3] |
						cdb->CDB12.TransferLength[2] << 8 |
						cdb->CDB12.TransferLength[1] << 16 |
						cdb->CDB12.TransferLength[0] << 24);

		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/

#ifdef DBGOUT
		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory((PVOID)SrbBuffer, (PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );

        status = SRB_STATUS_SUCCESS;
	}
	else if(cdb->CDB6GENERIC.OperationCode == SCSIOP_READ6)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_READ6\n");
#endif
		blk_offset.u.HighPart = 0;

		blk_offset.u.LowPart = (ULONG)(cdb->CDB6READWRITE.LogicalBlockLsb |
						cdb->CDB6READWRITE.LogicalBlockMsb0 << 8 |
						cdb->CDB6READWRITE.LogicalBlockMsb1 << 16);
	
		blk_length = (ULONG)(cdb->CDB6READWRITE.TransferBlocks);

		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/

		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory((PVOID)SrbBuffer, (PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );

        status = SRB_STATUS_SUCCESS;
	}
	else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE16)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_WRITE16\n");
#endif
		blk_offset.u.HighPart = (ULONG)(cdb->CDB16.LogicalBlock[3] |
						cdb->CDB16.LogicalBlock[2] << 8 |
						cdb->CDB16.LogicalBlock[1] << 16 |
						cdb->CDB16.LogicalBlock[0] << 24);
		
		blk_offset.u.LowPart = (ULONG)(cdb->CDB16.LogicalBlock[7] |
						cdb->CDB16.LogicalBlock[6] << 8 |
						cdb->CDB16.LogicalBlock[5] << 16 |
						cdb->CDB16.LogicalBlock[4] << 24);
	
		blk_length = (ULONG)(cdb->CDB16.TransferLength[3] |
						cdb->CDB16.TransferLength[2] << 8 |
						cdb->CDB16.TransferLength[1] << 16 |
						cdb->CDB16.TransferLength[0] << 24);

		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/
#ifdef DBGOUT
		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory((PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (PVOID)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

        status = SRB_STATUS_SUCCESS;
	}
	else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE12)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_WRITE12\n");
#endif
		blk_offset.u.HighPart = 0;

		blk_offset.u.LowPart = (ULONG)(cdb->CDB12.LogicalBlock[3] |
						cdb->CDB12.LogicalBlock[2] << 8 |
						cdb->CDB12.LogicalBlock[1] << 16 |
						cdb->CDB12.LogicalBlock[0] << 24);
	
		blk_length = (ULONG)(cdb->CDB12.TransferLength[3] |
						cdb->CDB12.TransferLength[2] << 8 |
						cdb->CDB12.TransferLength[1] << 16 |
						cdb->CDB12.TransferLength[0] << 24);

		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/

#ifdef DBGOUT
		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory((PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (PVOID)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

        status = SRB_STATUS_SUCCESS;
	}
	else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE6)
	{
#ifdef DBGOUT
		DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_WRITE6\n");
#endif
		blk_offset.u.HighPart = 0;

		blk_offset.u.LowPart = (ULONG)(cdb->CDB6READWRITE.LogicalBlockLsb |
						cdb->CDB6READWRITE.LogicalBlockMsb0 << 8 |
						cdb->CDB6READWRITE.LogicalBlockMsb1 << 16);
	
		blk_length = (ULONG)(cdb->CDB6READWRITE.TransferBlocks);

		/*
		blk_offset : 시작 섹터
		blk_length : 크기 섹터
		*/

#ifdef DBGOUT
		DbgPrint("[HJSTOR]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

		StorPortGetSystemAddress( DeviceExtension, srb, (PVOID *)&SrbBuffer );
		StorPortMoveMemory((PVOID)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (PVOID)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

        status = SRB_STATUS_SUCCESS;
	}
	else
	{
		// 나머지명령어들은 모두 StartIo로 보내라
		return TRUE;

	}

	srb->SrbStatus = (UCHAR)status;
	StorPortNotification(RequestComplete,   DeviceExtension, srb);
	StorPortNotification(NextRequest,  DeviceExtension, NULL);

	return FALSE; // StartIo로 명령을 보내지 말라는 의미
}
