//**************************************************************************
//**
//**	##   ##    ##    ##   ##   ####     ####   ###     ###
//**	##   ##  ##  ##  ##   ##  ##  ##   ##  ##  ####   ####
//**	 ## ##  ##    ##  ## ##  ##    ## ##    ## ## ## ## ##
//**	 ## ##  ########  ## ##  ##    ## ##    ## ##  ###  ##
//**	  ###   ##    ##   ###    ##  ##   ##  ##  ##       ##
//**	   #    ##    ##    #      ####     ####   ##       ##
//**
//**	$Id$
//**
//**	Copyright (C) 1999-2002 J�nis Legzdi��
//**
//**	This program is free software; you can redistribute it and/or
//**  modify it under the terms of the GNU General Public License
//**  as published by the Free Software Foundation; either version 2
//**  of the License, or (at your option) any later version.
//**
//**	This program is distributed in the hope that it will be useful,
//**  but WITHOUT ANY WARRANTY; without even the implied warranty of
//**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//**  GNU General Public License for more details.
//**
//**************************************************************************
//**
//**	Builtins.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"
#include "sv_local.h"
#include "cl_local.h"

// MACROS ------------------------------------------------------------------

#define PF(name)			static void PF_##name(void)
#define PF_M(cname, name)	static void PF_##cname##__##name(void)
#define _(name)				{#name, PF_##name, NULL}
#define _M(cname, name)		{#name, PF_##cname##__##name, V##cname::StaticClass()}
#define __(name)			{#name, name, NULL}

// TYPES -------------------------------------------------------------------

