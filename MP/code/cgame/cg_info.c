/*
===========================================================================

Return to Castle Wolfenstein multiplayer GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).  

RTCW MP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW MP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW MP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW MP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW MP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

// cg_info.c -- display information while data is being loading

#include "cg_local.h"


/*
======================
CG_LoadingString

======================
*/
void CG_LoadingString( const char *s ) {
	Q_strncpyz( cg.infoScreenText, s, sizeof( cg.infoScreenText ) );

	if ( s && s[0] != 0 ) {
		CG_Printf( "%s", va( "LOADING... %s\n",s ) );   //----(SA)	added so you can see from the console what's going on

	}
	trap_UpdateScreen();
}

/*
===================
CG_LoadingItem
===================
*/
void CG_LoadingItem( int itemNum ) {
#if 0 //----(SA)	Max Kaufman request that we don't show any pacifier stuff for items
	gitem_t     *item;

	item = &bg_itemlist[itemNum];

	if ( item->giType == IT_KEY ) { // do not show keys at level startup //----(SA)
		return;
	}

	if ( item->icon && loadingItemIconCount < MAX_LOADING_ITEM_ICONS ) {
		loadingItemIcons[loadingItemIconCount++] = trap_R_RegisterShaderNoMip( item->icon );
	}

	CG_LoadingString( item->pickup_name );
#endif //----(SA)	end
}

/*
===================
CG_LoadingClient
===================
*/
void CG_LoadingClient( int clientNum ) {
	const char      *info;
	char            *skin;
	char personality[MAX_QPATH];
	char model[MAX_QPATH];
	char iconName[MAX_QPATH];

	if ( cgs.gametype == GT_SINGLE_PLAYER  && clientNum > 0 ) { // for now only show the player's icon in SP games
		return;
	}

	info = CG_ConfigString( CS_PLAYERS + clientNum );

	Q_strncpyz( model, Info_ValueForKey( info, "model" ), sizeof( model ) );
	skin = strrchr( model, '/' );
	if ( skin ) {
		*skin++ = '\0';
	} else {
		skin = "default";
	}

	Com_sprintf( iconName, MAX_QPATH, "models/players/%s/icon_%s.tga", model, skin );

// (SA) ignore player icons for the moment
	if ( !( cg_entities[clientNum].currentState.aiChar ) ) {
//		if ( loadingPlayerIconCount < MAX_LOADING_PLAYER_ICONS ) {
//			loadingPlayerIcons[loadingPlayerIconCount++] = trap_R_RegisterShaderNoMip( iconName );
//		}
	}

	Q_strncpyz( personality, Info_ValueForKey( info, "n" ), sizeof( personality ) );
	Q_CleanStr( personality );

	if ( cgs.gametype == GT_SINGLE_PLAYER ) {
		trap_S_RegisterSound( va( "sound/player/announce/%s.wav", personality ) );
	}

	CG_LoadingString( personality );
}

/*
====================
CG_DrawStats
====================
*/

typedef struct {
	char    *label;
	int YOfs;
	int labelX;
	int labelFlags;
	vec4_t  *labelColor;

	char    *format;
	int formatX;
	int formatFlags;
	vec4_t  *formatColor;

	int numVars;
} statsItem_t;

// this defines the layout of the mission stats
// NOTE: these must match the stats sent in AICast_ScriptAction_ChangeLevel()
static statsItem_t statsItems[] = {
	{ "Kills",       170, 40,        UI_SMALLFONT | UI_DROPSHADOW,     &colorWhite,        "%3i/%3i",       600,    UI_SMALLFONT | UI_DROPSHADOW | UI_RIGHT,        &colorWhite,    2 },
	{ " Nazis",      40, 40,     UI_EXSMALLFONT | UI_DROPSHADOW,   &colorWhite,        "%3i/%3i",       600,    UI_EXSMALLFONT | UI_DROPSHADOW | UI_RIGHT,      &colorWhite,    2 },
	{ " Monsters",   15, 40,     UI_EXSMALLFONT | UI_DROPSHADOW,   &colorWhite,        "%3i/%3i",       600,    UI_EXSMALLFONT | UI_DROPSHADOW | UI_RIGHT,      &colorWhite,    2 },
	{ "Time",        30, 40,     UI_SMALLFONT | UI_DROPSHADOW,     &colorWhite,        "%2ih %2im %2is",    600,    UI_SMALLFONT | UI_DROPSHADOW | UI_RIGHT,        &colorWhite,    3 },
	{ "Secrets", 30, 40,     UI_SMALLFONT | UI_DROPSHADOW,     &colorWhite,        "%i/%i",         600,    UI_SMALLFONT | UI_DROPSHADOW | UI_RIGHT,        &colorWhite,    2 },
	{ "Attempts",    30, 40,     UI_SMALLFONT | UI_DROPSHADOW,     &colorWhite,        "%i",            600,    UI_SMALLFONT | UI_DROPSHADOW | UI_RIGHT,        &colorWhite,    1 },

	{ NULL }
};


