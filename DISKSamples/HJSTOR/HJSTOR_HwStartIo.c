#include "HJSTOR.h"

void translate( PUCHAR pBuffer, int Size )
{
	int count;
	for( count = 0 ; count < Size ; count ++ )
	{
		pBuffer[count] = ~pBuffer[count];
	}
}

BOOLEAN
HJSTOR_HwStartIo(
	PVOID					pContext,
	PSCSI_REQUEST_BLOCK		pSrb
	)
{
	PSRB_EXTENSION			pSrbExtension		= NULL;
	PHW_DEVICE_EXTENSION	pDeviceExtension	= NULL;
	UCHAR					nSrbStatus			= SRB_STATUS_SUCCESS;
	PCDB					cdb					= NULL;
	ULONG					diskSectors			= 0; 
	BOOLEAN					bRet;

#ifdef DBGOUT
	DbgPrint("[HJSTOR]HJSTOR_HwStartIo\n");
#endif

	if ( (pContext == NULL) || (pSrb == NULL) )
	{
		return FALSE;
	}

	pDeviceExtension	= (PHW_DEVICE_EXTENSION) pContext;
	pSrbExtension		= (PSRB_EXTENSION) pSrb->SrbExtension;

	cdb			= (PCDB)pSrb->Cdb;
	diskSectors	= pDeviceExtension->TotalBlocks;


	switch ( pSrb->Function )
	{
		case SRB_FUNCTION_EXECUTE_SCSI:
		{
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_EXECUTE_SCSI(cdb=0x%8X)\n", cdb->CDB6GENERIC.OperationCode);
#endif

			bRet = HJSTOR_Hw_BuildIo( pContext, pSrb );
			if( bRet != TRUE )
			{ // 처리했다는 의미..
				return TRUE;
			}

			if ( cdb->CDB6GENERIC.OperationCode == SCSIOP_REPORT_LUNS )
			{
				struct _REPORT_LUNS*	pReportLUNs = NULL;
				PLUN_LIST				pLunList	= NULL;

				// 12 Bytes Check!!
				if ( pSrb->CdbLength != 12 )
				{
#ifdef DBGOUT
					DbgPrint("[HJSTOR]param, pSrb->CdbLength != 12\n" );
#endif
					break;
				}

				pReportLUNs = (struct _REPORT_LUNS*) pSrb->Cdb;
				if ( pReportLUNs->AllocationLength <= 0 )
				{
#ifdef DBGOUT
					DbgPrint("[HJSTOR]param, pReportLUNs->AllocationLength <= 0\n" );
#endif
					break;
				}

				if ( pSrb->TargetId != 0 )
				{
#ifdef DBGOUT
					DbgPrint("[HJSTOR]param, pSrb->TargetId != 0\n" );
#endif
					pSrb->ScsiStatus = SCSISTAT_CHECK_CONDITION;
					nSrbStatus = SRB_STATUS_NO_DEVICE;
					break;
				}

				// LUN Info
				pLunList = (PLUN_LIST) pSrb->DataBuffer;
				memset( (PCHAR)pLunList, 0, pSrb->DataTransferLength );

				pLunList->LunListLength[3]	= 8; // Lun count;
				pLunList->Lun[0][0]			= 0; 
				pLunList->Lun[0][1]			= 0; 

				// Data Length
				pSrb->DataTransferLength	= sizeof(LUN_LIST) * 1;

				pSrb->ScsiStatus = SCSISTAT_GOOD;
				nSrbStatus = SRB_STATUS_SUCCESS;
#ifdef DBGOUT
				DbgPrint("[HJSTOR]param, pSrb->DataTransferLength = 0x%8X\n", pSrb->DataTransferLength );
#endif
			}
			else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_READ_CAPACITY)
			{
				PREAD_CAPACITY_DATA		capacity				= NULL;
				ULONG					logicalBlockAddress		= 0;
				ULONG					bytesPerBlock			= 0;

#ifdef DBGOUT
				DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SCSIOP_READ_CAPACITY\n");
#endif
				capacity = (PREAD_CAPACITY_DATA)pSrb->DataBuffer;

				logicalBlockAddress = diskSectors-256;
				bytesPerBlock		= SIZE_LOGICAL_BLOCK;

				REVERSE_BYTES(&capacity->LogicalBlockAddress, &logicalBlockAddress);
				REVERSE_BYTES(&capacity->BytesPerBlock, &bytesPerBlock);

#ifdef DBGOUT
			    DbgPrint("[HJSTOR]param, Lun = %d, bytesPerBlock = 0x%8X, logicalBlockAddress = 0x%8X\n", pSrb->Lun, bytesPerBlock, logicalBlockAddress );
#endif

				pSrb->ScsiStatus = SCSISTAT_GOOD;
			}
			else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_WRITE_DATA_BUFF)
			{
				if( pSrb->DataTransferLength != 512 )
				{
					nSrbStatus = SRB_STATUS_INVALID_REQUEST;
					break;
				}
				memcpy( pDeviceExtension->DataBuff, pSrb->DataBuffer, pSrb->DataTransferLength );
				translate( pDeviceExtension->DataBuff, pSrb->DataTransferLength ); // SCSIOP_READ_DATA_BUFF 명령어를 위해서 미리 준비한다
			}
			else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_READ_DATA_BUFF)
			{
				if( pSrb->DataTransferLength != 512 )
				{
					nSrbStatus = SRB_STATUS_INVALID_REQUEST;
					break;
				}
				memcpy( pSrb->DataBuffer, pDeviceExtension->DataBuff, pSrb->DataTransferLength );
			}
			else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_TEST_UNIT_READY)
			{
			}
			else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_VERIFY)
			{
			}
			else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_INQUIRY)
			{
				PINQUIRYDATA	pInquiryData	= NULL;
				ULONG			i				= 0;

#ifdef DBGOUT
			    DbgPrint("[HJSTOR]param, SCSIOP_INQUIRY\n" );
#endif
				pInquiryData = (PINQUIRYDATA)pSrb->DataBuffer;
				if ( pInquiryData == NULL )
				{
					break;
				}

				for ( i = 0; i < pSrb->DataTransferLength; i++ )
				{
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
				pInquiryData->ProductId[3] = 'T';
				pInquiryData->ProductId[4] = 'O';
				pInquiryData->ProductId[5] = 'R';
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

#ifdef DBGOUT
			    DbgPrint("[HJSTOR]param, pSrb->ScsiStatus = SCSISTAT_GOOD\n" );
#endif

				pSrb->ScsiStatus = SCSISTAT_GOOD;
			}
			else if (cdb->CDB6GENERIC.OperationCode == SCSIOP_MODE_SENSE)
			{
				ULONG	i = 0;

#ifdef DBGOUT
			    DbgPrint("[HJSTOR]param, SCSIOP_MODE_SENSE\n" );
#endif
				if ( (pSrb->DataBuffer == NULL) || (pSrb->DataTransferLength <= 0) )
				{
					break;
				}

				for ( i = 0; i < pSrb->DataTransferLength; i++ )
				{
					((PUCHAR)pSrb->DataBuffer)[i] = 0;
				}

				if ( cdb->MODE_SENSE.Pc != 1 )
				{
					PMODE_PARAMETER_HEADER  header		= NULL;
					ULONG					diskSize	= 0;
					
					diskSize						= pDeviceExtension->TotalBlocks; //DeviceExtension->DiskLength ;
					header							= (PMODE_PARAMETER_HEADER) pSrb->DataBuffer;
					header->ModeDataLength			= sizeof(MODE_PARAMETER_HEADER);
					header->MediumType				= 0;
					header->DeviceSpecificParameter = 0;

					if ( !cdb->MODE_SENSE.Dbd && (cdb->MODE_SENSE.AllocationLength >= (header->ModeDataLength + sizeof(MODE_PARAMETER_BLOCK))) )
					{
						PMODE_PARAMETER_BLOCK   block				= NULL;
						ULONG					logicalBlockAddress	= 0;
						ULONG					bytesPerBlock		= 0;

						block							= (PMODE_PARAMETER_BLOCK)((PUCHAR)header + header->ModeDataLength);
						logicalBlockAddress				= diskSize;
						bytesPerBlock					= 512;
						header->BlockDescriptorLength	= sizeof(MODE_PARAMETER_BLOCK);
						header->ModeDataLength			+= sizeof(MODE_PARAMETER_BLOCK);
						block->DensityCode				= 0;

						block->NumberOfBlocks[0]	= (UCHAR)((logicalBlockAddress >> 16) & 0xFF);
						block->NumberOfBlocks[1]	= (UCHAR)((logicalBlockAddress >> 8) & 0xFF);
						block->NumberOfBlocks[2]	= (UCHAR)(logicalBlockAddress & 0xFF);

						block->BlockLength[0]		= (UCHAR)((bytesPerBlock >> 16) & 0xFF);
						block->BlockLength[1]		= (UCHAR)((bytesPerBlock >> 8) & 0xFF);
						block->BlockLength[2]		= (UCHAR)(bytesPerBlock & 0xFF);
					}
					else
					{
						header->BlockDescriptorLength = 0;
#ifdef DBGOUT
					    DbgPrint("[HJSTOR]param, header->BlockDescriptorLength = 0\n" );
#endif
					}


					if ((cdb->MODE_SENSE.AllocationLength >= (header->ModeDataLength + sizeof(MODE_PAGE_FORMAT_DEVICE))) && 
						((cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) || 
						(cdb->MODE_SENSE.PageCode == MODE_PAGE_FORMAT_DEVICE)))
					{
						PMODE_FORMAT_PAGE format	= NULL;

						format = (PMODE_FORMAT_PAGE)((PUCHAR)header + header->ModeDataLength);
						header->ModeDataLength += sizeof(MODE_FORMAT_PAGE);

						format->PageCode							= MODE_PAGE_FORMAT_DEVICE;
						format->PageLength							= sizeof(MODE_FORMAT_PAGE);
						format->TracksPerZone[0]					= 0;		   // we have only one zone
						format->TracksPerZone[1]					= 0;
						format->AlternateSectorsPerZone[0]			= 0;
						format->AlternateSectorsPerZone[1]			= 0;
						format->AlternateTracksPerZone[0]			= 0;
						format->AlternateTracksPerZone[1]			= 0;
						format->AlternateTracksPerLogicalUnit[0]	= 0;
						format->AlternateTracksPerLogicalUnit[1]	= 0;
						format->SectorsPerTrack[0]					= (UCHAR)((diskSize >> 8) & 0xFF);
						format->SectorsPerTrack[1]					= (UCHAR)((diskSize >> 0) & 0xFF);
						format->BytesPerPhysicalSector[0]			= (UCHAR)((512 >> 8) & 0xFF);
						format->BytesPerPhysicalSector[1]			= (UCHAR)((512 >> 0) & 0xFF);
						format->SoftSectorFormating					= 1;
					}

					if ((cdb->MODE_SENSE.AllocationLength >= (header->ModeDataLength + sizeof(MODE_DISCONNECT_PAGE))) && 
						((cdb->MODE_SENSE.PageCode == MODE_SENSE_RETURN_ALL) || 
						(cdb->MODE_SENSE.PageCode == MODE_PAGE_DISCONNECT)))
					{
						PMODE_DISCONNECT_PAGE disconnectPage	= NULL;

						disconnectPage							= (PMODE_DISCONNECT_PAGE)((PUCHAR)header + header->ModeDataLength);
						header->ModeDataLength					+= sizeof(MODE_DISCONNECT_PAGE);

						disconnectPage->PageCode				= MODE_PAGE_DISCONNECT;
						disconnectPage->PageLength				= sizeof(MODE_DISCONNECT_PAGE);
						disconnectPage->BufferFullRatio			= 0xFF;
						disconnectPage->BufferEmptyRatio		= 0xFF;
						disconnectPage->BusInactivityLimit[0]	= 0;
						disconnectPage->BusInactivityLimit[1]	= 1;
						disconnectPage->BusDisconnectTime[0]	= 0;
						disconnectPage->BusDisconnectTime[1]	= 1;
						disconnectPage->BusConnectTime[0]		= 0;
						disconnectPage->BusConnectTime[1]		= 1;
						disconnectPage->MaximumBurstSize[0]		= 0;
						disconnectPage->MaximumBurstSize[1]		= 1;
						disconnectPage->DataTransferDisconnect	= 1;
					}

#ifdef DBGOUT
				    DbgPrint("[HJSTOR]param, header->ModeDataLength = 0x%8X\n", header->ModeDataLength );
#endif
					pSrb->DataTransferLength	= header->ModeDataLength;
					pSrb->ScsiStatus			= SCSISTAT_GOOD;
				}
			}

			break;
		}

		case SRB_FUNCTION_RESET_BUS:
		{
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_RESET_BUS\n");
#endif
			if ( HJSTOR_HwResetBus( pDeviceExtension, pSrb->PathId ) == FALSE ) 
			{
				nSrbStatus = SRB_STATUS_ERROR;
			} 
			else 
			{
				nSrbStatus = SRB_STATUS_SUCCESS;
			}

			break;
		}

		case SRB_FUNCTION_CLAIM_DEVICE:
		{
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_CLAIM_DEVICE\n");
#endif
			pSrb->DataBuffer = pDeviceExtension; 
			break;
		}
			
		case SRB_FUNCTION_IO_CONTROL:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_IO_CONTROL\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_RECEIVE_EVENT:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_RECEIVE_EVENT\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_RELEASE_QUEUE:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_RELEASE_QUEUE\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_ATTACH_DEVICE:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_ATTACH_DEVICE\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_SHUTDOWN:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_SHUTDOWN\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_FLUSH:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_FLUSH\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_ABORT_COMMAND:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_ABORT_COMMAND\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_RELEASE_RECOVERY:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_RELEASE_RECOVERY\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_RESET_DEVICE:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_RESET_DEVICE\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_TERMINATE_IO:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_TERMINATE_IO\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_FLUSH_QUEUE:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_FLUSH_QUEUE\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_REMOVE_DEVICE:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_REMOVE_DEVICE\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_WMI:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_WMI\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_LOCK_QUEUE:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_LOCK_QUEUE\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_UNLOCK_QUEUE:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_UNLOCK_QUEUE\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		case SRB_FUNCTION_RESET_LOGICAL_UNIT:
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, SRB_FUNCTION_RESET_LOGICAL_UNIT\n");
#endif
			nSrbStatus = SRB_STATUS_INVALID_REQUEST;
			break;

		default:
		{
#ifdef DBGOUT
			DbgPrint("[HJSTOR]HJSTOR_HwStartIo, default(0x%8X)\n", pSrb->Function);
#endif
			nSrbStatus = SRB_STATUS_SUCCESS;
			break;
		}
	}

	if ( nSrbStatus != SRB_STATUS_PENDING )
	{
		pSrb->SrbStatus = nSrbStatus;
		StorPortNotification( RequestComplete, pDeviceExtension, pSrb );
		StorPortNotification( NextRequest, pDeviceExtension, NULL );
	}

	return TRUE;
}