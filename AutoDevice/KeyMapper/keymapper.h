#pragma once

#include "stdafx.h"
#include <windows.h>

typedef struct _MODIFIER
{
	unsigned char LCTRL			: 1;
	unsigned char LSHIFT		: 1;
	unsigned char LALT			: 1;
	unsigned char LWINDOW		: 1;
	unsigned char RCTRL			: 1;
	unsigned char RSHIFT		: 1;
	unsigned char RALT			: 1;
	unsigned char RWINDOW		: 1;
}MODIFIER, *PMODIFIER;

#define HIDSTANDARDKEYARRAYNUMBER	(6)

// Define Code
#define		HKCODE_EMPTY	(0)

typedef struct _HIDKEYCODE
{
	union
	{
		MODIFIER		Modifier;
		unsigned char	ModifierU8;
	}u;
	unsigned char	res;
	unsigned char	Key[HIDSTANDARDKEYARRAYNUMBER];
}HIDKEYCODE, *PHIDKEYCODE;

typedef struct
{
	union
	{
		MODIFIER		Modifier;
		unsigned char	ModifierU8;
	}u;
}MODIFIERTEMP, *PMODIFIERTEMP;

#define		MAX_HKEYCODESIZE	(256)

typedef struct _HKEYARRAY
{
	unsigned short	ScanCode;
}HKEYARRAY, *PHKEYARRAY;


void _HK_InitializeHidKeyCodeLibrary();
void _HK_ResetAllKey();
BOOLEAN _HK_TranslatePrepareSpecialCharacter(unsigned short ScanCode, BOOLEAN bIsMakeCode, HIDKEYCODE &ShadowHKCode);
BOOLEAN _HK_InputModifier(unsigned short ModifierScanCode, BOOLEAN bIsMakeCode, HIDKEYCODE &ShadowHKCode);
BOOLEAN _HK_InputScanCode(unsigned short KeyScanCode, BOOLEAN bIsMakeCode, HIDKEYCODE &ShadowHKCode);