enum
{
	MSG_SV_DATAGRAM,
	MSG_SV_RELIABLE,
	MSG_SV_SIGNON,
	MSG_SV_CLIENT,
	MSG_CL_MESSAGE
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

VMapObject *SV_SpawnMobj(VClass *Class);
void SV_RemoveMobj(VMapObject *mobj);
void SV_ForceLightning(void);
void SV_SetFloorPic(int i, int texture);
void SV_SetCeilPic(int i, int texture);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static TMessage			*pr_msg;

// CODE --------------------------------------------------------------------

//**************************************************************************
//
//	Stack routines
//
//**************************************************************************

//==========================================================================
//
//	Push
//
//==========================================================================

static void Push(int value)
{
	*(pr_stackPtr++) = value;
}

//==========================================================================
//
// Pop
//
//==========================================================================

static int Pop(void)
{
	return *(--pr_stackPtr);
}

//==========================================================================
//
//	Pushf
//
//==========================================================================

static void Pushf(float value)
{
	*((float*)pr_stackPtr++) = value;
}

//==========================================================================
//
//	Popf
//
//==========================================================================

static float Popf(void)
{
	return *((float*)--pr_stackPtr);
}

//==========================================================================
//
//	Pushv
//
//==========================================================================

static void Pushv(const TVec &v)
{
	Pushf(v.x);
	Pushf(v.y);
	Pushf(v.z);
}

//==========================================================================
//
//	Popv
//
//==========================================================================

static TVec Popv(void)
{
	TVec v;
	v.z = Popf();
	v.y = Popf();
	v.x = Popf();
	return v;
}

//==========================================================================
//
//	Popf
//
//==========================================================================

static FName PopName(void)
{
	return *((FName*)--pr_stackPtr);
}

//**************************************************************************
//
//	Vararg strings
//
//**************************************************************************

static char		vastring[1024];

static char *PF_FormatString(void)
{
	int		count;
	int		params[16];
	char*	str;
	int		pi;

	count = Pop();
	for (pi = count - 1; pi >= 0; pi--)
	{
		params[pi] = Pop();
	}
	str = (char*)Pop();

	char *src = str;
	char *dst = vastring;
	memset(vastring, 0, sizeof(vastring));
	pi = 0;
	while (*src)
	{
		if (*src == '%')
		{
			src++;
			switch (*src)
			{
			 case '%':
				*dst = *src;
				break;

			 case 'i':
			 case 'd':
				strcat(vastring, va("%d", params[pi]));
				pi++;
				break;

			 case 'x':
				strcat(vastring, va("%x", params[pi]));
				pi++;
				break;

			 case 'f':
				strcat(vastring, va("%f", ((float*)params)[pi]));
				pi++;
				break;

			 case 'n':
				strcat(vastring, *((FName *)params)[pi]);
				pi++;
				break;

			 case 's':
				strcat(vastring, (char*)params[pi]);
				pi++;
				break;

			 case 'p':
				strcat(vastring, va("%p", ((void**)params)[pi]));
				pi++;
				break;

			 case 'v':
				strcat(vastring, va("(%f,%f,%f)", ((float*)params)[pi],
					((float*)params)[pi + 1], ((float*)params)[pi + 2]));
				pi += 3;
				break;

			 default:
				GCon->Logf(NAME_Dev, "PF_FormatString: Unknown format identifier %s", *src);
				src--;
				*dst = *src;
				break;
			}
			dst = vastring + strlen(vastring);
		}
		else
		{
			*dst = *src;
			dst++;
		}
		src++;
	}
	if (pi < count)
	{
		GCon->Log(NAME_Dev, "PF_FormatString: Not all params were used");
	}
	if (pi > count)
	{
		GCon->Log(NAME_Dev, "PF_FormatString: Param count overflow");
	}
	return vastring;
}

//**************************************************************************
//
//  Error functions
//
//**************************************************************************

//==========================================================================
//
//	PF_Error
//
//==========================================================================

PF(Error)
{
    Host_Error(PF_FormatString());
}

//==========================================================================
//
//	PF_FatalError
//
//==========================================================================

PF(FatalError)
{
    Sys_Error(PF_FormatString());
}

//**************************************************************************
//
//	Cvar functions
//
//**************************************************************************

//==========================================================================
//
//	PF_CreateCvar
//
//==========================================================================

PF(CreateCvar)
{
	int name;
	int def;
	int flags;

	flags = Pop();
	def = Pop();
	name = Pop();
	new TCvar((char*)name, (char*)def, flags);
}

//==========================================================================
//
//  PF_GetCvar
//
//==========================================================================

PF(GetCvar)
{
	int		name;

    name = Pop();
    Push(TCvar::Value((char*)name));
}

//==========================================================================
//
//  PF_SetCvar
//
//==========================================================================

PF(SetCvar)
{
	int		name;
	int		value;

    value = Pop();
    name = Pop();
    TCvar::Set((char*)name, value);
}

//==========================================================================
//
//  PF_GetCvarF
//
//==========================================================================

PF(GetCvarF)
{
	int		name;

    name = Pop();
    Pushf(TCvar::Float((char*)name));
}

//==========================================================================
//
//  PF_SetCvarF
//
//==========================================================================

PF(SetCvarF)
{
	int		name;
	float	value;

    value = Popf();
    name = Pop();
    TCvar::Set((char*)name, value);
}

//==========================================================================
//
//  PF_GetCvarS
//
//==========================================================================

PF(GetCvarS)
{
	int		name;

    name = Pop();
    Push((int)TCvar::String((char*)name));
}

//==========================================================================
//
//  PF_SetCvarS
//
//==========================================================================

PF(SetCvarS)
{
	int		name;
	int		value;

    value = Pop();
    name = Pop();
    TCvar::Set((char*)name, (char*)value);
}

//**************************************************************************
//
//	Math functions
//
//**************************************************************************

//==========================================================================
//
//	PF_AngleMod360
//
//==========================================================================

PF(AngleMod360)
{
	float an;

	an = Popf();
	Pushf(AngleMod(an));
}

//==========================================================================
//
//	PF_AngleMod180
//
//==========================================================================

PF(AngleMod180)
{
	float an;

	an = Popf();
	Pushf(AngleMod180(an));
}

//==========================================================================
//
//	PF_sin
//
//==========================================================================

PF(sin)
{
	float an;

    an = Popf();
    Pushf(msin(an));
}

//==========================================================================
//
//	PF_cos
//
//==========================================================================

PF(cos)
{
	float an;

	an = Popf();
    Pushf(mcos(an));
}

//==========================================================================
//
//	PF_tan
//
//==========================================================================

PF(tan)
{
	float an;

	an = Popf();
    Pushf(mtan(an));
}

//==========================================================================
//
//	PF_atan
//
//==========================================================================

PF(atan)
{
	float	slope;

	slope = Popf();
    Pushf(RAD2DEG(atan(slope)));
}

//==========================================================================
//
//	PF_atan2
//
//==========================================================================

PF(atan2)
{
	float	y;
	float	x;

	x = Popf();
	y = Popf();
    Pushf(matan(y, x));
}

//==========================================================================
//
//	PF_Length
//
//==========================================================================

PF(Length)
{
	TVec		vec;

	vec = Popv();
	Pushf(Length(vec));
}

//==========================================================================
//
//	PF_Normalize
//
//==========================================================================

PF(Normalize)
{
	TVec		vec;

	vec = Popv();
	Pushv(Normalize(vec));
}

//==========================================================================
//
//	PF_DotProduct
//
//==========================================================================

PF(DotProduct)
{
	TVec	v1;
	TVec	v2;

	v2 = Popv();
	v1 = Popv();
	Pushf(DotProduct(v1, v2));
}

//==========================================================================
//
//	PF_CrossProduct
//
//==========================================================================

PF(CrossProduct)
{
	TVec	v1;
	TVec	v2;

	v2 = Popv();
	v1 = Popv();
	Pushv(CrossProduct(v1, v2));
}

//==========================================================================
//
//	PF_AngleVectors
//
//==========================================================================

PF(AngleVectors)
{
	TAVec		*angles;
	TVec		*vforward;
	TVec		*vright;
	TVec		*vup;

	vup = (TVec*)Pop();
	vright = (TVec*)Pop();
	vforward = (TVec*)Pop();
	angles = (TAVec*)Pop();
	AngleVectors(*angles, *vforward, *vright, *vup);
}

//==========================================================================
//
//	PF_AngleVector
//
//==========================================================================

PF(AngleVector)
{
	TAVec		*angles;
	TVec		*vec;

	vec = (TVec*)Pop();
	angles = (TAVec*)Pop();
	AngleVector(*angles, *vec);
}

//==========================================================================
//
//	PF_VectorAngles
//
//==========================================================================

PF(VectorAngles)
{
	TVec		*vec;
	TAVec		*angles;

	angles = (TAVec*)Pop();
	vec = (TVec*)Pop();
	VectorAngles(*vec, *angles);
}

//**************************************************************************
//
//	String functions
//
//**************************************************************************

//==========================================================================
//
//	PF_ptrtos
//
//==========================================================================

PF(ptrtos)
{
	//	Nothing to do
}

//==========================================================================
//
//	PF_strgetchar
//
//==========================================================================

PF(strgetchar)
{
	char*	str;
	int		i;

	i = Pop();
	str = (char*)Pop();
	Push(byte(str[i]));
}

//==========================================================================
//
//	PF_strsetchar
//
//==========================================================================

PF(strsetchar)
{
	char*	str;
	int		i;
	int		chr;

	chr = Pop();
	i = Pop();
	str = (char*)Pop();
	str[i] = chr;
}

//==========================================================================
//
//	PF_strlen
//
//==========================================================================

PF(strlen)
{
	int		s;

	s = Pop();
	Push(strlen((char*)s));
}

//==========================================================================
//
//	PF_strcmp
//
//==========================================================================

PF(strcmp)
{
	int		s1;
	int		s2;

	s2 = Pop();
	s1 = Pop();
	Push(strcmp((char*)s1, (char*)s2));
}

//==========================================================================
//
//	PF_stricmp
//
//==========================================================================

PF(stricmp)
{
	int		s1;
	int		s2;

	s2 = Pop();
	s1 = Pop();
	Push(stricmp((char*)s1, (char*)s2));
}

//==========================================================================
//
//	PF_strcpy
//
//==========================================================================

PF(strcpy)
{
	int		s1;
	int		s2;

	s2 = Pop();
	s1 = Pop();
	strcpy((char*)s1, (char*)s2);
}

//==========================================================================
//
//	PF_strclr
//
//==========================================================================

PF(strclr)
{
	char*	s;

	s = (char*)Pop();
	s[0] = 0;
}

//==========================================================================
//
//	PF_strcat
//
//==========================================================================

PF(strcat)
{
	int		s1;
	int		s2;

	s2 = Pop();
	s1 = Pop();
	strcat((char*)s1, (char*)s2);
}

//==========================================================================
//
//	PF_sprint
//
//==========================================================================

PF(sprint)
{
	int		dst;

	PF_FormatString();
	dst = Pop();
	strcpy((char*)dst, vastring);
}

//==========================================================================
//
//	PF_va
//
//==========================================================================

PF(va)
{
	Push((int)PF_FormatString());
}

//==========================================================================
//
//	PF_atoi
//
//==========================================================================

PF(atoi)
{
	int		str;

	str = Pop();
	Push(atoi((char*)str));
}

//==========================================================================
//
//	PF_atof
//
//==========================================================================

PF(atof)
{
	int		str;

	str = Pop();
	Pushf(atof((char*)str));
}

//**************************************************************************
//
//	Random numbers
//
//**************************************************************************

//==========================================================================
//
//	PF_Random
//
//==========================================================================

PF(Random)
{
	Pushf(Random());
}

//==========================================================================
//
//	PF_P_Random
//
//==========================================================================

PF(P_Random)
{
	Push(rand() & 0xff);
}

//**************************************************************************
//
//	Texture utils
//
//**************************************************************************

//==========================================================================
//
//	PF_CheckTextureNumForName
//
//==========================================================================

PF(CheckTextureNumForName)
{
	int		name;

	name = Pop();
	Push(R_CheckTextureNumForName((char*)name));
}

//==========================================================================
//
//	PF_TextureNumForName
//
//==========================================================================

PF(TextureNumForName)
{
	int		name;

	name = Pop();
	Push(R_TextureNumForName((char*)name));
}

//==========================================================================
//
//	PF_CheckFlatNumForName
//
//==========================================================================

PF(CheckFlatNumForName)
{
	int		name;

	name = Pop();
	Push(R_CheckFlatNumForName((char*)name));
}

//==========================================================================
//
//	PF_FlatNumForName
//
//==========================================================================

PF(FlatNumForName)
{
	int		name;

	name = Pop();
	Push(R_FlatNumForName((char*)name));
}

//==========================================================================
//
//	PF_TextureHeight
//
//==========================================================================

PF(TextureHeight)
{
	int			pic;

	pic = Pop();
	Pushf(R_TextureHeight(pic));
}

//**************************************************************************
//
//	Message IO functions
//
//**************************************************************************

//==========================================================================
//
//	PF_MSG_Select
//
//==========================================================================

PF(MSG_Select)
{
	int			msgtype;

	msgtype = Pop();
	switch (msgtype)
	{
#ifdef SERVER
	 case MSG_SV_DATAGRAM:
		pr_msg = &sv_datagram;
		break;
	 case MSG_SV_RELIABLE:
		pr_msg = &sv_reliable;
		break;
	 case MSG_SV_SIGNON:
		pr_msg = &sv_signon;
		break;
#endif
#ifdef CLIENT
	 case MSG_CL_MESSAGE:
		pr_msg = &cls.message;
		break;
#endif
	}
}

//==========================================================================
//
//	PF_MSG_SelectClientMsg
//
//==========================================================================

#ifdef SERVER

PF(MSG_SelectClientMsg)
{
	int			msgtype;
	player_t	*client;

	client = (player_t*)Pop();
	msgtype = Pop();
	switch (msgtype)
	{
	 case MSG_SV_CLIENT:
		pr_msg = &client->Message;
		break;
	}
}

#endif

//==========================================================================
//
//	PF_MSG_WriteByte
//
//==========================================================================

PF(MSG_WriteByte)
{
	int		d;

	d = Pop();
	*pr_msg << (byte)d;
}

//==========================================================================
//
//	PF_MSG_WriteShort
//
//==========================================================================

PF(MSG_WriteShort)
{
	int		d;

	d = Pop();
	*pr_msg << (short)d;
}

//==========================================================================
//
//	PF_MSG_WriteLong
//
//==========================================================================

PF(MSG_WriteLong)
{
	int		d;

	d = Pop();
	*pr_msg << d;
}

//==========================================================================
//
//	PF_MSG_ReadChar
//
//==========================================================================

PF(MSG_ReadChar)
{
	Push((char)net_msg.ReadByte());
}

//==========================================================================
//
//	PF_MSG_ReadByte
//
//==========================================================================

PF(MSG_ReadByte)
{
	Push((byte)net_msg.ReadByte());
}

//==========================================================================
//
//	PF_MSG_ReadShort
//
//==========================================================================

PF(MSG_ReadShort)
{
	Push((short)net_msg.ReadShort());
}

//==========================================================================
//
//	PF_MSG_ReadWord
//
//==========================================================================

PF(MSG_ReadWord)
{
	Push((word)net_msg.ReadShort());
}

//==========================================================================
//
//	PF_MSG_ReadLong
//
//==========================================================================

PF(MSG_ReadLong)
{
	int l;

	net_msg >> l;
	Push(l);
}

//==========================================================================
//
//	PF_SpawnObject
//
//==========================================================================

PF(SpawnObject)
{
	VClass *Class;
	VObject *Outer;

	Outer = (VObject *)Pop();
	Class = (VClass *)Pop();
	Push((int)VObject::StaticSpawnObject(Class, Outer, PU_STRING));
}

//==========================================================================
//
//	PF_FindClass
//
//==========================================================================

PF(FindClass)
{
	FName	Name;

	Name = PopName();
	Push((int)VClass::FindClass(*Name));
}

#ifdef SERVER
//**************************************************************************
//
//	Print functions
//
//**************************************************************************

//==========================================================================
//
//	PF_bprint
//
//==========================================================================

PF(bprint)
{
	SV_BroadcastPrintf(PF_FormatString());
}

//==========================================================================
//
//	PF_cprint
//
//==========================================================================

PF(cprint)
{
	player_t*	player;

	PF_FormatString();
    player = (player_t*)Pop();
	SV_ClientPrintf(player, vastring);
}

//==========================================================================
//
//	PF_centerprint
//
//==========================================================================

PF(centerprint)
{
	player_t*	player;

	PF_FormatString();
    player = (player_t*)Pop();
	SV_ClientCenterPrintf(player, vastring);
}

//**************************************************************************
//
//	Map utilites
//
//**************************************************************************

//==========================================================================
//
//	PF_LineOpenings
//
//==========================================================================

PF(LineOpenings)
{
	line_t	*linedef;
	TVec	point;

	point = Popv();
	linedef = (line_t*)Pop();
	Push((int)SV_LineOpenings(linedef, point));
}

//==========================================================================
//
//	PF_P_BoxOnLineSide
//
//==========================================================================

PF(P_BoxOnLineSide)
{
	float	*tmbox;
	line_t	*ld;

	ld = (line_t*)Pop();
	tmbox = (float*)Pop();
	Push(P_BoxOnLineSide(tmbox, ld));
}

//==========================================================================
//
//  PF_P_BlockThingsIterator
//
//==========================================================================

PF(P_BlockThingsIterator)
{
	int		x;
    int		y;
    FName	FuncName;
	FFunction	*func = NULL;
	static FName PrevNames[8];
	static FFunction *PrevFuncs[8];
	int i;

	FuncName = PopName();
    y = Pop();
    x = Pop();
	for (i = 0; i < 8; i++)
	{
		if (PrevNames[i] == FuncName)
		{
			func = PrevFuncs[i];
			break;
		}
	}
	if (!func)
	{
		func = svpr.FindFunctionChecked(FuncName);
		for (i = 0; i < 8; i++)
		{
			if (PrevNames[i] == NAME_None)
			{
				PrevNames[i] = FuncName;
				PrevFuncs[i] = func;
				break;
			}
		}
	}
	Push(SV_BlockThingsIterator(x, y, NULL, func));
}

//==========================================================================
//
//	PF_P_PathTraverse
//
//==========================================================================

PF(P_PathTraverse)
{
	float	x1;
	float	y1;
	float	x2;
	float	y2;
	int		flags;
	FName	FuncName;
	FFunction	*func = NULL;
	static FName PrevNames[8];
	static FFunction *PrevFuncs[8];
	int i;

	FuncName = PopName();
	flags = Pop();
	y2 = Popf();
	x2 = Popf();
	y1 = Popf();
	x1 = Popf();
	for (i = 0; i < 8; i++)
	{
		if (PrevNames[i] == FuncName)
		{
			func = PrevFuncs[i];
			break;
		}
	}
	if (!func)
	{
		func = svpr.FindFunctionChecked(FuncName);
		for (i = 0; i < 8; i++)
		{
			if (PrevNames[i] == NAME_None)
			{
				PrevNames[i] = FuncName;
				PrevFuncs[i] = func;
				break;
			}
		}
	}
	Push(SV_PathTraverse(x1, y1, x2, y2, flags, NULL, func));
}

//==========================================================================
//
//	PF_FindThingGap
//
//==========================================================================

PF(FindThingGap)
{
	sec_region_t	*gaps;
	TVec			point;
	float			z1;
	float			z2;

	z2 = Popf();
	z1 = Popf();
	point = Popv();
	gaps = (sec_region_t*)Pop();
	Push((int)SV_FindThingGap(gaps, point, z1, z2));
}

//==========================================================================
//
//	PF_FindOpening
//
//==========================================================================

PF(FindOpening)
{
	opening_t	*gaps;
	float		z1;
	float		z2;

	z2 = Popf();
	z1 = Popf();
	gaps = (opening_t*)Pop();
	Push((int)SV_FindOpening(gaps, z1, z2));
}

//==========================================================================
//
//	PF_PointInRegion
//
//==========================================================================

PF(PointInRegion)
{
	sector_t	*sector;
	TVec		p;

	p = Popv();
	sector = (sector_t*)Pop();
	Push((int)SV_PointInRegion(sector, p));
}

//==========================================================================
//
//	PF_AddExtraFloor
//
//==========================================================================

PF(AddExtraFloor)
{
	line_t		*line;
	sector_t	*dst;

	dst = (sector_t*)Pop();
	line = (line_t*)Pop();
	Push((int)AddExtraFloor(line, dst));
	sv_signon << (byte)svc_extra_floor
				<< (short)(line - level.lines)
				<< (short)(dst - level.sectors);
}

//==========================================================================
//
//	PF_SwapPlanes
//
//==========================================================================

PF(SwapPlanes)
{
	sector_t	*s;

	s = (sector_t *)Pop();
	SwapPlanes(s);
	sv_signon << (byte)svc_swap_planes << (short)(s - level.sectors);
}

//==========================================================================
//
//	PF_MapBlock
//
//==========================================================================

PF(MapBlock)
{
	float x;

	x = Popf();
	Push(MapBlock(x));
}

//**************************************************************************
//
//	Mobj utilites
//
//**************************************************************************

//==========================================================================
//
//  PF_NewMobjThiker
//
//==========================================================================

PF(NewMobjThinker)
{
	VClass *Class;

	Class = (VClass *)Pop();
	Push((int)SV_SpawnMobj(Class));
}

//==========================================================================
//
//  PF_RemoveThinker
//
//==========================================================================

PF(RemoveMobjThinker)
{
	VMapObject		*mobj;

    mobj = (VMapObject*)Pop();
	SV_RemoveMobj(mobj);
}

//==========================================================================
//
//	PF_NextMobj
//
//==========================================================================

PF(NextMobj)
{
    VObject *th;
	int i;

    th = (VObject*)Pop();
	if (!th)
    {
    	i = 0;
	}
	else
	{
		i = th->GetIndex() + 1;
	}
    while (i < VObject::GetObjectsCount())
    {
		th = VObject::GetIndexObject(i);
        if (th && th->IsA(VMapObject::StaticClass()))
        {
            Push((int)th);
            return;
		}
		i++;
    }
	Push(0);
}

//**************************************************************************
//
//	Special thinker utilites
//
//**************************************************************************

//==========================================================================
//
//  PF_NewSpecialThinker
//
//==========================================================================

PF(NewSpecialThinker)
{
	VClass		*Class;
	VThinker	*spec;

	Class = (VClass *)Pop();
	spec = (VThinker*)VObject::StaticSpawnObject(Class, NULL, PU_LEVSPEC);
	Push((int)spec);
}

//==========================================================================
//
//  PF_RemoveSpecialThinker
//
//==========================================================================

PF(RemoveSpecialThinker)
{
	VThinker	*spec;

    spec = (VThinker*)Pop();
    spec->Destroy();
}

//==========================================================================
//
//  PF_P_ChangeSwitchTexture
//
//==========================================================================

PF(P_ChangeSwitchTexture)
{
	line_t* 	line;
	int 		useAgain;

    useAgain = Pop();
    line = (line_t*)Pop();
	P_ChangeSwitchTexture(line, useAgain);
}

//==========================================================================
//
//	PF_NextThinker
//
//==========================================================================

PF(NextThinker)
{
	VObject *th;
	VClass *Class;
	int i;

	Class = (VClass *)Pop();
	th = (VObject*)Pop();
	if (!th)
	{
		i = 0;
	}
	else
	{
		i = th->GetIndex() + 1;
	}
	while (i < VObject::GetObjectsCount())
	{
		th = VObject::GetIndexObject(i);
		if (th && th->IsA(Class))
		{
			Push((int)th);
			return;
		}
		i++;
	}
	Push(0);
}

//**************************************************************************
//
//	Polyobj functons
//
//**************************************************************************

//==========================================================================
//
//	PF_SpawnPolyobj
//
//==========================================================================

PF(SpawnPolyobj)
{
	float 	x;
	float 	y;
	int 	tag;
	int 	flags;

   	flags = Pop();
    tag = Pop();
    y = Popf();
    x = Popf();
	PO_SpawnPolyobj(x, y, tag, flags);
}

//==========================================================================
//
//	PF_AddAnchorPoint
//
//==========================================================================

PF(AddAnchorPoint)
{
	float 	x;
	float 	y;
	int 	tag;

    tag = Pop();
    y = Popf();
    x = Popf();
	PO_AddAnchorPoint(x, y, tag);
}

//==========================================================================
//
//	PF_GetPolyobj
//
//==========================================================================

PF(GetPolyobj)
{
	int 	polyNum;

    polyNum = Pop();
	Push((int)PO_GetPolyobj(polyNum));
}

//==========================================================================
//
//	PF_GetPolyobjMirror
//
//==========================================================================

PF(GetPolyobjMirror)
{
	int 	polyNum;

    polyNum = Pop();
	Push(PO_GetPolyobjMirror(polyNum));
}

//==========================================================================
//
//  PF_PO_MovePolyobj
//
//==========================================================================

PF(PO_MovePolyobj)
{
	int 	num;
	float 	x;
	float 	y;

	y = Popf();
    x = Popf();
    num = Pop();
	Push(PO_MovePolyobj(num, x, y));
}

//==========================================================================
//
//	PF_PO_RotatePolyobj
//
//==========================================================================

PF(PO_RotatePolyobj)
{
	int num;
	float angle;

	angle = Popf();
    num = Pop();
	Push(PO_RotatePolyobj(num, angle));
}

//**************************************************************************
//
//	ACS functions
//
//**************************************************************************

//==========================================================================
//
//  PF_StartACS
//
//==========================================================================

PF(StartACS)
{
	int		num;
    int		map;
	int 	*args;
    VMapObject	*activator;
    line_t	*line;
    int		side;

    side = Pop();
	line = (line_t*)Pop();
    activator = (VMapObject*)Pop();
	args = (int*)Pop();
    map = Pop();
    num = Pop();
	Push(P_StartACS(num, map, args, activator, line, side));
}

//==========================================================================
//
//  PF_SuspendACS
//
//==========================================================================

PF(SuspendACS)
{
	int 	number;
	int 	map;

    map = Pop();
    number = Pop();
	Push(P_SuspendACS(number, map));
}

//==========================================================================
//
//  PF_TerminateACS
//
//==========================================================================

PF(TerminateACS)
{
	int 	number;
	int 	map;

    map = Pop();
    number = Pop();
	Push(P_TerminateACS(number, map));
}

//==========================================================================
//
//  PF_TagFinished
//
//==========================================================================

PF(TagFinished)
{
	int		tag;

    tag = Pop();
	P_TagFinished(tag);
}

//==========================================================================
//
//  PF_PolyobjFinished
//
//==========================================================================

PF(PolyobjFinished)
{
	int		tag;

    tag = Pop();
	P_PolyobjFinished(tag);
}

//**************************************************************************
//
//  Sound functions
//
//**************************************************************************

//==========================================================================
//
//	PF_StartSoundAtVolume
//
//==========================================================================

PF(StartSoundAtVolume)
{
	VMapObject*		mobj;
    int			sound;
	int			channel;
    int			vol;

    vol = Pop();
	channel = Pop();
    sound = Pop();
    mobj = (VMapObject*)Pop();
    SV_StartSound(mobj, sound, channel, vol);
}

//==========================================================================
//
//	PF_SectorStartSound
//
//==========================================================================

PF(SectorStartSound)
{
	sector_t*	sec;
    int			sound;
	int			channel;

	channel = Pop();
    sound = Pop();
    sec = (sector_t*)Pop();
	SV_SectorStartSound(sec, sound, channel, 127);
}

//==========================================================================
//
//	PF_SectorStopSound
//
//==========================================================================

PF(SectorStopSound)
{
	sector_t*	sec;
	int			channel;

	channel = Pop();
    sec = (sector_t*)Pop();
    SV_SectorStopSound(sec, channel);
}

//==========================================================================
//
//	PF_GetSoundPlayingInfo
//
//==========================================================================

PF(GetSoundPlayingInfo)
{
	int			mobj;
    int			id;

    id = Pop();
    mobj = Pop();
#ifdef CLIENT
	Push(S_GetSoundPlayingInfo(mobj, id));
#else
	Push(0);
#endif
}

//==========================================================================
//
//	PF_GetSoundID
//
//==========================================================================

PF(GetSoundID)
{
	FName	Name;

    Name = PopName();
	Push(S_GetSoundID(Name));
}

//==========================================================================
//
//  PF_SectorStartSequence
//
//==========================================================================

PF(SectorStartSequence)
{
	sector_t*	sec;
	FName		name;

    name = PopName();
    sec = (sector_t*)Pop();
	SV_SectorStartSequence(sec, *name);
}

//==========================================================================
//
//  PF_SectorStopSequence
//
//==========================================================================

PF(SectorStopSequence)
{
	sector_t*	sec;

    sec = (sector_t*)Pop();
	SV_SectorStopSequence(sec);
}

//==========================================================================
//
//  PF_PolyobjStartSequence
//
//==========================================================================

PF(PolyobjStartSequence)
{
	polyobj_t*	poly;
	FName		name;

    name = PopName();
    poly = (polyobj_t*)Pop();
	SV_PolyobjStartSequence(poly, *name);
}

//==========================================================================
//
//  PF_PolyobjStopSequence
//
//==========================================================================

PF(PolyobjStopSequence)
{
	polyobj_t*	poly;

    poly = (polyobj_t*)Pop();
	SV_PolyobjStopSequence(poly);
}

//**************************************************************************
//
//	Savegame archieve / unarchieve utilite functions
//
//**************************************************************************

//==========================================================================
//
//	PF_SectorToNum
//
//==========================================================================

PF(SectorToNum)
{
	sector_t*	sector;

	sector = (sector_t*)Pop();
	if (sector)
		Push(sector - level.sectors);
	else
    	Push(-1);
}

//==========================================================================
//
//	PF_NumToSector
//
//==========================================================================

PF(NumToSector)
{
	int		num;

    num = Pop();
	if (num >= 0)
	    Push((int)&level.sectors[num]);
	else
    	Push(0);
}

//==========================================================================
//
//  PF_MobjToNum
//
//==========================================================================

PF(MobjToNum)
{
	VMapObject**	mobj;

    mobj = (VMapObject**)Pop();
    *mobj = (VMapObject*)GetMobjNum(*mobj);
}

//==========================================================================
//
//  PF_NumToMobj
//
//==========================================================================

PF(NumToMobj)
{
	VMapObject**	mobj;

	mobj = (VMapObject**)Pop();
    *mobj = SetMobjPtr((int)*mobj);
}

//==========================================================================
//
//	PF_ClearPlayer
//
//==========================================================================

PF(ClearPlayer)
{
	player_t	*pl;

    pl = (player_t*)Pop();

	pl->PClass = 0;
	pl->ForwardMove = 0;
	pl->SideMove = 0;
	pl->FlyMove = 0;
	pl->Buttons = 0;
	pl->Impulse = 0;
	pl->MO = NULL;
	pl->PlayerState = 0;
	pl->ViewOrg = TVec(0, 0, 0);
	pl->bFixAngle = false;
	pl->Health = 0;
	pl->Items = 0;
	pl->bAttackDown = false;
	pl->bUseDown = false;
	pl->ExtraLight = 0;
	pl->FixedColormap = 0;
	pl->Palette = 0;
	memset(pl->CShifts, 0, sizeof(pl->CShifts));
	pl->PSpriteSY = 0;
	memset(pl->user_fields, 0, sizeof(pl->user_fields));
}

//==========================================================================
//
//  PF_G_ExitLevel
//
//==========================================================================

PF(G_ExitLevel)
{
    G_ExitLevel();
}

//==========================================================================
//
//  PF_G_SecretExitLevel
//
//==========================================================================

PF(G_SecretExitLevel)
{
    G_SecretExitLevel();
}

//==========================================================================
//
//  PF_G_Completed
//
//==========================================================================

PF(G_Completed)
{
	int		map;
    int		pos;

    pos = Pop();
    map = Pop();
    G_Completed(map, pos);
}

//==========================================================================
//
//	PF_P_GetThingFloorType
//
//==========================================================================

PF(TerrainType)
{
	int			pic;

	pic = Pop();
	Push(SV_TerrainType(pic));
}

//==========================================================================
//
//	PF_P_GetPlayerNum
//
//==========================================================================

PF(P_GetPlayerNum)
{
	player_t*	player;
	int 		i;

    player = (player_t*)Pop();
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (player == &players[i])
		{
		    Push(i);
            return;
		}
	}
	Push(0);
}

