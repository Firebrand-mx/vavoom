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
//**
//**	EVENT HANDLING
//**
//**	Events are asynchronous inputs generally generated by the game user.
//**	Events can be discarded if no responder claims them
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "gamedefs.h"

// MACROS ------------------------------------------------------------------

#define MAXEVENTS		64

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

boolean F_CheckPal(event_t *event);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int				shiftdown = 0;
int				ctrldown = 0;
int				altdown = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static event_t	events[MAXEVENTS];
static int		eventhead = 0;
static int		eventtail = 0;

static char*	KeyNames[SCANCODECOUNT] =
{
	"UP",
	"LEFT",
	"RIGHT",
	"DOWN",
    "INSERT",
    "DELETE",
    "HOME",
    "END",
    "PAGEUP",
    "PAGEDOWN",

	"PAD0",
	"PAD1",
	"PAD2",
	"PAD3",
	"PAD4",
	"PAD5",
	"PAD6",
	"PAD7",
	"PAD8",
	"PAD9",

	"NUMLOCK",
	"PADDIVIDE",
	"PADMULTIPLE",
	"PADMINUS",
	"PADPLUS",
	"PADENTER",
	"PADDOT",

	"ESCAPE",
	"ENTER",
	"TAB",
	"BACKSPACE",
	"CAPSLOCK",

	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",

	"LSHIFT",
	"RSHIFT",
	"LCTRL",
	"RCTRL",
	"LALT",
	"RALT",

	"LWIN",
	"RWIN",
	"MENU",

    "PRINTSCREEN",
	"SCROLLLOCK",
	"PAUSE",

	"MOUSE1",
	"MOUSE2",
	"MOUSE3",

	"MOUSED1",
	"MOUSED2",
	"MOUSED3",

	"JOY1",
	"JOY2",
	"JOY3",
	"JOY4"
	"JOY5",
	"JOY6",
	"JOY7",
	"JOY8"
	"JOY9",
	"JOY10",
	"JOY11",
	"JOY12"
	"JOY13",
	"JOY14",
	"JOY15",
	"JOY16"
};

static char			*keybindings_down[256];
static char			*keybindings_up[256];

// CODE --------------------------------------------------------------------

//==========================================================================
//
//  IN_PostEvent
//
// 	Called by the I/O functions when input is detected
//
//==========================================================================

void IN_PostEvent(event_t* ev)
{
    events[eventhead] = *ev;
    eventhead = (++eventhead) & (MAXEVENTS - 1);
}

//==========================================================================
//
//  IN_KeyEvent
//
// 	Called by the I/O functions when a key or button is pressed or released
//
//==========================================================================

void IN_KeyEvent(int key, int press)
{
	if (!key)
    {
    	return;
    }
    events[eventhead].type = press ? ev_keydown : ev_keyup;
    events[eventhead].data1 = key;
    events[eventhead].data2 = 0;
    events[eventhead].data3 = 0;
    eventhead = (++eventhead) & (MAXEVENTS - 1);
}

//==========================================================================
//
//  IN_ProcessEvents
//
// 	Send all the events of the given timestamp down the responder chain
//
//==========================================================================

