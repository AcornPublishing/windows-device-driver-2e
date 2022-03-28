#include "stdafx.h"

#include <Windows.h>
#include "keymapper.h"

HKEYARRAY	g_HKeyArray[MAX_HKEYCODESIZE] = { 
//	     0       1       2       3       4       5       6       7       8       9       A       B       C       D       E       F
	0x0000, 0x0000, 0x0000, 0x0000, 0x001E, 0x0030, 0x002E, 0x0020, 0x0012, 0x0021, 0x0022, 0x0023, 0x0017, 0x0024, 0x0025, 0x0026, // 00 - 0F
	0x0032, 0x0031, 0x0018, 0x0019, 0x0010, 0x0013, 0x001F, 0x0014, 0x0016, 0x002F, 0x0011, 0x002D, 0x0015, 0x002C, 0x0002, 0x0003, // 10 - 1F
	0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x001C, 0x0001, 0x000E, 0x000F, 0x0039, 0x000C, 0x000D, 0x001A, // 20 - 2F
	0x001B, 0x002B, 0x002B, 0x0027, 0x0028, 0x0029, 0x0033, 0x0034, 0x0035, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F, 0x0040, // 30 - 3F
	0x0041, 0x0042, 0x0043, 0x0044, 0x0057, 0x0058, 0xE037, 0x0046, 0xE046, 0xE052, 0xE047, 0xE049, 0xE053, 0xE04F, 0xE051, 0xE04D, // 40 - 4F
	0x0000, 0x0000, 0x0000, 0x0045, 0xE035, 0x0037, 0x004A, 0x004E, 0xE01C, 0x004F, 0x0050, 0x0051, 0x004B, 0x004C, 0x004D, 0x0047, // 50 - 5F
	0x0048, 0x0049, 0x0052, 0x0053, 0x0056, 0xE05D, 0xE05E, 0x0059, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, // 60 - 6F
	0x006C, 0x006D, 0x006E, 0x0076, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 70 - 7F
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x007E, 0x0000, 0x0073, 0x0070, 0x007D, 0x0079, 0x007B, 0x005C, 0x0000, 0x0000, 0x0000, // 80 - 8F
	0x0072, 0x0071, 0x0078, 0x0077, 0x0076, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 90 - 9F
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // A0 - AF
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // B0 - BF
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // C0 - CF
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // D0 - DF
	0x001D, 0x002A, 0x0038, 0xE05B, 0xE01D, 0x0036, 0xE038, 0xE05C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // E0 - EF
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000  // F0 - FF
};

HIDKEYCODE	g_HidKeyCode = { 0, };

unsigned char findHKey(unsigned short ScanCode)
{
	int i;
	unsigned char HKCode = HKCODE_EMPTY;

	for (i = 0; i < MAX_HKEYCODESIZE; i++)
	{
		if (g_HKeyArray[i].ScanCode == ScanCode)
		{
			HKCode = (unsigned char)i;
			break;
		}
	}
	return HKCode;
}

void _HK_InitializeHidKeyCodeLibrary()
{
	memset(&g_HidKeyCode, 0, sizeof(HIDKEYCODE));
}

void _HK_ResetAllKey()
{
	memset(&g_HidKeyCode, 0, sizeof(HIDKEYCODE));
}

BOOLEAN _HK_TranslatePrepareSpecialCharacter(unsigned short ScanCode, BOOLEAN bIsMakeCode, HIDKEYCODE &ShadowHKCode)
{
	BOOLEAN bIsSpecialEntry = FALSE;
	BOOLEAN bIsEmptyEntry = FALSE;
	MODIFIERTEMP HKModifierBits = { 0, };
	unsigned char HKCode = 0;
	int StartPos = 0;

	// 특별한 문자들은 매번 지워줘야 한다
NextSpecialEntryfind:
	for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
	{
		HKCode = findHKey(0x0071);
		if (g_HidKeyCode.Key[StartPos] == HKCode)
		{
			g_HidKeyCode.Key[StartPos] = HKCODE_EMPTY;
			bIsSpecialEntry = TRUE;
			break;
		}
		HKCode = findHKey(0x0072);
		if (g_HidKeyCode.Key[StartPos] == HKCode)
		{
			g_HidKeyCode.Key[StartPos] = HKCODE_EMPTY;
			bIsSpecialEntry = TRUE;
			break;
		}
	}

	// 지운자리를 기준으로 뒤부분의 데이타를 앞으로 뽑아야 한다
	if (bIsSpecialEntry == TRUE)
	{
		for (; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			g_HidKeyCode.Key[StartPos] = g_HidKeyCode.Key[StartPos + 1];
			g_HidKeyCode.Key[StartPos + 1] = HKCODE_EMPTY;
		}
		bIsSpecialEntry = FALSE;
		goto NextSpecialEntryfind;
	}

	switch (ScanCode)
	{
	case 0xE01D: // RCTRL
		HKModifierBits.u.Modifier.RSHIFT = 1;
		break;
	case 0xE038: // RALT
		HKModifierBits.u.Modifier.RALT = 1;
		break;
	case 0xE05B: // LWINDOW
		HKModifierBits.u.Modifier.LWINDOW = 1;
		break;
	case 0xE05C: // RWINDOW
		HKModifierBits.u.Modifier.RWINDOW = 1;
		break;
	case 0x0071: // 한자
		// HKCode <- KeyScanCode
		if (bIsMakeCode == FALSE)
		{
			HKCode = findHKey(ScanCode);
			if (HKCode == HKCODE_EMPTY)
				goto exit;
		}
		for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			if (g_HidKeyCode.Key[StartPos] == HKCode)
			{
				bIsEmptyEntry = TRUE;
				ShadowHKCode = g_HidKeyCode;
				goto exit;
			}
		}

		for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			if (g_HidKeyCode.Key[StartPos] == HKCODE_EMPTY)
			{
				g_HidKeyCode.Key[StartPos] = HKCode;
				bIsEmptyEntry = TRUE;
				ShadowHKCode = g_HidKeyCode;
				break;
			}
		}
		goto exit;

	case 0x0072: // 한/영
				 // HKCode <- KeyScanCode
		if (bIsMakeCode == FALSE)
		{
			HKCode = findHKey(ScanCode);
			if (HKCode == HKCODE_EMPTY)
				goto exit;
		}
		for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			if (g_HidKeyCode.Key[StartPos] == HKCode)
			{
				bIsEmptyEntry = TRUE;
				ShadowHKCode = g_HidKeyCode;
				goto exit;
			}
		}
		for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			if (g_HidKeyCode.Key[StartPos] == HKCODE_EMPTY)
			{
				g_HidKeyCode.Key[StartPos] = HKCode;
				bIsEmptyEntry = TRUE;
				ShadowHKCode = g_HidKeyCode;
				break;
			}
		}
		goto exit;

	default:
		goto exit;
	}

	bIsEmptyEntry = TRUE;

	if (bIsMakeCode == FALSE) // 해당하는 키를 찾아서 지워야 한다
	{
		g_HidKeyCode.u.ModifierU8 &= ~HKModifierBits.u.ModifierU8;
	}
	else
	{
		g_HidKeyCode.u.ModifierU8 |= HKModifierBits.u.ModifierU8;
	}
	ShadowHKCode = g_HidKeyCode;