//==========================================================================
//
// 	PF_PointInSubsector
//
//==========================================================================

PF(PointInSubsector)
{
	float		x;
    float		y;

    y = Popf();
    x = Popf();
    Push((int)SV_PointInSubsector(x, y));
}

//==========================================================================
//
//	PF_SB_Start
//
//==========================================================================

PF(SB_Start)
{
#ifdef CLIENT
//	SB_Start();
#endif
}

//==========================================================================
//
//  PF_P_ForceLightning
//
//==========================================================================

PF(P_ForceLightning)
{
	SV_ForceLightning();
}

//==========================================================================
//
//	PF_SetFloorPic
//
//==========================================================================

PF(SetFloorPic)
{
	sector_t	*sec;
 	int 		texture;

	texture = Pop();
	sec = (sector_t*)Pop();
	SV_SetFloorPic(sec - level.sectors, texture);
}

//==========================================================================
//
//	PF_SetCeilPic
//
//==========================================================================

PF(SetCeilPic)
{
	sector_t	*sec;
 	int 		texture;

	texture = Pop();
	sec = (sector_t*)Pop();
	SV_SetCeilPic(sec - level.sectors, texture);
}

//==========================================================================
//
//	PF_SetLineTransluc
//
//==========================================================================

PF(SetLineTransluc)
{
	line_t	*line;
	int		trans;

	trans = Pop();
	line = (line_t*)Pop();
	SV_SetLineTransluc(line, trans);
}