void IN_ProcessEvents(void)
{
    event_t*	ev;
	char		*kb;

   	IN_ReadInput();

    for ( ; eventtail != eventhead ; eventtail = (++eventtail)&(MAXEVENTS-1))
    {
		ev = &events[eventtail];

		// Shift key state
	    if (ev->data1 == K_RSHIFT)
	    {
			shiftdown &= ~1;
			if (ev->type == ev_keydown) shiftdown |= 1;
	    }
	    if (ev->data1 == K_LSHIFT)
	    {
			shiftdown &= ~2;
			if (ev->type == ev_keydown) shiftdown |= 2;
	    }

		// Ctrl key state
		if (ev->data1 == K_RCTRL)
	    {
			ctrldown &= ~1;
			if (ev->type == ev_keydown) ctrldown |= 1;
	    }
		if (ev->data1 == K_LCTRL)
	    {
			ctrldown &= ~2;
			if (ev->type == ev_keydown) ctrldown |= 2;;
	    }

		// Alt key state
		if (ev->data1 == K_RALT)
	    {
			altdown &= ~1;
			if (ev->type == ev_keydown) altdown |= 1;
	    }
		if (ev->data1 == K_LALT)
	    {
			altdown &= ~2;
			if (ev->type == ev_keydown) altdown |= 2;;
	    }

		if (MB_Responder(ev))
			continue;

		if (C_Responder(ev))
			continue;

		if (CT_Responder(ev))
			continue;

		if (MN_Responder(ev))
			continue;

	    if (cls.state == ca_connected && !cl.intermission)
    	{
			if (SB_Responder(ev))
		    	continue;	// status window ate it
			if (AM_Responder(ev))
		    	continue;	// automap ate it
	    }

		if (F_Responder(ev))
			continue;

	    //
    	//	Key bindings
	    //
    	if (ev->type == ev_keydown)
	    {
			kb = keybindings_down[ev->data1];
	    	if (kb && *kb)
	        {
				if (kb[0] == '+' || kb[0] == '-')
				{
					// button commands add keynum as a parm
					CmdBuf << kb << " " << ev->data1 << "\n";
				}
				else
				{
		        	CmdBuf << kb << "\n";
				}
	            continue;
	        }
		}
	    if (ev->type == ev_keyup)
	    {
			kb = keybindings_up[ev->data1];
	    	if (kb && *kb)
			{
		    	if (kb[0] == '+' || kb[0] == '-')
				{
					// button commands add keynum as a parm
					CmdBuf << kb << " " << ev->data1 << "\n";
		        }
				else
				{
		        	CmdBuf << kb << "\n";
				}
            	continue;
			}
		}
		if (CL_Responder(ev))
			continue;
    }
}

//==========================================================================
//
//	IN_ReadKey
//
//==========================================================================

int IN_ReadKey(void)
{
	int		ret = 0;

	do
    {
    	IN_ReadInput();
	    while (eventtail != eventhead && !ret)
        {
			if (events[eventtail].type == ev_keydown)
            {
                ret = events[eventtail].data1;
            }
			eventtail = (++eventtail) & (MAXEVENTS - 1);
        }
    } while (!ret);

	return ret;
}

//==========================================================================
//
// 	CheckAbort
//
//==========================================================================
#if 0
void CheckAbort(void)
{
    event_t*	ev;
    int			stoptic;
	
    stoptic = I_GetTime () + 2; 
    while (I_GetTime() < stoptic) 
		IN_ReadInput();
	
    IN_ReadInput();
    for ( ; eventtail != eventhead 
	      ; eventtail = (++eventtail) & (MAXEVENTS - 1))
    { 
		ev = &events[eventtail];
		if (ev->type == ev_keydown && ev->data1 == K_ESCAPE)
	    	I_Error("Network game synchronization aborted.");
    }
}
#endif

//==========================================================================
//
//	KeyNumForName
//
//  Searches in key names for given name
// return key code
//
//==========================================================================

static int KeyNumForName(char* Name)
{
	int		i;

    if (!Name || !Name[0])
      return -1;

    if (!Name[1])
		return tolower(Name[0]);

    if (!stricmp(Name, "Space"))
		return ' ';

    if (!stricmp(Name, "Tilde"))
		return '`';

    for (i=0; i<SCANCODECOUNT; i++)
		if (!stricmp(Name, KeyNames[i]))
			return i + 0x80;

	return -1;
}

//==========================================================================
//
//	KeyNameForNum
//
//  Writes into given string key name
//
//==========================================================================

void KeyNameForNum(int KeyNr, char* NameString)
{
	if (KeyNr == ' ')
		sprintf(NameString, "SPACE");
	else if (KeyNr >= 0x80)
		sprintf(NameString, "%s", KeyNames[KeyNr - 0x80]);
	else if (KeyNr)
		sprintf(NameString, "%c", KeyNr);
	else
      	NameString[0] = 0;
}

//==========================================================================
//
//	IN_SetBinding
//
//==========================================================================

