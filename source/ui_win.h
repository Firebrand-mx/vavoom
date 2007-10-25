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

struct VClipRectOld
{
	VClipRectOld()
	{
		OriginX = 0;
		OriginY = 0;
	}
	VClipRectOld(float NewClipX, float NewClipY)
	{
		OriginX = NewClipX;
		OriginY = NewClipY;
	}
	float OriginX;
	float OriginY;
};

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
		WF_IsVisible		= 0x0001,	// True if the window is visible
		WF_IsSensitive		= 0x0002,	// True if the window can take input
		WF_IsSelectable		= 0x0004,	// True if the window can have keyboard focus
		WF_TickEnabled		= 0x0008,	// True if () event should be called
		WF_IsInitialised	= 0x0010,	// True if the window has been initialised

		// Destructor information
		WF_BeingDestroyed	= 0x0020,	// True if this window is going bye-bye
	};
	vuint32 WindowFlags;

	// Clipping rectangle
	VClipRectOld ClipRectOld;			// Clipping rectangle; maintained at all times

public:
	VWindow();
	virtual void Init(VWindow *InParent);
	virtual void CleanUp();
	void Destroy();

	// Ancestral routines
	VRootWindow *GetRootWindow();
	VModalWindow *GetModalWindow();
	VWindow *GetParent();

	// Child routines
	VWindow *GetBottomChild(bool bVisibleOnly = true);
	VWindow *GetTopChild(bool bVisibleOnly = true);

	// Sibling routines
	VWindow *GetLowerSibling(bool bVisibleOnly = true);
	VWindow *GetHigherSibling(bool bVisibleOnly = true);

	// Visibility routines
	void SetVisibility(bool NewVisibility);
	void Show() { SetVisibility(true); }
	void Hide() { SetVisibility(false); }
	bool IsVisible(bool bRecurse = true)
	{
		if (bRecurse)
		{
			VWindow *pParent = this;
			while (pParent)
			{
				if (!(pParent->WindowFlags & WF_IsVisible))
					break;
				pParent = static_cast<VWindow*>(pParent->ParentWidget);
			}
			return (pParent ? false : true);
		}
		else
			return !!(WindowFlags & WF_IsVisible);
	}

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
	//bool IsTraversable(bool bCheckModal = true);
	//bool IsFocusWindow();

	//	Reconfiguration routines.
	void SetPos(int NewX, int NewY);
	void SetSize(int NewWidth, int NewHeight);
	void SetConfiguration(int NewX, int NewY, int NewWidth, int HewHeight);
	void SetWidth(int NewWidth);
	void SetHeight(int NewHeight);

	// Slayer of innocent children
	void DestroyAllChildren() { KillAllChildren(); }

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

	virtual void ConfigurationChanged()
	{
		P_PASS_SELF;
		EV_RET_VOID(NAME_ConfigurationChanged);
	}
	virtual void VisibilityChanged(bool NewVisibility)
	{
		P_PASS_SELF;
		P_PASS_BOOL(NewVisibility);
		EV_RET_VOID(NAME_VisibilityChanged);
	}
	virtual void SensitivityChanged(bool bNewSensitivity)
	{
		P_PASS_SELF;
		P_PASS_BOOL(bNewSensitivity);
		EV_RET_VOID(NAME_SensitivityChanged);
	}

	virtual void DrawWindow()
	{
		P_PASS_SELF;
		EV_RET_VOID(NAME_DrawWindow);
	}
	virtual void PostDrawWindow()
	{
		P_PASS_SELF;
		EV_RET_VOID(NAME_PostDrawWindow);
	}

	virtual void Tick(float) { }
	void eventTick(float DeltaTime)
	{
		P_PASS_SELF;
		P_PASS_FLOAT(DeltaTime);
		EV_RET_VOID(NAME_Tick);
	}

	static VWindow *CreateNewWindow(VClass *NewClass, VWindow *ParentWindow);

protected:
	void KillAllChildren();

private:
	void DrawTree();

	void ClipTree();

	void TickTree(float DeltaTime);

public:
	DECLARE_FUNCTION(Destroy)
	DECLARE_FUNCTION(NewChild)
	DECLARE_FUNCTION(SetVisibility)
	DECLARE_FUNCTION(Show)
	DECLARE_FUNCTION(Hide)
	DECLARE_FUNCTION(IsVisible)
	DECLARE_FUNCTION(SetSensitivity)
	DECLARE_FUNCTION(Enable)
	DECLARE_FUNCTION(Disable)
	DECLARE_FUNCTION(IsSensitive)
	DECLARE_FUNCTION(SetSelectability)
	
	DECLARE_FUNCTION(GetRootWindow)
	DECLARE_FUNCTION(GetModalWindow)
	DECLARE_FUNCTION(GetParent)

	DECLARE_FUNCTION(SetPos)
	DECLARE_FUNCTION(SetSize)
	DECLARE_FUNCTION(SetConfiguration)
	DECLARE_FUNCTION(SetWidth)
	DECLARE_FUNCTION(SetHeight)

	DECLARE_FUNCTION(GetBottomChild)
	DECLARE_FUNCTION(GetTopChild)
	DECLARE_FUNCTION(GetLowerSibling)
	DECLARE_FUNCTION(GetHigherSibling)

	DECLARE_FUNCTION(DestroyAllChildren)
};
