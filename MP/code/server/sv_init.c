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

/*
 * name:		sv_init.c
 *
 * desc:
 *
*/

#include "server.h"

/*
===============
SV_SendConfigstring

Creates and sends the server command necessary to update the CS index for the
given client
===============
*/
static void SV_SendConfigstring(client_t *client, int index)
{
	int maxChunkSize = MAX_STRING_CHARS - 24;
	int len;

	len = strlen(sv.configstrings[index]);

	if( len >= maxChunkSize ) {
		int		sent = 0;
		int		remaining = len;
		char	*cmd;
		char	buf[MAX_STRING_CHARS];

		while (remaining > 0 ) {
			if ( sent == 0 ) {
				cmd = "bcs0";
			}
			else if( remaining < maxChunkSize ) {
				cmd = "bcs2";
			}
			else {
				cmd = "bcs1";
			}
			Q_strncpyz( buf, &sv.configstrings[index][sent],
				maxChunkSize );

			SV_SendServerCommand( client, "%s %i \"%s\"\n", cmd,
				index, buf );

			sent += (maxChunkSize - 1);
			remaining -= (maxChunkSize - 1);
		}
	} else {
		// standard cs, just send it
		SV_SendServerCommand( client, "cs %i \"%s\"\n", index,
			sv.configstrings[index] );
	}
}

/*
===============
SV_UpdateConfigstrings

Called when a client goes from CS_PRIMED to CS_ACTIVE.  Updates all
Configstring indexes that have changed while the client was in CS_PRIMED
===============
*/
void SV_UpdateConfigstrings(client_t *client)
{
	int index;

	for( index = 0; index < MAX_CONFIGSTRINGS; index++ ) {
		// if the CS hasn't changed since we went to CS_PRIMED, ignore
		if(!client->csUpdated[index])
			continue;

		// do not always send server info to all clients
		if ( index == CS_SERVERINFO && client->gentity &&
			(client->gentity->r.svFlags & SVF_NOSERVERINFO) ) {
			continue;
		}
		SV_SendConfigstring(client, index);
		client->csUpdated[index] = qfalse;
	}
}

/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring( int index, const char *val ) {
	int		i;
	client_t    *client;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i", index);
	}

	if ( !val ) {
		val = "";
	}

	// don't bother broadcasting an update if no change
	if ( !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

	// change the string in sv
	Z_Free( sv.configstrings[index] );
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME || sv.restarting ) {
//		SV_SendServerCommand( NULL, "cs %i \"%s\"\n", index, val );

		// send the data to all relevent clients
		for ( i = 0, client = svs.clients; i < sv_maxclients->integer ; i++, client++ ) {
			if ( client->state < CS_ACTIVE ) {
				if ( client->state == CS_PRIMED )
					client->csUpdated[ index ] = qtrue;
				continue;
			}
			// do not always send server info to all clients
			if ( index == CS_SERVERINFO && client->gentity && ( client->gentity->r.svFlags & SVF_NOSERVERINFO ) ) {
				continue;
			}

			// RF, don't send to bot/AI
			if ( sv_gametype->integer == GT_SINGLE_PLAYER && client->gentity && ( client->gentity->r.svFlags & SVF_CASTAI ) ) {
				continue;
			}

//			SV_SendServerCommand( client, "cs %i \"%s\"\n", index, val );

			SV_SendConfigstring(client, index);
		}
	}
}



/*
===============
SV_GetConfigstring

===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bad index %i", index );
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = 0;
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}


/*
===============
SV_SetUserinfo

===============
*/
void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error( ERR_DROP, "SV_SetUserinfo: bad index %i", index );
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof( svs.clients[index].name ) );
}



/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bad index %i", index );
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


/*
================
SV_CreateBaseline

Entity baselines are used to compress non-delta messages
to the clients -- only the fields that differ from the
baseline will be transmitted
================
*/
static void SV_CreateBaseline( void ) {
	sharedEntity_t *svent;
	int entnum;

	for ( entnum = 1; entnum < sv.num_entities ; entnum++ ) {
		svent = SV_GentityNum( entnum );
		if ( !svent->r.linked ) {
			continue;
		}
		svent->s.number = entnum;

		//
		// take current state as baseline
		//
		sv.svEntities[entnum].baseline = svent->s;
	}
}


