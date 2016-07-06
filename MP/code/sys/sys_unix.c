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

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "sys_local.h"

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pwd.h>
#include <libgen.h>
#include <fcntl.h>
#include <fenv.h>
#include <sys/wait.h>

qboolean stdinIsATTY;

// Used to determine where to store user-specific files
static char homePath[ MAX_OSPATH ] = { 0 };
static const char DEFAULT_XDG_DATA_HOME[] = {'.', 'l', 'o', 'c', 'a', 'l', PATH_SEP, 's', 'h', 'a', 'r', 'e', '\0'};

#ifndef STANDALONE
// Used to store the Steam RTCW installation path
static char steamPath[ MAX_OSPATH ] = { 0 };
#endif

/*
==================
Sys_DefaultHomePath
==================
*/
char *Sys_DefaultHomePath(void)
{
	char *p1;
	char *p2;

	if( !*homePath && com_homepath != NULL )
	{

#ifdef MACOS_X
		if( ( p1 = getenv( "HOME" ) ) != NULL )
		{
			Com_sprintf(homePath, sizeof(homePath), "%s%c", p1, PATH_SEP);

			Q_strcat(homePath, sizeof(homePath),
				"Library/Application Support/");

			if(com_homepath->string[0])
				Q_strcat(homePath, sizeof(homePath), com_homepath->string);
			else
				Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_MACOSX);
		}
#else
		if( ( p1 = getenv( "XDG_DATA_HOME" ) ) != NULL )
		{
			Com_sprintf(homePath, sizeof(homePath), "%s%c", p1, PATH_SEP);

		}
		else if( ( p2 = getenv( "HOME" ) ) != NULL)
		{
			Com_sprintf(homePath, sizeof(homePath), "%s%c%s%c", p2, PATH_SEP, DEFAULT_XDG_DATA_HOME, PATH_SEP);
		}

		if (p1 || p2)
		{
			if(com_homepath->string[0])
				Q_strcat(homePath, sizeof(homePath), com_homepath->string);
			else
				Q_strcat(homePath, sizeof(homePath), HOMEPATH_NAME_UNIX);
		}
#endif
	}

	return homePath;
}

#ifndef STANDALONE
/*
================
Sys_SteamPath
================
*/
char *Sys_SteamPath( void )
{
	// Disabled since Steam doesn't let you install RTCW on Mac/Linux
#if 0
	char *p;

	if( ( p = getenv( "HOME" ) ) != NULL )
	{
#ifdef MACOS_X
		char *steamPathEnd = "/Library/Application Support/Steam/SteamApps/common/" STEAMPATH_NAME;
#else
		char *steamPathEnd = "/.steam/steam/SteamApps/common/" STEAMPATH_NAME;
#endif
		Com_sprintf(steamPath, sizeof(steamPath), "%s%s", p, steamPathEnd);
	}
#endif

	return steamPath;
}
#endif

/*
================
Sys_Milliseconds
================
*/
/* base time in seconds, that's our origin
   timeval:tv_sec is an int:
   assuming this wraps every 0x7fffffff - ~68 years since the Epoch (1970) - we're safe till 2038 */
unsigned long sys_timeBase = 0;
/* current time in ms, using sys_timeBase as origin
   NOTE: sys_timeBase*1000 + curtime -> ms since the Epoch
     0x7fffffff ms - ~24 days
   although timeval:tv_usec is an int, I'm not sure wether it is actually used as an unsigned int
     (which would affect the wrap period) */
int curtime;
int Sys_Milliseconds (void)
{
	struct timeval tp;

	gettimeofday(&tp, NULL);

	if (!sys_timeBase)
	{
		sys_timeBase = tp.tv_sec;
		return tp.tv_usec/1000;
	}

	curtime = (tp.tv_sec - sys_timeBase)*1000 + tp.tv_usec/1000;

	return curtime;
}

/*
==================
Sys_RandomBytes
==================
*/
qboolean Sys_RandomBytes( byte *string, int len )
{
	FILE *fp;

	fp = fopen( "/dev/urandom", "r" );
	if( !fp )
		return qfalse;

	setvbuf( fp, NULL, _IONBF, 0 ); // don't buffer reads from /dev/urandom

	if( fread( string, sizeof( byte ), len, fp ) != len )
	{
		fclose( fp );
		return qfalse;
	}

	fclose( fp );
	return qtrue;
}

