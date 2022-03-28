#include "HJSTOR.h"


SCSI_ADAPTER_CONTROL_STATUS
HJSTOR_HwAdapterControl(
	PVOID						pContext,
	SCSI_ADAPTER_CONTROL_TYPE	ControlType,
	PVOID						pParameters
	)
{
	PHW_DEVICE_EXTENSION				pDeviceExtension	= NULL;
	PSCSI_SUPPORTED_CONTROL_TYPE_LIST	pControlTypeList	= NULL;

	if ( pContext == NULL )
	{
		return FALSE;
	}

	pDeviceExtension = (PHW_DEVICE_EXTENSION) pContext;

	switch ( ControlType )
	{
		case ScsiQuerySupportedControlTypes:
		{
			BOOLEAN supportedTypes[ScsiAdapterControlMax] = {
				TRUE,       // ScsiQuerySupportedControlTypes
				TRUE,       // ScsiStopAdapter
				FALSE, //TRUE,       // ScsiRestartAdapter
				FALSE,      // ScsiSetBootConfig
				FALSE       // ScsiSetRunningConfig
			};

			ULONG max	= ScsiAdapterControlMax;
			ULONG i		= 0;

			pControlTypeList = (PSCSI_SUPPORTED_CONTROL_TYPE_LIST) pParameters;
			if ( pControlTypeList == NULL )
			{
				break;
			}

			if(pControlTypeList->MaxControlType < max) {
				max = pControlTypeList->MaxControlType;
			}

			for(i = 0; i < max; i++) {
				pControlTypeList->SupportedTypeList[i] = supportedTypes[i];
			}

			break;
		}

		case ScsiStopAdapter:
		{
#ifdef DBGOUT
			DbgPrint("[HJSTOR] Stop Device\n");
#endif
			ExFreePool( pDeviceExtension->Base );
			break;
		}

		case ScsiRestartAdapter:
		{
#ifdef DBGOUT
			DbgPrint("[HJSTOR] Restart Device\n");
#endif
			break;
		}

		case ScsiSetBootConfig:
		{
			break;
		}

		case ScsiSetRunningConfig:
		{
			break;
		}

		default:
		{
			break;
		}
	}

	return ScsiAdapterControlSuccess;
}