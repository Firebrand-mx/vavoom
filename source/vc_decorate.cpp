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

// HEADER FILES ------------------------------------------------------------

#include "vc_local.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum
{
	OLDDEC_Decoration,
	OLDDEC_Breakable,
	OLDDEC_Projectile,
	OLDDEC_Pickup,
};

enum
{
	BOUNCE_None,
	BOUNCE_Doom,
	BOUNCE_Heretic,
	BOUNCE_Hexen
};

struct VClassFixup
{
	VClass**	Ptr;
	VStr		Name;
	VClass*		ReqParent;
};

//==========================================================================
//
//	VDecorateSingleName
//
//==========================================================================

class VDecorateSingleName : public VExpression
{
public:
	VName			Name;

	VDecorateSingleName(VName, const TLocation&);
	VExpression* DoResolve(VEmitContext&);
	void Emit(VEmitContext&);
};

//==========================================================================
//
//	VDecorateInvocation
//
//==========================================================================

class VDecorateInvocation : public VExpression
{
public:
	VName			Name;
	int				NumArgs;
	VExpression*	Args[VMethod::MAX_PARAMS + 1];

	VDecorateInvocation(VName, const TLocation&, int, VExpression**);
	~VDecorateInvocation();
	VExpression* DoResolve(VEmitContext&);
	void Emit(VEmitContext&);
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static VExpression* ParseExpressionPriority13(VScriptParser* sc);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static VPackage*		DecPkg;

static VClass*			ActorClass;
static VClass*			ScriptedEntityClass;
static VClass*			FakeInventoryClass;
static VClass*			InventoryClass;
static VClass*			AmmoClass;
static VClass*			BasicArmorPickupClass;
static VClass*			BasicArmorBonusClass;
static VClass*			HealthClass;
static VClass*			PowerupGiverClass;
static VClass*			PuzzleItemClass;
static VClass*			WeaponClass;
//static VClass*			WeaponPieceClass;
static VClass*			PlayerPawnClass;

static VMethod*			FuncA_Scream;
static VMethod*			FuncA_NoBlocking;
static VMethod*			FuncA_ScreamAndUnblock;
static VMethod*			FuncA_ActiveSound;
static VMethod*			FuncA_ActiveAndUnblock;
static VMethod*			FuncA_ExplodeParms;

// CODE --------------------------------------------------------------------

//==========================================================================
//
//	VDecorateSingleName::VDecorateSingleName
//
//==========================================================================

VDecorateSingleName::VDecorateSingleName(VName AName, const TLocation& ALoc)
: VExpression(ALoc)
, Name(AName)
{
}

//==========================================================================
//
//	VDecorateSingleName::DoResolve
//
//==========================================================================

VExpression* VDecorateSingleName::DoResolve(VEmitContext& ec)
{
	guard(VDecorateSingleName::DoResolve);
	//	Look only for constants defined in DECORATE scripts.
	VConstant* Const = ec.Package->FindConstant(Name);
	if (Const)
	{
		VExpression* e = new VConstantValue(Const, Loc);
		delete this;
		return e->Resolve(ec);
	}

	ParseError(Loc, "Illegal expression identifier %s", *Name);
	delete this;
	return NULL;
	unguard;
}

//==========================================================================
//
//	VDecorateSingleName::Emit
//
//==========================================================================

void VDecorateSingleName::Emit(VEmitContext&)
{
	ParseError(Loc, "Should not happen");
}

//==========================================================================
//
//	VDecorateInvocation::VDecorateInvocation
//
//==========================================================================

VDecorateInvocation::VDecorateInvocation(VName AName, const TLocation& ALoc, int ANumArgs,
	VExpression** AArgs)
: VExpression(ALoc)
, Name(AName)
, NumArgs(ANumArgs)
{
	for (int i = 0; i < NumArgs; i++)
		Args[i] = AArgs[i];
}

//==========================================================================
//
//	VDecorateInvocation::~VDecorateInvocation
//
//==========================================================================

VDecorateInvocation::~VDecorateInvocation()
{
	for (int i = 0; i < NumArgs; i++)
		if (Args[i])
			delete Args[i];
}

//==========================================================================
//
//	VDecorateInvocation::DoResolve
//
//==========================================================================

VExpression* VDecorateInvocation::DoResolve(VEmitContext& ec)
{
	guard(VDecorateInvocation::DoResolve);
	if (ec.SelfClass)
	{
		//	First try with decorate_ prefix, then without.
		VMethod* M = ec.SelfClass->FindMethod(va("decorate_%s", *Name));
		if (!M)
		{
			M = ec.SelfClass->FindMethod(Name);
		}
		if (M)
		{
			if (M->Flags & FUNC_Iterator)
			{
				ParseError(Loc, "Iterator methods can only be used in foreach statements");
				delete this;
				return NULL;
			}
			VExpression* e = new VInvocation(NULL, M, NULL,
				false, false, Loc, NumArgs, Args);
			NumArgs = 0;
			delete this;
			return e->Resolve(ec);
		}
	}

	ParseError(Loc, "Unknown method %s", *Name);
	delete this;
	return NULL;
	unguard;
}

//==========================================================================
//
//	VDecorateInvocation::Emit
//
//==========================================================================

void VDecorateInvocation::Emit(VEmitContext&)
{
	ParseError(Loc, "Should not happen");
}

//==========================================================================
//
//	GetClassFieldFloat
//
//==========================================================================

static float GetClassFieldFloat(VClass* Class, const char* FieldName)
{
	guard(GetClassFieldFloat);
	VField* F = Class->FindFieldChecked(FieldName);
	float* Ptr = (float*)(Class->Defaults + F->Ofs);
	return *Ptr;
	unguard;
}

//==========================================================================
//
//	SetClassFieldInt
//
//==========================================================================

static void SetClassFieldInt(VClass* Class, const char* FieldName,
	int Value, int Idx = 0)
{
	guard(SetClassFieldInt);
	VField* F = Class->FindFieldChecked(FieldName);
	vint32* Ptr = (vint32*)(Class->Defaults + F->Ofs);
	Ptr[Idx] = Value;
	unguard;
}

//==========================================================================
//
//	SetClassFieldByte
//
//==========================================================================

static void SetClassFieldByte(VClass* Class, const char* FieldName,
	vuint8 Value)
{
	guard(SetClassFieldByte);
	VField* F = Class->FindFieldChecked(FieldName);
	vuint8* Ptr = Class->Defaults + F->Ofs;
	*Ptr = Value;
	unguard;
}

//==========================================================================
//
//	SetClassFieldFloat
//
//==========================================================================

static void SetClassFieldFloat(VClass* Class, const char* FieldName,
	float Value)
{
	guard(SetClassFieldFloat);
	VField* F = Class->FindFieldChecked(FieldName);
	float* Ptr = (float*)(Class->Defaults + F->Ofs);
	*Ptr = Value;
	unguard;
}

//==========================================================================
//
//	SetClassFieldBool
//
//==========================================================================

static void SetClassFieldBool(VClass* Class, const char* FieldName, int Value)
{
	guard(SetClassFieldBool);
	VField* F = Class->FindFieldChecked(FieldName);
	vuint32* Ptr = (vuint32*)(Class->Defaults + F->Ofs);
	if (Value)
		*Ptr |= F->Type.BitMask;
	else
		*Ptr &= ~F->Type.BitMask;
	unguard;
}

//==========================================================================
//
//	SetClassFieldName
//
//==========================================================================

static void SetClassFieldName(VClass* Class, const char* FieldName,
	VName Value)
{
	guard(SetClassFieldName);
	VField* F = Class->FindFieldChecked(FieldName);
	VName* Ptr = (VName*)(Class->Defaults + F->Ofs);
	*Ptr = Value;
	unguard;
}

//==========================================================================
//
//	SetClassFieldStr
//
//==========================================================================

static void SetClassFieldStr(VClass* Class, const char* FieldName,
	const VStr& Value)
{
	guard(SetClassFieldStr);
	VField* F = Class->FindFieldChecked(FieldName);
	VStr* Ptr = (VStr*)(Class->Defaults + F->Ofs);
	*Ptr = Value;
	unguard;
}

//==========================================================================
//
//	AddClassFixup
//
//==========================================================================

static void AddClassFixup(VClass* Class, const char* FieldName,
	const VStr& ClassName, TArray<VClassFixup>& ClassFixups)
{
	guard(AddClassFixup);
	VField* F = Class->FindFieldChecked(FieldName);
	VClassFixup& CF = ClassFixups.Alloc();
	CF.Ptr = (VClass**)(Class->Defaults + F->Ofs);
	CF.Name = ClassName;
	CF.ReqParent = F->Type.Class;
	unguard;
}

//==========================================================================
//
//	SkipBlock
//
//==========================================================================

static void SkipBlock(VScriptParser* sc, int Level)
{
	while (!sc->AtEnd() && Level > 0)
	{
		if (sc->Check("{"))
		{
			Level++;
		}
		else if (sc->Check("}"))
		{
			Level--;
		}
		else
		{
			sc->GetString();
		}
	}
}

//==========================================================================
//
//	ParseMethodCall
//
//==========================================================================

static VExpression* ParseMethodCall(VScriptParser* sc, VName Name,
	TLocation Loc)
{
	guard(ParseMethodCall);
	VExpression* Args[VMethod::MAX_PARAMS + 1];
	int NumArgs = 0;
	if (!sc->Check(")"))
	{
		do
		{
			Args[NumArgs] = ParseExpressionPriority13(sc);
			if (NumArgs == VMethod::MAX_PARAMS)
				ParseError(sc->GetLoc(), "Too many arguments");
			else
				NumArgs++;
		} while (sc->Check(","));
		sc->Expect(")");
	}
	return new VDecorateInvocation(Name, Loc, NumArgs, Args);
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority0
//
//==========================================================================

static VExpression* ParseExpressionPriority0(VScriptParser* sc)
{
	guard(ParseExpressionPriority0);
	TLocation l = sc->GetLoc();
	if (sc->CheckNumber())
	{
		vint32 Val = sc->Number;
		return new VIntLiteral(Val, l);
	}

	if (sc->CheckFloat())
	{
		float Val = sc->Float;
		return new VFloatLiteral(Val, l);
	}

	if (sc->CheckQuotedString())
	{
		int Val = DecPkg->FindString(*sc->String);
		return new VStringLiteral(Val, l);
	}

	if (sc->Check("false"))
	{
		return new VIntLiteral(0, l);
	}

	if (sc->Check("true"))
	{
		return new VIntLiteral(1, l);
	}

	if (sc->Check("none"))
	{
		return new VNoneLiteral(l);
	}

	if (sc->Check("("))
	{
		VExpression* op = ParseExpressionPriority13(sc);
		if (!op)
		{
			ParseError(l, "Expression expected");
		}
		sc->Expect(")");
		return op;
	}

	if (sc->CheckIdentifier())
	{
		VStr Name = sc->String;
		if (sc->Check("("))
		{
			return ParseMethodCall(sc, *Name, l);
		}
		return new VDecorateSingleName(*sc->String.ToLower(), l);
	}

	return NULL;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority1
//
//==========================================================================

static VExpression* ParseExpressionPriority1(VScriptParser* sc)
{
	guard(ParseExpressionPriority1);
	return ParseExpressionPriority0(sc);
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority2
//
//==========================================================================

static VExpression* ParseExpressionPriority2(VScriptParser* sc)
{
	guard(ParseExpressionPriority2);
	VExpression*	op;

	TLocation l = sc->GetLoc();

	if (sc->Check("+"))
	{
		op = ParseExpressionPriority2(sc);
		return new VUnary(VUnary::Plus, op, l);
	}

	if (sc->Check("-"))
	{
		op = ParseExpressionPriority2(sc);
		return new VUnary(VUnary::Minus, op, l);
	}

	if (sc->Check("!"))
	{
		op = ParseExpressionPriority2(sc);
		return new VUnary(VUnary::Not, op, l);
	}

	if (sc->Check("~"))
	{
		op = ParseExpressionPriority2(sc);
		return new VUnary(VUnary::BitInvert, op, l);
	}

	return ParseExpressionPriority1(sc);
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority3
//
//==========================================================================

static VExpression* ParseExpressionPriority3(VScriptParser* sc)
{
	guard(ParseExpressionPriority3);
	VExpression* op1 = ParseExpressionPriority2(sc);
	if (!op1)
	{
		return NULL;
	}
	bool done = false;
	do
	{
		TLocation l = sc->GetLoc();
		if (sc->Check("*"))
		{
			VExpression* op2 = ParseExpressionPriority2(sc);
			op1 = new VBinary(VBinary::Multiply, op1, op2, l);
		}
		else if (sc->Check("/"))
		{
			VExpression* op2 = ParseExpressionPriority2(sc);
			op1 = new VBinary(VBinary::Divide, op1, op2, l);
		}
		else if (sc->Check("%"))
		{
			VExpression* op2 = ParseExpressionPriority2(sc);
			op1 = new VBinary(VBinary::Modulus, op1, op2, l);
		}
		else
		{
			done = true;
		}
	}
	while (!done);
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority4
//
//==========================================================================

static VExpression* ParseExpressionPriority4(VScriptParser* sc)
{
	guard(ParseExpressionPriority4);
	VExpression* op1 = ParseExpressionPriority3(sc);
	if (!op1)
	{
		return NULL;
	}
	bool done = false;
	do
	{
		TLocation l = sc->GetLoc();
		if (sc->Check("+"))
		{
			VExpression* op2 = ParseExpressionPriority3(sc);
			op1 = new VBinary(VBinary::Add, op1, op2, l);
		}
		else if (sc->Check("-"))
		{
			VExpression* op2 = ParseExpressionPriority3(sc);
			op1 = new VBinary(VBinary::Subtract, op1, op2, l);
		}
		else
		{
			done = true;
		}
	}
	while (!done);
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority5
//
//==========================================================================

static VExpression* ParseExpressionPriority5(VScriptParser* sc)
{
	guard(ParseExpressionPriority5);
	VExpression* op1 = ParseExpressionPriority4(sc);
	if (!op1)
	{
		return NULL;
	}
	bool done = false;
	do
	{
		TLocation l = sc->GetLoc();
		if (sc->Check("<<"))
		{
			VExpression* op2 = ParseExpressionPriority4(sc);
			op1 = new VBinary(VBinary::LShift, op1, op2, l);
		}
		else if (sc->Check(">>"))
		{
			VExpression* op2 = ParseExpressionPriority4(sc);
			op1 = new VBinary(VBinary::RShift, op1, op2, l);
		}
		else
		{
			done = true;
		}
	}
	while (!done);
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority6
//
//==========================================================================

static VExpression* ParseExpressionPriority6(VScriptParser* sc)
{
	guard(ParseExpressionPriority6);
	VExpression* op1 = ParseExpressionPriority5(sc);
	if (!op1)
	{
		return NULL;
	}
	bool done = false;
	do
	{
		TLocation l = sc->GetLoc();
		if (sc->Check("<"))
		{
			VExpression* op2 = ParseExpressionPriority5(sc);
			op1 = new VBinary(VBinary::Less, op1, op2, l);
		}
		else if (sc->Check("<="))
		{
			VExpression* op2 = ParseExpressionPriority5(sc);
			op1 = new VBinary(VBinary::LessEquals, op1, op2, l);
		}
		else if (sc->Check(">"))
		{
			VExpression* op2 = ParseExpressionPriority5(sc);
			op1 = new VBinary(VBinary::Greater, op1, op2, l);
		}
		else if (sc->Check(">="))
		{
			VExpression* op2 = ParseExpressionPriority5(sc);
			op1 = new VBinary(VBinary::GreaterEquals, op1, op2, l);
		}
		else
		{
			done = true;
		}
	}
	while (!done);
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority7
//
//==========================================================================

static VExpression* ParseExpressionPriority7(VScriptParser* sc)
{
	guard(ParseExpressionPriority7);
	VExpression* op1 = ParseExpressionPriority6(sc);
	if (!op1)
	{
		return NULL;
	}
	bool done = false;
	do
	{
		TLocation l = sc->GetLoc();
		if (sc->Check("=="))
		{
			VExpression* op2 = ParseExpressionPriority6(sc);
			op1 = new VBinary(VBinary::Equals, op1, op2, l);
		}
		else if (sc->Check("!="))
		{
			VExpression* op2 = ParseExpressionPriority6(sc);
			op1 = new VBinary(VBinary::NotEquals, op1, op2, l);
		}
		else
		{
			done = true;
		}
	} while (!done);
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority8
//
//==========================================================================

static VExpression* ParseExpressionPriority8(VScriptParser* sc)
{
	guard(ParseExpressionPriority8);
	VExpression* op1 = ParseExpressionPriority7(sc);
	if (!op1)
	{
		return NULL;
	}
	TLocation l = sc->GetLoc();
	while (sc->Check("&"))
	{
		VExpression* op2 = ParseExpressionPriority7(sc);
		op1 = new VBinary(VBinary::And, op1, op2, l);
		l = sc->GetLoc();
	}
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority9
//
//==========================================================================

static VExpression* ParseExpressionPriority9(VScriptParser* sc)
{
	guard(ParseExpressionPriority9);
	VExpression* op1 = ParseExpressionPriority8(sc);
	if (!op1)
	{
		return NULL;
	}
	TLocation l = sc->GetLoc();
	while (sc->Check("^"))
	{
		VExpression* op2 = ParseExpressionPriority8(sc);
		op1 = new VBinary(VBinary::XOr, op1, op2, l);
		l = sc->GetLoc();
	}
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority10
//
//==========================================================================

static VExpression* ParseExpressionPriority10(VScriptParser* sc)
{
	guard(ParseExpressionPriority10);
	VExpression* op1 = ParseExpressionPriority9(sc);
	if (!op1)
	{
		return NULL;
	}
	TLocation l = sc->GetLoc();
	while (sc->Check("|"))
	{
		VExpression* op2 = ParseExpressionPriority9(sc);
		op1 = new VBinary(VBinary::Or, op1, op2, l);
		l = sc->GetLoc();
	}
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority11
//
//==========================================================================

static VExpression* ParseExpressionPriority11(VScriptParser* sc)
{
	guard(ParseExpressionPriority11);
	VExpression* op1 = ParseExpressionPriority10(sc);
	if (!op1)
	{
		return NULL;
	}
	TLocation l = sc->GetLoc();
	while (sc->Check("&&"))
	{
		VExpression* op2 = ParseExpressionPriority10(sc);
		op1 = new VBinaryLogical(VBinaryLogical::And, op1, op2, l);
		l = sc->GetLoc();
	}
	return op1;
	unguard;
}

//==========================================================================
//
//	ParseExpressionPriority12
//
//==========================================================================

static VExpression* ParseExpressionPriority12(VScriptParser* sc)
{
	guard(ParseExpressionPriority12);
	VExpression* op1 = ParseExpressionPriority11(sc);
	if (!op1)
	{
		return NULL;
	}
	TLocation l = sc->GetLoc();
	while (sc->Check("||"))
	{
		VExpression* op2 = ParseExpressionPriority11(sc);
		op1 = new VBinaryLogical(VBinaryLogical::Or, op1, op2, l);
		l = sc->GetLoc();
	}
	return op1;
	unguard;
}

//==========================================================================
//
//	VParser::ParseExpressionPriority13
//
//==========================================================================

static VExpression* ParseExpressionPriority13(VScriptParser* sc)
{
	guard(ParseExpressionPriority13);
	VExpression* op = ParseExpressionPriority12(sc);
	if (!op)
	{
		return NULL;
	}
	TLocation l = sc->GetLoc();
	if (sc->Check("?"))
	{
		VExpression* op1 = ParseExpressionPriority13(sc);
		sc->Expect(":");
		VExpression* op2 = ParseExpressionPriority13(sc);
		op = new VConditional(op, op1, op2, l);
	}
	return op;
	unguard;
}

//==========================================================================
//
//	ParseExpression
//
//==========================================================================

static VExpression* ParseExpression(VScriptParser* sc)
{
	guard(ParseExpression);
	return ParseExpressionPriority13(sc);
	unguard;
}

//==========================================================================
//
//	ParseConst
//
//==========================================================================

static void ParseConst(VScriptParser* sc)
{
	guard(ParseConst);
	sc->SetCMode(true);
	sc->Expect("int");
	sc->ExpectString();
	TLocation Loc = sc->GetLoc();
	VStr Name = sc->String.ToLower();
	sc->Expect("=");

	VExpression* Expr = ParseExpression(sc);
	if (!Expr)
	{
		sc->Error("Constant value expected");
	}
	else
	{
		VEmitContext ec(DecPkg);
		Expr = Expr->Resolve(ec);
		if (Expr)
		{
			int Val = Expr->GetIntConst();
			delete Expr;
			GCon->Logf("Constant %s with value %d", *Name, Val);
			VConstant* C = new VConstant(*Name, DecPkg, Loc);
			C->Type = TYPE_Int;
			C->Value = Val;
		}
	}
	sc->Expect(";");
	sc->SetCMode(false);
	unguard;
}

//==========================================================================
//
//	ParseClass
//
//==========================================================================

static void ParseClass(VScriptParser* sc)
{
	guard(ParseClass);
	sc->ExpectString();
	GCon->Logf("Class %s", *sc->String);
	sc->Expect("extends");
	sc->ExpectString();
	sc->Expect("native");
	sc->Expect("{");
	SkipBlock(sc, 1);
	unguard;
}

//==========================================================================
//
//	ParseEnum
//
//==========================================================================

static void ParseEnum(VScriptParser* sc)
{
	guard(ParseEnum);
	GCon->Logf("Enum");
	sc->Expect("{");
	SkipBlock(sc, 1);
	unguard;
}

//==========================================================================
//
//	ParseFlag
//
//==========================================================================

static bool ParseFlag(VScriptParser* sc, VClass* Class, bool Value)
{
	guard(ParseFlag);
	//	Get full name of the flag.
	sc->ExpectIdentifier();
	VStr Flag = sc->String;
	while (sc->Check("."))
	{
		sc->ExpectIdentifier();
		Flag += ".";
		Flag += sc->String;
	}

	//
	//	Physics
	//
	if (!Flag.ICmp("Solid"))
	{
		SetClassFieldBool(Class, "bSolid", Value);
		return true;
	}
	if (!Flag.ICmp("Shootable"))
	{
		SetClassFieldBool(Class, "bShootable", Value);
		return true;
	}
	if (!Flag.ICmp("Float"))
	{
		SetClassFieldBool(Class, "bFloat", Value);
		return true;
	}
	if (!Flag.ICmp("NoGravity"))
	{
		SetClassFieldBool(Class, "bNoGravity", Value);
		return true;
	}
	if (!Flag.ICmp("LowGravity"))
	{
		SetClassFieldFloat(Class, "Gravity", 0.125);
		return true;
	}
	if (!Flag.ICmp("WindThrust"))
	{
		SetClassFieldBool(Class, "bWindThrust", Value);
		return true;
	}
	if (!Flag.ICmp("HereticBounce"))
	{
		SetClassFieldByte(Class, "BounceType", Value ? BOUNCE_Heretic : BOUNCE_None);
		return true;
	}
	if (!Flag.ICmp("HexenBounce"))
	{
		SetClassFieldByte(Class, "BounceType", Value ? BOUNCE_Hexen : BOUNCE_None);
		return true;
	}
	if (!Flag.ICmp("DoomBounce"))
	{
		SetClassFieldByte(Class, "BounceType", Value ? BOUNCE_Doom : BOUNCE_None);
		return true;
	}
	if (!Flag.ICmp("Pushable"))
	{
		SetClassFieldBool(Class, "bPushable", Value);
		return true;
	}
	if (!Flag.ICmp("DontFall"))
	{
		SetClassFieldBool(Class, "bNoGravKill", Value);
		return true;
	}
	if (!Flag.ICmp("CanPass"))
	{
		SetClassFieldBool(Class, "bPassMobj", Value);
		return true;
	}
	if (!Flag.ICmp("ActLikeBridge"))
	{
		//FIXME
		GCon->Logf("Unsupported flag ActLikeBridge in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoBlockmap"))
	{
		SetClassFieldBool(Class, "bNoBlockmap", Value);
		return true;
	}
	if (!Flag.ICmp("NoLiftDrop"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoLiftDrop in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("SlidesOnWalls"))
	{
		SetClassFieldBool(Class, "bSlide", Value);
		return true;
	}
	if (!Flag.ICmp("NoDropOff"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoDropOff in %s", Class->GetName());
		return true;
	}
	//
	//	Behavior
	//
	if (!Flag.ICmp("Ambush"))
	{
		SetClassFieldBool(Class, "bAmbush", Value);
		return true;
	}
	if (!Flag.ICmp("Boss"))
	{
		SetClassFieldBool(Class, "bBoss", Value);
		return true;
	}
	if (!Flag.ICmp("NoSplashAlert"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoSplashAlert in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("LookAllAround"))
	{
		SetClassFieldBool(Class, "bLookAllAround", Value);
		return true;
	}
	if (!Flag.ICmp("StandStill"))
	{
		SetClassFieldBool(Class, "bStanding", Value);
		return true;
	}
	if (!Flag.ICmp("QuickToRetaliate"))
	{
		SetClassFieldBool(Class, "bNoGrudge", Value);
		return true;
	}
	if (!Flag.ICmp("Dormant"))
	{
		SetClassFieldBool(Class, "bDormant", Value);
		return true;
	}
	if (!Flag.ICmp("Friendly"))
	{
		SetClassFieldBool(Class, "bFriendly", Value);
		return true;
	}
	if (!Flag.ICmp("LongMeleeRange"))
	{
		SetClassFieldFloat(Class, "MissileMinRange", Value ? 196.0 : 0.0);
		return true;
	}
	if (!Flag.ICmp("MissileMore"))
	{
		SetClassFieldBool(Class, "bTriggerHappy", Value);
		return true;
	}
	if (!Flag.ICmp("MissileEvenMore"))
	{
		//FIXME
		GCon->Logf("Unsupported flag MissileEvenMore in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("ShortMissileRange"))
	{
		SetClassFieldFloat(Class, "MissileMaxRange", Value ? 896.0 : 0.0);
		return true;
	}
	if (!Flag.ICmp("NoTargetSwitch"))
	{
		SetClassFieldBool(Class, "bNoTargetSwitch", Value);
		return true;
	}
	//
	//	Abilities
	//
	if (!Flag.ICmp("CannotPush"))
	{
		SetClassFieldBool(Class, "bCannotPush", Value);
		return true;
	}
	if (!Flag.ICmp("NoTeleport"))
	{
		SetClassFieldBool(Class, "bNoTeleport", Value);
		return true;
	}
	if (!Flag.ICmp("ActivateImpact"))
	{
		SetClassFieldBool(Class, "bActivateImpact", Value);
		return true;
	}
	if (!Flag.ICmp("CanPushWalls"))
	{
		SetClassFieldBool(Class, "bActivatePushWall", Value);
		return true;
	}
	if (!Flag.ICmp("CanUseWalls"))
	{
		//FIXME
		GCon->Logf("Unsupported flag CanUseWalls in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("ActivateMCross"))
	{
		SetClassFieldBool(Class, "bActivateMCross", Value);
		return true;
	}
	if (!Flag.ICmp("ActivatePCross"))
	{
		SetClassFieldBool(Class, "bActivatePCross", Value);
		return true;
	}
	if (!Flag.ICmp("CantLeaveFloorPic"))
	{
		SetClassFieldBool(Class, "bCantLeaveFloorpic", Value);
		return true;
	}
	if (!Flag.ICmp("Telestomp"))
	{
		SetClassFieldBool(Class, "bTelestomp", Value);
		return true;
	}
	if (!Flag.ICmp("StayMorphed"))
	{
		//FIXME
		GCon->Logf("Unsupported flag StayMorphed in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("CanBlast"))
	{
		//FIXME
		GCon->Logf("Unsupported flag CanBlast in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoBlockMonst"))
	{
		SetClassFieldBool(Class, "bNoBlockMonst", Value);
		return true;
	}
	if (!Flag.ICmp("CanBounceWater"))
	{
		SetClassFieldBool(Class, "bCanBounceWater", Value);
		return true;
	}
	if (!Flag.ICmp("ThruGhost"))
	{
		SetClassFieldBool(Class, "bThruGhost", Value);
		return true;
	}
	if (!Flag.ICmp("Spectral"))
	{
		SetClassFieldBool(Class, "bSpectral", Value);
		return true;
	}
	if (!Flag.ICmp("Frightened"))
	{
		//FIXME
		GCon->Logf("Unsupported flag Frightened in %s", Class->GetName());
		return true;
	}
	//
	//	Defenses
	//
	if (!Flag.ICmp("Invulnerable"))
	{
		SetClassFieldBool(Class, "bInvulnerable", Value);
		return true;
	}
	if (!Flag.ICmp("Reflective"))
	{
		SetClassFieldBool(Class, "bReflective", Value);
		return true;
	}
	if (!Flag.ICmp("ShieldReflect"))
	{
		//FIXME
		GCon->Logf("Unsupported flag ShieldReflect in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("Deflect"))
	{
		//FIXME
		GCon->Logf("Unsupported flag Deflect in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("FireResist"))
	{
		//FIXME
		GCon->Logf("Unsupported flag FireResist in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoRadiusDmg"))
	{
		SetClassFieldBool(Class, "bNoRadiusDamage", Value);
		return true;
	}
	if (!Flag.ICmp("DontBlast"))
	{
		//FIXME
		GCon->Logf("Unsupported flag DontBlast in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoTarget"))
	{
		SetClassFieldBool(Class, "bNeverTarget", Value);
		return true;
	}
	if (!Flag.ICmp("Ghost"))
	{
		//FIXME
		GCon->Logf("Unsupported flag Ghost in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("DontMorph"))
	{
		SetClassFieldBool(Class, "bNoMorph", Value);
		return true;
	}
	if (!Flag.ICmp("DontSquash"))
	{
		//FIXME
		GCon->Logf("Unsupported flag DontSquash in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoTeleOther"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoTeleOther in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("DontHurtSpecies"))
	{
		//FIXME
		GCon->Logf("Unsupported flag DontHurtSpecies in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoDamage"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoDamage in %s", Class->GetName());
		return true;
	}
	//
	//	Appearance and sound
	//
	if (!Flag.ICmp("Invisible"))
	{
		SetClassFieldBool(Class, "bInvisible", Value);
		return true;
	}
	if (!Flag.ICmp("Shadow"))
	{
		//FIXME
		GCon->Logf("Unsupported flag Shadow in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoBlood"))
	{
		SetClassFieldBool(Class, "bNoBlood", Value);
		return true;
	}
	if (!Flag.ICmp("NoBloodDecals"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoBloodDecals in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("Stealth"))
	{
		SetClassFieldBool(Class, "bStealth", Value);
		return true;
	}
	if (!Flag.ICmp("FloorClip"))
	{
		SetClassFieldBool(Class, "bFloorClip", Value);
		return true;
	}
	if (!Flag.ICmp("SpawnFloat"))
	{
		SetClassFieldBool(Class, "bSpawnFloat", Value);
		return true;
	}
	if (!Flag.ICmp("SpawnCeiling"))
	{
		SetClassFieldBool(Class, "bSpawnCeiling", Value);
		return true;
	}
	if (!Flag.ICmp("FloatBob"))
	{
		SetClassFieldBool(Class, "bFloatBob", Value);
		return true;
	}
	if (!Flag.ICmp("NoIceDeath"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoIceDeath in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("DontGib"))
	{
		//FIXME
		GCon->Logf("Unsupported flag DontGib in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("DontSplash"))
	{
		SetClassFieldBool(Class, "bNoSplash", Value);
		return true;
	}
	if (!Flag.ICmp("DontOverlap"))
	{
		SetClassFieldBool(Class, "bDontOverlap", Value);
		return true;
	}
	if (!Flag.ICmp("Randomize"))
	{
		SetClassFieldBool(Class, "bRandomise", Value);
		return true;
	}
	if (!Flag.ICmp("FixMapThingPos"))
	{
		//FIXME
		GCon->Logf("Unsupported flag FixMapThingPos in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("FullVolActive"))
	{
		SetClassFieldBool(Class, "bFullVolActive", Value);
		return true;
	}
	if (!Flag.ICmp("FullVolDeath"))
	{
		SetClassFieldBool(Class, "bFullVolDeath", Value);
		return true;
	}
	if (!Flag.ICmp("NoWallBounceSnd"))
	{
		SetClassFieldBool(Class, "bNoWallBounceSnd", Value);
		return true;
	}
	if (!Flag.ICmp("VisibilityPulse"))
	{
		//FIXME
		GCon->Logf("Unsupported flag VisibilityPulse in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("RocketTrail"))
	{
		SetClassFieldBool(Class, "bLeaveTrail", Value);
		return true;
	}
	if (!Flag.ICmp("GrenadeTrail"))
	{
		//FIXME
		GCon->Logf("Unsupported flag GrenadeTrail in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoBounceSound"))
	{
		SetClassFieldBool(Class, "bNoBounceSound", Value);
		return true;
	}
	if (!Flag.ICmp("NoSkin"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoSkin in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("DontTranslate"))
	{
		//FIXME
		GCon->Logf("Unsupported flag DontTranslate in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NoPain"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NoPain in %s", Class->GetName());
		return true;
	}
	//
	//	Projectile
	//
	if (!Flag.ICmp("Missile"))
	{
		SetClassFieldBool(Class, "bMissile", Value);
		return true;
	}
	if (!Flag.ICmp("Ripper"))
	{
		SetClassFieldBool(Class, "bRip", Value);
		return true;
	}
	if (!Flag.ICmp("FireDamage"))
	{
		SetClassFieldName(Class, "DamageType", Value ? VName("Fire") : NAME_None);
		return true;
	}
	if (!Flag.ICmp("IceDamage"))
	{
		SetClassFieldName(Class, "DamageType", Value ? VName("Ice") : NAME_None);
		return true;
	}
	if (!Flag.ICmp("NoDamageThrust"))
	{
		SetClassFieldBool(Class, "bNoDamageThrust", Value);
		return true;
	}
	if (!Flag.ICmp("DontReflect"))
	{
		//FIXME
		GCon->Logf("Unsupported flag DontReflect in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("FloorHugger"))
	{
		SetClassFieldBool(Class, "bIgnoreFloorStep", Value);
		return true;
	}
	if (!Flag.ICmp("CeilingHugger"))
	{
		SetClassFieldBool(Class, "bIgnoreCeilingStep", Value);
		return true;
	}
	if (!Flag.ICmp("BloodlessImpact"))
	{
		SetClassFieldBool(Class, "bBloodlessImpact", Value);
		return true;
	}
	if (!Flag.ICmp("BloodSplatter"))
	{
		//FIXME
		GCon->Logf("Unsupported flag BloodSplatter in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("FoilInvul"))
	{
		SetClassFieldBool(Class, "bDamageInvulnerable", Value);
		return true;
	}
	if (!Flag.ICmp("SeekerMissile"))
	{
		SetClassFieldBool(Class, "bSeekerMissile", Value);
		return true;
	}
	if (!Flag.ICmp("SkyExplode"))
	{
		SetClassFieldBool(Class, "bExplodeOnSky", Value);
		return true;
	}
	if (!Flag.ICmp("NoExplodeFloor"))
	{
		SetClassFieldBool(Class, "bNoExplodeFloor", Value);
		return true;
	}
	if (!Flag.ICmp("StrifeDamage"))
	{
		SetClassFieldBool(Class, "bStrifeDamage", Value);
		return true;
	}
	if (!Flag.ICmp("ExtremeDeath"))
	{
		SetClassFieldBool(Class, "bExtremeDeath", Value);
		return true;
	}
	if (!Flag.ICmp("NoExtremeDeath"))
	{
		SetClassFieldBool(Class, "bNoExtremeDeath", Value);
		return true;
	}
	if (!Flag.ICmp("BounceOnActors"))
	{
		SetClassFieldBool(Class, "bBounceOnActors", Value);
		return true;
	}
	if (!Flag.ICmp("ExplodeOnWater"))
	{
		SetClassFieldBool(Class, "bExplodeOnWater", Value);
		return true;
	}
	if (!Flag.ICmp("DehExplosion"))
	{
		//FIXME
		GCon->Logf("Unsupported flag DehExplosion in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("PierceArmor"))
	{
		//FIXME
		GCon->Logf("Unsupported flag PierceArmor in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("ForceRadiusDmg"))
	{
		//FIXME
		GCon->Logf("Unsupported flag ForceRadiusDmg in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("SpawnSoundSource"))
	{
		//FIXME
		GCon->Logf("Unsupported flag SpawnSoundSource in %s", Class->GetName());
		return true;
	}
	//
	//	Miscellaneous
	//
	if (!Flag.ICmp("Dropped"))
	{
		SetClassFieldBool(Class, "bDropped", Value);
		return true;
	}
	if (!Flag.ICmp("IsMonster"))
	{
		SetClassFieldBool(Class, "bMonster", Value);
		return true;
	}
	if (!Flag.ICmp("Corpse"))
	{
		SetClassFieldBool(Class, "bCorpse", Value);
		return true;
	}
	if (!Flag.ICmp("CountKill"))
	{
		SetClassFieldBool(Class, "bCountKill", Value);
		return true;
	}
	if (!Flag.ICmp("CountItem"))
	{
		SetClassFieldBool(Class, "bCountItem", Value);
		return true;
	}
	if (!Flag.ICmp("NotDMatch"))
	{
		SetClassFieldBool(Class, "bNoDeathmatch", Value);
		return true;
	}
	if (!Flag.ICmp("NonShootable"))
	{
		SetClassFieldBool(Class, "bNonShootable", Value);
		return true;
	}
	if (!Flag.ICmp("DropOff"))
	{
		SetClassFieldBool(Class, "bDropOff", Value);
		return true;
	}
	if (!Flag.ICmp("PuffOnActors"))
	{
		SetClassFieldBool(Class, "bPuffOnActors", Value);
		return true;
	}
	if (!Flag.ICmp("AllowParticles"))
	{
		//FIXME
		GCon->Logf("Unsupported flag AllowParticles in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("AlwaysPuff"))
	{
		//FIXME
		GCon->Logf("Unsupported flag AlwaysPuff in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("Synchronized"))
	{
		//FIXME
		GCon->Logf("Unsupported flag Synchronized in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("Faster"))
	{
		SetClassFieldBool(Class, "bFaster", Value);
		return true;
	}
	if (!Flag.ICmp("AlwaysFast"))
	{
		//FIXME
		GCon->Logf("Unsupported flag AlwaysFast in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("NeverFast"))
	{
		//FIXME
		GCon->Logf("Unsupported flag NeverFast in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("FastMelee"))
	{
		SetClassFieldBool(Class, "bFastMelee", Value);
		return true;
	}
	if (!Flag.ICmp("OldRadiusDmg"))
	{
		//FIXME
		GCon->Logf("Unsupported flag OldRadiusDmg in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("UseSpecial"))
	{
		//FIXME
		GCon->Logf("Unsupported flag UseSpecial in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("BossDeath"))
	{
		//FIXME
		GCon->Logf("Unsupported flag BossDeath in %s", Class->GetName());
		return true;
	}
	//
	//	Limited use
	//
	if (!Flag.ICmp("SeesDaggers"))
	{
		//FIXME
		GCon->Logf("Unsupported flag SeesDaggers in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("InCombat"))
	{
		SetClassFieldBool(Class, "bInCombat", Value);
		return true;
	}
	if (!Flag.ICmp("NoClip"))
	{
		SetClassFieldBool(Class, "bColideWithThings", !Value);
		SetClassFieldBool(Class, "bColideWithWorld", !Value);
		return true;
	}
	if (!Flag.ICmp("NoSector"))
	{
		SetClassFieldBool(Class, "bNoSector", Value);
		return true;
	}
	if (!Flag.ICmp("IceCorpse"))
	{
		SetClassFieldBool(Class, "bIceCorpse", Value);
		return true;
	}
	if (!Flag.ICmp("JustHit"))
	{
		SetClassFieldBool(Class, "bJustHit", Value);
		return true;
	}
	if (!Flag.ICmp("JustAttacked"))
	{
		SetClassFieldBool(Class, "bJustAttacked", Value);
		return true;
	}
	if (!Flag.ICmp("Teleport"))
	{
		SetClassFieldBool(Class, "bTeleport", Value);
		return true;
	}
	if (!Flag.ICmp("ForceYBillboard"))
	{
		//FIXME
		GCon->Logf("Unsupported flag ForceYBillboard in %s", Class->GetName());
		return true;
	}
	if (!Flag.ICmp("ForceXYBillboard"))
	{
		//FIXME
		GCon->Logf("Unsupported flag ForceXYBillboard in %s", Class->GetName());
		return true;
	}

	//
	//	Inventory class flags.
	//
	if (Class->IsChildOf(InventoryClass))
	{
		if (!Flag.ICmp("Inventory.Quiet"))
		{
			//FIXME
			GCon->Logf("Unsupported flag Inventory.Quiet in %s", Class->GetName());
			return true;
		}
		if (!Flag.ICmp("Inventory.AutoActivate"))
		{
			SetClassFieldBool(Class, "bAutoActivate", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.Undroppable"))
		{
			SetClassFieldBool(Class, "bUndroppable", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.InvBar"))
		{
			SetClassFieldBool(Class, "bInvBar", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.HubPower"))
		{
			SetClassFieldBool(Class, "bHubPower", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.InterHubStrip"))
		{
			//FIXME
			GCon->Logf("Unsupported flag Inventory.InterHubStrip in %s", Class->GetName());
			return true;
		}
		if (!Flag.ICmp("Inventory.PickupFlash"))
		{
			SetClassFieldBool(Class, "bPickupFlash", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.AlwaysPickup"))
		{
			SetClassFieldBool(Class, "bAlwaysPickup", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.FancyPickupSound"))
		{
			SetClassFieldBool(Class, "bFullVolPickupSound", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.BigPowerup"))
		{
			SetClassFieldBool(Class, "bBigPowerup", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.KeepDepleted"))
		{
			SetClassFieldBool(Class, "bKeepDepleted", Value);
			return true;
		}
		if (!Flag.ICmp("Inventory.IgnoreSkill"))
		{
			SetClassFieldBool(Class, "bIgnoreSkill", Value);
			return true;
		}
	}

	//
	//	Weapon class flags.
	//
	if (Class->IsChildOf(WeaponClass))
	{
		if (!Flag.ICmp("Weapon.NoAutoFire"))
		{
			SetClassFieldBool(Class, "bNoAutoFire", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.ReadySndHalf"))
		{
			SetClassFieldBool(Class, "bReadySndHalf", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.DontBob"))
		{
			SetClassFieldBool(Class, "bDontBob", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.AxeBlood"))
		{
			//FIXME
			GCon->Logf("Unsupported flag Weapon.AxeBlood in %s", Class->GetName());
			return true;
		}
		if (!Flag.ICmp("Weapon.NoAlert"))
		{
			SetClassFieldBool(Class, "bNoAlert", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.Ammo_Optional"))
		{
			SetClassFieldBool(Class, "bAmmoOptional", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.Alt_Ammo_Optional"))
		{
			//FIXME
			GCon->Logf("Unsupported flag Weapon.Alt_Ammo_Optional in %s", Class->GetName());
			return true;
		}
		if (!Flag.ICmp("Weapon.Primary_Uses_Both"))
		{
			//FIXME
			GCon->Logf("Unsupported flag Weapon.Primary_Uses_Both in %s", Class->GetName());
			return true;
		}
		if (!Flag.ICmp("Weapon.Wimpy_Weapon"))
		{
			SetClassFieldBool(Class, "bWimpyWeapon", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.Powered_Up"))
		{
			SetClassFieldBool(Class, "bPoweredUp", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.Staff2_Kickback"))
		{
			SetClassFieldBool(Class, "bStaff2Kickback", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.Explosive"))
		{
			SetClassFieldBool(Class, "bBotProjectile", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.MeleeWeapon"))
		{
			SetClassFieldBool(Class, "bBotMelee", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.BFG"))
		{
			SetClassFieldBool(Class, "bBotBfg", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.CheatNotWeapon"))
		{
			SetClassFieldBool(Class, "bCheatNotWeapon", Value);
			return true;
		}
		if (!Flag.ICmp("Weapon.No_Auto_Switch"))
		{
			//FIXME
			GCon->Logf("Unsupported flag Weapon.No_Auto_Switch in %s", Class->GetName());
			return true;
		}
	}

	sc->Error(va("Unknown flag %s", *Flag));
	return false;
	unguard;
}

//==========================================================================
//
//	ParseStateString
//
//==========================================================================

static VStr ParseStateString(VScriptParser* sc)
{
	guard(ParseStateString);
	VStr		StateStr;

	sc->ExpectIdentifier();
	StateStr = sc->String;

	if (sc->Check("::"))
	{
		sc->ExpectIdentifier();
		StateStr += "::";
		StateStr += sc->String;
	}

	if (sc->Check("."))
	{
		sc->ExpectIdentifier();
		StateStr += ".";
		StateStr += sc->String;
	}

	return StateStr;
	unguard;
}

//==========================================================================
//
//	ParseStates
//
//==========================================================================

static bool ParseStates(VScriptParser* sc, VClass* Class,
	TArray<VState*>& States)
{
	guard(ParseStates);
	VState* PrevState = NULL;
	VState* LoopStart = NULL;
	int NewLabelsStart = Class->StateLabelDefs.Num();

	sc->Expect("{");
	//	Disable escape sequences in states.
	sc->SetEscape(false);
	while (!sc->Check("}"))
	{
		TLocation TmpLoc = sc->GetLoc();
		VStr TmpName = ParseStateString(sc);

		//	Goto command.
		if (!TmpName.ICmp("Goto"))
		{
			VName GotoLabel = *ParseStateString(sc);
			int GotoOffset = 0;
			if (sc->Check("+"))
			{
				sc->ExpectNumber();
				GotoOffset = sc->Number;
			}

			if (!PrevState && NewLabelsStart == Class->StateLabelDefs.Num())
			{
				sc->Error("Goto before first state");
			}
			if (PrevState)
			{
				PrevState->GotoLabel = GotoLabel;
				PrevState->GotoOffset = GotoOffset;
			}
			for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); i++)
			{
				Class->StateLabelDefs[i].GotoLabel = GotoLabel;
				Class->StateLabelDefs[i].GotoOffset = GotoOffset;
			}
			NewLabelsStart = Class->StateLabelDefs.Num();
			PrevState = NULL;
			continue;
		}

		//	Stop command.
		if (!TmpName.ICmp("Stop"))
		{
			if (!PrevState && NewLabelsStart == Class->StateLabelDefs.Num())
			{
				sc->Error("Stop before first state");
				continue;
			}
			if (PrevState)
			{
				PrevState->NextState = NULL;
			}
			for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); i++)
			{
				Class->StateLabelDefs[i].State = NULL;
			}
			NewLabelsStart = Class->StateLabelDefs.Num();
			PrevState = NULL;
			continue;
		}

		//	Wait command.
		if (!TmpName.ICmp("Wait") || !TmpName.ICmp("Fail"))
		{
			if (!PrevState)
			{
				sc->Error(va("%s before first state", *TmpName));
				continue;
			}
			PrevState->NextState = PrevState;
			PrevState = NULL;
			continue;
		}

		//	Loop command.
		if (!TmpName.ICmp("Loop"))
		{
			if (!PrevState)
			{
				sc->Error("Loop before first state");
				continue;
			}
			PrevState->NextState = LoopStart;
			PrevState = NULL;
			continue;
		}

		//	Check for label.
		if (sc->Check(":"))
		{
			VStateLabelDef& Lbl = Class->StateLabelDefs.Alloc();
			Lbl.Loc = TmpLoc;
			Lbl.Name = TmpName;
			continue;
		}

		VState* State = new VState(va("S_%d", States.Num()), Class, TmpLoc);
		States.Append(State);
		State->InClassIndex = States.Num() - 1;

		//	Sprite name
		if (TmpName.Length() != 4)
		{
			sc->Error("Invalid sprite name");
		}
		State->SpriteName = *TmpName.ToLower();

		//  Frame
		sc->ExpectString();
		char FChar = VStr::ToUpper(sc->String[0]);
		if (FChar < 'A' || FChar > ']')
		{
			sc->Error("Frames must be A-Z, [, \\ or ]");
		}
		State->Frame = FChar - 'A';
		VStr FramesString = sc->String;

		//  Tics
		bool Neg = sc->Check("-");
		sc->ExpectNumber();
		if (Neg)
		{
			State->Time = -sc->Number;
		}
		else
		{
			State->Time = float(sc->Number) / 35.0;
		}

		bool NeedsUnget = true;
		while (sc->GetString() && !sc->Crossed)
		{
			//	Check for bright parameter.
			if (!sc->String.ICmp("Bright"))
			{
				State->Frame |= FF_FULLBRIGHT;
				continue;
			}

			//	Check for offsets.
			if (!sc->String.ICmp("Offset"))
			{
				sc->Expect("(");
				Neg = sc->Check("-");
				sc->ExpectNumber();
				State->Misc1 = sc->Number * (Neg ? -1 : 1);
				sc->Expect(",");
				Neg = sc->Check("-");
				sc->ExpectNumber();
				State->Misc2 = sc->Number * (Neg ? -1 : 1);
				sc->Expect(")");
				continue;
			}

			//	Get function name and parse arguments.
			VStr FuncName = sc->String;
			VExpression* Args[VMethod::MAX_PARAMS + 1];
			int NumArgs = 0;
			if (sc->Check("("))
			{
				if (!sc->Check(")"))
				{
					do
					{
						Args[NumArgs] = ParseExpressionPriority13(sc);
						if (NumArgs == VMethod::MAX_PARAMS)
							ParseError(sc->GetLoc(), "Too many arguments");
						else
							NumArgs++;
					} while (sc->Check(","));
					sc->Expect(")");
				}
			}

			//	First look for method with decorate_ prefix.
			VMethod* Func = Class->FindMethod(va("decorate_%s", *FuncName));
			if (!Func)
			{
				Func = Class->FindMethod(*FuncName);
			}
			if (!Func)
			{
				GCon->Logf("Unknown state action %s in %s", *FuncName, Class->GetName());
			}
			else if (Func->ReturnType.Type != TYPE_Void)
			{
				GCon->Logf("State action %s in %s desn't return void", *FuncName, Class->GetName());
			}
			else if (Func->NumParams || NumArgs)
			{
				VExpression* Expr = new VInvocation(NULL, Func, NULL,
					false, false, sc->GetLoc(), NumArgs, Args);
				VExpressionStatement* Stmt = new VExpressionStatement(Expr);
				VMethod* M = new VMethod(NAME_None, Class, sc->GetLoc());
				M->Flags = FUNC_Final;
				M->ReturnType = TYPE_Void;
				M->Statement = Stmt;
				M->ParamsSize = 1;
				Class->AddMethod(M);
				State->Function = M;
			}
			else
			{
				State->Function = Func;
			}

			//	If state function is not assigned, it means something is wrong.
			// In that case we need to free argument expressions.
			if (!State->Function)
			{
				for (int i = 0; i < NumArgs; i++)
				{
					if (Args[i])
					{
						delete Args[i];
					}
				}
			}
			NeedsUnget = false;
			break;
		}
		if (NeedsUnget)
		{
			sc->UnGet();
		}

		//	Link previous state.
		if (PrevState)
		{
			PrevState->NextState = State;
		}

		//	Assign state to the labels.
		for (int i = NewLabelsStart; i < Class->StateLabelDefs.Num(); i++)
		{
			Class->StateLabelDefs[i].State = State;
			LoopStart = State;
		}
		NewLabelsStart = Class->StateLabelDefs.Num();
		PrevState = State;

		for (size_t i = 1; i < FramesString.Length(); i++)
		{
			char FChar = VStr::ToUpper(FramesString[i]);
			if (FChar < 'A' || FChar > ']')
			{
				sc->Error("Frames must be A-Z, [, \\ or ]");
			}

			//	Create a new state.
			VState* s2 = new VState(va("S_%d", States.Num()), Class,
				sc->GetLoc());
			States.Append(s2);
			s2->InClassIndex = States.Num() - 1;
			s2->SpriteName = State->SpriteName;
			s2->Frame = (State->Frame & FF_FULLBRIGHT) | (FChar - 'A');
			s2->Time = State->Time;
			s2->Misc1 = State->Misc1;
			s2->Misc2 = State->Misc2;
			s2->Function = State->Function;

			//	Link previous state.
			PrevState->NextState = s2;
			PrevState = s2;
		}
	}
	//	Re-enable escape sequences.
	sc->SetEscape(true);
	return true;
	unguard;
}

//==========================================================================
//
//	ParseActor
//
//==========================================================================

static void ParseActor(VScriptParser* sc, TArray<VClassFixup>& ClassFixups)
{
	guard(ParseActor);
	//	Parse actor name. In order to allow dots in actor names, this is done
	// in non-C mode, so we have to do a little bit more complex parsing.
	sc->ExpectString();
	VStr NameStr;
	VStr ParentStr;
	int ColonPos = sc->String.IndexOf(':');
	if (ColonPos >= 0)
	{
		//	There's a colon inside, so plit up the string.
		NameStr = VStr(sc->String, 0, ColonPos);
		ParentStr = VStr(sc->String, ColonPos + 1, sc->String.Length() -
			ColonPos - 1);
	}
	else
	{
		NameStr = sc->String;
	}

	if (VClass::FindClassNoCase(*sc->String))
	{
		sc->Error(va("Redeclared class %s", *sc->String));
	}

	if (ColonPos < 0)
	{
		//	There's no colon, check if next string starts with it.
		sc->ExpectString();
		if (sc->String[0] == ':')
		{
			ColonPos = 0;
			ParentStr = VStr(sc->String, 1, sc->String.Length() - 1);
		}
		else
		{
			sc->UnGet();
		}
	}

	//	If we got colon but no parent class name, then get it.
	if (ColonPos >= 0 && !ParentStr)
	{
		sc->ExpectString();
		ParentStr = sc->String;
	}

	VClass* ParentClass = ActorClass;
	if (ParentStr)
	{
		ParentClass = VClass::FindClassNoCase(*ParentStr);
		if (!ParentClass)
		{
			sc->Error(va("Parent class %s not found", *ParentStr));
		}
		if (!ParentClass->IsChildOf(ScriptedEntityClass))
		{
			sc->Error(va("Parent class %s is not an actor class", *ParentStr));
		}
	}

	VClass* Class = ParentClass->CreateDerivedClass(*NameStr, DecPkg,
		sc->GetLoc());
	DecPkg->ParsedClasses.Append(Class);

	VClass* ReplaceeClass = NULL;
	if (sc->Check("replaces"))
	{
		sc->ExpectString();
		ReplaceeClass = VClass::FindClassNoCase(*sc->String);
		if (!ReplaceeClass)
		{
			sc->Error(va("Replaced class %s not found", *sc->String));
		}
		if (!ReplaceeClass->IsChildOf(ScriptedEntityClass))
		{
			sc->Error(va("Replaced class %s is not an actor class", *sc->String));
		}
	}

	//	Time to switch to the C mode.
	sc->SetCMode(true);

	int GameFilter = 0;
	int DoomEdNum = -1;
	int SpawnNum = -1;
	TArray<VState*> States;

	if (sc->CheckNumber())
	{
		if (sc->Number < -1 || sc->Number > 32767)
		{
			sc->Error("DoomEdNum is out of range [-1, 32767]");
		}
		DoomEdNum = sc->Number;
	}

	sc->Expect("{");
	while (!sc->Check("}"))
	{
		if (sc->Check("+"))
		{
			if (!ParseFlag(sc, Class, true))
			{
				return;
			}
			continue;
		}
		if (sc->Check("-"))
		{
			if (!ParseFlag(sc, Class, false))
			{
				return;
			}
			continue;
		}

		//	Get full name of the property.
		sc->ExpectIdentifier();
		VStr Prop = sc->String;
		while (sc->Check("."))
		{
			sc->ExpectIdentifier();
			Prop += ".";
			Prop += sc->String;
		}

		//
		//	Map editing control
		//
		if (!Prop.ICmp("Game"))
		{
			if (sc->Check("Doom"))
			{
				GameFilter |= GAME_Doom;
			}
			else if (sc->Check("Heretic"))
			{
				GameFilter |= GAME_Heretic;
			}
			else if (sc->Check("Hexen"))
			{
				GameFilter |= GAME_Hexen;
			}
			else if (sc->Check("Strife"))
			{
				GameFilter |= GAME_Strife;
			}
			else if (sc->Check("Raven"))
			{
				GameFilter |= GAME_Raven;
			}
			else if (sc->Check("Any"))
			{
				GameFilter |= GAME_Any;
			}
			else if (GameFilter)
			{
				sc->Error("Unknown game filter");
			}
			continue;
		}
		if (!Prop.ICmp("SpawnID"))
		{
			sc->ExpectNumber();
			SpawnNum = sc->Number;
			continue;
		}
		if (!Prop.ICmp("ConversationID"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "ConversationID", sc->Number);
			continue;
		}
		if (!Prop.ICmp("Tag"))
		{
			sc->ExpectString();
			SetClassFieldStr(Class, "StrifeName", sc->String);
			continue;
		}
		//
		//	Behaviour
		//
		if (!Prop.ICmp("Health"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "Health", sc->Number);
			continue;
		}
		if (!Prop.ICmp("GibHealth"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "GibsHealth", sc->Number);
			continue;
		}
		if (!Prop.ICmp("WoundHealth"))
		{
			//FIXME
			sc->ExpectNumber();
			GCon->Logf("Property WoundHealth in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("ReactionTime"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "ReactionCount", sc->Number);
			continue;
		}
		if (!Prop.ICmp("PainChance"))
		{
			sc->ExpectNumber();
			SetClassFieldFloat(Class, "PainChance", float(sc->Number) / 256.0);
			continue;
		}
		if (!Prop.ICmp("DamageFactor"))
		{
			//FIXME
			sc->ExpectString();
			sc->Expect(",");
			sc->ExpectFloat();
			GCon->Logf("Property DamageFactor in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Damage"))
		{
			if (sc->Check("("))
			{
				VExpression* Expr = ParseExpression(sc);
				if (!Expr)
				{
					ParseError(sc->GetLoc(), "Damage expression expected");
				}
				else
				{
					VMethod* M = new VMethod("GetMissileDamage", Class, sc->GetLoc());
					M->ReturnTypeExpr = new VTypeExpr(TYPE_Int, sc->GetLoc());
					M->ReturnType = TYPE_Int;
					M->NumParams = 2;
					M->Params[0].Name = "Mask";
					M->Params[0].Loc = sc->GetLoc();
					M->Params[0].TypeExpr = new VTypeExpr(TYPE_Int, sc->GetLoc());
					M->Params[1].Name = "Add";
					M->Params[1].Loc = sc->GetLoc();
					M->Params[1].TypeExpr = new VTypeExpr(TYPE_Int, sc->GetLoc());
					M->Statement = new VReturn(Expr, sc->GetLoc());
					Class->AddMethod(M);
					M->Define();
				}
				sc->Expect(")");
			}
			else
			{
				sc->ExpectNumber();
				SetClassFieldFloat(Class, "MissileDamage", sc->Number);
			}
			continue;
		}
		if (!Prop.ICmp("PoisonDamage"))
		{
			//FIXME
			sc->ExpectNumber();
			GCon->Logf("Property PoisonDamage in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("RadiusDamageFactor"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "RDFactor", sc->Float);
			continue;
		}
		if (!Prop.ICmp("Speed"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Speed", sc->Float * 35.0);
			continue;
		}
		if (!Prop.ICmp("VSpeed"))
		{
			//FIXME
			sc->ExpectFloat();
			GCon->Logf("Property VSpeed in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("FastSpeed"))
		{
			//FIXME
			sc->ExpectFloat();
			GCon->Logf("Property FastSpeed in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("FloatSpeed"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "FloatSpeed", sc->Float * 35.0);
			continue;
		}
		//
		//	Collision and physics
		//
		if (!Prop.ICmp("Radius"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Radius", sc->Float);
			continue;
		}
		if (!Prop.ICmp("Height"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Height", sc->Float);
			continue;
		}
		if (!Prop.ICmp("DeathHeight"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "DeathHeight", sc->Float);
			continue;
		}
		if (!Prop.ICmp("BurnHeight"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "BurnHeight", sc->Float);
			continue;
		}
		if (!Prop.ICmp("CameraHeight"))
		{
			//FIXME
			sc->ExpectFloat();
			GCon->Logf("Property CameraHeight in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Gravity"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Gravity", sc->Float);
			continue;
		}
		if (!Prop.ICmp("Mass"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Mass", sc->Float);
			continue;
		}
		if (!Prop.ICmp("MaxStepHeight"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "MaxStepHeight", sc->Float);
			continue;
		}
		if (!Prop.ICmp("MaxDropOffHeight"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "MaxDropoffHeight", sc->Float);
			continue;
		}
		if (!Prop.ICmp("BounceFactor"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "BounceFactor", sc->Float);
			continue;
		}
		if (!Prop.ICmp("BounceCount"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "BounceCount", sc->Number);
			continue;
		}
		//
		//	Sound
		//
		if (!Prop.ICmp("SeeSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "SightSound", *sc->String);
			continue;
		}
		if (!Prop.ICmp("AttackSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "AttackSound", *sc->String);
			continue;
		}
		if (!Prop.ICmp("PainSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "PainSound", *sc->String);
			continue;
		}
		if (!Prop.ICmp("DeathSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "DeathSound", *sc->String);
			continue;
		}
		if (!Prop.ICmp("ActiveSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "ActiveSound", *sc->String);
			continue;
		}
		if (!Prop.ICmp("HowlSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "HowlSound", *sc->String);
			continue;
		}
		//
		//	Rendering
		//
		if (!Prop.ICmp("RenderStyle"))
		{
			int RenderStyle = 0;
			if (sc->Check("None"))
			{
				RenderStyle = STYLE_None;
			}
			else if (sc->Check("Normal"))
			{
				RenderStyle = STYLE_Normal;
			}
			else if (sc->Check("Fuzzy"))
			{
				RenderStyle = STYLE_Fuzzy;
			}
			else if (sc->Check("SoulTrans"))
			{
				RenderStyle = STYLE_SoulTrans;
			}
			else if (sc->Check("OptFuzzy"))
			{
				RenderStyle = STYLE_OptFuzzy;
			}
			else if (sc->Check("Translucent"))
			{
				RenderStyle = STYLE_Translucent;
			}
			else if (sc->Check("Add"))
			{
				RenderStyle = STYLE_Add;
			}
			else if (sc->Check("Stencil"))
			{
				//FIXME
				GCon->Logf("Render style Stencil in %s is not yet supported", Class->GetName());
			}
			else
			{
				sc->Error("Bad render style");
			}
			SetClassFieldByte(Class, "RenderStyle", RenderStyle);
			continue;
		}
		if (!Prop.ICmp("Alpha"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Alpha", MID(0.0, sc->Float, 1.0));
			continue;
		}
		if (!Prop.ICmp("XScale"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "ScaleX", MID(0.0, sc->Float, 4.0));
			continue;
		}
		if (!Prop.ICmp("YScale"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "ScaleY", MID(0.0, sc->Float, 4.0));
			continue;
		}
		if (!Prop.ICmp("Scale"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "ScaleX", MID(0.0, sc->Float, 4.0));
			SetClassFieldFloat(Class, "ScaleY", MID(0.0, sc->Float, 4.0));
			continue;
		}
		if (!Prop.ICmp("Translation"))
		{
			SetClassFieldInt(Class, "Translation",
				R_ParseDecorateTranslation(sc));
			continue;
		}
		if (!Prop.ICmp("BloodColor"))
		{
			//FIXME
			if (sc->CheckNumber())
			{
				sc->ExpectNumber();
				sc->ExpectNumber();
			}
			else
			{
				sc->ExpectString();
			}
			GCon->Logf("Property BloodColor in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("BloodType"))
		{
			//FIXME
			sc->ExpectString();
			if (sc->Check(","))
			{
				sc->ExpectString();
				if (sc->Check(","))
				{
					sc->ExpectString();
				}
			}
			GCon->Logf("Property BloodType in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Decal"))
		{
			//FIXME
			sc->ExpectString();
			GCon->Logf("Property Decal in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("StencilColor"))
		{
			//FIXME
			if (sc->CheckNumber())
			{
				sc->ExpectNumber();
				sc->ExpectNumber();
			}
			else
			{
				sc->ExpectString();
			}
			GCon->Logf("Property StencilColor in %s is not yet supported", Class->GetName());
			continue;
		}
		//
		//	Obituaries
		//
		if (!Prop.ICmp("Obituary"))
		{
			//FIXME
			sc->ExpectString();
			GCon->Logf("Property Obituary in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("HitObituary"))
		{
			//FIXME
			sc->ExpectString();
			GCon->Logf("Property HitObituary in %s is not yet supported", Class->GetName());
			continue;
		}
		//
		//	Attacks
		//
		if (!Prop.ICmp("MinMissileChance"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "MissileChance", sc->Float);
			continue;
		}
		if (!Prop.ICmp("DamageType"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "DamageType", *sc->String);
			continue;
		}
		if (!Prop.ICmp("MeleeThreshold"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "MissileMinRange", sc->Float);
			continue;
		}
		if (!Prop.ICmp("MeleeRange"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "MeleeRange", sc->Float);
			continue;
		}
		if (!Prop.ICmp("MaxTargetRange"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "MissileMaxRange", sc->Float);
			continue;
		}
		if (!Prop.ICmp("MeleeDamage"))
		{
			//FIXME
			sc->ExpectNumber();
			GCon->Logf("Property MeleeDamage in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("MeleeSound"))
		{
			//FIXME
			sc->ExpectString();
			GCon->Logf("Property MeleeSound in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("MissileHeight"))
		{
			//FIXME
			sc->ExpectFloat();
			GCon->Logf("Property MissileHeight in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("MissileType"))
		{
			//FIXME
			sc->ExpectString();
			GCon->Logf("Property MissileType in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("ExplosionRadius"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "ExplosionRadius", sc->Number);
			continue;
		}
		if (!Prop.ICmp("ExplosionDamage"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "ExplosionDamage", sc->Number);
			continue;
		}
		if (!Prop.ICmp("DontHurtShooter"))
		{
			SetClassFieldBool(Class, "bExplosionDontHurtSelf", true);
			continue;
		}
		//
		//	Flag combos
		//
		if (!Prop.ICmp("Monster"))
		{
			SetClassFieldBool(Class, "bShootable", true);
			SetClassFieldBool(Class, "bCountKill", true);
			SetClassFieldBool(Class, "bSolid", true);
			SetClassFieldBool(Class, "bActivatePushWall", true);
			SetClassFieldBool(Class, "bActivateMCross", true);
			SetClassFieldBool(Class, "bPassMobj", true);
			SetClassFieldBool(Class, "bMonster", true);
			continue;
		}
		if (!Prop.ICmp("Projectile"))
		{
			SetClassFieldBool(Class, "bNoBlockmap", true);
			SetClassFieldBool(Class, "bNoGravity", true);
			SetClassFieldBool(Class, "bDropOff", true);
			SetClassFieldBool(Class, "bMissile", true);
			SetClassFieldBool(Class, "bActivateImpact", true);
			SetClassFieldBool(Class, "bActivatePCross", true);
			SetClassFieldBool(Class, "bNoTeleport", true);
			continue;
		}
		//
		//	Special
		//
		if (!Prop.ICmp("ClearFlags"))
		{
			//FIXME
			GCon->Logf("Property ClearFlags in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("DropItem"))
		{
			//FIXME
			sc->ExpectString();
			sc->CheckNumber();
			sc->CheckNumber();
			GCon->Logf("Property DropItem in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("States"))
		{
			if (!ParseStates(sc, Class, States))
			{
				return;
			}
			continue;
		}
		if (!Prop.ICmp("skip_super"))
		{
			//FIXME
			GCon->Logf("Property skip_super in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Spawn"))
		{
			//FIXME
			GCon->Logf("Property Spawn in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("See"))
		{
			//FIXME
			GCon->Logf("Property See in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Melee"))
		{
			//FIXME
			GCon->Logf("Property Melee in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Missile"))
		{
			//FIXME
			GCon->Logf("Property Missile in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Pain"))
		{
			//FIXME
			GCon->Logf("Property Pain in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Death"))
		{
			//FIXME
			GCon->Logf("Property Death in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("XDeath"))
		{
			//FIXME
			GCon->Logf("Property XDeath in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Burn"))
		{
			//FIXME
			GCon->Logf("Property Burn in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Ice"))
		{
			//FIXME
			GCon->Logf("Property Ice in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Disintegrate"))
		{
			//FIXME
			GCon->Logf("Property Disintegrate in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Raise"))
		{
			//FIXME
			GCon->Logf("Property Raise in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Crash"))
		{
			//FIXME
			GCon->Logf("Property Crash in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Wound"))
		{
			//FIXME
			GCon->Logf("Property Wound in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Crush"))
		{
			//FIXME
			GCon->Logf("Property Crush in %s is not yet supported", Class->GetName());
			continue;
		}
		if (!Prop.ICmp("Heal"))
		{
			//FIXME
			GCon->Logf("Property Heal in %s is not yet supported", Class->GetName());
			continue;
		}

		//
		//	Inventory class properties.
		//
		if (Class->IsChildOf(InventoryClass))
		{
			if (!Prop.ICmp("Inventory.Amount"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "Amount", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Inventory.DefMaxAmount"))
			{
				SetClassFieldInt(Class, "MaxAmount", -2);
				continue;
			}
			if (!Prop.ICmp("Inventory.MaxAmount"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "MaxAmount", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Inventory.Icon"))
			{
				sc->ExpectString();
				SetClassFieldName(Class, "IconName", *sc->String.ToLower());
				continue;
			}
			if (!Prop.ICmp("Inventory.PickupMessage"))
			{
				sc->ExpectString();
				SetClassFieldStr(Class, "PickupMessage", sc->String);
				continue;
			}
			if (!Prop.ICmp("Inventory.PickupSound"))
			{
				sc->ExpectString();
				SetClassFieldName(Class, "PickupSound", *sc->String);
				continue;
			}
			if (!Prop.ICmp("Inventory.PickupFlash"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Inventory.PickupFlash in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Inventory.UseSound"))
			{
				sc->ExpectString();
				SetClassFieldName(Class, "UseSound", *sc->String);
				continue;
			}
			if (!Prop.ICmp("Inventory.RespawnTics"))
			{
				sc->ExpectNumber();
				SetClassFieldFloat(Class, "RespawnTime", sc->Number / 35.0);
				continue;
			}
			if (!Prop.ICmp("Inventory.GiveQuest"))
			{
				sc->ExpectNumber();
				AddClassFixup(Class, "GiveQuestType", va("QuestItem%d",
					sc->Number), ClassFixups);
				continue;
			}
		}

		//
		//	Ammo class properties.
		//
		if (Class->IsChildOf(AmmoClass))
		{
			if (!Prop.ICmp("Ammo.BackpackAmount"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "BackpackAmount", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Ammo.BackpackMaxAmount"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "BackpackMaxAmount", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Ammo.DropAmount"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "DropAmount", sc->Number);
				continue;
			}
		}

		//
		//	Armor class properties.
		//
		if (Class->IsChildOf(BasicArmorPickupClass) || Class->IsChildOf(BasicArmorBonusClass))
		{
			if (!Prop.ICmp("Armor.SaveAmount"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "SaveAmount", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Armor.SavePercent"))
			{
				sc->ExpectFloat();
				SetClassFieldFloat(Class, "SavePercent", sc->Float);
				continue;
			}
		}
		if (Class->IsChildOf(BasicArmorBonusClass))
		{
			if (!Prop.ICmp("Armor.MaxSaveAmount"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "MaxSaveAmount", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Armor.MaxBonus"))
			{
				//FIXME
				sc->ExpectNumber();
				GCon->Logf("Property Armor.MaxBonus in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Armor.MaxBonusMax"))
			{
				//FIXME
				sc->ExpectNumber();
				GCon->Logf("Property Armor.MaxBonusMax in %s is not yet supported", Class->GetName());
				continue;
			}
		}

		//
		//	Health class properties.
		//
		if (Class->IsChildOf(HealthClass))
		{
			if (!Prop.ICmp("Health.LowMessage"))
			{
				sc->ExpectString();
				SetClassFieldStr(Class, "LowHealthMessage", sc->String);
				continue;
			}
		}

		//
		//	PowerupGiver class properties.
		//
		if (Class->IsChildOf(PowerupGiverClass))
		{
			if (!Prop.ICmp("Powerup.Color"))
			{
				if (sc->Check("InverseMap"))
				{
					SetClassFieldInt(Class, "BlendColour", 0x00123456);
				}
				else if (sc->Check("GoldMap"))
				{
					SetClassFieldInt(Class, "BlendColour", 0x00123457);
				}
				else
				{
					vuint32 Col;
					if (sc->CheckNumber())
					{
						int r = MID(0, sc->Number, 255);
						sc->Check(",");
						sc->ExpectNumber();
						int g = MID(0, sc->Number, 255);
						sc->Check(",");
						sc->ExpectNumber();
						int b = MID(0, sc->Number, 255);
						Col = (r << 16) | (g << 8) | b;
					}
					else
					{
						sc->ExpectString();
						Col = M_ParseColour(sc->String);
					}
					sc->Check(",");
					sc->ExpectFloat();
					int a = MID(0, (int)(sc->Float * 255), 255);
					Col |= a << 24;
					SetClassFieldInt(Class, "BlendColour", Col);
				}
				continue;
			}
			if (!Prop.ICmp("Powerup.Duration"))
			{
				sc->ExpectNumber();
				SetClassFieldFloat(Class, "EffectTime",
					(float)sc->Number / 35.0);
				continue;
			}
			if (!Prop.ICmp("Powerup.Type"))
			{
				sc->ExpectString();
				AddClassFixup(Class, "PowerupType", sc->String, ClassFixups);
				continue;
			}
			if (!Prop.ICmp("Powerup.Mode"))
			{
				sc->ExpectString();
				SetClassFieldName(Class, "Mode", *sc->String);
				continue;
			}
		}

		//
		//	PuzzleItem class properties.
		//
		if (Class->IsChildOf(PuzzleItemClass))
		{
			if (!Prop.ICmp("PuzzleItem.Number"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "PuzzleItemNumber", sc->Number);
				continue;
			}
			if (!Prop.ICmp("PuzzleItem.FailMessage"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property PuzzleItem.FailMessage in %s is not yet supported", Class->GetName());
				continue;
			}
		}

		//
		//	Weapon class properties.
		//
		if (Class->IsChildOf(WeaponClass))
		{
			if (!Prop.ICmp("Weapon.AmmoGive") || !Prop.ICmp("Weapon.AmmoGive1"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "AmmoGive1", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Weapon.AmmoGive2"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "AmmoGive2", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Weapon.AmmoType") || !Prop.ICmp("Weapon.AmmoType1"))
			{
				sc->ExpectString();
				AddClassFixup(Class, "AmmoType1", sc->String, ClassFixups);
				continue;
			}
			if (!Prop.ICmp("Weapon.AmmoType2"))
			{
				sc->ExpectString();
				AddClassFixup(Class, "AmmoType2", sc->String, ClassFixups);
				continue;
			}
			if (!Prop.ICmp("Weapon.AmmoUse") || !Prop.ICmp("Weapon.AmmoUse1"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "AmmoUse1", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Weapon.AmmoUse2"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "AmmoUse2", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Weapon.Kickback"))
			{
				sc->ExpectFloat();
				SetClassFieldFloat(Class, "Kickback", sc->Float);
				continue;
			}
			if (!Prop.ICmp("Weapon.ReadySound"))
			{
				sc->ExpectString();
				SetClassFieldName(Class, "ReadySound", *sc->String);
				continue;
			}
			if (!Prop.ICmp("Weapon.SelectionOrder"))
			{
				sc->ExpectNumber();
				SetClassFieldInt(Class, "SelectionOrder", sc->Number);
				continue;
			}
			if (!Prop.ICmp("Weapon.SisterWeapon"))
			{
				sc->ExpectString();
				AddClassFixup(Class, "SisterWeaponType", sc->String, ClassFixups);
				continue;
			}
			if (!Prop.ICmp("Weapon.UpSound"))
			{
				sc->ExpectString();
				SetClassFieldName(Class, "UpSound", *sc->String);
				continue;
			}
			if (!Prop.ICmp("Weapon.YAdjust"))
			{
				sc->ExpectFloat();
				SetClassFieldFloat(Class, "PSpriteSY", sc->Float);
				continue;
			}
		}

		//
		//	WeaponPiece class properties.
		//
/*		if (Class->IsChildOf(WeaponPieceClass))
		{
			if (!Prop.ICmp("WeaponPiece.Number"))
			{
				//FIXME
				sc->ExpectNumber();
				GCon->Logf("Property WeaponPiece.Number in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("WeaponPiece.Weapon"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property WeaponPiece.Weapon in %s is not yet supported", Class->GetName());
				continue;
			}
		}*/

		//
		//	PlayerPawn class properties.
		//
		if (Class->IsChildOf(PlayerPawnClass))
		{
			if (!Prop.ICmp("Player.AttackZOffset"))
			{
				//FIXME
				sc->ExpectFloat();
				GCon->Logf("Property Player.AttackZOffset in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.ColorRange"))
			{
				//FIXME
				sc->ExpectNumber();
				sc->ExpectNumber();
				GCon->Logf("Property Player.ColorRange in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.CrouchSprite"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.CrouchSprite in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.DamageScreenColor"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.DamageScreenColor in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.DisplayName"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.DisplayName in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.ForwardMove"))
			{
				//FIXME
				sc->ExpectFloat();
				sc->CheckFloat();
				GCon->Logf("Property Player.ForwardMove in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.HealRadiusType"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.HealRadiusType in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.HexenArmor"))
			{
				//FIXME
				sc->ExpectFloat();
				sc->Expect(",");
				sc->ExpectFloat();
				sc->Expect(",");
				sc->ExpectFloat();
				sc->Expect(",");
				sc->ExpectFloat();
				sc->Expect(",");
				sc->ExpectFloat();
				GCon->Logf("Property Player.HexenArmor in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.InvulnerabilityMode"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.InvulnerabilityMode in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.JumpZ"))
			{
				//FIXME
				sc->ExpectFloat();
				GCon->Logf("Property Player.JumpZ in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.MaxHealth"))
			{
				//FIXME
				sc->ExpectNumber();
				GCon->Logf("Property Player.MaxHealth in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.RunHealth"))
			{
				//FIXME
				sc->ExpectNumber();
				GCon->Logf("Property Player.RunHealth in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.ScoreIcon"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.ScoreIcon in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.SideMove"))
			{
				//FIXME
				sc->ExpectFloat();
				sc->CheckFloat();
				GCon->Logf("Property Player.SideMove in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.SoundClass"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.SoundClass in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.SpawnClass"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.SpawnClass in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.StartItem"))
			{
				//FIXME
				sc->ExpectString();
				sc->CheckNumber();
				GCon->Logf("Property Player.StartItem in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.ViewHeight"))
			{
				//FIXME
				sc->ExpectFloat();
				GCon->Logf("Property Player.ViewHeight in %s is not yet supported", Class->GetName());
				continue;
			}
			if (!Prop.ICmp("Player.MorphWeapon"))
			{
				//FIXME
				sc->ExpectString();
				GCon->Logf("Property Player.MorphWeapon in %s is not yet supported", Class->GetName());
				continue;
			}
		}

		sc->Error(va("Unknown property %s", *Prop));
	}

	sc->SetCMode(false);

	Class->EmitStateLabels();

	//	Set up linked list of states.
	if (States.Num())
	{
		Class->States = States[0];
		Class->NetStates = States[0];
		for (int i = 0; i < States.Num() - 1; i++)
		{
			States[i]->Next = States[i + 1];
			States[i]->NetNext = States[i + 1];
		}

		for (int i = 0; i < States.Num(); i++)
		{
			States[i]->SpriteIndex = VClass::FindSprite(States[i]->SpriteName);
			if (States[i]->GotoLabel != NAME_None)
			{
				States[i]->NextState = Class->ResolveStateLabel(
					States[i]->Loc, States[i]->GotoLabel, States[i]->GotoOffset);
			}
		}
	}

	if (DoomEdNum > 0)
	{
		mobjinfo_t& MI = VClass::GMobjInfos.Alloc();
		MI.Class = Class;
		MI.DoomEdNum = DoomEdNum;
		MI.GameFilter = GameFilter;
	}
	if (SpawnNum > 0)
	{
		mobjinfo_t& SI = VClass::GScriptIds.Alloc();
		SI.Class = Class;
		SI.DoomEdNum = SpawnNum;
		SI.GameFilter = GameFilter;
	}

	if (ReplaceeClass)
	{
		ReplaceeClass->Replacement = Class;
		Class->Replacee = ReplaceeClass;
	}
	unguard;
}

//==========================================================================
//
//	ParseOldDecStates
//
//==========================================================================

static void ParseOldDecStates(VScriptParser* sc, TArray<VState*>& States,
	VClass* Class)
{
	guard(ParseOldDecStates);
	TArray<VStr> Tokens;
	sc->String.Split(",\t\r\n", Tokens);
	for (int TokIdx = 0; TokIdx < Tokens.Num(); TokIdx++)
	{
		const char* pFrame = *Tokens[TokIdx];
		int DurColon = Tokens[TokIdx].IndexOf(':');
		float Duration = 4;
		if (DurColon >= 0)
		{
			Duration = atoi(pFrame);
			pFrame = *Tokens[TokIdx] + DurColon + 1;
		}

		bool GotState = false;
		while (*pFrame)
		{
			if (*pFrame == ' ')
			{
			}
			else if (*pFrame == '*')
			{
				if (!GotState)
				{
					sc->Error("* must come after a frame");
				}
				States[States.Num() - 1]->Frame |= FF_FULLBRIGHT;
			}
			else if (*pFrame < 'A' || *pFrame > ']')
			{
				sc->Error("Frames must be A-Z, [, \\, or ]");
			}
			else
			{
				GotState = true;
				VState* State = new VState(va("S_%d", States.Num()), Class,
					sc->GetLoc());
				States.Append(State);
				State->InClassIndex = States.Num() - 1;
				State->Frame = *pFrame - 'A';
				State->Time = Duration >= 0 ? float(Duration) / 35.0 : -1.0;
			}
			pFrame++;
		}
	}
	unguard;
}

//==========================================================================
//
//	ParseOldDecoration
//
//==========================================================================

static void ParseOldDecoration(VScriptParser* sc, int Type)
{
	guard(ParseOldDecoration);
	//	Get name of the class.
	sc->ExpectString();
	VName ClassName = *sc->String;

	//	Create class.
	VClass* Class = Type == OLDDEC_Pickup ?
		FakeInventoryClass->CreateDerivedClass(ClassName, DecPkg,
		sc->GetLoc()) :
		ActorClass->CreateDerivedClass(ClassName, DecPkg, sc->GetLoc());
	DecPkg->ParsedClasses.Append(Class);
	if (Type == OLDDEC_Breakable)
	{
		SetClassFieldBool(Class, "bShootable", true);
	}
	if (Type == OLDDEC_Projectile)
	{
		SetClassFieldBool(Class, "bMissile", true);
		SetClassFieldBool(Class, "bDropOff", true);
	}

	//	Parse game filters.
	int GameFilter = 0;
	while (!sc->Check("{"))
	{
		if (sc->Check("Doom"))
		{
			GameFilter |= GAME_Doom;
		}
		else if (sc->Check("Heretic"))
		{
			GameFilter |= GAME_Heretic;
		}
		else if (sc->Check("Hexen"))
		{
			GameFilter |= GAME_Hexen;
		}
		else if (sc->Check("Strife"))
		{
			GameFilter |= GAME_Strife;
		}
		else if (sc->Check("Raven"))
		{
			GameFilter |= GAME_Raven;
		}
		else if (sc->Check("Any"))
		{
			GameFilter |= GAME_Any;
		}
		else if (GameFilter)
		{
			sc->Error("Unknown game filter");
		}
		else
		{
			sc->Error("Unknown identifier");
		}
	}

	int DoomEdNum = -1;
	int SpawnNum = -1;
	VName Sprite("tnt1");
	VName DeathSprite(NAME_None);
	TArray<VState*> States;
	int SpawnStart = 0;
	int SpawnEnd = 0;
	int DeathStart = 0;
	int DeathEnd = 0;
	bool DiesAway = false;
	bool SolidOnDeath = false;
	float DeathHeight = 0.0;
	int BurnStart = 0;
	int BurnEnd = 0;
	bool BurnsAway = false;
	bool SolidOnBurn = false;
	float BurnHeight = 0.0;
	int IceStart = 0;
	int IceEnd = 0;
	bool GenericIceDeath = false;
	bool Explosive = false;

	while (!sc->Check("}"))
	{
		if (sc->Check("DoomEdNum"))
		{
			sc->ExpectNumber();
			if (sc->Number < -1 || sc->Number > 32767)
			{
				sc->Error("DoomEdNum is out of range [-1, 32767]");
			}
			DoomEdNum = sc->Number;
		}
		else if (sc->Check("SpawnNum"))
		{
			sc->ExpectNumber();
			if (sc->Number < 0 || sc->Number > 255)
			{
				sc->Error("SpawnNum is out of range [0, 255]");
			}
			SpawnNum = sc->Number;
		}

		//	Spawn state
		else if (sc->Check("Sprite"))
		{
			sc->ExpectString();
			if (sc->String.Length() != 4)
			{
				sc->Error("Sprite name must be 4 characters long");
			}
			Sprite = *sc->String.ToLower();
		}
		else if (sc->Check("Frames"))
		{
			sc->ExpectString();
			SpawnStart = States.Num();
			ParseOldDecStates(sc, States, Class);
			SpawnEnd = States.Num();
		}

		//	Death states
		else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) &&
			sc->Check("DeathSprite"))
		{
			sc->ExpectString();
			if (sc->String.Length() != 4)
			{
				sc->Error("Sprite name must be 4 characters long");
			}
			DeathSprite = *sc->String.ToLower();
		}
		else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) &&
			sc->Check("DeathFrames"))
		{
			sc->ExpectString();
			DeathStart = States.Num();
			ParseOldDecStates(sc, States, Class);
			DeathEnd = States.Num();
		}
		else if (Type == OLDDEC_Breakable && sc->Check("DiesAway"))
		{
			DiesAway = true;
		}
		else if (Type == OLDDEC_Breakable && sc->Check("BurnDeathFrames"))
		{
			sc->ExpectString();
			BurnStart = States.Num();
			ParseOldDecStates(sc, States, Class);
			BurnEnd = States.Num();
		}
		else if (Type == OLDDEC_Breakable && sc->Check("BurnsAway"))
		{
			BurnsAway = true;
		}
		else if (Type == OLDDEC_Breakable && sc->Check("IceDeathFrames"))
		{
			sc->ExpectString();
			IceStart = States.Num();
			ParseOldDecStates(sc, States, Class);

			//	Make a copy of the last state for A_FreezeDeathChunks
			VState* State = new VState(va("S_%d", States.Num()), Class,
				sc->GetLoc());
			States.Append(State);
			State->InClassIndex = States.Num() - 1;
			State->Frame = States[States.Num() - 2]->Frame;

			IceEnd = States.Num();
		}
		else if (Type == OLDDEC_Breakable && sc->Check("GenericIceDeath"))
		{
			GenericIceDeath = true;
		}

		//	Misc properties
		else if (sc->Check("Radius"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Radius", sc->Float);
		}
		else if (sc->Check("Height"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Height", sc->Float);
		}
		else if (sc->Check("Mass"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Mass", sc->Float);
		}
		else if (sc->Check("Scale"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "ScaleX", sc->Float);
			SetClassFieldFloat(Class, "ScaleY", sc->Float);
		}
		else if (sc->Check("Alpha"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Alpha", MID(0.0, sc->Float, 1.0));
		}
		else if (sc->Check("RenderStyle"))
		{
			int RenderStyle = 0;
			if (sc->Check("STYLE_None"))
			{
				RenderStyle = STYLE_None;
			}
			else if (sc->Check("STYLE_Normal"))
			{
				RenderStyle = STYLE_Normal;
			}
			else if (sc->Check("STYLE_Fuzzy"))
			{
				RenderStyle = STYLE_Fuzzy;
			}
			else if (sc->Check("STYLE_SoulTrans"))
			{
				RenderStyle = STYLE_SoulTrans;
			}
			else if (sc->Check("STYLE_OptFuzzy"))
			{
				RenderStyle = STYLE_OptFuzzy;
			}
			else if (sc->Check("STYLE_Translucent"))
			{
				RenderStyle = STYLE_Translucent;
			}
			else if (sc->Check("STYLE_Add"))
			{
				RenderStyle = STYLE_Add;
			}
			else
			{
				sc->Error("Bad render style");
			}
			SetClassFieldByte(Class, "RenderStyle", RenderStyle);
		}
		else if (sc->Check("Translation1"))
		{
			sc->ExpectNumber();
			if (sc->Number < 0 || sc->Number > 2)
			{
				sc->Error("Translation1 is out of range [0, 2]");
			}
			SetClassFieldInt(Class, "Translation", (TRANSL_Standard <<
				TRANSL_TYPE_SHIFT) + sc->Number);
		}
		else if (sc->Check("Translation2"))
		{
			sc->ExpectNumber();
			if (sc->Number < 0 || sc->Number > MAX_LEVEL_TRANSLATIONS)
			{
				sc->Error(va("Translation2 is out of range [0, %d]",
					MAX_LEVEL_TRANSLATIONS));
			}
			SetClassFieldInt(Class, "Translation", (TRANSL_Level <<
				TRANSL_TYPE_SHIFT) + sc->Number);
		}

		//	Breakable decoration properties.
		else if (Type == OLDDEC_Breakable && sc->Check("Health"))
		{
			sc->ExpectNumber();
			SetClassFieldInt(Class, "Health", sc->Number);
		}
		else if (Type == OLDDEC_Breakable && sc->Check("DeathHeight"))
		{
			sc->ExpectFloat();
			DeathHeight = sc->Float;
		}
		else if (Type == OLDDEC_Breakable && sc->Check("BurnHeight"))
		{
			sc->ExpectFloat();
			BurnHeight = sc->Float;
		}
		else if (Type == OLDDEC_Breakable && sc->Check("SolidOnDeath"))
		{
			SolidOnDeath = true;
		}
		else if (Type == OLDDEC_Breakable && sc->Check("SolidOnBurn"))
		{
			SolidOnBurn = true;
		}
		else if ((Type == OLDDEC_Breakable || Type == OLDDEC_Projectile) &&
			sc->Check("DeathSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "DeathSound", *sc->String);
		}
		else if (Type == OLDDEC_Breakable && sc->Check("BurnDeathSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "ActiveSound", *sc->String);
		}

		//	Projectile properties
		else if (Type == OLDDEC_Projectile && sc->Check("Speed"))
		{
			sc->ExpectFloat();
			SetClassFieldFloat(Class, "Speed", sc->Float * 35.0);
		}
		else if (Type == OLDDEC_Projectile && sc->Check("Damage"))
		{
			sc->ExpectNumber();
			SetClassFieldFloat(Class, "MissileDamage", sc->Number);
		}
		else if (Type == OLDDEC_Projectile && sc->Check("DamageType"))
		{
			if (sc->Check("Normal"))
			{
				SetClassFieldName(Class, "DamageType", NAME_None);
			}
			else
			{
				sc->ExpectString();
				SetClassFieldName(Class, "DamageType", *sc->String);
			}
		}
		else if (Type == OLDDEC_Projectile && sc->Check("SpawnSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "SightSound", *sc->String);
		}
		else if (Type == OLDDEC_Projectile && sc->Check("ExplosionRadius"))
		{
			sc->ExpectNumber();
			SetClassFieldFloat(Class, "ExplosionRadius", sc->Number);
			Explosive = true;
		}
		else if (Type == OLDDEC_Projectile && sc->Check("ExplosionDamage"))
		{
			sc->ExpectNumber();
			SetClassFieldFloat(Class, "ExplosionDamage", sc->Number);
			Explosive = true;
		}
		else if (Type == OLDDEC_Projectile && sc->Check("DoNotHurtShooter"))
		{
			SetClassFieldBool(Class, "bExplosionDontHurtSelf", true);
		}
		else if (Type == OLDDEC_Projectile && sc->Check("DoomBounce"))
		{
			SetClassFieldByte(Class, "BounceType", BOUNCE_Doom);
		}
		else if (Type == OLDDEC_Projectile && sc->Check("HereticBounce"))
		{
			SetClassFieldByte(Class, "BounceType", BOUNCE_Heretic);
		}
		else if (Type == OLDDEC_Projectile && sc->Check("HexenBounce"))
		{
			SetClassFieldByte(Class, "BounceType", BOUNCE_Hexen);
		}

		//	Pickup properties
		else if (Type == OLDDEC_Pickup && sc->Check("PickupMessage"))
		{
			sc->ExpectString();
			SetClassFieldStr(Class, "PickupMessage", sc->String);
		}
		else if (Type == OLDDEC_Pickup && sc->Check("PickupSound"))
		{
			sc->ExpectString();
			SetClassFieldName(Class, "PickupSound", *sc->String);
		}
		else if (Type == OLDDEC_Pickup && sc->Check("Respawns"))
		{
			SetClassFieldBool(Class, "bRespawns", true);
		}

		//	Compatibility flags
		else if (sc->Check("LowGravity"))
		{
			SetClassFieldFloat(Class, "Gravity", 0.125);
		}
		else if (sc->Check("FireDamage"))
		{
			SetClassFieldName(Class, "DamageType", "Fire");
		}

		//	Flags
		else if (sc->Check("Solid"))
		{
			SetClassFieldBool(Class, "bSolid", true);
		}
		else if (sc->Check("NoSector"))
		{
			SetClassFieldBool(Class, "bNoSector", true);
		}
		else if (sc->Check("NoBlockmap"))
		{
			SetClassFieldBool(Class, "bNoBlockmap", true);
		}
		else if (sc->Check("SpawnCeiling"))
		{
			SetClassFieldBool(Class, "bSpawnCeiling", true);
		}
		else if (sc->Check("NoGravity"))
		{
			SetClassFieldBool(Class, "bNoGravity", true);
		}
		else if (sc->Check("Shadow"))
		{
			GCon->Logf("Shadow flag is not currently supported");
		}
		else if (sc->Check("NoBlood"))
		{
			SetClassFieldBool(Class, "bNoBlood", true);
		}
		else if (sc->Check("CountItem"))
		{
			SetClassFieldBool(Class, "bCountItem", true);
		}
		else if (sc->Check("WindThrust"))
		{
			SetClassFieldBool(Class, "bWindThrust", true);
		}
		else if (sc->Check("FloorClip"))
		{
			SetClassFieldBool(Class, "bFloorClip", true);
		}
		else if (sc->Check("SpawnFloat"))
		{
			SetClassFieldBool(Class, "bSpawnFloat", true);
		}
		else if (sc->Check("NoTeleport"))
		{
			SetClassFieldBool(Class, "bNoTeleport", true);
		}
		else if (sc->Check("Ripper"))
		{
			SetClassFieldBool(Class, "bRip", true);
		}
		else if (sc->Check("Pushable"))
		{
			SetClassFieldBool(Class, "bPushable", true);
		}
		else if (sc->Check("SlidesOnWalls"))
		{
			SetClassFieldBool(Class, "bSlide", true);
		}
		else if (sc->Check("CanPass"))
		{
			SetClassFieldBool(Class, "bPassMobj", true);
		}
		else if (sc->Check("CannotPush"))
		{
			SetClassFieldBool(Class, "bCannotPush", true);
		}
		else if (sc->Check("ThruGhost"))
		{
			SetClassFieldBool(Class, "bThruGhost", true);
		}
		else if (sc->Check("NoDamageThrust"))
		{
			SetClassFieldBool(Class, "bNoDamageThrust", true);
		}
		else if (sc->Check("Telestomp"))
		{
			SetClassFieldBool(Class, "bTelestomp", true);
		}
		else if (sc->Check("FloatBob"))
		{
			SetClassFieldBool(Class, "bFloatBob", true);
		}
		else if (sc->Check("ActivateImpact"))
		{
			SetClassFieldBool(Class, "bActivateImpact", true);
		}
		else if (sc->Check("CanPushWalls"))
		{
			SetClassFieldBool(Class, "bActivatePushWall", true);
		}
		else if (sc->Check("ActivateMCross"))
		{
			SetClassFieldBool(Class, "bActivateMCross", true);
		}
		else if (sc->Check("ActivatePCross"))
		{
			SetClassFieldBool(Class, "bActivatePCross", true);
		}
		else if (sc->Check("Reflective"))
		{
			SetClassFieldBool(Class, "bReflective", true);
		}
		else if (sc->Check("FloorHugger"))
		{
			SetClassFieldBool(Class, "bIgnoreFloorStep", true);
		}
		else if (sc->Check("CeilingHugger"))
		{
			SetClassFieldBool(Class, "bIgnoreCeilingStep", true);
		}
		else if (sc->Check("DontSplash"))
		{
			SetClassFieldBool(Class, "bNoSplash", true);
		}
		else
		{
			Sys_Error("Unknown property %s", *sc->String);
		}
	}

	if (SpawnEnd == 0)
	{
		sc->Error(va("%s has no Frames definition", *ClassName));
	}
	if (Type == OLDDEC_Breakable && DeathEnd == 0)
	{
		sc->Error(va("%s has no DeathFrames definition", *ClassName));
	}
	if (GenericIceDeath && IceEnd != 0)
	{
		sc->Error("IceDeathFrames and GenericIceDeath are mutually exclusive");
	}

	if (DoomEdNum > 0)
	{
		mobjinfo_t& MI = VClass::GMobjInfos.Alloc();
		MI.Class = Class;
		MI.DoomEdNum = DoomEdNum;
		MI.GameFilter = GameFilter;
	}
	if (SpawnNum > 0)
	{
		mobjinfo_t& SI = VClass::GScriptIds.Alloc();
		SI.Class = Class;
		SI.DoomEdNum = SpawnNum;
		SI.GameFilter = GameFilter;
	}

	//	Set up linked list of states.
	Class->States = States[0];
	Class->NetStates = States[0];
	for (int i = 0; i < States.Num() - 1; i++)
	{
		States[i]->Next = States[i + 1];
		States[i]->NetNext = States[i + 1];
	}

	//	Set up default sprite for all states.
	for (int i = 0; i < States.Num(); i++)
	{
		States[i]->SpriteName = Sprite;
		States[i]->SpriteIndex = VClass::FindSprite(Sprite);
	}
	//	Set death sprite if it's defined.
	if (DeathSprite != NAME_None && DeathEnd != 0)
	{
		for (int i = DeathStart; i < DeathEnd; i++)
		{
			States[i]->SpriteName = DeathSprite;
			States[i]->SpriteIndex = VClass::FindSprite(DeathSprite);
		}
	}

	//	Set up links of spawn states.
	if (SpawnEnd - SpawnStart == 1)
	{
		States[SpawnStart]->Time = -1.0;
	}
	else
	{
		for (int i = SpawnStart; i < SpawnEnd - 1; i++)
		{
			States[i]->NextState = States[i + 1];
		}
		States[SpawnEnd - 1]->NextState = States[SpawnStart];
	}
	Class->SetStateLabel("Spawn", States[SpawnStart]);

	//	Set up links of death states.
	if (DeathEnd != 0)
	{
		for (int i = DeathStart; i < DeathEnd - 1; i++)
		{
			States[i]->NextState = States[i + 1];
		}
		if (!DiesAway && Type != OLDDEC_Projectile)
		{
			States[DeathEnd - 1]->Time = -1.0;
		}
		if (Type == OLDDEC_Projectile)
		{
			if (Explosive)
			{
				States[DeathStart]->Function = FuncA_ExplodeParms;
			}
		}
		else
		{
			//	First death state plays death sound, second makes it
			// non-blocking unless it should stay solid.
			States[DeathStart]->Function = FuncA_Scream;
			if (!SolidOnDeath)
			{
				if (DeathEnd - DeathStart > 1)
				{
					States[DeathStart + 1]->Function = FuncA_NoBlocking;
				}
				else
				{
					States[DeathStart]->Function = FuncA_ScreamAndUnblock;
				}
			}

			if (!DeathHeight)
			{
				DeathHeight = GetClassFieldFloat(Class, "Height");
			}
			SetClassFieldFloat(Class, "DeathHeight", DeathHeight);
		}

		Class->SetStateLabel("Death", States[DeathStart]);
	}

	//	Set up links of burn death states.
	if (BurnEnd != 0)
	{
		for (int i = BurnStart; i < BurnEnd - 1; i++)
		{
			States[i]->NextState = States[i + 1];
		}
		if (!BurnsAway)
		{
			States[BurnEnd - 1]->Time = -1.0;
		}
		//	First death state plays active sound, second makes it
		// non-blocking unless it should stay solid.
		States[BurnStart]->Function = FuncA_ActiveSound;
		if (!SolidOnBurn)
		{
			if (BurnEnd - BurnStart > 1)
			{
				States[BurnStart + 1]->Function = FuncA_NoBlocking;
			}
			else
			{
				States[BurnStart]->Function = FuncA_ActiveAndUnblock;
			}
		}

		if (!BurnHeight)
		{
			BurnHeight = GetClassFieldFloat(Class, "Height");
		}
		SetClassFieldFloat(Class, "BurnHeight", BurnHeight);

		Class->SetStateLabel("Burn", States[BurnStart]);
	}

	//	Set up links of ice death states.
	if (IceEnd != 0)
	{
		for (int i = IceStart; i < IceEnd - 1; i++)
		{
			States[i]->NextState = States[i + 1];
		}

		States[IceEnd - 2]->Time = 5.0 / 35.0;
		//States[IceEnd - 2]->Function = FuncA_FreezeDeath;

		States[IceEnd - 1]->NextState = States[IceEnd - 1];
		States[IceEnd - 1]->Time = 1.0 / 35.0;
		//States[IceEnd - 2]->Function = FuncA_FreezeDeathChunks;

		Class->SetStateLabel("Ice", States[IceStart]);
	}
	else if (GenericIceDeath)
	{
		VStateLabel* Lbl = Class->FindStateLabel("GenericIceDeath");
		Class->SetStateLabel("Ice", Lbl ? Lbl->State : NULL);
	}
	unguard;
}

//==========================================================================
//
//	ParseDecorate
//
//==========================================================================

static void ParseDecorate(VScriptParser* sc, TArray<VClassFixup>& ClassFixups)
{
	guard(ParseDecorate);
	while (!sc->AtEnd())
	{
		if (sc->Check("#include"))
		{
			sc->ExpectString();
			int Lump = W_CheckNumForFileName(sc->String);
			//	Check WAD lump only if it's no longer than 8 characters and
			// has no path separator.
			if (Lump < 0 && sc->String.Length() <= 8 &&
				sc->String.IndexOf('/') < 0)
			{
				Lump = W_CheckNumForName(VName(*sc->String, VName::AddLower8));
			}
			if (Lump < 0)
			{
				sc->Error(va("Lump %s not found", *sc->String));
			}
			ParseDecorate(new VScriptParser(sc->String,
				W_CreateLumpReaderNum(Lump)), ClassFixups);
		}
		else if (sc->Check("const"))
		{
			ParseConst(sc);
		}
		else if (sc->Check("enum"))
		{
			ParseEnum(sc);
		}
		else if (sc->Check("class"))
		{
			ParseClass(sc);
		}
		else if (sc->Check("actor"))
		{
			ParseActor(sc, ClassFixups);
		}
		else if (sc->Check("breakable"))
		{
			ParseOldDecoration(sc, OLDDEC_Breakable);
		}
		else if (sc->Check("pickup"))
		{
			ParseOldDecoration(sc, OLDDEC_Pickup);
		}
		else if (sc->Check("projectile"))
		{
			ParseOldDecoration(sc, OLDDEC_Projectile);
		}
		else
		{
			ParseOldDecoration(sc, OLDDEC_Decoration);
		}
	}
	delete sc;
	unguard;
}

//==========================================================================
//
//	ProcessDecorateScripts
//
//==========================================================================

void ProcessDecorateScripts()
{
	guard(ProcessDecorateScripts);
	GCon->Logf(NAME_Init, "Processing DECORATE scripts");

	DecPkg = new VPackage(NAME_decorate);

	//	Find classes.
	ActorClass = VClass::FindClass("Actor");
	ScriptedEntityClass = VClass::FindClass("ScriptedEntity");
	FakeInventoryClass = VClass::FindClass("FakeInventory");
	InventoryClass = VClass::FindClass("Inventory");
	AmmoClass = VClass::FindClass("Ammo");
	BasicArmorPickupClass = VClass::FindClass("BasicArmorPickup");
	BasicArmorBonusClass = VClass::FindClass("BasicArmorBonus");
	HealthClass = VClass::FindClass("Health");
	PowerupGiverClass = VClass::FindClass("PowerupGiver");
	PuzzleItemClass = VClass::FindClass("PuzzleItem");
	WeaponClass = VClass::FindClass("Weapon");
//	WeaponPieceClass = VClass::FindClass("WeaponPiece");
	PlayerPawnClass = VClass::FindClass("PlayerPawn");

	//	Find methods used by old style decorations.
	FuncA_Scream = ActorClass->FindMethodChecked("A_Scream");
	FuncA_NoBlocking = ActorClass->FindMethodChecked("A_NoBlocking");
	FuncA_ScreamAndUnblock = ActorClass->FindMethodChecked("A_ScreamAndUnblock");
	FuncA_ActiveSound = ActorClass->FindMethodChecked("A_ActiveSound");
	FuncA_ActiveAndUnblock = ActorClass->FindMethodChecked("A_ActiveAndUnblock");
	FuncA_ExplodeParms = ActorClass->FindMethodChecked("A_ExplodeParms");

	//	Parse scripts.
	TArray<VClassFixup> ClassFixups;
	for (int Lump = W_IterateNS(-1, WADNS_Global); Lump >= 0;
		Lump = W_IterateNS(Lump, WADNS_Global))
	{
		if (W_LumpName(Lump) == NAME_decorate)
		{
			ParseDecorate(new VScriptParser(*W_LumpName(Lump),
				W_CreateLumpReaderNum(Lump)), ClassFixups);
		}
	}

	//	Set class properties.
	for (int i = 0; i < ClassFixups.Num(); i++)
	{
		VClassFixup& CF = ClassFixups[i];
		check(CF.ReqParent);
		if (!CF.Name.ICmp("None"))
		{
			*CF.Ptr = NULL;
		}
		else
		{
			VClass* C = VClass::FindClassNoCase(*CF.Name);
			if (!C)
			{
				GCon->Logf("No such class %s", *CF.Name);
			}
			else if (!C->IsChildOf(CF.ReqParent))
			{
				GCon->Logf("Class %s is not a descendant of %s",
					*CF.Name, CF.ReqParent->GetName());
			}
			else
			{
				*CF.Ptr = C;
			}
		}
	}

	//	Emit code.
	for (int i = 0; i < DecPkg->ParsedClasses.Num(); i++)
	{
		DecPkg->ParsedClasses[i]->DecorateEmit();
	}

	if (NumErrors)
	{
		BailOut();
	}

	VClass::StaticReinitStatesLookup();
	unguard;
}