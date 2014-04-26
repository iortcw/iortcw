/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <stdlib.h>
#include <stdio.h>

#include "../qcommon/q_shared.h"
#include "../client/snd_local.h"

/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init(void)
{
	return qtrue;
}

/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos(void)
{
   return 0;
}

/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown(void)
{
}

/*
===============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
}

/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting (void)
{
}