/*
==================
Sys_GetCurrentUser
==================
*/
char *Sys_GetCurrentUser( void )
{
	struct passwd *p;

	if ( (p = getpwuid( getuid() )) == NULL ) {
		return "player";
	}
	return p->pw_name;
}

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory

TODO
==================
*/
qboolean Sys_LowPhysicalMemory( void )
{
	return qfalse;
}

/*
==================
Sys_Basename
==================
*/
const char *Sys_Basename( char *path )
{
	return basename( path );
}

/*
==================
Sys_Dirname
==================
*/
const char *Sys_Dirname( char *path )
{
	return dirname( path );
}

/*
==============
Sys_FOpen
==============
*/
FILE *Sys_FOpen( const char *ospath, const char *mode ) {
	struct stat buf;

	// check if path exists and is a directory
	if ( !stat( ospath, &buf ) && S_ISDIR( buf.st_mode ) )
		return NULL;

	return fopen( ospath, mode );
}

/*
==================
Sys_Mkdir
==================
*/
qboolean Sys_Mkdir( const char *path )
{
	int result = mkdir( path, 0750 );

	if( result != 0 )
		return errno == EEXIST;

	return qtrue;
}

/*
==================
Sys_Mkfifo
==================
*/
FILE *Sys_Mkfifo( const char *ospath )
{
	FILE	*fifo;
	int	result;
	int	fn;
	struct	stat buf;

	// if file already exists AND is a pipefile, remove it
	if( !stat( ospath, &buf ) && S_ISFIFO( buf.st_mode ) )
		FS_Remove( ospath );

	result = mkfifo( ospath, 0600 );
	if( result != 0 )
		return NULL;

	fifo = fopen( ospath, "w+" );
	if( fifo )
	{
		fn = fileno( fifo );
		fcntl( fn, F_SETFL, O_NONBLOCK );
	}

	return fifo;
}

/*
==================
Sys_Cwd
==================
*/
char *Sys_Cwd( void )
{
	static char cwd[MAX_OSPATH];

	char *result = getcwd( cwd, sizeof( cwd ) - 1 );
	if( result != cwd )
		return NULL;

	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/*
==============================================================

DIRECTORY SCANNING

==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==================
Sys_ListFilteredFiles
==================
*/
void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles )
{
	char          search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char          filename[MAX_OSPATH];
	DIR           *fdir;
	struct dirent *d;
	struct stat   st;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if (strlen(subdirs)) {
		Com_sprintf( search, sizeof(search), "%s/%s", basedir, subdirs );
	}
	else {
		Com_sprintf( search, sizeof(search), "%s", basedir );
	}

	if ((fdir = opendir(search)) == NULL) {
		return;
	}

	while ((d = readdir(fdir)) != NULL) {
		Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
		if (stat(filename, &st) == -1)
			continue;

		if (st.st_mode & S_IFDIR) {
			if (Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, "..")) {
				if (strlen(subdirs)) {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
				}
				else {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof(filename), "%s/%s", subdirs, d->d_name );
		if (!Com_FilterPath( filter, filename, qfalse ))
			continue;
		list[ *numfiles ] = CopyString( filename );
		(*numfiles)++;
	}

	closedir(fdir);
}

/*
==================
Sys_ListFiles
==================
*/
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs )
{
	struct dirent *d;
	DIR           *fdir;
	qboolean      dironly = wantsubs;
	char          search[MAX_OSPATH];
	int           nfiles;
	char          **listCopy;
	char          *list[MAX_FOUND_FILES];
	int           i;
	struct stat   st;

	int           extLen;

	if (filter) {

		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = NULL;
		*numfiles = nfiles;

		if (!nfiles)
			return NULL;

		listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension)
		extension = "";

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = qtrue;
	}

	extLen = strlen( extension );

	// search
	nfiles = 0;

	if ((fdir = opendir(directory)) == NULL) {
		*numfiles = 0;
		return NULL;
	}

	while ((d = readdir(fdir)) != NULL) {
		Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);
		if (stat(search, &st) == -1)
			continue;
		if ((dironly && !(st.st_mode & S_IFDIR)) ||
			(!dironly && (st.st_mode & S_IFDIR)))
			continue;

		if (*extension) {
			if ( strlen( d->d_name ) < extLen ||
				Q_stricmp(
					d->d_name + strlen( d->d_name ) - extLen,
					extension ) ) {
				continue; // didn't match
			}
		}

		if ( nfiles == MAX_FOUND_FILES - 1 )
			break;
		list[ nfiles ] = CopyString( d->d_name );
		nfiles++;
	}

	list[ nfiles ] = NULL;

	closedir(fdir);

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