//==========================================================================
//
//	PF_SendFloorSlope
//
//==========================================================================

PF(SendFloorSlope)
{
	sector_t	*sector;

	sector = (sector_t*)Pop();
	sector->floor.CalcBits();
	sv_signon << (byte)svc_sec_floor_plane
			<< (word)(sector - level.sectors)
			<< sector->floor.normal.x
			<< sector->floor.normal.y
			<< sector->floor.normal.z
			<< sector->floor.dist;
}

//==========================================================================
//
//	PF_SendCeilingSlope
//
//==========================================================================

PF(SendCeilingSlope)
{
	sector_t	*sector;

	sector = (sector_t*)Pop();
	sector->ceiling.CalcBits();
	sv_signon << (byte)svc_sec_ceil_plane
			<< (word)(sector - level.sectors)
			<< sector->ceiling.normal.x
			<< sector->ceiling.normal.y
			<< sector->ceiling.normal.z
			<< sector->ceiling.dist;
}

//==========================================================================
//
//	PF_FindModel
//
//==========================================================================

PF(FindModel)
{
	char *name;

	name = (char*)Pop();
	Push(SV_FindModel(name));
}

//==========================================================================
//
//	PF_GetModelIndex
//
//==========================================================================

PF(GetModelIndex)
{
	FName Name;

	Name = PopName();
	Push(SV_GetModelIndex(Name));
}