/*
===============
SV_BoundMaxClients

===============
*/
static void SV_BoundMaxClients( int minimum ) {
	// get the current maxclients value
	Cvar_Get( "sv_maxclients", "20", 0 );         // NERVE - SMF - changed to 20 from 8

	sv_maxclients->modified = qfalse;

	if ( sv_maxclients->integer < minimum ) {
		Cvar_Set( "sv_maxclients", va( "%i", minimum ) );
	} else if ( sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_Set( "sv_maxclients", va( "%i", MAX_CLIENTS ) );
	}
}


/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
static void SV_Startup( void ) {
	if ( svs.initialized ) {
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized" );
	}
	SV_BoundMaxClients( 1 );

	// RF, avoid trying to allocate large chunk on a fragmented zone
	svs.clients = calloc( sizeof( client_t ) * sv_maxclients->integer, 1 );
	if ( !svs.clients ) {
		Com_Error( ERR_FATAL, "SV_Startup: unable to allocate svs.clients" );
	}
	//svs.clients = Z_Malloc (sizeof(client_t) * sv_maxclients->integer );

	if ( com_dedicated->integer ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES;
	} else {
		// we don't need nearly as many when playing locally
		svs.numSnapshotEntities = sv_maxclients->integer * 4 * MAX_SNAPSHOT_ENTITIES;
	}
	svs.initialized = qtrue;

	// Don't respect sv_killserver unless a server is actually running
	if ( sv_killserver->integer ) {
		Cvar_Set( "sv_killserver", "0" );
	}

	Cvar_Set( "sv_running", "1" );

	// Join the ipv6 multicast group now that a map is running so clients can scan for us on the local network.
	NET_JoinMulticast6();
}


/*
==================
SV_ChangeMaxClients
==================
*/
void SV_ChangeMaxClients( void ) {
	int oldMaxClients;
	int i;
	client_t    *oldClients;
	int count;

	// get the highest client number in use
	count = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if ( i > count ) {
				count = i;
			}
		}
	}
	count++;

	oldMaxClients = sv_maxclients->integer;
	// never go below the highest client number in use
	SV_BoundMaxClients( count );
	// if still the same
	if ( sv_maxclients->integer == oldMaxClients ) {
		return;
	}

	oldClients = Hunk_AllocateTempMemory( count * sizeof( client_t ) );
	// copy the clients to hunk memory
	for ( i = 0 ; i < count ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			oldClients[i] = svs.clients[i];
		} else {
			Com_Memset( &oldClients[i], 0, sizeof( client_t ) );
		}
	}

	// free old clients arrays
	//Z_Free( svs.clients );
	free( svs.clients );    // RF, avoid trying to allocate large chunk on a fragmented zone

	// allocate new clients
	// RF, avoid trying to allocate large chunk on a fragmented zone
	svs.clients = calloc( sizeof( client_t ) * sv_maxclients->integer, 1 );
	if ( !svs.clients ) {
		Com_Error( ERR_FATAL, "SV_Startup: unable to allocate svs.clients" );
	}
	//svs.clients = Z_Malloc ( sv_maxclients->integer * sizeof(client_t) );

	Com_Memset( svs.clients, 0, sv_maxclients->integer * sizeof( client_t ) );

	// copy the clients over
	for ( i = 0 ; i < count ; i++ ) {
		if ( oldClients[i].state >= CS_CONNECTED ) {
			svs.clients[i] = oldClients[i];
		}
	}

	// free the old clients on the hunk
	Hunk_FreeTempMemory( oldClients );

	// allocate new snapshot entities
	if ( com_dedicated->integer ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * MAX_SNAPSHOT_ENTITIES;
	} else {
		// we don't need nearly as many when playing locally
		svs.numSnapshotEntities = sv_maxclients->integer * 4 * MAX_SNAPSHOT_ENTITIES;
	}
}