/*
==================
Sys_FreeFileList
==================
*/
void Sys_FreeFileList( char **list )
{
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}

/*
==================
Sys_Sleep

Block execution for msec or until input is recieved.
==================
*/
void Sys_Sleep( int msec )
{
	if( msec == 0 )
		return;

	if( stdinIsATTY )
	{
		fd_set fdset;

		FD_ZERO(&fdset);
		FD_SET(STDIN_FILENO, &fdset);
		if( msec < 0 )
		{
			select(STDIN_FILENO + 1, &fdset, NULL, NULL, NULL);
		}
		else
		{
			struct timeval timeout;

			timeout.tv_sec = msec/1000;
			timeout.tv_usec = (msec%1000)*1000;
			select(STDIN_FILENO + 1, &fdset, NULL, NULL, &timeout);
		}
	}
	else
	{
		// With nothing to select() on, we can't wait indefinitely
		if( msec < 0 )
			msec = 10;

		usleep( msec * 1000 );
	}
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog( const char *error )
{
	char buffer[ 1024 ];
	unsigned int size;
	int f = -1;
	const char *homepath = Cvar_VariableString( "fs_homepath" );
	const char *gamedir = Cvar_VariableString( "fs_game" );
	const char *fileName = "crashlog.txt";
	char *dirpath = FS_BuildOSPath( homepath, gamedir, "");
	char *ospath = FS_BuildOSPath( homepath, gamedir, fileName );

	Sys_Print( va( "%s\n", error ) );

#ifndef DEDICATED
	Sys_Dialog( DT_ERROR, va( "%s. See \"%s\" for details.", error, ospath ), "Error" );
#endif

	// Make sure the write path for the crashlog exists...

	if(!Sys_Mkdir(homepath))
	{
		Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", homepath);
		return;
	}

	if(!Sys_Mkdir(dirpath))
	{
		Com_Printf("ERROR: couldn't create path '%s' for crash log.\n", dirpath);
		return;
	}

	// We might be crashing because we maxed out the Quake MAX_FILE_HANDLES,
	// which will come through here, so we don't want to recurse forever by
	// calling FS_FOpenFileWrite()...use the Unix system APIs instead.
	f = open( ospath, O_CREAT | O_TRUNC | O_WRONLY, 0640 );
	if( f == -1 )
	{
		Com_Printf( "ERROR: couldn't open %s\n", fileName );
		return;
	}

	// We're crashing, so we don't care much if write() or close() fails.
	while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 ) {
		if( write( f, buffer, size ) != size ) {
			Com_Printf( "ERROR: couldn't fully write to %s\n", fileName );
			break;
		}
	}

	close( f );
}

#ifndef MACOS_X
static char execBuffer[ 1024 ];
static char *execBufferPointer;
static char *execArgv[ 16 ];
static int execArgc;

/*
==============
Sys_ClearExecBuffer
==============
*/
static void Sys_ClearExecBuffer( void )
{
	execBufferPointer = execBuffer;
	Com_Memset( execArgv, 0, sizeof( execArgv ) );
	execArgc = 0;
}

/*
==============
Sys_AppendToExecBuffer
==============
*/
static void Sys_AppendToExecBuffer( const char *text )
{
	size_t size = sizeof( execBuffer ) - ( execBufferPointer - execBuffer );
	int length = strlen( text ) + 1;

	if( length > size || execArgc >= ARRAY_LEN( execArgv ) )
		return;

	Q_strncpyz( execBufferPointer, text, size );
	execArgv[ execArgc++ ] = execBufferPointer;

	execBufferPointer += length;
}

