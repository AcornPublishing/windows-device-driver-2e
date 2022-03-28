#include "HJSCSI.h"

#ifdef DBGOUT
void printCdb( PCDB	cdb ) // 31bytes
{
	unsigned char *pCdbTemp;

	pCdbTemp = (unsigned char *)cdb;
	
	DbgPrint( "[HJSCSI] %2x %2x %2x %2x %2x %2x %2x %2x\n", 
		pCdbTemp[0], pCdbTemp[1], pCdbTemp[2], pCdbTemp[3], pCdbTemp[4], pCdbTemp[5], pCdbTemp[6], pCdbTemp[7] );
	DbgPrint( "[HJSCSI] %2x %2x %2x %2x %2x %2x %2x %2x\n", 
		pCdbTemp[8], pCdbTemp[9], pCdbTemp[10], pCdbTemp[11], pCdbTemp[12], pCdbTemp[13], pCdbTemp[14], pCdbTemp[15] );
	DbgPrint( "[HJSCSI] %2x %2x %2x %2x %2x %2x %2x %2x\n", 
		pCdbTemp[16], pCdbTemp[17], pCdbTemp[18], pCdbTemp[19], pCdbTemp[20], pCdbTemp[21], pCdbTemp[22], pCdbTemp[23] );
	DbgPrint( "[HJSCSI] %2x %2x %2x %2x %2x %2x %2x\n", 
		pCdbTemp[24], pCdbTemp[25], pCdbTemp[26], pCdbTemp[27], pCdbTemp[28], pCdbTemp[29], pCdbTemp[30], pCdbTemp[31] );
}
#endif

/***********************************************************************************
함수명 : HJSCSI_HwStartIo
인  자 :
			IN PVOID HwDeviceExtension
			IN PSCSI_REQUEST_BLOCK Srb
리턴값 : BOOLEAN
설  명 : StartIo

램디스크의 특성상, 하드웨어 인터럽트를 가지지 않는다. 
따라서, 항상 모든 Srb는 동기식으로 처리한다
***********************************************************************************/