//==========================================================================
//
//	PF_FindSkin
//
//==========================================================================

PF(FindSkin)
{
	char *name;

	name = (char*)Pop();
	Push(SV_FindSkin(name));
}
#endif
#ifdef CLIENT

//**************************************************************************
//
//	Graphics
//
//**************************************************************************

//==========================================================================
//
//	PF_R_RegisterPic
//
//==========================================================================

PF(R_RegisterPic)
{
	int name;
	int type;

	type = Pop();
	name = Pop();
	Push(R_RegisterPic((char*)name, type));
}

//==========================================================================
//
//	PF_R_RegisterPicPal
//
//==========================================================================

PF(R_RegisterPicPal)
{
	int name;
	int type;
	int palname;

	palname = Pop();
	type = Pop();
	name = Pop();
	Push(R_RegisterPicPal((char*)name, type, (char*)palname));
}

//==========================================================================
//
//	PF_R_GetPicInfo
//
//==========================================================================

PF(R_GetPicInfo)
{
	int			handle;
	picinfo_t	*info;

	info = (picinfo_t*)Pop();
	handle = Pop();
	R_GetPicInfo(handle, info);
}

//==========================================================================
//
//	PF_R_DrawPic
//
//==========================================================================

PF(R_DrawPic)
{
	int			x;
	int			y;
	int			handle;

	handle = Pop();
	y = Pop();
	x = Pop();
	R_DrawPic(x, y, handle);
}