/*
====================
SV_SetExpectedHunkUsage

  Sets com_expectedhunkusage, so the client knows how to draw the percentage bar
====================
*/
void SV_SetExpectedHunkUsage( char *mapname ) {
	int handle;
	char *memlistfile = "hunkusage.dat";
	char *buf;
	char *buftrav;
	char *token;
	int len;

	len = FS_FOpenFileByMode( memlistfile, &handle, FS_READ );
	if ( len >= 0 ) { // the file exists, so read it in, strip out the current entry for this map, and save it out, so we can append the new value

		buf = (char *)Z_Malloc( len + 1 );
		memset( buf, 0, len + 1 );

		FS_Read( (void *)buf, len, handle );
		FS_FCloseFile( handle );

		// now parse the file, filtering out the current map
		buftrav = buf;
		while ( ( token = COM_Parse( &buftrav ) ) && token[0] ) {
			if ( !Q_strcasecmp( token, mapname ) ) {
				// found a match
				token = COM_Parse( &buftrav );  // read the size
				if ( token && token[0] ) {
					// this is the usage
					Cvar_Set( "com_expectedhunkusage", token );
					Z_Free( buf );
					return;
				}
			}
		}

		Z_Free( buf );
	}
	// just set it to a negative number,so the cgame knows not to draw the percent bar
	Cvar_Set( "com_expectedhunkusage", "-1" );
}

/*
================
SV_ClearServer
================
*/
static void SV_ClearServer(void) {
	int i;

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		if ( sv.configstrings[i] ) {
			Z_Free( sv.configstrings[i] );
		}
	}
	Com_Memset (&sv, 0, sizeof(sv));
}

/*
================
SV_TouchFile
================
*/
static void SV_TouchFile( const char *filename ) { 
	fileHandle_t f;

	FS_FOpenFileRead_Filtered( filename, &f, qfalse, FS_EXCLUDE_DIR );
	if ( f ) {
		FS_FCloseFile( f );
	}
}


/*
================
SV_TouchCGameDLL
  touch the cgame DLL so that a pure client (with DLL sv_pure support) can load do the correct checks
================

void SV_TouchCGameDLL( void ) {
	fileHandle_t f;
	char *filename;

	// Only touch the legacy dll since we have qvm support
	filename = "cgame_mp_x86.dll";
	FS_FOpenFileRead_Filtered( filename, &f, qfalse, FS_EXCLUDE_DIR );
	if ( f ) {
		FS_FCloseFile( f );
	}
}
*/

