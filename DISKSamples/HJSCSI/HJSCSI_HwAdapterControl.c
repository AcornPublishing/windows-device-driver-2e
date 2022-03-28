#include "HJSCSI.h"

SCSI_ADAPTER_CONTROL_STATUS HJSCSI_HwAdapterControl(
    IN PVOID HwDeviceExtension,
    IN SCSI_ADAPTER_CONTROL_TYPE ControlType,
    IN PVOID Parameters
    )
{
    PHW_DEVICE_EXTENSION deviceExtension = HwDeviceExtension;
    PSCSI_SUPPORTED_CONTROL_TYPE_LIST pControlTypeList;


    switch(ControlType) {
        case ScsiQuerySupportedControlTypes: {
            BOOLEAN supportedTypes[ScsiAdapterControlMax] = {
                TRUE,       // ScsiQuerySupportedControlTypes
                TRUE,       // ScsiStopAdapter
                TRUE,       // ScsiRestartAdapter
                FALSE,      // ScsiSetBootConfig
                FALSE       // ScsiSetRunningConfig
            };

            ULONG max = ScsiAdapterControlMax;
            ULONG i;

            pControlTypeList = (PSCSI_SUPPORTED_CONTROL_TYPE_LIST) Parameters;

            if(pControlTypeList->MaxControlType < max) {
                max = pControlTypeList->MaxControlType;
            }

            for(i = 0; i < max; i++) {
                pControlTypeList->SupportedTypeList[i] = supportedTypes[i];
            }

            break;
        }
        case ScsiStopAdapter: {
#ifdef DBGOUT
			DbgPrint("[HJSCSI] Stop Device\n");
#endif
			ExFreePool( deviceExtension->Base );
            break;
        }

        case ScsiRestartAdapter: {
#ifdef DBGOUT
			DbgPrint("[HJSCSI] Restart Device\n");
#endif
            break;
        }

        default: {
            return ScsiAdapterControlUnsuccessful;
        }
    }

    return ScsiAdapterControlSuccess;
}