//==========================================================================
//
//	PF_R_DrawPic2
//
//==========================================================================

PF(R_DrawPic2)
{
	int			x;
	int			y;
	int			handle;
	int			trans;

	trans = Pop();
	handle = Pop();
	y = Pop();
	x = Pop();
	R_DrawPic(x, y, handle, trans);
}

//==========================================================================
//
//	PF_R_DrawShadowedPic
//
//==========================================================================

PF(R_DrawShadowedPic)
{
	int			x;
	int			y;
	int			handle;

	handle = Pop();
	y = Pop();
	x = Pop();
	R_DrawShadowedPic(x, y, handle);
}

//==========================================================================
//
//	PF_R_InstallSprite
//
//==========================================================================

PF(R_InstallSprite)
{
	int		name;
	int		index;

	index = Pop();
	name = Pop();
	R_InstallSprite((char*)name, index);
}

//==========================================================================
//
//	PF_R_DrawSpritePatch
//
//==========================================================================

PF(R_DrawSpritePatch)
{
	int		x;
	int		y;
	int		sprite;
	int		frame;
	int		rot;
	int		translation;

	translation = Pop();
	rot = Pop();
	frame = Pop();
	sprite = Pop();
	y = Pop();
	x = Pop();
	R_DrawSpritePatch(x, y, sprite, frame, rot, translation);
}

//==========================================================================
//
//	PF_InstallModel
//
//==========================================================================

PF(InstallModel)
{
	int			name;

	name = Pop();
	if (FL_FindFile((char*)name, NULL))
	{
		Push((int)Mod_FindName((char*)name));
	}
	else
	{
		Push(0);
	}
}

//==========================================================================
//
//	PF_R_DrawModelFrame
//
//==========================================================================

PF(R_DrawModelFrame)
{
	TVec		origin;
	float		angle;
	model_t		*model;
	int			frame;
	int			skin;

	skin = Pop();
	frame = Pop();
	model = (model_t*)Pop();
	angle = Popf();
	origin = Popv();
	R_DrawModelFrame(origin, angle, model, frame, (char*)skin);
}

//==========================================================================
//
//	PF_R_FillRectWithFlat
//
//==========================================================================

PF(R_FillRectWithFlat)
{
	int		x;
	int		y;
	int		width;
	int		height;
	int		name;

	name = Pop();
	height = Pop();
	width = Pop();
	y = Pop();
	x = Pop();
	R_FillRectWithFlat(x, y, width, height, (char*)name);
}

//==========================================================================
//
//	PF_R_ShadeRect
//
//==========================================================================

void R_ShadeRect(int x, int y, int width, int height, int shade);

PF(R_ShadeRect)
{
	int		x;
	int		y;
	int		width;
	int		height;
	int		shade;

	shade = Pop();
	height = Pop();
	width = Pop();
	y = Pop();
	x = Pop();
	R_ShadeRect(x, y, width, height, shade);
}

//**************************************************************************
//
//	Text
//
//**************************************************************************

//==========================================================================
//
//	PF_T_SetFont
//
//==========================================================================

PF(T_SetFont)
{
	int font;

	font = Pop();
	T_SetFont((font_e)font);
}

//==========================================================================
//
//	PF_T_SetAlign
//
//==========================================================================

PF(T_SetAlign)
{
	int			halign;
	int			valign;

	valign = Pop();
	halign = Pop();
	T_SetAlign((halign_e)halign, (valign_e)valign);
}

//==========================================================================
//
//	PF_T_SetDist
//
//==========================================================================

PF(T_SetDist)
{
	int			hdist;
	int			vdist;

	vdist = Pop();
	hdist = Pop();
	T_SetDist(hdist, vdist);
}

//==========================================================================
//
//	PF_T_SetShadow
//
//==========================================================================

PF(T_SetShadow)
{
	bool		state;

	state = !!Pop();
	T_SetShadow(state);
}

//==========================================================================
//
//	PF_T_TextWidth
//
//==========================================================================

PF(T_TextWidth)
{
	int			text;

	text = Pop();
	Push(T_TextWidth((char*)text));
}

//==========================================================================
//
//	PF_T_TextHeight
//
//==========================================================================

PF(T_TextHeight)
{
	int			text;

	text = Pop();
	Push(T_TextHeight((char*)text));
}

//==========================================================================
//
//	PF_T_DrawText
//
//==========================================================================

PF(T_DrawText)
{
	int			x;
	int			y;
	int			txt;

	txt = Pop();
	y = Pop();
	x = Pop();
	T_DrawText(x, y, (char*)txt);
}

//==========================================================================
//
//	PF_T_DrawNText
//
//==========================================================================

PF(T_DrawNText)
{
	int			x;
	int			y;
	int			txt;
	int			n;

	n = Pop();
	txt = Pop();
	y = Pop();
	x = Pop();
	T_DrawNText(x, y, (char*)txt, n);
}