void IN_SetBinding(int keynum, const char *binding_down, const char *binding_up)
{
	char	*str;
	int		l;
			
	if (keynum == -1)
		return;

	// free old bindings
	if (keybindings_down[keynum])
	{
		Z_Free(keybindings_down[keynum]);
		keybindings_down[keynum] = NULL;
	}
	if (keybindings_up[keynum])
	{
		Z_Free(keybindings_up[keynum]);
		keybindings_up[keynum] = NULL;
	}
			
	// allocate memory for new binding
	l = strlen(binding_down);
	str = (char*)Z_Malloc(l + 1);
	strcpy(str, binding_down);
	str[l] = 0;
	keybindings_down[keynum] = str;

	l = strlen(binding_up);
	str = (char*)Z_Malloc(l + 1);
	strcpy(str, binding_up);
	str[l] = 0;
	keybindings_up[keynum] = str;
}

//==========================================================================
//
//	COMMAND Unbind
//
//==========================================================================

COMMAND(Unbind)
{
	int		b;

	if (Argc() != 2)
	{
		con << "unbind <key> : remove commands from a key\n";
		return;
	}
	
	b = KeyNumForName(Argv(1));
	if (b == -1)
	{
		con << "\"" << Argv(1) << "\" isn't a valid key\n";
		return;
	}

	IN_SetBinding(b, "", "");
}

//==========================================================================
//
//	COMMAND UnbindAll
//
//==========================================================================

COMMAND(UnbindAll)
{
	int		i;
	
	for (i = 0; i < 256; i++)
		if (keybindings_down[i])
			IN_SetBinding(i, "", "");
}

//==========================================================================
//
//	COMMAND Bind
//
//==========================================================================

COMMAND(Bind)
{
	int			c, b;
	
	c = Argc();

	if (c != 2 && c != 3 && c != 4)
	{
		con << "bind <key> [down_command] [up_command]: attach a command to a key\n";
		return;
	}
	b = KeyNumForName(Argv(1));
	if (b == -1)
	{
		con << "\"" << Argv(1) << "\" isn't a valid key\n";
		return;
	}

	if (c == 2)
	{
		if (keybindings_down[b] || keybindings_up[b])
			con << "\"" << Argv(1) << "\" = \"" << keybindings_down[b]
				<< "\" / \"" << keybindings_up[b] << "\"\n";
		else
			con << "\"" << Argv(1) << "\" is not bound\n";
		return;
	}
	if (c == 3 && Argv(2)[0] == '+')
	{
		char on_up[256];
		strcpy(on_up, Argv(2));
		on_up[0] = '-';
		IN_SetBinding(b, Argv(2), on_up);
	}
	else
	{
		IN_SetBinding(b, Argv(2), Argv(3));
	}
}

//==========================================================================
//
//	IN_WriteBindings
//
//	Writes lines containing "bind key value"
//
//==========================================================================

void IN_WriteBindings(FILE* f)
{
	int		i;
	char	name[32];

	fprintf(f, "UnbindAll\n");
	for (i = 0; i < 256; i++)
	{
		if (keybindings_down[i] && (*keybindings_down[i] || *keybindings_up[i]))
		{
			KeyNameForNum(i, name);
			fprintf(f, "bind \"%s\" \"%s\" \"%s\"\n", name, keybindings_down[i], keybindings_up[i]);
		}
	}
}

//==========================================================================
//
//	IN_GetBindingKeys
//
//==========================================================================

void IN_GetBindingKeys(const char *binding, int &key1, int &key2)
{
	int		i;

	key1 = -1;
	key2 = -1;
	for (i = 0; i < 256; i++)
	{
		if (keybindings_down[i] && !stricmp(binding, keybindings_down[i]))
		{
			if (key1 != -1)
			{
				key2 = i;
				return;
			}
			key1 = i;
		}
	}
}

//**************************************************************************
//
//	$Log$
//	Revision 1.4  2001/08/15 17:25:53  dj_jl
//	Removed F_CheckPal
//
//	Revision 1.3  2001/07/31 17:16:30  dj_jl
//	Just moved Log to the end of file
//	
//	Revision 1.2  2001/07/27 14:27:54  dj_jl
//	Update with Id-s and Log-s, some fixes
//
//**************************************************************************