/*
================
SV_SpawnServer

Change the server to a new map, taking all connected
clients along with it.
This is NOT called for map_restart
================
*/
void SV_SpawnServer( char *server, qboolean killBots ) {
	int i;
	int checksum;
	qboolean isBot;
	char systemInfo[16384];
	const char  *p;

	// shut down the existing game if it is running
	SV_ShutdownGameProgs();

	Com_Printf( "------ Server Initialization ------\n" );
	Com_Printf( "Server: %s\n",server );

	// if not running a dedicated server CL_MapLoading will connect the client to the server
	// also print some status stuff
	CL_MapLoading();

	// make sure all the client stuff is unloaded
	CL_ShutdownAll(qfalse);

	// clear the whole hunk because we're (re)loading the server
	Hunk_Clear();

	// clear collision map data
	CM_ClearMap();

	// init client structures and svs.numSnapshotEntities
	if ( !Cvar_VariableValue( "sv_running" ) ) {
		SV_Startup();
	} else {
		// check for maxclients change
		if ( sv_maxclients->modified ) {
			SV_ChangeMaxClients();
		}
	}

	// clear pak references
	FS_ClearPakReferences( 0 );

	// allocate the snapshot entities on the hunk
	svs.snapshotEntities = Hunk_Alloc( sizeof( entityState_t ) * svs.numSnapshotEntities, h_high );
	svs.nextSnapshotEntities = 0;

	// toggle the server bit so clients can detect that a
	// server has changed
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// set nextmap to the same map, but it may be overriden
	// by the game startup or another console command
	Cvar_Set( "nextmap", "map_restart 0" );
//	Cvar_Set( "nextmap", va("map %s", server) );

	for (i=0 ; i<sv_maxclients->integer ; i++) {
		// save when the server started for each client already connected
		if (svs.clients[i].state >= CS_CONNECTED) {
			svs.clients[i].oldServerTime = sv.time;
		}
	}

	// wipe the entire per-level structure
	SV_ClearServer();

	// MrE: main zone should be pretty much emtpy at this point
	// except for file system data and cached renderer data
	Z_LogHeap();

	// allocate empty config strings
	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		sv.configstrings[i] = CopyString( "" );
	}

	// Ridah
	// DHM - Nerve :: We want to use the completion bar in multiplayer as well
	if ( sv_gametype->integer == GT_SINGLE_PLAYER || sv_gametype->integer >= GT_WOLF ) {
		SV_SetExpectedHunkUsage( va( "maps/%s.bsp", server ) );
	} else {
		// just set it to a negative number,so the cgame knows not to draw the percent bar
		Cvar_Set( "com_expectedhunkusage", "-1" );
	}

	// make sure we are not paused
	Cvar_Set( "cl_paused", "0" );

	// get a new checksum feed and restart the file system
	sv.checksumFeed = ( ( (unsigned int)rand() << 16 ) ^ (unsigned int)rand() ) ^ Com_Milliseconds();

	FS_Restart( sv.checksumFeed );

	// Load map config if present
	Cbuf_ExecuteText(EXEC_NOW, va( "exec mapcfgs/%s.cfg\n", server ) );

	CM_LoadMap( va( "maps/%s.bsp", server ), qfalse, &checksum );

	// set serverinfo visible name
	Cvar_Set( "mapname", server );

	Cvar_Set( "sv_mapChecksum", va( "%i",checksum ) );

	// serverid should be different each time
	sv.serverId = com_frameTime;
	sv.restartedServerId = sv.serverId;
	sv.checksumFeedServerId = sv.serverId;
	Cvar_Set( "sv_serverid", va( "%i", sv.serverId ) );

	// clear physics interaction links
	SV_ClearWorld();

	// media configstring setting should be done during
	// the loading stage, so connected clients don't have
	// to load during actual gameplay
	sv.state = SS_LOADING;

	Cvar_Set( "sv_serverRestarting", "1" );

	// load and spawn all other entities
	SV_InitGameProgs();

	// don't allow a map_restart if game is modified
	sv_gametype->modified = qfalse;

	// run a few frames to allow everything to settle
	for (i = 0;i < 3; i++)
	{
		VM_Call (gvm, GAME_RUN_FRAME, sv.time);
		SV_BotFrame (sv.time);
		sv.time += 100;
		svs.time += 100;
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline();

	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		// send the new gamestate to all connected clients
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			char    *denied;

			if ( svs.clients[i].netchan.remoteAddress.type == NA_BOT ) {
				if ( killBots || Cvar_VariableValue( "g_gametype" ) == GT_SINGLE_PLAYER ) {
					SV_DropClient( &svs.clients[i], "" );
					continue;
				}
				isBot = qtrue;
			} else {
				isBot = qfalse;
			}

			// connect the client again
			denied = VM_ExplicitArgPtr( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );   // firstTime = qfalse
			if ( denied ) {
				// this generally shouldn't happen, because the client
				// was connected before the level change
				SV_DropClient( &svs.clients[i], denied );
			} else {
				if ( !isBot ) {
					// when we get the next packet from a connected client,
					// the new gamestate will be sent
					svs.clients[i].state = CS_CONNECTED;
				} else {
					client_t        *client;
					sharedEntity_t  *ent;

					client = &svs.clients[i];
					client->state = CS_ACTIVE;
					ent = SV_GentityNum( i );
					ent->s.number = i;
					client->gentity = ent;

					client->deltaMessage = -1;
					client->lastSnapshotTime = 0;	// generate a snapshot immediately

					VM_Call( gvm, GAME_CLIENT_BEGIN, i );
				}
			}
		}
	}

	// run another frame to allow things to look at all the players
	VM_Call (gvm, GAME_RUN_FRAME, sv.time);
	SV_BotFrame (sv.time);
	sv.time += 100;
	svs.time += 100;

	if ( sv_pure->integer ) {
		// the server sends these to the clients so they will only
		// load pk3s also loaded at the server
		p = FS_LoadedPakChecksums();
		Cvar_Set( "sv_paks", p );
		if ( strlen( p ) == 0 ) {
			Com_Printf( "WARNING: sv_pure set but no PK3 files loaded\n" );
		}
		p = FS_LoadedPakNames();
		Cvar_Set( "sv_pakNames", p );

		// we want the server to reference the mp_bin pk3 that the client is expected to load from
		// we need to touch the cgame and ui because they could be in
		// seperate pk3 files and the client will need to download the pk3
		// files with the latest cgame and ui to pass the pure check

		// Only touch the legacy dlls since we have qvm support
		SV_TouchFile( "cgame_mp_x86.dll" );
		SV_TouchFile( "ui_mp_x86.dll" );
	} else {
		Cvar_Set( "sv_paks", "" );
		Cvar_Set( "sv_pakNames", "" );
	}
	// the server sends these to the clients so they can figure
	// out which pk3s should be auto-downloaded
	// NOTE: we consider the referencedPaks as 'required for operation'
	p = FS_ReferencedPakChecksums();
	Cvar_Set( "sv_referencedPaks", p );
	p = FS_ReferencedPakNames();
	Cvar_Set( "sv_referencedPakNames", p );

	// save systeminfo and serverinfo strings
	Q_strncpyz( systemInfo, Cvar_InfoString_Big( CVAR_SYSTEMINFO ), sizeof( systemInfo ) );
	cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	SV_SetConfigstring( CS_SYSTEMINFO, systemInfo );

	SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
	cvar_modifiedFlags &= ~CVAR_SERVERINFO;

	// NERVE - SMF
	SV_SetConfigstring( CS_WOLFINFO, Cvar_InfoString( CVAR_WOLFINFO ) );
	cvar_modifiedFlags &= ~CVAR_WOLFINFO;

	// any media configstring setting now should issue a warning
	// and any configstring changes should be reliably transmitted
	// to all clients
	sv.state = SS_GAME;

	// send a heartbeat now so the master will get up to date info
	SV_Heartbeat_f();

	Hunk_SetMark();

