#include "HJSTOR.h"

BOOLEAN
HJSTOR_HwResetBus(
	PVOID pContext,
	ULONG PathId
	)
{
	UNREFERENCED_PARAMETER(PathId);

	if ( pContext == NULL )
	{
		return FALSE;
	}
	return TRUE;
}