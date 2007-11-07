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

enum EWinType
{
	WIN_Normal,
	WIN_Modal,
	WIN_Root
};

class VWindow : public VWidget
{
	DECLARE_CLASS(VWindow, VWidget, 0)

	friend class VRootWindow;

	// Quick class reference
	vuint8 WindowType;				// Contains window type

	// Booleans
	enum
	{
		WF_IsSensitive		= 0x0001,	// True if the window can take input
		WF_IsSelectable		= 0x0002,	// True if the window can have keyboard focus
		WF_TickEnabled		= 0x0004,	// True if () event should be called
		WF_IsInitialised	= 0x0008,	// True if the window has been initialised

		// Destructor information
		WF_BeingDestroyed	= 0x0010,	// True if this window is going bye-bye
	};
	vuint32 WindowFlags;

public:
	VWindow();
	virtual void Init(VWindow *InParent);
	virtual void CleanUp();
	void Destroy();

	// Ancestral routines
	VRootWindow *GetRootWindow();
	VModalWindow *GetModalWindow();
	VWindow *GetParent();

	// Sensitivity routines
	void SetSensitivity(bool NewSensitivity);
	void Enable() { SetSensitivity(true); }
	void Disable() { SetSensitivity(false); }
	bool IsSensitive(bool bRecurse = true)
	{
		if (bRecurse)
		{
			VWindow *pParent = this;
			while (pParent)
			{
				if (!(pParent->WindowFlags & WF_IsSensitive))
					break;
				pParent = static_cast<VWindow*>(pParent->ParentWidget);
			}
			return (pParent ? false : true);
		}
		else
			return !!(WindowFlags & WF_IsSensitive);
	}

	// Selectability routines
	void SetSelectability(bool NewSelectability);
	bool IsSelectable() { return !!(WindowFlags & WF_IsSelectable); }

	void InitWindow()
	{
		P_PASS_SELF;
		EV_RET_VOID(NAME_InitWindow);
	}
	void DestroyWindow()
	{
		P_PASS_SELF;
		EV_RET_VOID(NAME_DestroyWindow);
	}
	void WindowReady()
	{
		P_PASS_SELF;
		EV_RET_VOID(NAME_WindowReady);
	}

	virtual void SensitivityChanged(bool bNewSensitivity)
	{
		P_PASS_SELF;
		P_PASS_BOOL(bNewSensitivity);
		EV_RET_VOID(NAME_SensitivityChanged);
	}

	virtual void Tick(float) { }
	void eventTick(float DeltaTime)
	{
		P_PASS_SELF;
		P_PASS_FLOAT(DeltaTime);
		EV_RET_VOID(NAME_Tick);
	}

	static VWindow *CreateNewWindow(VClass *NewClass, VWindow *ParentWindow);

private:
	void TickTree(float DeltaTime);

public:
	DECLARE_FUNCTION(Destroy)
	DECLARE_FUNCTION(NewChild)
	DECLARE_FUNCTION(SetSensitivity)
	DECLARE_FUNCTION(Enable)
	DECLARE_FUNCTION(Disable)
	DECLARE_FUNCTION(IsSensitive)
	DECLARE_FUNCTION(SetSelectability)
	
	DECLARE_FUNCTION(GetRootWindow)
	DECLARE_FUNCTION(GetModalWindow)
	DECLARE_FUNCTION(GetParent)
};