#ifndef DEDICATED
	if ( com_dedicated->integer ) {
		// restart renderer in order to show console for dedicated servers
		// launched through the regular binary
		CL_StartHunkUsers( qtrue );
	}
#endif

	Cvar_Set( "sv_serverRestarting", "0" );

	Com_Printf( "-----------------------------------\n" );
}

// DHM - Nerve :: Update Server
#ifdef UPDATE_SERVER
/*
====================
SV_ParseVersionMapping

  Reads versionmap.cfg which sets up a mapping of client version to installer to download
====================
*/
void SV_ParseVersionMapping( void ) {
	int handle;
	char *filename = "versionmap.cfg";
	char *buf;
	char *buftrav;
	char *token;
	int len;

	len = FS_SV_FOpenFileRead( filename, &handle );
	if ( len >= 0 ) { // the file exists

		buf = (char *)Z_Malloc( len + 1 );
		memset( buf, 0, len + 1 );

		FS_Read( (void *)buf, len, handle );
		FS_FCloseFile( handle );

		// now parse the file, setting the version table info
		buftrav = buf;

		token = COM_Parse( &buftrav );
		if ( strcmp( token, "RTCW-VersionMap" ) ) {
			Z_Free( buf );
			Com_Error( ERR_FATAL, "invalid versionmap.cfg" );
			return;
		}

		Com_Printf( "\n------------Update Server-------------\n\nParsing version map..." );

		while ( ( token = COM_Parse( &buftrav ) ) && token[0] ) {
			// read the version number
			strcpy( versionMap[ numVersions ].version, token );

			// read the platform
			token = COM_Parse( &buftrav );
			if ( token && token[0] ) {
				strcpy( versionMap[ numVersions ].platform, token );
			} else {
				Z_Free( buf );
				Com_Error( ERR_FATAL, "error parsing versionmap.cfg, after %s", versionMap[ numVersions ].version );
				return;
			}

			// read the installer name
			token = COM_Parse( &buftrav );
			if ( token && token[0] ) {
				strcpy( versionMap[ numVersions ].installer, token );
			} else {
				Z_Free( buf );
				Com_Error( ERR_FATAL, "error parsing versionmap.cfg, after %s", versionMap[ numVersions ].platform );
				return;
			}

			numVersions++;
			if ( numVersions >= MAX_UPDATE_VERSIONS ) {
				Z_Free( buf );
				Com_Error( ERR_FATAL, "Exceeded maximum number of mappings(%d)", MAX_UPDATE_VERSIONS );
				return;
			}

		}

		Com_Printf( " found %d mapping%c\n--------------------------------------\n\n", numVersions, numVersions > 1 ? 's' : ' ' );

		Z_Free( buf );
	} else {
		Com_Error( ERR_FATAL, "Couldn't open versionmap.cfg" );
	}
}
#endif