/*
==============
Sys_Exec
==============
*/
static int Sys_Exec( void )
{
	pid_t pid = fork( );

	if( pid < 0 )
		return -1;

	if( pid )
	{
		// Parent
		int exitCode;

		wait( &exitCode );

		return WEXITSTATUS( exitCode );
	}
	else
	{
		// Child
		execvp( execArgv[ 0 ], execArgv );

		// Failed to execute
		exit( -1 );

		return -1;
	}
}

/*
==============
Sys_ZenityCommand
==============
*/
static void Sys_ZenityCommand( dialogType_t type, const char *message, const char *title )
{
	Sys_ClearExecBuffer( );
	Sys_AppendToExecBuffer( "zenity" );

	switch( type )
	{
		default:
		case DT_INFO:      Sys_AppendToExecBuffer( "--info" ); break;
		case DT_WARNING:   Sys_AppendToExecBuffer( "--warning" ); break;
		case DT_ERROR:     Sys_AppendToExecBuffer( "--error" ); break;
		case DT_YES_NO:
			Sys_AppendToExecBuffer( "--question" );
			Sys_AppendToExecBuffer( "--ok-label=Yes" );
			Sys_AppendToExecBuffer( "--cancel-label=No" );
			break;

		case DT_OK_CANCEL:
			Sys_AppendToExecBuffer( "--question" );
			Sys_AppendToExecBuffer( "--ok-label=OK" );
			Sys_AppendToExecBuffer( "--cancel-label=Cancel" );
			break;
	}

	Sys_AppendToExecBuffer( va( "--text=%s", message ) );
	Sys_AppendToExecBuffer( va( "--title=%s", title ) );
}

/*
==============
Sys_KdialogCommand
==============
*/
static void Sys_KdialogCommand( dialogType_t type, const char *message, const char *title )
{
	Sys_ClearExecBuffer( );
	Sys_AppendToExecBuffer( "kdialog" );

	switch( type )
	{
		default:
		case DT_INFO:      Sys_AppendToExecBuffer( "--msgbox" ); break;
		case DT_WARNING:   Sys_AppendToExecBuffer( "--sorry" ); break;
		case DT_ERROR:     Sys_AppendToExecBuffer( "--error" ); break;
		case DT_YES_NO:    Sys_AppendToExecBuffer( "--warningyesno" ); break;
		case DT_OK_CANCEL: Sys_AppendToExecBuffer( "--warningcontinuecancel" ); break;
	}

	Sys_AppendToExecBuffer( message );
	Sys_AppendToExecBuffer( va( "--title=%s", title ) );
}

/*
==============
Sys_XmessageCommand
==============
*/
static void Sys_XmessageCommand( dialogType_t type, const char *message, const char *title )
{
	Sys_ClearExecBuffer( );
	Sys_AppendToExecBuffer( "xmessage" );
	Sys_AppendToExecBuffer( "-buttons" );

	switch( type )
	{
		default:           Sys_AppendToExecBuffer( "OK:0" ); break;
		case DT_YES_NO:    Sys_AppendToExecBuffer( "Yes:0,No:1" ); break;
		case DT_OK_CANCEL: Sys_AppendToExecBuffer( "OK:0,Cancel:1" ); break;
	}

	Sys_AppendToExecBuffer( "-center" );
	Sys_AppendToExecBuffer( message );
}

