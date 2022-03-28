#pragma once

#include "stdafx.h"
#include <windows.h>

typedef struct _LED
{
	unsigned char NumLock : 1;
	unsigned char CapsLock : 1;
	unsigned char ScrollLock : 1;
	unsigned char res : 5;
}LED, *PLED;

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

typedef struct _MOUSEBUTTON
{
	unsigned char BUTTON1 : 1;
	unsigned char BUTTON2 : 1;
	unsigned char BUTTON3 : 1;
	unsigned char res	  : 5;
}MOUSEBUTTON, *PMOUSEBUTTON;

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

typedef struct _HIDLED
{
	union
	{
		LED				Led;
		unsigned char	LedU8;
	}u;
}HIDLED, *PHIDLED;

typedef struct _HIDMOUSECODE
{
	union
	{
		MOUSEBUTTON		Buttons;
		unsigned char	ModifierU8;
	}u;
	unsigned char	X;
	unsigned char	Y;
	unsigned char	Wheel;
}HIDMOUSECODE, *PHIDMOUSECODE;

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