/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_Init( void ) {
	int index;

	SV_AddOperatorCommands();

	// serverinfo vars
	Cvar_Get( "dmflags", "0", /*CVAR_SERVERINFO*/ 0 );
	Cvar_Get( "fraglimit", "0", /*CVAR_SERVERINFO*/ 0 );
	Cvar_Get( "timelimit", "0", CVAR_SERVERINFO );
	// DHM - Nerve :: default to GT_WOLF
	sv_gametype = Cvar_Get( "g_gametype", "5", CVAR_SERVERINFO | CVAR_LATCH );

	// Rafael gameskill
	sv_gameskill = Cvar_Get( "g_gameskill", "3", CVAR_SERVERINFO | CVAR_LATCH );
	// done

	Cvar_Get( "sv_keywords", "", CVAR_SERVERINFO );
	sv_mapname = Cvar_Get( "mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM );
	sv_privateClients = Cvar_Get( "sv_privateClients", "0", CVAR_SERVERINFO );
	sv_hostname = Cvar_Get( "sv_hostname", "WolfHost", CVAR_SERVERINFO | CVAR_ARCHIVE );
	sv_maxclients = Cvar_Get( "sv_maxclients", "20", CVAR_SERVERINFO | CVAR_LATCH );               // NERVE - SMF - changed to 20 from 8

	sv_minRate = Cvar_Get ("sv_minRate", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_maxRate = Cvar_Get( "sv_maxRate", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_dlRate = Cvar_Get("sv_dlRate", "100", CVAR_ARCHIVE | CVAR_SERVERINFO);
	sv_minPing = Cvar_Get( "sv_minPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_maxPing = Cvar_Get( "sv_maxPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_floodProtect = Cvar_Get( "sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO );
	sv_allowAnonymous = Cvar_Get( "sv_allowAnonymous", "0", CVAR_SERVERINFO );
	sv_friendlyFire = Cvar_Get( "g_friendlyFire", "1", CVAR_SERVERINFO | CVAR_ARCHIVE );           // NERVE - SMF
	sv_maxlives = Cvar_Get( "g_maxlives", "0", CVAR_ARCHIVE | CVAR_LATCH | CVAR_SERVERINFO );      // NERVE - SMF
	sv_tourney = Cvar_Get( "g_noTeamSwitching", "0", CVAR_ARCHIVE );                               // NERVE - SMF

	// systeminfo
	Cvar_Get( "sv_cheats", "1", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_serverid = Cvar_Get( "sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM );
	sv_pure = Cvar_Get( "sv_pure", "1", CVAR_SYSTEMINFO );
#ifdef USE_VOIP
	sv_voip = Cvar_Get("sv_voip", "1", CVAR_LATCH);
	Cvar_CheckRange(sv_voip, 0, 1, qtrue);
	sv_voipProtocol = Cvar_Get("sv_voipProtocol", sv_voip->integer ? "opus" : "", CVAR_SYSTEMINFO | CVAR_ROM );
#endif
	Cvar_Get( "sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM );
	Cvar_Get( "sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM );

#if defined ANTIWALLHACK
	awh_active = Cvar_Get("awh_active", "0", CVAR_SYSTEMINFO);
	awh_bbox_horz = Cvar_Get("awh_bbox_horz", "30", CVAR_SYSTEMINFO);
	awh_bbox_vert = Cvar_Get("awh_bbox_vert", "60", CVAR_SYSTEMINFO);

	AWH_Init();
#endif

	// server vars
	sv_rconPassword = Cvar_Get( "rconPassword", "", CVAR_TEMP );
	sv_privatePassword = Cvar_Get( "sv_privatePassword", "", CVAR_TEMP );
#ifndef UPDATE_SERVER
	sv_fps = Cvar_Get( "sv_fps", "20", CVAR_TEMP );
#else
	sv_fps = Cvar_Get( "sv_fps", "60", CVAR_TEMP ); // this allows faster downloads
#endif
	sv_timeout = Cvar_Get( "sv_timeout", "240", CVAR_TEMP );
	sv_zombietime = Cvar_Get( "sv_zombietime", "2", CVAR_TEMP );
	Cvar_Get( "nextmap", "", CVAR_TEMP );

	sv_allowDownload = Cvar_Get( "sv_allowDownload", "1", CVAR_ARCHIVE );
	Cvar_Get ("sv_dlURL", "", CVAR_SERVERINFO | CVAR_ARCHIVE);
	
	sv_master[0] = Cvar_Get("sv_master1", MASTER_SERVER_NAME, 0);
	sv_master[1] = Cvar_Get("sv_master2", "master.iortcw.org", 0);
	for(index = 2; index < MAX_MASTER_SERVERS; index++)
		sv_master[index] = Cvar_Get(va("sv_master%d", index + 1), "", CVAR_ARCHIVE);

	sv_reconnectlimit = Cvar_Get( "sv_reconnectlimit", "3", 0 );
	sv_showloss = Cvar_Get( "sv_showloss", "0", 0 );
	sv_padPackets = Cvar_Get( "sv_padPackets", "0", 0 );
	sv_killserver = Cvar_Get( "sv_killserver", "0", 0 );
	sv_mapChecksum = Cvar_Get( "sv_mapChecksum", "", CVAR_ROM );
	sv_lanForceRate = Cvar_Get( "sv_lanForceRate", "1", CVAR_ARCHIVE );

	sv_banFile = Cvar_Get("sv_banFile", "serverbans.dat", CVAR_ARCHIVE);

	sv_onlyVisibleClients = Cvar_Get( "sv_onlyVisibleClients", "0", 0 );       // DHM - Nerve

	sv_forceNameUniq = Cvar_Get( "sv_forceNameUniq", "0", CVAR_ARCHIVE );

	sv_showAverageBPS = Cvar_Get( "sv_showAverageBPS", "0", 0 );           // NERVE - SMF - net debugging

	// NERVE - SMF - create user set cvars
	Cvar_Get( "g_userTimeLimit", "0", 0 );
	Cvar_Get( "g_userAlliedRespawnTime", "0", 0 );
	Cvar_Get( "g_userAxisRespawnTime", "0", 0 );
	Cvar_Get( "g_maxlives", "0", 0 );
	Cvar_Get( "g_noTeamSwitching", "0", CVAR_ARCHIVE );
	Cvar_Get( "g_altStopwatchMode", "0", CVAR_ARCHIVE );
	Cvar_Get( "g_minGameClients", "8", CVAR_SERVERINFO );
	Cvar_Get( "g_complaintlimit", "3", CVAR_ARCHIVE );
	Cvar_Get( "gamestate", "-1", CVAR_WOLFINFO | CVAR_ROM );
	Cvar_Get( "g_currentRound", "0", CVAR_WOLFINFO );
	Cvar_Get( "g_nextTimeLimit", "0", CVAR_WOLFINFO );
	// -NERVE - SMF

	// TTimo - some UI additions
	// NOTE: sucks to have this hardcoded really, I suppose this should be in UI
	Cvar_Get( "g_axismaxlives", "0", 0 );
	Cvar_Get( "g_alliedmaxlives", "0", 0 );
	Cvar_Get( "g_fastres", "0", CVAR_ARCHIVE );
	Cvar_Get( "g_fastResMsec", "1000", CVAR_ARCHIVE );

	// ATVI Tracker Wolfenstein Misc #273
	Cvar_Get( "g_voteFlags", "255", CVAR_ARCHIVE | CVAR_SERVERINFO );

	// ATVI Tracker Wolfenstein Misc #263
	Cvar_Get( "g_antilag", "0", CVAR_ARCHIVE | CVAR_SERVERINFO );

	// TTimo - autodownload speed tweaks
#ifndef UPDATE_SERVER
	// the download netcode tops at 18/20 kb/s, no need to make you think you can go above
	sv_dl_maxRate = Cvar_Get( "sv_dl_maxRate", "42000", CVAR_ARCHIVE );
#else
	// the update server is on steroids, sv_fps 60 and no snapshotMsec limitation, it can go up to 30 kb/s
	sv_dl_maxRate = Cvar_Get( "sv_dl_maxRate", "60000", CVAR_ARCHIVE );
#endif

	// initialize bot cvars so they are listed and can be set before loading the botlib
	SV_BotInitCvars();

	// init the botlib here because we need the pre-compiler in the UI
	SV_BotInitBotLib();

	// DHM - Nerve
#ifdef UPDATE_SERVER
	SV_Startup();
	SV_ParseVersionMapping();

	// serverid should be different each time
	sv.serverId = com_frameTime + 100;
	sv.restartedServerId = sv.serverId; // I suppose the init here is just to be safe
	sv.checksumFeedServerId = sv.serverId;
	Cvar_Set( "sv_serverid", va( "%i", sv.serverId ) );
	Cvar_Set( "mapname", "Update" );

	// allocate empty config strings
	{
		int i;

		for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
			sv.configstrings[i] = CopyString( "" );
		}
	}
#endif

	// Load saved bans
	Cbuf_AddText("rehashbans\n");
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( char *message ) {
	int i, j;
	client_t    *cl;

	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) {
		for ( i = 0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++ ) {
			if ( cl->state >= CS_CONNECTED ) {
				// don't send a disconnect to a local client
				if ( cl->netchan.remoteAddress.type != NA_LOOPBACK ) {
					SV_SendServerCommand( cl, "print \"%s\n\"\n", message );
					SV_SendServerCommand( cl, "disconnect \"%s\"", message );
				}
				// force a snapshot to be sent
				cl->lastSnapshotTime = 0;
				SV_SendClientSnapshot( cl );
			}
		}
	}
}


/*
================
SV_Shutdown

Called when each game quits,
before Sys_Quit or Sys_Error
================
*/
void SV_Shutdown( char *finalmsg ) {
	if ( !com_sv_running || !com_sv_running->integer ) {
		return;
	}

	Com_Printf( "----- Server Shutdown (%s) -----\n", finalmsg );

	NET_LeaveMulticast6();

	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();

	// free current level
	SV_ClearServer();

	// free server static data
	if(svs.clients)
	{
		int index;
		
		for(index = 0; index < sv_maxclients->integer; index++)
			SV_FreeClient(&svs.clients[index]);
		
		free(svs.clients);
	}
	memset( &svs, 0, sizeof( svs ) );

	Cvar_Set( "sv_running", "0" );

	Com_Printf( "---------------------------\n" );

	// disconnect any local clients
	if( sv_killserver->integer != 2 )
		CL_Disconnect( qfalse );
}