/*
==============
Sys_Dialog

Display a *nix dialog box
==============
*/
dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title )
{
	typedef enum
	{
		NONE = 0,
		ZENITY,
		KDIALOG,
		XMESSAGE,
		NUM_DIALOG_PROGRAMS
	} dialogCommandType_t;
	typedef void (*dialogCommandBuilder_t)( dialogType_t, const char *, const char * );

	const char              *session = getenv( "DESKTOP_SESSION" );
	qboolean                tried[ NUM_DIALOG_PROGRAMS ] = { qfalse };
	dialogCommandBuilder_t  commands[ NUM_DIALOG_PROGRAMS ] = { NULL };
	dialogCommandType_t     preferredCommandType = NONE;
	int                     i;

	commands[ ZENITY ] = &Sys_ZenityCommand;
	commands[ KDIALOG ] = &Sys_KdialogCommand;
	commands[ XMESSAGE ] = &Sys_XmessageCommand;

	// This may not be the best way
	if( !Q_stricmp( session, "gnome" ) )
		preferredCommandType = ZENITY;
	else if( !Q_stricmp( session, "kde" ) )
		preferredCommandType = KDIALOG;

	for( i = NONE + 1; i < NUM_DIALOG_PROGRAMS; i++ )
	{
		if( preferredCommandType != NONE && preferredCommandType != i )
			continue;

		if( !tried[ i ] )
		{
			int exitCode;

			commands[ i ]( type, message, title );
			exitCode = Sys_Exec( );

			if( exitCode >= 0 )
			{
				switch( type )
				{
					case DT_YES_NO:    return exitCode ? DR_NO : DR_YES;
					case DT_OK_CANCEL: return exitCode ? DR_CANCEL : DR_OK;
					default:           return DR_OK;
				}
			}

			tried[ i ] = qtrue;

			// The preference failed, so start again in order
			if( preferredCommandType != NONE )
			{
				preferredCommandType = NONE;
				i = NONE + 1;
			}
		}
	}

	Com_DPrintf( S_COLOR_YELLOW "WARNING: failed to show a dialog\n" );
	return DR_OK;
}
#endif

/*
==============
Sys_GLimpSafeInit

Unix specific "safe" GL implementation initialisation
==============
*/
void Sys_GLimpSafeInit( void )
{
	// NOP
}

/*
==============
Sys_GLimpInit

Unix specific GL implementation initialisation
==============
*/
void Sys_GLimpInit( void )
{
	// NOP
}

void Sys_SetFloatEnv(void)
{
	// rounding toward nearest
	fesetround(FE_TONEAREST);
}

/*
==============
Sys_PlatformInit

Unix specific initialisation
==============
*/
void Sys_PlatformInit( void )
{
	const char* term = getenv( "TERM" );

	signal( SIGHUP, Sys_SigHandler );
	signal( SIGQUIT, Sys_SigHandler );
	signal( SIGTRAP, Sys_SigHandler );
	signal( SIGABRT, Sys_SigHandler );
	signal( SIGBUS, Sys_SigHandler );

	Sys_SetFloatEnv();

	stdinIsATTY = isatty( STDIN_FILENO ) &&
		!( term && ( !strcmp( term, "raw" ) || !strcmp( term, "dumb" ) ) );
}

