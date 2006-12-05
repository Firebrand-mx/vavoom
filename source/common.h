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
//**	Copyright (C) 1999-2006 Jānis Legzdiņš
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

#ifdef _MSC_VER
// Disable some unwanted warnings
#pragma warning(disable : 4097) // typedef-name '' used as synonym for class-name ''
#pragma warning(disable : 4127) // Conditional expression is constant
#pragma warning(disable : 4244) // conversion to float, possible loss of data
#pragma warning(disable : 4284) // return type is not a UDT or reference to a UDT
#pragma warning(disable : 4291) // no matching operator delete found
#pragma warning(disable : 4305) // truncation from 'const double' to 'float'
#pragma warning(disable : 4512) // assignment operator could not be generated
#pragma warning(disable : 4514)	// unreferenced inline function has been removed
#pragma warning(disable : 4702) // unreachable code in inline expanded function
#pragma warning(disable : 4710) // inline function not expanded

// Workaround of for variable scoping
#define for		if (0) ; else for
#endif

#ifdef __BORLANDC__
// Disable some unwanted warnings
#pragma warn -8027		// inline function not expanded
#pragma warn -8071		// conversation may loose significant digits
#endif

#ifndef __GNUC__
#define __attribute__(whatever)
#endif

#ifndef _WIN32
#define __declspec(whatever)
#endif

//==========================================================================
//
//	Basic types
//
//==========================================================================

#define MIN_VINT8	((vint8)-128)
#define MIN_VINT16	((vint16)-32768)
#define MIN_VINT32	((vint32)-2147483648)

#define MAX_VINT8	((vint8)0x7f)
#define MAX_VINT16	((vint16)0x7fff)
#define MAX_VINT32	((vint32)0x7fffffff)

#define MAX_VUINT8	((vuint8)0xff)
#define MAX_VUINT16	((vuint16)0xffff)
#define MAX_VUINT32	((vuint32)0xffffffff)

enum ENoInit { E_NoInit };

typedef unsigned char 		byte;
typedef unsigned short	 	word;

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
typedef int8_t				vint8;
typedef uint8_t				vuint8;
typedef int16_t				vint16;
typedef uint16_t			vuint16;
typedef int32_t				vint32;
typedef uint32_t			vuint32;
/* Needs more changes to compile with MSVC
#elif defined _WIN32
typedef __int8				vint8;
typedef unsigned __int8		vuint8;
typedef __int16				vint16;
typedef unsigned __int16	vuint16;
typedef __int32				vint32;
typedef unsigned __int32	vuint32;
*/
#else
typedef char				vint8;
typedef unsigned char		vuint8;
typedef short				vint16;
typedef unsigned short		vuint16;
typedef int					vint32;
typedef unsigned int		vuint32;
#endif

//==========================================================================
//
//	Standard macros
//
//==========================================================================

//	Number of elements in an array.
#define ARRAY_COUNT(array)				(sizeof(array) / sizeof((array)[0]))

//	Offset of a struct member.
#define STRUCT_OFFSET(struc, member)	((int)&((struc *)NULL)->member)

//==========================================================================
//
//	Guard macros
//
//==========================================================================

//	Turn on usage of context in guard macros on platforms where it's not
// safe to throw an exception in signal handler.
#ifdef __linux__
#define USE_GUARD_SIGNAL_CONTEXT
#endif

#ifdef USE_GUARD_SIGNAL_CONTEXT
#include <setjmp.h>
//	Stack control.
class __Context
{
public:
	__Context() { memcpy(&Last, &Env, sizeof(jmp_buf)); }
	~__Context() { memcpy(&Env, &Last, sizeof(jmp_buf)); }
	static jmp_buf Env;
	static const char* ErrToThrow;

protected:
	jmp_buf Last;
};
#endif

#if defined(_DEBUG) || !DO_GUARD
#define guard(name)		static const char __FUNC_NAME__[] = #name; {
#define unguard			}
#define unguardf(msg)	}
#elif defined(USE_GUARD_SIGNAL_CONTEXT)
#define guard(name)		{static const char __FUNC_NAME__[] = #name; \
	__Context __LOCAL_CONTEXT__; try { if (setjmp(__Context::Env)) { \
	throw VavoomError(__Context::ErrToThrow); } else {
#define unguard			}} catch (RecoverableError &e) { throw e; } \
	catch (...) { Host_CoreDump(__FUNC_NAME__); throw; }}
#define unguardf(msg)	}} catch (RecoverableError &e) { throw e; } \
	catch (...) { Host_CoreDump(__FUNC_NAME__); Host_CoreDump msg; throw; }}
#else
#define guard(name)		{static const char __FUNC_NAME__[] = #name; try {
#define unguard			} catch (RecoverableError &e) { throw e; } \
	catch (...) { Host_CoreDump(__FUNC_NAME__); throw; }}
#define unguardf(msg)	} catch (RecoverableError &e) { throw e; } \
	catch (...) { Host_CoreDump(__FUNC_NAME__); Host_CoreDump msg; throw; }}
#endif

#if !defined(_DEBUG) && DO_GUARD_SLOW
#define guardSlow(name)		guard(name)
#define unguardSlow			unguard
#define unguardfSlow(msg)	unguardf(msg)
#else
#define guardSlow(name)		{
#define unguardSlow			}
#define unguardfSlow(msg)	}
#endif

//==========================================================================
//
//	Errors
//
//==========================================================================

#define MAX_ERROR_TEXT_SIZE		1024

class VavoomError
{
public:
	char message[MAX_ERROR_TEXT_SIZE];

	explicit VavoomError(const char *text);
};

class RecoverableError : public VavoomError
{
public:
	explicit RecoverableError(const char *text) : VavoomError(text) { }
};

//==========================================================================
//
//	Assertion macros
//
//==========================================================================

#if DO_CHECK
#define check(e)	if (!(e)) throw VavoomError("Assertion failed: " #e)
#define verify(e)	if (!(e)) throw VavoomError("Assertion failed: " #e)
#else
#define check(e)
#define verify(e)	(e)
#endif

#if DO_CHECK_SLOW
#define checkSlow(e)	check(e)
#define verifySlow(e)	verify(e)
#else
#define checkSlow(e)
#define verifySlow(e)	(e)
#endif

//==========================================================================
//
//	Basic templates
//
//==========================================================================

template<class T> T Min(T val1, T val2)
{
	return val1 < val2 ? val1 : val2;
}

template<class T> T Max(T val1, T val2)
{
	return val1 > val2 ? val1 : val2;
}

template<class T> T Clamp(T val, T low, T high)
{
	return val < low ? low : val > high ? high : val;
}

//==========================================================================
//
//	Forward declarations
//
//==========================================================================

class	VName;
class	VStr;
class	VStream;

class	VMemberBase;
class	VPackage;
class	VField;
class	VMethod;
class	VState;
class	VConstant;
class	VStruct;
class	VClass;
struct	mobjinfo_t;

class	VObject;