/*
==============
CG_DrawStats
==============
*/
void CG_DrawStats( char *stats ) {
	int i, y, v, j;
	#define MAX_STATS_VARS  64
	int vars[MAX_STATS_VARS];
	char *str, *token;
	char *formatStr = NULL; // TTimo: init
	int varIndex;
	char string[MAX_QPATH];

	UI_DrawProportionalString( 320, 120, "MISSION STATS",
							   UI_CENTER | UI_SMALLFONT | UI_DROPSHADOW, colorWhite );

	Q_strncpyz( string, stats, sizeof( string ) );
	str = string;
	// convert commas to spaces
	for ( i = 0; str[i]; i++ ) {
		if ( str[i] == ',' ) {
			str[i] = ' ';
		}
	}

	for ( i = 0, y = 0, v = 0; statsItems[i].label; i++ ) {
		y += statsItems[i].YOfs;

		UI_DrawProportionalString( statsItems[i].labelX, y, statsItems[i].label,
								   statsItems[i].labelFlags, *statsItems[i].labelColor );

		if ( statsItems[i].numVars ) {
			varIndex = v;
			for ( j = 0; j < statsItems[i].numVars; j++ ) {
				token = COM_Parse( &str );
				if ( !token[0] ) {
					CG_Error( "error parsing mission stats\n" );
					return;
				}

				vars[v++] = atoi( token );
			}

			// build the formatStr
			switch ( statsItems[i].numVars ) {
			case 1:
				formatStr = va( statsItems[i].format, vars[varIndex] );
				break;
			case 2:
				formatStr = va( statsItems[i].format, vars[varIndex], vars[varIndex + 1] );
				break;
			case 3:
				formatStr = va( statsItems[i].format, vars[varIndex], vars[varIndex + 1], vars[varIndex + 2] );
				break;
			case 4:
				formatStr = va( statsItems[i].format, vars[varIndex], vars[varIndex + 1], vars[varIndex + 2], vars[varIndex + 3] );
				break;
			}

			UI_DrawProportionalString( statsItems[i].formatX, y, formatStr,
									   statsItems[i].formatFlags, *statsItems[i].formatColor );
		}
	}
}

/*
====================
CG_DrawInformation

Draw all the status / pacifier stuff during level loading
====================
*/
void CG_DrawInformation( void ) {
	const char  *s;
	const char  *info;
	qhandle_t levelshot = 0; // TTimo: init
	static int callCount = 0;
	float percentDone;

	int expectedHunk;
	char hunkBuf[MAX_QPATH];

	if ( cg.snap ) {
		return;     // we are in the world, no need to draw information
	}

	if ( callCount ) {    // reject recursive calls
		return;
	}

	callCount++;

	info = CG_ConfigString( CS_SERVERINFO );

	trap_Cvar_VariableStringBuffer( "com_expectedhunkusage", hunkBuf, MAX_QPATH );
	expectedHunk = atoi( hunkBuf );


	s = Info_ValueForKey( info, "mapname" );
	levelshot = trap_R_RegisterShaderNoMip( va( "levelshots/%s.tga", s ) );
	if ( !levelshot ) {
		levelshot = trap_R_RegisterShaderNoMip( "levelshots/unknownmap.jpg" );
	}
	trap_R_SetColor( NULL );

	// Pillarboxes
	if ( cg_fixedAspect.integer ) {
		if ( cgs.glconfig.vidWidth * 480.0 > cgs.glconfig.vidHeight * 640.0 ) {
			vec4_t col = { 0, 0, 0, 1 };
			float pillar = 0.5 * ( ( cgs.glconfig.vidWidth - ( cgs.screenXScale * 640.0 ) ) / cgs.screenXScale );

			CG_SetScreenPlacement( PLACE_LEFT, PLACE_CENTER );
			CG_FillRect( 0, 0, pillar + 1, 480, col );
			CG_SetScreenPlacement( PLACE_RIGHT, PLACE_CENTER );
			CG_FillRect( 640 - pillar, 0, pillar + 1, 480, col );
			CG_SetScreenPlacement( PLACE_CENTER, PLACE_CENTER );
		}
	}

	CG_DrawPic( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, levelshot );

	// show the server motd
	CG_DrawMotd();

	// show the percent complete bar
	if ( expectedHunk >= 0 ) {
		vec2_t xy = { 200, 468 };
		vec2_t wh = { 240, 10 };

		percentDone = (float)( cg_hunkUsed.integer + cg_soundAdjust.integer ) / (float)( expectedHunk );
		if ( percentDone > 0.97 ) { // never actually show 100%, since we are not in the game yet
			percentDone = 0.97;
		}
		CG_HorizontalPercentBar( xy[0], xy[1], wh[0], wh[1], percentDone );

	}

	callCount--;
}