exit:
	return bIsEmptyEntry;
}

BOOLEAN _HK_InputModifier(unsigned short ModifierScanCode, BOOLEAN bIsMakeCode, HIDKEYCODE &ShadowHKCode)
{
	BOOLEAN bIsEmptyEntry = FALSE;
	MODIFIERTEMP HKModifierBits = { 0, };

	// HKModifierBits <- ModifierScanCode
	switch (ModifierScanCode)
	{
		case 0x2a: // LSHIFT
			HKModifierBits.u.Modifier.LSHIFT = 1;
			break;
		case 0x36: // RSHIFT
			HKModifierBits.u.Modifier.RSHIFT = 1;
			break;
		case 0x1D: // LCTRL
			HKModifierBits.u.Modifier.LCTRL = 1;
			break;
		case 0x38: // LALT
			HKModifierBits.u.Modifier.LALT = 1;
			break;
		default:
			goto exit;
	}

	bIsEmptyEntry = TRUE;

	if (bIsMakeCode == FALSE) // 해당하는 키를 찾아서 지워야 한다
	{
		g_HidKeyCode.u.ModifierU8 &= ~ HKModifierBits.u.ModifierU8;
	}
	else
	{
		g_HidKeyCode.u.ModifierU8 |= HKModifierBits.u.ModifierU8;
	}
	ShadowHKCode = g_HidKeyCode;

exit:
	return bIsEmptyEntry;
}

BOOLEAN _HK_InputScanCode(unsigned short KeyScanCode, BOOLEAN bIsMakeCode, HIDKEYCODE &ShadowHKCode)
{
	BOOLEAN bIsEmptyEntry = FALSE;
	int StartPos = 0;
	unsigned char HKCode = 0;

	// HKCode <- KeyScanCode
	HKCode = findHKey(KeyScanCode);

	if (HKCode == HKCODE_EMPTY)
		goto exit;

	if (bIsMakeCode == FALSE) // 해당하는 키를 찾아서 지워야 한다
	{
		for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			if (g_HidKeyCode.Key[StartPos] == HKCode)
			{
				g_HidKeyCode.Key[StartPos] = HKCODE_EMPTY;
				ShadowHKCode = g_HidKeyCode;
				bIsEmptyEntry = TRUE;
				break;
			}
		}
		if (bIsEmptyEntry == FALSE)
			goto exit; // 해당하는 키가 없기 때문에 그냥 리턴한다

		// 지운자리를 기준으로 뒤부분의 데이타를 앞으로 뽑아야 한다
		for (; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			g_HidKeyCode.Key[StartPos] = g_HidKeyCode.Key[StartPos + 1];
			g_HidKeyCode.Key[StartPos + 1] = HKCODE_EMPTY;
		}
	}
	else
	{
		for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			if (g_HidKeyCode.Key[StartPos] == HKCode)
			{
				bIsEmptyEntry = TRUE;
				ShadowHKCode = g_HidKeyCode;
				goto exit;
			}
		}

		for (StartPos = 0; StartPos < HIDSTANDARDKEYARRAYNUMBER; StartPos++)
		{
			if (g_HidKeyCode.Key[StartPos] == HKCODE_EMPTY)
			{
				g_HidKeyCode.Key[StartPos] = HKCode;
				bIsEmptyEntry = TRUE;
				ShadowHKCode = g_HidKeyCode;
				break;
			}
		}
	}

exit:
	return bIsEmptyEntry;
}