//**************************************************************************
//
//	Client side sound
//
//**************************************************************************

//==========================================================================
//
//	PF_LocalSound
//
//==========================================================================

PF(LocalSound)
{
	FName	name;

	name = PopName();
	S_StartSoundName(*name);
}

//==========================================================================
//
//	PF_PlayVoice
//
//==========================================================================

PF(PlayVoice)
{
	char*	name;

	name = (char *)Pop();
	S_PlayVoice(name);
}

//==========================================================================
//
//	PF_LocalSoundTillDone
//
//==========================================================================

PF(LocalSoundTillDone)
{
	FName	name;

	name = PopName();
	S_PlayTillDone(const_cast<char*>(*name));
}

//==========================================================================
//
//	PF_InputLine_SetValue
//
//==========================================================================

PF(TranslateKey)
{
	int ch;

	ch = Pop();
	Push(IN_TranslateKey(ch));
}

#endif

//==========================================================================
//
//	PF_print
//
//==========================================================================

PF(print)
{
	GCon->Log(PF_FormatString());
}

//==========================================================================
//
//	PF_dprint
//
//==========================================================================

PF(dprint)
{
	GCon->Log(NAME_Dev, PF_FormatString());
}

//==========================================================================
//
//	PF_Cmd_CheckParm
//
//==========================================================================

PF(Cmd_CheckParm)
{
	int		str;

    str = Pop();
    Push(Cmd_CheckParm((char*)str));
}

//==========================================================================
//
//	CmdBuf_AddText
//
//==========================================================================

PF(CmdBuf_AddText)
{
	CmdBuf << PF_FormatString();
}

//==========================================================================
//
//	PF_Info_ValueForKey
//
//==========================================================================

PF(Info_ValueForKey)
{
	int		info;
	int		key;

	key = Pop();
	info = Pop();
	Push((int)Info_ValueForKey((char*)info, (char*)key));
}

//==========================================================================
//
//	PF_itof
//
//==========================================================================

PF(itof)
{
	int			x;

	x = Pop();
	Pushf((float)x);
}

//==========================================================================
//
//	PF_ftoi
//
//==========================================================================

PF(ftoi)
{
	float		x;

	x = Popf();
	Push((int)x);
}

//==========================================================================
//
//	PF_WadLumpPresent
//
//==========================================================================

PF(WadLumpPresent)
{
	int			name;

	name = Pop();
	Push(W_CheckNumForName((char*)name) >= 0);
}

//==========================================================================
//
//	Temporary menu stuff
//
//==========================================================================

#ifdef CLIENT

struct slist_t;

char* P_GetMapName(int map);
char* P_GetMapLumpName(int map);
char *P_TranslateMap(int map);
void KeyNameForNum(int KeyNr, char* NameString);

void StartSearch(void);
slist_t * GetSlist(void);

PF(P_GetMapName)
{
	int		map;

	map = Pop();
	Push((int)P_GetMapName(map));
}

PF(P_GetMapLumpName)
{
	int		map;

	map = Pop();
	Push((int)P_GetMapLumpName(map));
}

PF(P_TranslateMap)
{
	int map;

	map = Pop();
	Push((int)P_TranslateMap(map));
}

PF(KeyNameForNum)
{
	int		keynum;
	int		str;

	str = Pop();
	keynum = Pop();
	KeyNameForNum(keynum, (char*)str);
}

PF(IN_GetBindingKeys)
{
	int			name;
	int			*key1;
	int			*key2;

	key2 = (int*)Pop();
	key1 = (int*)Pop();
	name = Pop();
	IN_GetBindingKeys((char*)name, *key1, *key2);
}

PF(IN_SetBinding)
{
	int			keynum;
	int			ondown;
	int			onup;

	onup = Pop();
	ondown = Pop();
	keynum = Pop();
	IN_SetBinding(keynum, (char*)ondown, (char*)onup);
}

PF(SV_GetSaveString)
{
	int		i;
	int		buf;

	buf = Pop();
	i = Pop();
#ifdef SERVER
	Push(SV_GetSaveString(i, (char*)buf));
#else
	Push(0);
#endif
}

PF(GetSlist)
{
	Push((int)GetSlist());
}

void LoadTextLump(char *name, char *buf, int bufsize);

PF(LoadTextLump)
{
	int			name;
	char		*buf;
	int			bufsize;

	bufsize = Pop();
	buf = (char*)Pop();
	name = Pop();
	LoadTextLump((char*)name, buf, bufsize);
}

PF(AllocDlight)
{
	int key = Pop();
	Push((int)CL_AllocDlight(key));
}

PF(NewParticle)
{
	Push((int)R_NewParticle());
}

#endif

//**************************************************************************
//
//		BUILTIN INFO TABLE
//
//**************************************************************************

