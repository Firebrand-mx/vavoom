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
//**	Copyright (C) 1999-2001 J�nis Legzdi��
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

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"

// MACROS ------------------------------------------------------------------

#define FINEANGLES		8192
#define FINEMASK		(FINEANGLES - 1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT	19		

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	mlog2
//
//==========================================================================

int mlog2(int val)
{
	int answer = 0;
	while (val >>= 1)
		answer++;
	return answer;
}

//==========================================================================
//
//	AngleVectors
//
//==========================================================================

void AngleVectors(const TBAMVec &angles, TVec &forward, TVec &right, TVec &up)
{
#if 0
	float		sr, sp, sy, cr, cp, cy;

	sy = msin(angles.yaw);
	cy = mcos(angles.yaw);
	sp = msin(angles.pitch);
	cp = mcos(angles.pitch);
	sr = msin(angles.roll);
	cr = mcos(angles.roll);
#else
	double		ay, ap, ar;
	double		sr, sp, sy, cr, cp, cy;

	ay = BAM2RAD(angles.yaw);
	ap = BAM2RAD(angles.pitch);
	ar = BAM2RAD(angles.roll);

	sy = sin(ay);
	cy = cos(ay);
	sp = sin(ap);
	cp = cos(ap);
	sr = sin(ar);
	cr = cos(ar);
#endif

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;
	right[0] = -sr * sp * cy + cr * sy;
	right[1] = -sr * sp * sy - cr * cy;
	right[2] = -sr * cp;
	up[0] = cr * sp * cy + sr * sy;
	up[1] = cr * sp * sy - sr * cy;
	up[2] = cr * cp;
}

//==========================================================================
//
//	AngleVector
//
//==========================================================================

void AngleVector(const TBAMVec &angles, TVec &forward)
{
	float		sp, sy, cp, cy;
	
	sy = msin(angles.yaw);
	cy = mcos(angles.yaw);
	sp = msin(angles.pitch);
	cp = mcos(angles.pitch);

	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;
}

//==========================================================================
//
//	VectorAngles
//
//==========================================================================

void VectorAngles(const TVec &vec, TBAMVec &angles)
{
	angles.pitch = -bam_matan(vec.z, sqrt(vec.x * vec.x + vec.y * vec.y));
	angles.yaw = bam_matan(vec.y, vec.x);
	angles.roll = 0;
}

//==========================================================================
//
//	AngleMod
//
//==========================================================================

float AngleMod(float angle)
{
	while (angle < 0.0)
		angle += 360.0;
	while (angle >= 360.0)
		angle -= 360.0;
	return angle;
}

//==========================================================================
//
//	AngleMod180
//
//==========================================================================

float AngleMod180(float angle)
{
	while (angle < -180.0)
		angle += 360.0;
	while (angle >= 180.0)
		angle -= 360.0;
	return angle;
}

//==========================================================================
//
//	AngleVectors
//
//==========================================================================

void AngleVectors(const TAVec &angles, TVec &forward, TVec &right, TVec &up)
{
#if 0
	float		sr, sp, sy, cr, cp, cy;

	sy = msin(angles.yaw);
	cy = mcos(angles.yaw);
	sp = msin(angles.pitch);
	cp = mcos(angles.pitch);
	sr = msin(angles.roll);
	cr = mcos(angles.roll);
#else
	double		ay, ap, ar;
	double		sr, sp, sy, cr, cp, cy;

	ay = DEG2RAD(angles.yaw);
	ap = DEG2RAD(angles.pitch);
	ar = DEG2RAD(angles.roll);

	sy = sin(ay);
	cy = cos(ay);
	sp = sin(ap);
	cp = cos(ap);
	sr = sin(ar);
	cr = cos(ar);
#endif

	forward[0] = cp * cy;
	forward[1] = cp * sy;
	forward[2] = -sp;
	right[0] = -sr * sp * cy + cr * sy;
	right[1] = -sr * sp * sy - cr * cy;
	right[2] = -sr * cp;
	up[0] = cr * sp * cy + sr * sy;
	up[1] = cr * sp * sy - sr * cy;
	up[2] = cr * cp;
}

//==========================================================================
//
//	AngleVector
//
//==========================================================================

void AngleVector(const TAVec &angles, TVec &forward)
{
	float		sp, sy, cp, cy;
	
	sy = msin(angles.yaw);
	cy = mcos(angles.yaw);
	sp = msin(angles.pitch);
	cp = mcos(angles.pitch);

	forward.x = cp * cy;
	forward.y = cp * sy;
	forward.z = -sp;
}

//==========================================================================
//
//	VectorAngles
//
//==========================================================================

void VectorAngles(const TVec &vec, TAVec &angles)
{
	angles.pitch = -matan(vec.z, sqrt(vec.x * vec.x + vec.y * vec.y));
	angles.yaw = matan(vec.y, vec.x);
	angles.roll = 0;
}

//==========================================================================
//
//	ProjectPointOnPlane
//
//==========================================================================

void ProjectPointOnPlane(TVec &dst, const TVec &p, const TVec &normal)
{
	float d;
	float inv_denom;

	inv_denom = 1.0F / DotProduct(normal, normal);

	d = DotProduct(normal, p) * inv_denom;

	dst = p - d * (normal * inv_denom);
}

//==========================================================================
//
//	PerpendicularVector
//
//	assumes "src" is normalized
//
//==========================================================================

void PerpendicularVector(TVec &dst, const TVec &src)
{
	int pos;
	int i;
	float minelem = 1.0F;
	TVec tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for (pos = 0, i = 0; i < 3; i++)
	{
		if (fabs(src[i]) < minelem)
		{
			pos = i;
			minelem = fabs(src[i]);
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane(dst, tempvec, src);

	/*
	** normalize the result
	*/
	dst = Normalize(dst);
}

//**************************************************************************
//
//	$Log$
//	Revision 1.4  2001/10/18 17:36:31  dj_jl
//	A lots of changes for Alpha 2
//
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