/*
==============
Sys_PlatformExit

Unix specific deinitialisation
==============
*/
void Sys_PlatformExit( void )
{
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/

void Sys_SetEnv(const char *name, const char *value)
{
	if(value && *value)
		setenv(name, value, 1);
	else
		unsetenv(name);
}

/*
==============
Sys_PID
==============
*/
int Sys_PID( void )
{
	return getpid( );
}

/*
==============
Sys_PIDIsRunning
==============
*/
qboolean Sys_PIDIsRunning( int pid )
{
	return kill( pid, 0 ) == 0;
}



/*
==================
chmod OR on a file
==================
*/
void Sys_Chmod( char *file, int mode ) {
	struct stat s_buf;
	int perm;
	if ( stat( file, &s_buf ) != 0 ) {
		Com_Printf( "stat('%s')  failed: errno %d\n", file, errno );
		return;
	}
	perm = s_buf.st_mode | mode;
	if ( chmod( file, perm ) != 0 ) {
		Com_Printf( "chmod('%s', %d) failed: errno %d\n", file, perm, errno );
	}
	Com_DPrintf( "chmod +%d '%s'\n", mode, file );
}


#define MAX_CMD 1024
static char exit_cmdline[MAX_CMD] = "";
/*
==================
Sys_DoStartProcess
actually forks and starts a process

UGLY HACK:
  Sys_StartProcess works with a command line only
  if this command line is actually a binary with command line parameters,
  the only way to do this on unix is to use a system() call
  but system doesn't replace the current process, which leads to a situation like:
  wolf.x86--spawned_process.x86
  in the case of auto-update for instance, this will cause write access denied on wolf.x86:
  wolf-beta/2002-March/000079.html
  we hack the implementation here, if there are no space in the command line, assume it's a straight process and use execl
  otherwise, use system ..
  The clean solution would be Sys_StartProcess and Sys_StartProcess_Args..
==================
*/
void Sys_DoStartProcess( char *cmdline ) {
	switch ( fork() )
	{
	case - 1:
		// main thread
		break;
	case 0:
		if ( strchr( cmdline, ' ' ) )
		{
			int ret;

			ret = system( cmdline );
			if ( ret == -1 )
				printf( "Failed to run '%s' command\n", cmdline );

		}
		else
		{
			execl( cmdline, cmdline, NULL );
			printf( "execl failed: %s\n", strerror( errno ) );
		}
		_exit( 0 );
		break;
	}
}

/*
==================
Sys_StartProcess
if !doexit, start the process asap
otherwise, push it for execution at exit
(i.e. let complete shutdown of the game and freeing of resources happen)
NOTE: might even want to add a small delay?
==================
*/
void Sys_StartProcess( char *cmdline, qboolean doexit ) {

	if ( doexit ) {
		Com_DPrintf( "Sys_StartProcess %s (delaying to final exit)\n", cmdline );
		Q_strncpyz( exit_cmdline, cmdline, MAX_CMD );
		Cbuf_ExecuteText( EXEC_APPEND, "quit" );
	}

	Cbuf_ExecuteText( EXEC_NOW, "net_stop" );
	Com_DPrintf( "Sys_StartProcess %s\n", cmdline );
	Sys_DoStartProcess( cmdline );
}

/*
=================
Sys_OpenURL
=================
*/
void Sys_OpenURL( const char *url, qboolean doexit ) {
	char *basepath, *homepath, *pwdpath;
	char fname[20];
	char fn[MAX_OSPATH];
	char cmdline[MAX_CMD];

	Com_Printf( "Sys_OpenURL %s\n", url );
	// opening an URL on *nix can mean a lot of things ..
	// just spawn a script instead of deciding for the user :-)

	// do the setup before we fork
	// search for an openurl.sh script
	// search procedure taken from Sys_LoadDll
	Q_strncpyz( fname, "openurl.sh", 20 );

	pwdpath = Sys_Cwd();
	Com_sprintf( fn, MAX_OSPATH, "%s/%s", pwdpath, fname );
	if ( access( fn, X_OK ) == -1 ) {
		Com_DPrintf( "%s not found\n", fn );
		// try in home path
		homepath = Cvar_VariableString( "fs_homepath" );
		Com_sprintf( fn, MAX_OSPATH, "%s/%s", homepath, fname );
		if ( access( fn, X_OK ) == -1 ) {
			Com_DPrintf( "%s not found\n", fn );
			// basepath, last resort
			basepath = Cvar_VariableString( "fs_basepath" );
			Com_sprintf( fn, MAX_OSPATH, "%s/%s", basepath, fname );
			if ( access( fn, X_OK ) == -1 ) {
				Com_DPrintf( "%s not found\n", fn );
				Com_Printf( "Can't find script '%s' to open requested URL (use +set developer 1 for more verbosity)\n", fname );
				// we won't quit
				return;
			}
		}
	}

	Com_DPrintf( "URL script: %s\n", fn );

	// build the command line
	Com_sprintf( cmdline, MAX_CMD, "%s '%s' &", fn, url );

	if ( doexit ) {
		Sys_StartProcess( cmdline, qtrue );
	} else {
		Sys_StartProcess( cmdline, qfalse );
	}

}

// TTimo - Wolf MP specific, adding .mp. to shared objects
char* Sys_GetDLLName( const char *name ) {
	return va("%s.mp." ARCH_STRING DLL_EXT, name);
}

int Sys_GetHighQualityCPU() {
	// TODO TTimo see win_shared.c IsP3 || IsAthlon
	return 1;
}