builtin_info_t BuiltinInfo[] =
{
	//	Error functions
	_(Error),
	_(FatalError),

	//	Cvar functions
	_(CreateCvar),
    _(GetCvar),
    _(SetCvar),
    _(GetCvarF),
    _(SetCvarF),
    _(GetCvarS),
    _(SetCvarS),

	//	Math functions
	_(AngleMod360),
	_(AngleMod180),
	_(sin),
	_(cos),
	_(tan),
	_(atan),
	_(atan2),
	_(Normalize),
	_(Length),
	_(DotProduct),
	_(CrossProduct),
	_(AngleVectors),
	_(AngleVector),
	_(VectorAngles),

	//	String functions
	_(ptrtos),
	_(strgetchar),
	_(strsetchar),
	_(strlen),
	_(strcmp),
	_(stricmp),
	_(strcpy),
	_(strclr),
	_(strcat),
	_(sprint),
	_(va),
	_(atoi),
	_(atof),

	//	Random numbers
    _(Random),
    _(P_Random),

	//	Textures
	_(CheckTextureNumForName),
	_(TextureNumForName),
	_(CheckFlatNumForName),
	_(FlatNumForName),
	_(TextureHeight),

	//	Message IO functions
	_(MSG_Select),
	_(MSG_WriteByte),
	_(MSG_WriteShort),
	_(MSG_WriteLong),
	_(MSG_ReadChar),
	_(MSG_ReadByte),
	_(MSG_ReadShort),
	_(MSG_ReadWord),
	_(MSG_ReadLong),

	//	Printinf in console
	_(print),
	_(dprint),

	_(itof),
	_(ftoi),
    _(Cmd_CheckParm),
	_(CmdBuf_AddText),
	_(Info_ValueForKey),
	_(WadLumpPresent),
	_(SpawnObject),
	_(FindClass),

#ifdef CLIENT
	_(P_GetMapName),
	_(P_GetMapLumpName),
	_(P_TranslateMap),
	_(KeyNameForNum),
	_(IN_GetBindingKeys),
	_(IN_SetBinding),
	_(SV_GetSaveString),
	__(StartSearch),
	_(GetSlist),

	_(LoadTextLump),
	_(AllocDlight),
	_(NewParticle),

	//	Graphics
	_(R_RegisterPic),
	_(R_RegisterPicPal),
	_(R_GetPicInfo),
	_(R_DrawPic),
	_(R_DrawPic2),
	_(R_DrawShadowedPic),
	_(R_InstallSprite),
	_(R_DrawSpritePatch),
	_(InstallModel),
	_(R_DrawModelFrame),
	_(R_FillRectWithFlat),
	_(R_ShadeRect),

	//	Text
	_(T_SetFont),
	_(T_SetAlign),
	_(T_SetDist),
	_(T_SetShadow),
	_(T_TextWidth),
	_(T_TextHeight),
	_(T_DrawText),
	_(T_DrawNText),
	__(T_DrawCursor),

	//	Client side sound
	_(LocalSound),
	_(PlayVoice),
	_(LocalSoundTillDone),

	_(TranslateKey),
#endif
#ifdef SERVER
	//	Print functions
	_(bprint),
	_(cprint),
	_(centerprint),

	//	Map utilites
	_(LineOpenings),
	_(P_BoxOnLineSide),
    _(P_BlockThingsIterator),
	_(P_PathTraverse),
	_(FindThingGap),
	_(FindOpening),
	_(PointInRegion),
	_(AddExtraFloor),
	_(SwapPlanes),
	_(MapBlock),

	//	Mobj utilites
    _(NewMobjThinker),
    _(RemoveMobjThinker),
    _(NextMobj),

    //	Special thinker utilites
    _(NewSpecialThinker),
    _(RemoveSpecialThinker),
    _(P_ChangeSwitchTexture),
	_(NextThinker),

    //	Polyobj functions
    _(SpawnPolyobj),
    _(AddAnchorPoint),
	_(GetPolyobj),
    _(GetPolyobjMirror),
	_(PO_MovePolyobj),
 	_(PO_RotatePolyobj),

	//	ACS functions
	_(StartACS),
    _(SuspendACS),
    _(TerminateACS),
    _(TagFinished),
    _(PolyobjFinished),

	//	Sound functions
	_(StartSoundAtVolume),
    _(SectorStartSound),
    _(SectorStopSound),
    _(GetSoundPlayingInfo),
    _(GetSoundID),
    _(SectorStartSequence),
    _(SectorStopSequence),
    _(PolyobjStartSequence),
    _(PolyobjStopSequence),

    //  Savegame archieve / unarchieve utilite functions
    _(SectorToNum),
    _(NumToSector),
    _(NumToMobj),
    _(MobjToNum),

    _(G_ExitLevel),
    _(G_SecretExitLevel),
    _(G_Completed),
    _(PointInSubsector),
    _(P_GetPlayerNum),
    _(SB_Start),
    _(ClearPlayer),
	_(TerrainType),
    _(P_ForceLightning),
	_(SetFloorPic),
	_(SetCeilPic),
	_(SetLineTransluc),
	_(SendFloorSlope),
	_(SendCeilingSlope),
	_(FindModel),
	_(GetModelIndex),
	_(FindSkin),
	_(MSG_SelectClientMsg),
#endif
    {NULL, NULL, NULL}
};

//**************************************************************************
//
//	$Log$
//	Revision 1.42  2002/07/27 18:10:11  dj_jl
//	Implementing Strife conversations.
//
//	Revision 1.41  2002/07/23 16:29:56  dj_jl
//	Replaced console streams with output device class.
//	
//	Revision 1.40  2002/07/13 07:48:08  dj_jl
//	Moved some global functions to Entity class.
//	
//	Revision 1.39  2002/06/22 07:10:42  dj_jl
//	Added FindClass.
//	
//	Revision 1.38  2002/04/11 16:44:44  dj_jl
//	Got rid of some warnings.
//	
//	Revision 1.37  2002/03/28 18:03:53  dj_jl
//	Added SV_GetModelIndex
//	
//	Revision 1.36  2002/03/09 18:05:34  dj_jl
//	Added support for defining native functions outside pr_cmds
//	
//	Revision 1.35  2002/03/02 17:27:48  dj_jl
//	Renamed builtin Spawn to SpawnObject
//	
//	Revision 1.34  2002/02/26 17:53:08  dj_jl
//	Fixes for menus.
//	
//	Revision 1.33  2002/02/22 18:09:52  dj_jl
//	Some improvements, beautification.
//	
//	Revision 1.32  2002/02/15 19:12:03  dj_jl
//	Property namig style change
//	
//	Revision 1.31  2002/02/02 19:20:41  dj_jl
//	FFunction pointers used instead of the function numbers
//	
//	Revision 1.30  2002/01/29 18:17:27  dj_jl
//	Fixed saving of mobj pointers
//	
//	Revision 1.29  2002/01/12 18:04:01  dj_jl
//	Added unarchieving of names
//	
//	Revision 1.28  2002/01/11 18:22:41  dj_jl
//	Started to use names in progs
//	
//	Revision 1.27  2002/01/11 08:08:26  dj_jl
//	Added names to progs
//	Added sector plane swapping
//	
//	Revision 1.26  2002/01/07 12:16:43  dj_jl
//	Changed copyright year
//	
//	Revision 1.25  2001/12/27 17:33:29  dj_jl
//	Removed thinker list
//	
//	Revision 1.24  2001/12/18 19:03:16  dj_jl
//	A lots of work on VObject
//	
//	Revision 1.23  2001/12/12 19:28:49  dj_jl
//	Some little changes, beautification
//	
//	Revision 1.22  2001/12/04 18:16:28  dj_jl
//	Player models and skins handled by server
//	
//	Revision 1.21  2001/12/01 17:43:13  dj_jl
//	Renamed ClassBase to VObject
//	
//	Revision 1.20  2001/11/09 14:33:35  dj_jl
//	Moved input line to progs
//	Builtins for accessing and changing characters in strings
//	
//	Revision 1.19  2001/10/27 07:50:55  dj_jl
//	Some new builtins
//	
//	Revision 1.18  2001/10/22 17:25:55  dj_jl
//	Floatification of angles
//	
//	Revision 1.17  2001/10/18 17:36:31  dj_jl
//	A lots of changes for Alpha 2
//	
//	Revision 1.16  2001/10/12 17:31:13  dj_jl
//	no message
//	
//	Revision 1.15  2001/10/08 17:34:57  dj_jl
//	A lots of small changes and cleanups
//	
//	Revision 1.14  2001/10/02 17:36:08  dj_jl
//	Removed status bar widgets
//	
//	Revision 1.13  2001/09/27 17:34:22  dj_jl
//	Fixed bug with input line
//	
//	Revision 1.12  2001/09/27 17:03:20  dj_jl
//	Support for multiple mobj classes
//	
//	Revision 1.11  2001/09/25 17:05:34  dj_jl
//	Added stricmp
//	
//	Revision 1.10  2001/09/24 17:35:24  dj_jl
//	Support for thinker classes
//	
//	Revision 1.9  2001/09/20 16:30:28  dj_jl
//	Started to use object-oriented stuff in progs
//	
//	Revision 1.8  2001/08/30 17:45:35  dj_jl
//	Sound channels, moving messsage box to progs
//	
//	Revision 1.7  2001/08/23 17:47:22  dj_jl
//	Started work on pics with custom palettes
//	
//	Revision 1.6  2001/08/21 17:39:22  dj_jl
//	Real string pointers in progs
//	
//	Revision 1.5  2001/08/17 17:43:40  dj_jl
//	LINUX fixes
//	
//	Revision 1.4  2001/08/15 17:21:47  dj_jl
//	Added model drawing for menu
//	
//	Revision 1.3  2001/07/31 17:16:31  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