BOOLEAN HJSCSI_HwStartIo(IN PVOID HwDeviceExtension,IN PSCSI_REQUEST_BLOCK srb)
{
    PHW_DEVICE_EXTENSION	DeviceExtension = HwDeviceExtension;
    ULONG					status  = SRB_STATUS_SUCCESS;
	PCDB					cdb;
	ULONG					i;
	ULONG					diskSectors	= DeviceExtension->TotalBlocks;

	// Read / Write Command 처리용..
	ULONG_PTR				SrbBuffer;
	LARGE_INTEGER			blk_offset;
	ULONG					blk_length;

	PINQUIRYDATA			pInquiryData = NULL;

	cdb = (PCDB)srb->Cdb;

	if (srb->Lun != 0 || srb->TargetId != 0 || srb->SrbStatus != 0 || srb->PathId != 0)
	{	
		status = SRB_STATUS_SELECTION_TIMEOUT;
		srb->SrbStatus = (UCHAR)status;
    
		ScsiPortNotification(RequestComplete,   DeviceExtension, srb);
        ScsiPortNotification(NextRequest,  DeviceExtension, NULL);

#ifdef DBGOUT
	    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, error\n");
#endif
		return TRUE;
	}

	switch (srb->Function)
	{
	case SRB_FUNCTION_EXECUTE_SCSI:
		if (cdb->CDB6GENERIC.OperationCode == SCSIOP_READ)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_READ\n");
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
			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

#ifdef DBGOUT
			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif
			ScsiPortMoveMemory( (void *)SrbBuffer, (void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_WRITE\n");
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
			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

			ScsiPortMoveMemory((void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (void *)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if(cdb->CDB6GENERIC.OperationCode == SCSIOP_READ16)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_READ16\n");
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
			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

			ScsiPortMoveMemory((void *)SrbBuffer, (void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if(cdb->CDB6GENERIC.OperationCode == SCSIOP_READ12)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_READ12\n");
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
			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

			ScsiPortMoveMemory((void *)SrbBuffer, (void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if(cdb->CDB6GENERIC.OperationCode == SCSIOP_READ6)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_READ6\n");
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

			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );


			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

			ScsiPortMoveMemory((void *)SrbBuffer, (void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE16)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_WRITE16\n");
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
			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

			ScsiPortMoveMemory((void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (void *)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE12)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_WRITE12\n");
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
			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

			ScsiPortMoveMemory((void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (void *)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE6)
		{
#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_WRITE6\n");
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
			DbgPrint("[HJSCSI]param, blk_length = 0x%8X, blk_offset = 0x%u\n", blk_length, blk_offset.QuadPart  );
#endif

			SrbBuffer		= (ULONG_PTR)srb->DataBuffer;

			ScsiPortMoveMemory((void *)(DeviceExtension->Base + blk_offset.QuadPart * SIZE_LOGICAL_BLOCK), (void *)SrbBuffer, blk_length * SIZE_LOGICAL_BLOCK );

            status = SRB_STATUS_SUCCESS;
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_READ_CAPACITY)
		{
			PREAD_CAPACITY_DATA capacity;
			ULONG			   logicalBlockAddress;
			ULONG			   bytesPerBlock;

#ifdef DBGOUT
		    DbgPrint("[HJSCSI]HJSCSI_HwStartIo, SCSIOP_READ_CAPACITY\n");
#endif
			capacity = (PREAD_CAPACITY_DATA)srb->DataBuffer;

			logicalBlockAddress = diskSectors-256;
			bytesPerBlock		= SIZE_LOGICAL_BLOCK;

#ifdef DBGOUT
		    DbgPrint("[HJSCSI]param, bytesPerBlock = 0x%8X, logicalBlockAddress = 0x%8X\n", bytesPerBlock, logicalBlockAddress );
#endif
			REVERSE_BYTES(&capacity->LogicalBlockAddress, &logicalBlockAddress);
			REVERSE_BYTES(&capacity->BytesPerBlock, &bytesPerBlock);
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_TEST_UNIT_READY)
		{
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_VERIFY)
		{
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_INQUIRY)
		{
			pInquiryData = (PINQUIRYDATA)srb->DataBuffer;

			for (i = 0; i < srb->DataTransferLength; i++) {
				((PUCHAR)pInquiryData)[i] = 0;
			}

			pInquiryData->DeviceType			= DIRECT_ACCESS_DEVICE;			
			pInquiryData->Versions				= 3;						//
			pInquiryData->AdditionalLength		= 91;						// 전체 길이 96-5
			pInquiryData->SoftReset				= 0;
			pInquiryData->CommandQueue			= 1;
			pInquiryData->TransferDisable		= 0;
			pInquiryData->LinkedCommands		= 1;
			pInquiryData->Synchronous			= 1;
			pInquiryData->Wide16Bit				= 1;
			pInquiryData->Wide32Bit				= 0;
			pInquiryData->RelativeAddressing	= 0;

			//
			// Use 'intel' for now, should key off of PCI Ids
			// to determine whose name should go here.
			//
			pInquiryData->VendorId[0] = 'H';
			pInquiryData->VendorId[1] = 'a';
			pInquiryData->VendorId[2] = 'j';
			pInquiryData->VendorId[3] = 'e';
			pInquiryData->VendorId[4] = ' ';
			pInquiryData->VendorId[5] = ' ';

			//
			// For some reason, there wasn't any volume name
			// info. Make some stuff up.
			//
			pInquiryData->ProductId[0] = 'H';
			pInquiryData->ProductId[1] = 'J';
			pInquiryData->ProductId[2] = 'S';
			pInquiryData->ProductId[3] = 'C';
			pInquiryData->ProductId[4] = 'S';
			pInquiryData->ProductId[5] = 'I';
			pInquiryData->ProductId[6] = ' ';
			pInquiryData->ProductId[7] = 'M';
			pInquiryData->ProductId[8] = 'i';
			pInquiryData->ProductId[9] = 'n';
			pInquiryData->ProductId[10] = 'i';
			pInquiryData->ProductId[11] = 'D';
			pInquiryData->ProductId[12] = 'e';
			pInquiryData->ProductId[13] = 'v';
	
			pInquiryData->ProductRevisionLevel[0] = '1';
			pInquiryData->ProductRevisionLevel[1] = '.';
			pInquiryData->ProductRevisionLevel[2] = '0';
			pInquiryData->ProductRevisionLevel[3] = '0';
			
			// SAM-3 ANSI
			pInquiryData->Reserved3[2]		= 0x0;
			pInquiryData->Reserved3[3]		= 0x77;

			// SPC-3 ANSI
			pInquiryData->Reserved3[4]		= 0x3;
			pInquiryData->Reserved3[5]		= 0x14;

			// SBC-2 ANSI
			pInquiryData->Reserved3[6]		= 0x3;
			pInquiryData->Reserved3[7]		= 0x3D;

			// SAS-1.1 rev 10
			pInquiryData->Reserved3[8]		= 0xC;
			pInquiryData->Reserved3[9]		= 0xF;
		}
		else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_MODE_SENSE)
		{
	        for (i = 0; i < srb->DataTransferLength; i++) {
                ((PUCHAR)srb->DataBuffer)[i] = 0;
            }
			if (cdb->MODE_SENSE.Pc != 1)
			{
				PMODE_PARAMETER_HEADER  header;

				ULONG diskSize		= DeviceExtension->TotalBlocks; //DeviceExtension->DiskLength ;

				header = (PMODE_PARAMETER_HEADER)srb->DataBuffer;

				header->ModeDataLength = sizeof(MODE_PARAMETER_HEADER);
				header->MediumType = 0;
				header->DeviceSpecificParameter = 0;
				
				if (!cdb->MODE_SENSE.Dbd && (cdb->MODE_SENSE.AllocationLength >= (header->ModeDataLength + sizeof(MODE_PARAMETER_BLOCK))))
				{
					PMODE_PARAMETER_BLOCK   block;
					ULONG				   logicalBlockAddress;
					ULONG				   bytesPerBlock;

					block = (PMODE_PARAMETER_BLOCK)((PUCHAR)header + header->ModeDataLength);
					logicalBlockAddress = diskSize;
					bytesPerBlock = SIZE_LOGICAL_BLOCK;

					header->BlockDescriptorLength = sizeof(MODE_PARAMETER_BLOCK);
					header->ModeDataLength += sizeof(MODE_PARAMETER_BLOCK);

					block->DensityCode = 0;

					block->NumberOfBlocks[0] = (UCHAR)((logicalBlockAddress >> 16) & 0xFF);
					block->NumberOfBlocks[1] = (UCHAR)((logicalBlockAddress >> 8) & 0xFF);
					block->NumberOfBlocks[2] = (UCHAR)(logicalBlockAddress & 0xFF);

					block->BlockLength[0] = (UCHAR)((bytesPerBlock >> 16) & 0xFF);
					block->BlockLength[1] = (UCHAR)((bytesPerBlock >> 8) & 0xFF);
					block->BlockLength[2] = (UCHAR)(bytesPerBlock & 0xFF);
				}
				else
				{
					header->BlockDescriptorLength = 0;
				}

				if ((cdb->MODE_SENSE.AllocationLength >= (header->ModeDataLength + sizeof(MODE_PAGE_FORMAT_DEVICE))) && 
					((cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) || 
					 (cdb->MODE_SENSE.PageCode == MODE_PAGE_FORMAT_DEVICE)))
				{
					PMODE_FORMAT_PAGE format;

					format = (PMODE_FORMAT_PAGE)((PUCHAR)header + header->ModeDataLength);
					header->ModeDataLength += sizeof(MODE_FORMAT_PAGE);

					format->PageCode = MODE_PAGE_FORMAT_DEVICE;
					format->PageLength = sizeof(MODE_FORMAT_PAGE);
					format->TracksPerZone[0] = 0;		   // we have only one zone
					format->TracksPerZone[1] = 0;
					format->AlternateSectorsPerZone[0] = 0;
					format->AlternateSectorsPerZone[1] = 0;
					format->AlternateTracksPerZone[0] = 0;
					format->AlternateTracksPerZone[1] = 0;
					format->AlternateTracksPerLogicalUnit[0] = 0;
					format->AlternateTracksPerLogicalUnit[1] = 0;
					format->SectorsPerTrack[0] = (UCHAR)((diskSize >> 8) & 0xFF);
					format->SectorsPerTrack[1] = (UCHAR)((diskSize >> 0) & 0xFF);
					format->BytesPerPhysicalSector[0] = (UCHAR)((512 >> 8) & 0xFF);
					format->BytesPerPhysicalSector[1] = (UCHAR)((512 >> 0) & 0xFF);
					format->SoftSectorFormating = 1;
				}

				if ((cdb->MODE_SENSE.AllocationLength >= (header->ModeDataLength + sizeof(MODE_DISCONNECT_PAGE))) && 
					((cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) || 
					 (cdb->MODE_SENSE.PageCode == MODE_PAGE_DISCONNECT)))
				{
					PMODE_DISCONNECT_PAGE disconnectPage;

					disconnectPage = (PMODE_DISCONNECT_PAGE)((PUCHAR)header + header->ModeDataLength);
					header->ModeDataLength += sizeof(MODE_DISCONNECT_PAGE);

					disconnectPage->PageCode = MODE_PAGE_DISCONNECT;
					disconnectPage->PageLength = sizeof(MODE_DISCONNECT_PAGE);
					disconnectPage->BufferFullRatio = 0xFF;
					disconnectPage->BufferEmptyRatio = 0xFF;
					disconnectPage->BusInactivityLimit[0] = 0;
					disconnectPage->BusInactivityLimit[1] = 1;
					disconnectPage->BusDisconnectTime[0] = 0;
					disconnectPage->BusDisconnectTime[1] = 1;
					disconnectPage->BusConnectTime[0] = 0;
					disconnectPage->BusConnectTime[1] = 1;
					disconnectPage->MaximumBurstSize[0] = 0;
					disconnectPage->MaximumBurstSize[1] = 1;
					disconnectPage->DataTransferDisconnect = 1;
				}

				srb->DataTransferLength = header->ModeDataLength;
			}

		}
		else if(cdb->CDB12.OperationCode == SCSIOP_REPORT_LUNS)
		{
		}
		break;
	case SRB_FUNCTION_CLAIM_DEVICE:
		srb->DataBuffer = DeviceExtension; 
		break;
	case SRB_FUNCTION_IO_CONTROL:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_RECEIVE_EVENT:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_RELEASE_QUEUE:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_ATTACH_DEVICE:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_RELEASE_DEVICE:
		break;
	case SRB_FUNCTION_SHUTDOWN:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_FLUSH:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_ABORT_COMMAND:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_RELEASE_RECOVERY:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_RESET_BUS:
		if (!HJSCSI_HwResetBus(DeviceExtension, srb->PathId)) {
            status = SRB_STATUS_ERROR;
        } else {
            status = SRB_STATUS_SUCCESS;
		}
		break;
	case SRB_FUNCTION_RESET_DEVICE:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_TERMINATE_IO:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_FLUSH_QUEUE:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_REMOVE_DEVICE:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_WMI:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_LOCK_QUEUE:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_UNLOCK_QUEUE:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	case SRB_FUNCTION_RESET_LOGICAL_UNIT:
		status = SRB_STATUS_INVALID_REQUEST;
		break;
	default:
		break;
	}

	if (status != SRB_STATUS_PENDING)
	{
		srb->SrbStatus = (UCHAR)status;
		ScsiPortNotification(RequestComplete,   DeviceExtension, srb);
		ScsiPortNotification(NextRequest,  DeviceExtension, NULL);

		DeviceExtension->CurrentSrb			= NULL;
	}

    return TRUE;
}
