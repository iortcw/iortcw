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


/*****************************************************************************
 * name:		files.c
 *
 * desc:		handle based filesystem for Quake III Arena
 *
 *
 *****************************************************************************/


#include "q_shared.h"
#include "qcommon.h"
#include "../zlib-1.2.11/unzip.h"

/*
=============================================================================

QUAKE3 FILESYSTEM

All of Quake's data access is through a hierarchical file system, but the contents of
the file system can be transparently merged from several sources.

A "qpath" is a reference to game file data.  MAX_ZPATH is 256 characters, which must include
a terminating zero. "..", "\\", and ":" are explicitly illegal in qpaths to prevent any
references outside the quake directory system.

The "base path" is the path to the directory holding all the game directories and usually
the executable.  It defaults to ".", but can be overridden with a "+set fs_basepath c:\quake3"
command line to allow code debugging in a different directory.  Basepath cannot
be modified at all after startup.  Any files that are created (demos, screenshots,
etc) will be created relative to the base path, so base path should usually be writable.

The "home path" is the path used for all write access. On win32 systems we have "base path"
== "home path", but on *nix systems the base installation is usually readonly, and
"home path" points to ~/.q3a or similar

The user can also install custom mods and content in "home path", so it should be searched
along with "home path" and "cd path" for game content.


The "base game" is the directory under the paths where data comes from by default, and
can be either "baseq3" or "demoq3".

The "current game" may be the same as the base game, or it may be the name of another
directory under the paths that should be searched for files before looking in the base game.
This is the basis for addons.

Clients automatically set the game directory after receiving a gamestate from a server,
so only servers need to worry about +set fs_game.

No other directories outside of the base game and current game will ever be referenced by
filesystem functions.

To save disk space and speed loading, directory trees can be collapsed into zip files.
The files use a ".pk3" extension to prevent users from unzipping them accidentally, but
otherwise the are simply normal uncompressed zip files.  A game directory can have multiple
zip files of the form "pak0.pk3", "pak1.pk3", etc.  Zip files are searched in decending order
from the highest number to the lowest, and will always take precedence over the filesystem.
This allows a pk3 distributed as a patch to override all existing data.

Because we will have updated executables freely available online, there is no point to
trying to restrict demo / oem versions of the game with code changes.  Demo / oem versions
should be exactly the same executables as release versions, but with different data that
automatically restricts where game media can come from to prevent add-ons from working.

File search order: when FS_FOpenFileRead gets called it will go through the fs_searchpaths
structure and stop on the first successful hit. fs_searchpaths is built with successive
calls to FS_AddGameDirectory

Additionaly, we search in several subdirectories:
current game is the current mode
base game is a variable to allow mods based on other mods
(such as baseq3 + missionpack content combination in a mod for instance)
BASEGAME is the hardcoded base game ("baseq3")

e.g. the qpath "sound/newstuff/test.wav" would be searched for in the following places:

home path + current game's zip files
home path + current game's directory
base path + current game's zip files
base path + current game's directory
cd path + current game's zip files
cd path + current game's directory

home path + base game's zip file
home path + base game's directory
base path + base game's zip file
base path + base game's directory
cd path + base game's zip file
cd path + base game's directory

home path + BASEGAME's zip file
home path + BASEGAME's directory
base path + BASEGAME's zip file
base path + BASEGAME's directory
cd path + BASEGAME's zip file
cd path + BASEGAME's directory

server download, to be written to home path + current game's directory


The filesystem can be safely shutdown and reinitialized with different
basedir / cddir / game combinations, but all other subsystems that rely on it
(sound, video) must also be forced to restart.

Because the same files are loaded by both the clip model (CM_) and renderer (TR_)
subsystems, a simple single-file caching scheme is used.  The CM_ subsystems will
load the file with a request to cache.  Only one file will be kept cached at a time,
so any models that are going to be referenced by both subsystems should alternate
between the CM_ load function and the ref load function.

TODO: A qpath that starts with a leading slash will always refer to the base game, even if another
game is currently active.  This allows character models, skins, and sounds to be downloaded
to a common directory no matter which game is active.

How to prevent downloading zip files?
Pass pk3 file names in systeminfo, and download before FS_Restart()?

Aborting a download disconnects the client from the server.

How to mark files as downloadable?  Commercial add-ons won't be downloadable.

Non-commercial downloads will want to download the entire zip file.
the game would have to be reset to actually read the zip in

Auto-update information

Path separators

Casing

  separate server gamedir and client gamedir, so if the user starts
  a local game after having connected to a network game, it won't stick
  with the network game.

  allow menu options for game selection?

Read / write config to floppy option.

Different version coexistance?

When building a pak file, make sure a wolfconfig.cfg isn't present in it,
or configs will never get loaded from disk!

  todo:

  downloading (outside fs?)
  game directory passing and restarting

=============================================================================

*/

// TTimo: moved to qcommon.h
// NOTE: could really do with a cvar
//#define	BASEGAME			"main"
//#define DEMOGAME            "demomain"

// every time a new demo pk3 file is built, this checksum must be updated.
// the easiest way to get it is to just run the game and see what it spits out
//DHM - Nerve :: Wolf Multiplayer demo checksum
// NOTE TTimo: always needs the 'u' for unsigned int (gcc)
#ifndef STANDALONE
#define DEMO_PAK0_CHECKSUM   2031778175u

static const unsigned int pak_checksums[] = {
	1886207346u
};

static const unsigned int mppak_checksums[] = {
	764840216u,
	-1023558518u,
	125907563u,
	131270674u,
	-137448799u,
	2149774797u
};

#ifndef DEDICATED
static const unsigned int binpak_checksums[] = {
	4022537001u,	// mp_bin.pk3 (1.4/1.41)
	2925068652u,	// mp_bin0.pk3 (1.51c)
	3375536862u,	// Degeneration (1.11)
	2977960779u,	// Omni-Bot (0.81/0.82)
	618492725u,	// Omnit-Bot (0.85)
	2205235539u,	// OSP (0.83)
	718455065u,	// OSP (0.9)
	220076119u,	// Wild West (1.613)
	2849242342u	// Wolftactics (1.0/1.01)
};
#endif

#endif

#define MAX_ZPATH           256
#define MAX_SEARCH_PATHS    4096
#define MAX_FILEHASH_SIZE   1024

typedef struct fileInPack_s {
	char                    *name;      // name of the file
	unsigned long pos;                  // file info position in zip
	unsigned long			len;		// uncompress file size
	struct  fileInPack_s*   next;       // next file in the hash
} fileInPack_t;

typedef struct {
	char pakPathname[MAX_OSPATH];		    // c:\quake3\baseq3
	char pakFilename[MAX_OSPATH];               // c:\quake3\baseq3\pak0.pk3
	char pakBasename[MAX_OSPATH];               // pak0
	char pakGamename[MAX_OSPATH];               // baseq3
	unzFile handle;                             // handle to zip file
	int checksum;                               // regular checksum
	int pure_checksum;                          // checksum for pure
	int numfiles;                               // number of files in pk3
	int referenced;                             // referenced file flags
	int hashSize;                               // hash table size (power of 2)
	fileInPack_t*   *hashTable;                 // hash table
	fileInPack_t*   buildBuffer;                // buffer with the filenames etc.
} pack_t;

typedef struct {
	char path[MAX_OSPATH];              // c:\quake3
	char		fullpath[MAX_OSPATH];		// c:\quake3\baseq3
	char gamedir[MAX_OSPATH];           // baseq3
	qboolean	allowUnzippedDLLs;		// whether to load unzipped dlls from directory
} directory_t;

typedef struct searchpath_s {
	struct searchpath_s *next;

	pack_t      *pack;      // only one of pack / dir will be non NULL
	directory_t *dir;
} searchpath_t;

static char fs_gamedir[MAX_OSPATH];         // this will be a single file name with no separators
static cvar_t      *fs_debug;
static cvar_t      *fs_homepath;

#ifdef __APPLE__
// Also search the .app bundle for .pk3 files
static  cvar_t          *fs_apppath;
#endif

#ifndef STANDALONE
static	cvar_t		*fs_steampath;
static	cvar_t		*fs_gogpath;
#endif

static cvar_t      *fs_basepath;
static cvar_t      *fs_basegame;
static cvar_t      *fs_gamedirvar;
static searchpath_t    *fs_searchpaths;
static int fs_readCount;                    // total bytes read
static int fs_loadCount;                    // total files read
static int fs_loadStack;                    // total files in memory
static int fs_packFiles = 0;                    // total number of files in packs

static int fs_checksumFeed;

typedef union qfile_gus {
	FILE*       o;
	unzFile z;
} qfile_gut;

typedef struct qfile_us {
	qfile_gut file;
	qboolean unique;
} qfile_ut;

typedef struct {
	qfile_ut handleFiles;
	qboolean handleSync;
	int fileSize;
	int zipFilePos;
	int zipFileLen;
	qboolean zipFile;
	char name[MAX_ZPATH];
} fileHandleData_t;

static fileHandleData_t fsh[MAX_FILE_HANDLES];

// TTimo - show_bug.cgi?id=540
// wether we did a reorder on the current search path when joining the server
static qboolean fs_reordered;

// never load anything from pk3 files that are not present at the server when pure
// ex: when fs_numServerPaks != 0, FS_FOpenFileRead won't load anything outside of pk3 except .cfg .menu .game .dat
static int fs_numServerPaks = 0;
static int fs_serverPaks[MAX_SEARCH_PATHS];                     // checksums
static char     *fs_serverPakNames[MAX_SEARCH_PATHS];           // pk3 names

// only used for autodownload, to make sure the client has at least
// all the pk3 files that are referenced at the server side
static int fs_numServerReferencedPaks;
static int fs_serverReferencedPaks[MAX_SEARCH_PATHS];               // checksums
static char     *fs_serverReferencedPakNames[MAX_SEARCH_PATHS];     // pk3 names

// last valid game folder used
char lastValidBase[MAX_OSPATH];
char lastValidComBaseGame[MAX_OSPATH];
char lastValidFsBaseGame[MAX_OSPATH];
char lastValidGame[MAX_OSPATH];

#ifdef FS_MISSING
FILE*       missingFiles = NULL;
#endif

/* C99 defines __func__ */
#if __STDC_VERSION__ < 199901L
#  if __GNUC__ >= 2 || _MSC_VER >= 1300
#    define __func__ __FUNCTION__
#  else
#    define __func__ "(unknown)"
#  endif
#endif

/*
==============
FS_Initialized
==============
*/

qboolean FS_Initialized( void ) {
	return ( fs_searchpaths != NULL );
}

/*
=================
FS_PakIsPure
=================
*/
qboolean FS_PakIsPure( pack_t *pack ) {
	int i;

	if ( fs_numServerPaks ) {
		for ( i = 0 ; i < fs_numServerPaks ; i++ ) {
			// FIXME: also use hashed file names
			// NOTE TTimo: a pk3 with same checksum but different name would be validated too
			//   I don't see this as allowing for any exploit, it would only happen if the client does manips of its file names 'not a bug'
			if ( pack->checksum == fs_serverPaks[i] ) {
				return qtrue;       // on the approved list
			}
		}
		return qfalse;  // not on the pure server pak list
	}
	return qtrue;
}


/*
=================
FS_LoadStack
return load stack
=================
*/
int FS_LoadStack( void ) {
	return fs_loadStack;
}

/*
================
return a hash value for the filename
================
*/
static long FS_HashFileName( const char *fname, int hashSize ) {
	int i;
	long hash;
	char letter;

	hash = 0;
	i = 0;
	while ( fname[i] != '\0' ) {
		letter = tolower( fname[i] );
		if ( letter == '.' ) {
			break;                          // don't include extension
		}
		if ( letter == '\\' ) {
			letter = '/';                   // damn path names
		}
		if ( letter == PATH_SEP ) {
			letter = '/';                           // damn path names
		}
		hash += (long)( letter ) * ( i + 119 );
		i++;
	}
	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ) );
	hash &= ( hashSize - 1 );
	return hash;
}

static fileHandle_t FS_HandleForFile( void ) {
	int i;

	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o == NULL ) {
			return i;
		}
	}
	Com_Error( ERR_DROP, "FS_HandleForFile: none free" );
	return 0;
}

static FILE *FS_FileForHandle( fileHandle_t f ) {
	if ( f < 1 || f >= MAX_FILE_HANDLES ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: out of range" );
	}
	if ( fsh[f].zipFile == qtrue ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: can't get FILE on zip file" );
	}
	if ( !fsh[f].handleFiles.file.o ) {
		Com_Error( ERR_DROP, "FS_FileForHandle: NULL" );
	}

	return fsh[f].handleFiles.file.o;
}

void    FS_ForceFlush( fileHandle_t f ) {
	FILE *file;

	file = FS_FileForHandle( f );
	setvbuf( file, NULL, _IONBF, 0 );
}

/*
================
FS_fplength
================
*/

long FS_fplength(FILE *h)
{
	long		pos;
	long		end;

	pos = ftell(h);
	fseek(h, 0, SEEK_END);
	end = ftell(h);
	fseek(h, pos, SEEK_SET);

	return end;
}

/*
================
FS_filelength

If this is called on a non-unique FILE (from a pak file),
it will return the size of the pak file, not the expected
size of the file.
================
*/
long FS_filelength(fileHandle_t f)
{
	FILE	*h;

	h = FS_FileForHandle(f);
	
	if(h == NULL)
	        return -1;
        else
	        return FS_fplength(h);
}

/*
====================
FS_ReplaceSeparators

Fix things up differently for win/unix/mac
====================
*/
static void FS_ReplaceSeparators( char *path ) {
	char    *s;
	qboolean lastCharWasSep = qfalse;

	for ( s = path ; *s ; s++ ) {
		if ( *s == '/' || *s == '\\' ) {
			if ( !lastCharWasSep ) {
				*s = PATH_SEP;
				lastCharWasSep = qtrue;
			} else {
				memmove (s, s + 1, strlen (s));
			}
		} else {
			lastCharWasSep = qfalse;
		}
	}
}

/*
===================
FS_BuildOSPath

Qpath may have either forward or backwards slashes
===================
*/
char *FS_BuildOSPath( const char *base, const char *game, const char *qpath ) {
	char temp[MAX_OSPATH];
	static char ospath[2][MAX_OSPATH];
	static int toggle;

	toggle ^= 1;        // flip-flop to allow two returns without clash

	if ( !game || !game[0] ) {
		game = fs_gamedir;
	}

	Com_sprintf( temp, sizeof( temp ), "/%s/%s", game, qpath );
	FS_ReplaceSeparators( temp );
	Com_sprintf( ospath[toggle], sizeof( ospath[0] ), "%s%s", base, temp );

	return ospath[toggle];
}


/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
qboolean FS_CreatePath( char *OSPath ) {
	char    *ofs;
	char	path[MAX_OSPATH];

	// make absolutely sure that it can't back up the path
	// FIXME: is c: allowed???
	if ( strstr( OSPath, ".." ) || strstr( OSPath, "::" ) ) {
		Com_Printf( "WARNING: refusing to create relative path \"%s\"\n", OSPath );
		return qtrue;
	}

	Q_strncpyz( path, OSPath, sizeof( path ) );
	FS_ReplaceSeparators( path );

	// Skip creation of the root directory as it will always be there
	ofs = strchr( path, PATH_SEP );
	if ( ofs != NULL ) {
		ofs++;
	}

	for (; ofs != NULL && *ofs ; ofs++) {
		if (*ofs == PATH_SEP) {
			// create the directory
			*ofs = 0;
			if (!Sys_Mkdir (path)) {
				Com_Error( ERR_FATAL, "FS_CreatePath: failed to create path \"%s\"",
					path );
			}
			*ofs = PATH_SEP;
		}
	}
	return qfalse;
}

/*
=================
FS_CheckFilenameIsMutable

ERR_FATAL if trying to maniuplate a file with the platform library, QVM, or pk3 extension
=================
 */
static void FS_CheckFilenameIsMutable( const char *filename,
		const char *function )
{
	// Check if the filename ends with the library, QVM, or pk3 extension
	if( Sys_DllExtension( filename )
		|| COM_CompareExtension( filename, ".qvm" )
		|| COM_CompareExtension( filename, ".pk3" ) )
	{
		Com_Error( ERR_FATAL, "%s: Not allowed to manipulate '%s' due "
			"to %s extension", function, filename, COM_GetExtension( filename ) );
	}
}

/*
===========
FS_Remove

===========
*/
void FS_Remove( const char *osPath ) {
	FS_CheckFilenameIsMutable( osPath, __func__ );

	remove( osPath );
}

/*
===========
FS_HomeRemove

===========
*/
void FS_HomeRemove( const char *homePath ) {
	FS_CheckFilenameIsMutable( homePath, __func__ );

	remove( FS_BuildOSPath( fs_homepath->string,
			fs_gamedir, homePath ) );
}

/*
================
FS_FileInPathExists

Tests if path and file exists
================
*/
qboolean FS_FileInPathExists(const char *testpath)
{
	FILE *filep;

	filep = Sys_FOpen(testpath, "rb");	

	if(filep)
	{
		fclose(filep);
		return qtrue;
	}
	
	return qfalse;
}

/*
================
FS_FileExists

Tests if the file exists in the current gamedir, this DOES NOT
search the paths.  This is to determine if opening a file to write
(which always goes into the current gamedir) will cause any overwrites.
NOTE TTimo: this goes with FS_FOpenFileWrite for opening the file afterwards
================
*/
qboolean FS_FileExists(const char *file)
{
	return FS_FileInPathExists(FS_BuildOSPath(fs_homepath->string, fs_gamedir, file));
}

/*
================
FS_SV_FileExists

Tests if the file exists 
================
*/
qboolean FS_SV_FileExists( const char *file )
{
	char *testpath;

	testpath = FS_BuildOSPath( fs_homepath->string, file, "");
	testpath[strlen(testpath)-1] = '\0';

	return FS_FileInPathExists(testpath);
}

/*
===========
FS_SV_FOpenFileWrite

===========
*/
fileHandle_t FS_SV_FOpenFileWrite( const char *filename ) {
	char *ospath;
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	ospath = FS_BuildOSPath( fs_homepath->string, filename, "" );
	ospath[strlen(ospath)-1] = '\0';

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	if ( fs_debug->integer ) {
		Com_Printf( "FS_SV_FOpenFileWrite: %s\n", ospath );
	}

//	FS_CheckFilenameIsMutable( ospath, __func__ );

	if( FS_CreatePath( ospath ) ) {
		return 0;
	}

	Com_DPrintf( "writing to: %s\n", ospath );
	fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "wb" );

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o) {
		f = 0;
	}
	return f;
}

/*
===========
FS_SV_FOpenFileRead
Search for a file somewhere below the home path then base path
in that order
===========
*/
long FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp ) {
	char *ospath;
	fileHandle_t f = 0;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	// don't let sound stutter
//	S_ClearSoundBuffer();

	// search homepath
	ospath = FS_BuildOSPath( fs_homepath->string, filename, "" );
	// remove trailing slash
	ospath[strlen( ospath ) - 1] = '\0';

	if ( fs_debug->integer ) {
		Com_Printf( "FS_SV_FOpenFileRead (fs_homepath): %s\n", ospath );
	}

	fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "rb" );
	fsh[f].handleSync = qfalse;
	if (!fsh[f].handleFiles.file.o)
	{
		// If fs_homepath == fs_basepath, don't bother
		if (Q_stricmp(fs_homepath->string,fs_basepath->string))
		{
			// search basepath
			ospath = FS_BuildOSPath( fs_basepath->string, filename, "" );
			ospath[strlen(ospath)-1] = '\0';

			if ( fs_debug->integer )
			{
				Com_Printf( "FS_SV_FOpenFileRead (fs_basepath): %s\n", ospath );
			}

			fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "rb" );
			fsh[f].handleSync = qfalse;
		}

#ifndef STANDALONE
		// Check fs_steampath
		if (!fsh[f].handleFiles.file.o && fs_steampath->string[0])
		{
			ospath = FS_BuildOSPath( fs_steampath->string, filename, "" );
			ospath[strlen(ospath)-1] = '\0';

			if ( fs_debug->integer )
			{
				Com_Printf( "FS_SV_FOpenFileRead (fs_steampath): %s\n", ospath );
			}

			fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "rb" );
			fsh[f].handleSync = qfalse;
		}

		// Check fs_gogpath
		if (!fsh[f].handleFiles.file.o && fs_gogpath->string[0])
		{
			ospath = FS_BuildOSPath( fs_gogpath->string, filename, "" );
			ospath[strlen(ospath)-1] = '\0';

			if ( fs_debug->integer )
			{
				Com_Printf( "FS_SV_FOpenFileRead (fs_gogpath): %s\n", ospath );
			}

			fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "rb" );
			fsh[f].handleSync = qfalse;
		}
#endif


		if ( !fsh[f].handleFiles.file.o )
		{
			f = 0;
		}
	}

	*fp = f;
	if ( f ) {
		return FS_filelength( f );
	}
	return -1;
}


/*
===========
FS_SV_Rename

===========
*/
void FS_SV_Rename( const char *from, const char *to, qboolean safe ) {
	char            *from_ospath, *to_ospath;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	// don't let sound stutter
//	S_ClearSoundBuffer();

	from_ospath = FS_BuildOSPath( fs_homepath->string, from, "" );
	to_ospath = FS_BuildOSPath( fs_homepath->string, to, "" );
	from_ospath[strlen( from_ospath ) - 1] = '\0';
	to_ospath[strlen( to_ospath ) - 1] = '\0';

	if ( fs_debug->integer ) {
		Com_Printf( "FS_SV_Rename: %s --> %s\n", from_ospath, to_ospath );
	}

	if ( safe ) {
		FS_CheckFilenameIsMutable( to_ospath, __func__ );
	}

	rename(from_ospath, to_ospath);
}




/*
===========
FS_Rename

===========
*/
void FS_Rename( const char *from, const char *to ) {
	char            *from_ospath, *to_ospath;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	// don't let sound stutter
//	S_ClearSoundBuffer();

	from_ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, from );
	to_ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, to );

	if ( fs_debug->integer ) {
		Com_Printf( "FS_Rename: %s --> %s\n", from_ospath, to_ospath );
	}

	FS_CheckFilenameIsMutable( to_ospath, __func__ );

	rename(from_ospath, to_ospath);
}

/*
==============
FS_FCloseFile

If the FILE pointer is an open pak file, leave it open.

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile( fileHandle_t f ) {
	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( fsh[f].zipFile == qtrue ) {
		unzCloseCurrentFile( fsh[f].handleFiles.file.z );
		if ( fsh[f].handleFiles.unique ) {
			unzClose( fsh[f].handleFiles.file.z );
		}
		Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
		return;
	}

	// we didn't find it as a pak, so close it as a unique file
	if ( fsh[f].handleFiles.file.o ) {
		fclose( fsh[f].handleFiles.file.o );
	}
	Com_Memset( &fsh[f], 0, sizeof( fsh[f] ) );
}

/*
===========
FS_FOpenFileWrite

===========
*/
fileHandle_t FS_FOpenFileWrite( const char *filename ) {
	char            *ospath;
	fileHandle_t f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	if ( fs_debug->integer ) {
		Com_Printf( "FS_FOpenFileWrite: %s\n", ospath );
	}

//	FS_CheckFilenameIsMutable( ospath, __func__ );

	if ( FS_CreatePath( ospath ) ) {
		return 0;
	}

	// enabling the following line causes a recursive function call loop
	// when running with +set logfile 1 +set developer 1
	//Com_DPrintf( "writing to: %s\n", ospath );
	fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "wb" );

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	fsh[f].handleSync = qfalse;
	if ( !fsh[f].handleFiles.file.o ) {
		f = 0;
	}
	return f;
}

/*
===========
FS_FOpenFileAppend

===========
*/
fileHandle_t FS_FOpenFileAppend( const char *filename ) {
	char            *ospath;
	fileHandle_t f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	// don't let sound stutter
//	S_ClearSoundBuffer();

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	if ( fs_debug->integer ) {
		Com_Printf( "FS_FOpenFileAppend: %s\n", ospath );
	}

	FS_CheckFilenameIsMutable( ospath, __func__ );

	if ( FS_CreatePath( ospath ) ) {
		return 0;
	}

	fsh[f].handleFiles.file.o = Sys_FOpen( ospath, "ab" );
	fsh[f].handleSync = qfalse;
	if ( !fsh[f].handleFiles.file.o ) {
		f = 0;
	}
	return f;
}

/*
===========
FS_FCreateOpenPipeFile

===========
*/
fileHandle_t FS_FCreateOpenPipeFile( const char *filename ) {
	char	    		*ospath;
	FILE					*fifo;
	fileHandle_t	f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	f = FS_HandleForFile();
	fsh[f].zipFile = qfalse;

	Q_strncpyz( fsh[f].name, filename, sizeof( fsh[f].name ) );

	// don't let sound stutter
//	S_ClearSoundBuffer();

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	if ( fs_debug->integer ) {
		Com_Printf( "FS_FCreateOpenPipeFile: %s\n", ospath );
	}

	FS_CheckFilenameIsMutable( ospath, __func__ );

	fifo = Sys_Mkfifo( ospath );
	if( fifo ) {
		fsh[f].handleFiles.file.o = fifo;
		fsh[f].handleSync = qfalse;
	}
	else
	{
		Com_Printf( S_COLOR_YELLOW "WARNING: Could not create new com_pipefile at %s. "
			"com_pipefile will not be used.\n", ospath );
		f = 0;
	}

	return f;
}

/*
===========
FS_FilenameCompare

Ignore case and seprator char distinctions
===========
*/
qboolean FS_FilenameCompare( const char *s1, const char *s2 ) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( c1 >= 'a' && c1 <= 'z' ) {
			c1 -= ( 'a' - 'A' );
		}
		if ( c2 >= 'a' && c2 <= 'z' ) {
			c2 -= ( 'a' - 'A' );
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}

		if ( c1 != c2 ) {
			return qtrue;      // strings not equal
		}
	} while ( c1 );

	return qfalse;       // strings are equal
}

/*
===========
FS_IsExt

Return qtrue if ext matches file extension filename
===========
*/

qboolean FS_IsExt(const char *filename, const char *ext, int namelen)
{
	int extlen;

	extlen = strlen(ext);

	if(extlen > namelen)
		return qfalse;

	filename += namelen - extlen;

	return !Q_stricmp(filename, ext);
}

/*
===========
FS_IsDemoExt

Return qtrue if filename has a demo extension
===========
*/

qboolean FS_IsDemoExt(const char *filename, int namelen)
{
	char *ext_test;
	int index, protocol;

	ext_test = strrchr(filename, '.');
	if(ext_test && !Q_stricmpn(ext_test + 1, DEMOEXT, ARRAY_LEN(DEMOEXT) - 1))
	{
		protocol = atoi(ext_test + ARRAY_LEN(DEMOEXT));

		if(protocol == com_protocol->integer)
			return qtrue;

#ifdef LEGACY_PROTOCOL
                if(protocol == com_legacyprotocol->integer)
                        return qtrue;
#endif

		for(index = 0; demo_protocols[index]; index++)
		{
			if(demo_protocols[index] == protocol)
			return qtrue;
		}
	}

	return qfalse;
}

/*
===========
FS_ShiftedStrStr
===========
*/
char *FS_ShiftedStrStr( const char *string, const char *substring, int shift ) {
	char buf[MAX_STRING_TOKENS];
	int i;

	for ( i = 0; substring[i]; i++ ) {
		buf[i] = substring[i] + shift;
	}
	buf[i] = '\0';
	return strstr( string, buf );
}

/*
==========
FS_ShiftStr
perform simple string shifting to avoid scanning from the exe
==========
*/
char *FS_ShiftStr( const char *string, int shift ) {
	static char buf[MAX_STRING_CHARS];
	int i,l;

	l = strlen( string );
	for ( i = 0; i < l; i++ ) {
		buf[i] = string[i] + shift;
	}
	buf[i] = '\0';
	return buf;
}

/*
===========
FS_FOpenFileReadDir

Tries opening file "filename" in searchpath "search"
Returns filesize and an open FILE pointer.
===========
*/
extern qboolean		com_fullyInitialized;

// see FS_FOpenFileRead_Filtered
static int fs_filter_flag = 0;

long FS_FOpenFileReadDir(const char *filename, searchpath_t *search, fileHandle_t *file, qboolean uniqueFILE, qboolean unpure)
{
	long			hash;
	pack_t		*pak;
	fileInPack_t	*pakFile;
	directory_t	*dir;
	char		*netpath;
	FILE		*filep;
	int			len;

	if(filename == NULL)
		Com_Error(ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed");

	// qpaths are not supposed to have a leading slash
	if(filename[0] == '/' || filename[0] == '\\')
		filename++;

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo" 
	if(strstr(filename, ".." ) || strstr(filename, "::"))
	{
	        if(file == NULL)
	                return qfalse;
	                
		*file = 0;
		return -1;
	}
        
	// make sure the rtcwkey file is only readable by the WolfMP.exe at initialization
	// any other time the key should only be accessed in memory using the provided functions
	if(com_fullyInitialized && strstr(filename, "rtcwkey"))
	{
	        if(file == NULL)
	                return qfalse;

		*file = 0;
		return -1;
	}

	if(file == NULL)
	{
		// just wants to see if file is there

		// is the element a pak file?
		if(search->pack)
		{
			hash = FS_HashFileName(filename, search->pack->hashSize);
                        
                        if(search->pack->hashTable[hash] && !(fs_filter_flag & FS_EXCLUDE_PK3))
                        {
				// look through all the pak file elements
				pak = search->pack;
				pakFile = pak->hashTable[hash];

				do
				{
					// case and separator insensitive comparisons
					if(!FS_FilenameCompare(pakFile->name, filename))
					{
						// found it!
						if(pakFile->len)
        						return pakFile->len;
                                                else
                                                {
                                                        // It's not nice, but legacy code depends
                                                        // on positive value if file exists no matter
                                                        // what size
                                                        return 1;
                                                }
					}

					pakFile = pakFile->next;
				} while(pakFile != NULL);
			}
		}
		else if(search->dir && !(fs_filter_flag & FS_EXCLUDE_DIR))
		{
			dir = search->dir;
		
			netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
			filep = Sys_FOpen(netpath, "rb");

			if(filep)
			{
			        len = FS_fplength(filep);
				fclose(filep);
				
				if(len)
        				return len;
                                else
                                        return 1;
			}
		}
		
		return 0;
	}

	*file = FS_HandleForFile();
	fsh[*file].handleFiles.unique = uniqueFILE;
	
	// is the element a pak file?
	if(search->pack)
	{
		hash = FS_HashFileName(filename, search->pack->hashSize);

		if(search->pack->hashTable[hash] && !(fs_filter_flag & FS_EXCLUDE_PK3))
		{
			// disregard if it doesn't match one of the allowed pure pak files
			if(!unpure && !FS_PakIsPure(search->pack))
			{
				*file = 0;
				return -1;
			}

			// look through all the pak file elements
			pak = search->pack;
			pakFile = pak->hashTable[hash];
		
			do
			{
				// case and separator insensitive comparisons
				if(!FS_FilenameCompare(pakFile->name, filename))
				{
					// found it!

					// Mark the pak as having been referenced and mark specifics on cgame and ui.
					// Shaders, txt, and arena files by themselves do not count as a reference as 
					// these are loaded from all pk3s 
					len = strlen(filename);

					if (!(pak->referenced & FS_GENERAL_REF))
					{
						if(!FS_IsExt(filename, ".shader", len) &&
						   !FS_IsExt(filename, ".txt", len) &&
						   !FS_IsExt(filename, ".cfg", len) &&
						   !FS_IsExt(filename, ".config", len) &&
						   !FS_IsExt(filename, ".bot", len) &&
						   !FS_IsExt(filename, ".arena", len) &&
						   !FS_IsExt(filename, ".menu", len) &&
//						   Q_stricmp(filename, "vm/qagame.qvm") != 0 && // Commented out for now
						   !strstr(filename, "levelshots"))
						{
							pak->referenced |= FS_GENERAL_REF;
						}
					}

					if(strstr(filename, Sys_GetDLLName( "cgame" )))
						pak->referenced |= FS_CGAME_REF;

					if(strstr(filename, Sys_GetDLLName( "ui" )))
						pak->referenced |= FS_UI_REF;

					if(strstr(filename, "cgame.mp.qvm"))
						pak->referenced |= FS_CGAME_REF;

					if(strstr(filename, "ui.mp.qvm"))
						pak->referenced |= FS_UI_REF;

					// DHM -- Nerve :: Don't allow singleplayer maps to be loaded from pak0
					if ( Q_stricmp( filename + len - 4, ".bsp" ) == 0 &&
						 Q_stricmp( pak->pakBasename, "pak0" ) == 0 ) {

						*file = 0;
						return -1;
					}

					if(uniqueFILE)
					{
						// open a new file on the pakfile
						fsh[*file].handleFiles.file.z = unzOpen(pak->pakFilename);
					
						if(fsh[*file].handleFiles.file.z == NULL)
							Com_Error(ERR_FATAL, "Couldn't open %s", pak->pakFilename);
					}
					else
						fsh[*file].handleFiles.file.z = pak->handle;

					Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
					fsh[*file].zipFile = qtrue;
				
					// set the file position in the zip file (also sets the current file info)
					unzSetOffset(fsh[*file].handleFiles.file.z, pakFile->pos);

					// open the file in the zip
					unzOpenCurrentFile(fsh[*file].handleFiles.file.z);
					fsh[*file].zipFilePos = pakFile->pos;
					fsh[*file].zipFileLen = pakFile->len;

					if(fs_debug->integer)
					{
						Com_Printf("FS_FOpenFileRead: %s (found in '%s')\n", 
							filename, pak->pakFilename);
					}
				
					return pakFile->len;
				}
			
				pakFile = pakFile->next;
			} while(pakFile != NULL);
		}
	}
	else if(search->dir && !(fs_filter_flag & FS_EXCLUDE_DIR))
	{
		// check a file in the directory tree

		// if we are running restricted, the only files we
		// will allow to come from the directory are .cfg files
		len = strlen(filename);
		// FIXME TTimo I'm not sure about the fs_numServerPaks test
		// if you are using FS_ReadFile to find out if a file exists,
		//   this test can make the search fail although the file is in the directory
		// I had the problem on https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=8
		// turned out I used FS_FileExists instead
		if(!unpure && fs_numServerPaks)
		{
			if(!FS_IsExt(filename, ".cfg", len) &&		// for config files
			   !FS_IsExt(filename, ".menu", len) &&		// menu files
			   !FS_IsExt(filename, ".game", len) &&		// menu files
			   !FS_IsExt(filename, ".dat", len) &&		// for journal files
			   !FS_IsDemoExt(filename, len))			// demos
			{
				*file = 0;
				return -1;
			}
		}

		dir = search->dir;

		netpath = FS_BuildOSPath(dir->path, dir->gamedir, filename);
		filep = Sys_FOpen(netpath, "rb");

		if (filep == NULL)
		{
			*file = 0;
                        return -1;
		}

		Q_strncpyz(fsh[*file].name, filename, sizeof(fsh[*file].name));
		fsh[*file].zipFile = qfalse;
		
		if(fs_debug->integer)
		{
			Com_Printf("FS_FOpenFileRead: %s (found in '%s%c%s')\n", filename,
				dir->path, PATH_SEP, dir->gamedir);
		}

		fsh[*file].handleFiles.file.o = filep;
		return FS_fplength(filep);
	}

	*file = 0;
	return -1;
}

/*
===========
FS_FOpenFileRead

Finds the file in the search path.
Returns filesize and an open FILE pointer.
Used for streaming data out of either a
separate file or a ZIP file.
===========
*/
long FS_FOpenFileRead(const char *filename, fileHandle_t *file, qboolean uniqueFILE)
{
	searchpath_t *search;
	long len;
	qboolean isLocalConfig;

	if(!fs_searchpaths)
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");

	isLocalConfig = !strcmp(filename, "autoexec.cfg") || !strcmp(filename, Q3CONFIG_CFG);
	for(search = fs_searchpaths; search; search = search->next)
	{
		// autoexec.cfg and wolfconfig_mp.cfg can only be loaded outside of pk3 files.
		if (isLocalConfig && search->pack)
			continue;

	        len = FS_FOpenFileReadDir(filename, search, file, uniqueFILE, qfalse);
	        
	        if(file == NULL)
	        {
	                if(len > 0)
	                        return len;
	        }
	        else
	        {
	                if(len >= 0 && *file)
	                        return len;
	        }
	        
	}
	
#ifdef FS_MISSING
	if(missingFiles)
		fprintf(missingFiles, "%s\n", filename);
#endif

	if(file)
	{
		*file = 0;
		return -1;
	}
	else
	{
		// When file is NULL, we're querying the existance of the file
		// If we've got here, it doesn't exist
		return 0;
	}
}

int FS_FOpenFileRead_Filtered( const char *qpath, fileHandle_t *file, qboolean uniqueFILE, int filter_flag ) {
	int ret;

	fs_filter_flag = filter_flag;
	ret = FS_FOpenFileRead( qpath, file, uniqueFILE );
	fs_filter_flag = 0;

	return ret;
}

/*
=================
FS_FindVM

Find a suitable VM file in search path order.

In each searchpath try:
 - open DLL file
 - open QVM file if QVM loading enabled

write found DLL or QVM to "found" and return VMI_NATIVE if DLL, VMI_COMPILED if QVM
Return the searchpath in "startSearch".
=================
*/

int FS_FindVM(void **startSearch, char *found, int foundlen, const char *name, qboolean unpure, int enableQvm)
{
	searchpath_t *search, *lastSearch;
	directory_t *dir;
	pack_t *pack;
	char dllName[MAX_OSPATH], qvmName[MAX_OSPATH];
	char *netpath;

	if(!fs_searchpaths)
		Com_Error(ERR_FATAL, "Filesystem call made without initialization");

	if(enableQvm)
		Com_sprintf(qvmName, sizeof(qvmName), "vm/%s.mp.qvm", name);

	Q_strncpyz(dllName, Sys_GetDLLName(name), sizeof(dllName));

	lastSearch = *startSearch;
	if(*startSearch == NULL)
		search = fs_searchpaths;
	else
		search = lastSearch->next;
        
	while(search)
	{
		if(search->dir && (unpure || !Q_stricmp(name, "qagame")))
		{
			dir = search->dir;

			netpath = FS_BuildOSPath(dir->path, dir->gamedir, dllName);

			if(enableQvm && FS_FOpenFileReadDir(qvmName, search, NULL, qfalse, unpure) > 0)
			{
				*startSearch = search;
				return VMI_COMPILED;
			}

			if(dir->allowUnzippedDLLs && FS_FileInPathExists(netpath))
			{
				Q_strncpyz(found, netpath, foundlen);
				*startSearch = search;
				
				return VMI_NATIVE;
			}
		}
		else if(search->pack)
		{
			pack = search->pack;

		        if(lastSearch && lastSearch->pack)
		        {
		                // make sure we only try loading one VM file per game dir
		                // i.e. if VM from pak7.pk3 fails we won't try one from pak6.pk3
		                
		                if(!FS_FilenameCompare(lastSearch->pack->pakPathname, pack->pakPathname))
                                {
                                        search = search->next;
                                        continue;
                                }
		        }

			if(enableQvm && FS_FOpenFileReadDir(qvmName, search, NULL, qfalse, unpure) > 0)
			{
				*startSearch = search;

				return VMI_COMPILED;
			}

#ifndef DEDICATED
			// extract the dlls from the mp_bin.pk3 so
			// that they can be referenced
			if (Q_stricmp(name, "qagame"))
			{
				netpath = FS_BuildOSPath(fs_homepath->string, pack->pakGamename, dllName);
#ifndef STANDALONE
				int index;
				qboolean whitelisted = qfalse;
			
				// Check whether this pak's checksum is on the whitelist
				for(index = 0; index < ARRAY_LEN(binpak_checksums); index++)
				{
					if(pack->checksum == binpak_checksums[index])
					{
						whitelisted = FS_CL_ExtractFromPakFile(search, netpath, dllName, NULL);
					}
				}

				if (FS_FOpenFileReadDir(dllName, search, NULL, qfalse, unpure) > 0
						&& whitelisted)
#else
				if (FS_FOpenFileReadDir(dllName, search, NULL, qfalse, unpure) > 0
						&& FS_CL_ExtractFromPakFile(search, netpath, dllName, NULL))
#endif
				{
					Com_Printf( "Loading %s dll from %s\n", name, pack->pakFilename );
					Q_strncpyz(found, netpath, foundlen);
					*startSearch = search;

					return VMI_NATIVE;
				}
			}
#endif
		}
		
		search = search->next;
	}

	return -1;
}

// TTimo
// relevant to client only
#if !defined( DEDICATED )
/*
==================
FS_CL_ExtractFromPakFile

NERVE - SMF - Extracts the latest file from a pak file.

Compares packed file against extracted file. If no differences, does not copy.
This is necessary for exe/dlls which may or may not be locked.

NOTE TTimo:
  fullpath gives the full OS path to the dll that will potentially be loaded
	on win32 it's always in fs_basepath/<fs_game>/
	on linux it can be in fs_homepath/<fs_game>/ or fs_basepath/<fs_game>/
  the dll is extracted to fs_homepath (== fs_basepath on win32) if needed

  the return value doesn't tell wether file was extracted or not, it just says wether it's ok to continue
  (i.e. either the right file was extracted successfully, or it was already present)

  cvar_lastVersion is the optional name of a CVAR_ARCHIVE used to store the wolf version for the last extracted .so
  show_bug.cgi?id=463

==================
*/
qboolean FS_CL_ExtractFromPakFile( void *searchpath, const char *fullpath, const char *filename, const char *cvar_lastVersion ) {
	int srcLength;
	int destLength;
	unsigned char   *srcData;
	unsigned char   *destData;
	qboolean needToCopy;
	FILE            *destHandle;
	int	read;

	needToCopy = qtrue;

	// read in compressed file
	srcLength = FS_ReadFileDir(filename, searchpath, qfalse, (void **)&srcData);

	// if its not in the pak, we bail
	if ( srcLength == -1 ) {
		return qfalse;
	}

	// read in local file
	destHandle = Sys_FOpen( fullpath, "rb" );

	// if we have a local file, we need to compare the two
	if ( destHandle ) {
		fseek( destHandle, 0, SEEK_END );
		destLength = ftell( destHandle );
		fseek( destHandle, 0, SEEK_SET );

		if ( destLength > 0 ) {
			destData = (unsigned char*)Z_Malloc( destLength );

			read = fread( destData, 1, destLength, destHandle );

			if (read == 0) {
				Com_Error (ERR_FATAL, "FS_CL_ExtractFromPakFile: 0 bytes read");
			}

			// compare files
			if ( destLength == srcLength ) {
				int i;

				for ( i = 0; i < destLength; i++ ) {
					if ( destData[i] != srcData[i] ) {
						break;
					}
				}

				if ( i == destLength ) {
					needToCopy = qfalse;
				}
			}

			Z_Free( destData ); // TTimo
		}

		fclose( destHandle );
	}

	// write file
	if ( needToCopy ) {
		fileHandle_t f;

		Com_DPrintf("FS_ExtractFromPakFile: FS_FOpenFileWrite '%s'\n", filename);
		f = FS_FOpenFileWrite( filename );
		if ( !f ) {
			Com_Printf( "Failed to open %s\n", filename );
			return qfalse;
		}

		FS_Write( srcData, srcLength, f );

		FS_FCloseFile( f );

#ifdef __linux__
		// show_bug.cgi?id=463
		// need to keep track of what versions we extract
		if ( cvar_lastVersion ) {
			Cvar_Set( cvar_lastVersion, Cvar_VariableString( "version" ) );
		}
#endif
	}

	FS_FreeFile( srcData );
	return qtrue;
}
#endif

/*
==============
FS_Delete
TTimo - this was not in the 1.30 filesystem code
using fs_homepath for the file to remove
==============
*/
int FS_Delete( char *filename ) {
#if 0 // Stub...not used in MP
	char *ospath;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization\n" );
	}

	if ( !filename || filename[0] == 0 ) {
		return 0;
	}

	// for safety, only allow deletion from the save directory
	if ( Q_strncmp( filename, "save/", 5 ) != 0 ) {
		return 0;
	}

	ospath = FS_BuildOSPath( fs_homepath->string, fs_gamedir, filename );

	if ( remove( ospath ) != -1 ) {  // success
		return 1;
	}
#endif

	return 0;
}

/*
=================
FS_Read

Properly handles partial reads
=================
*/
int FS_Read( void *buffer, int len, fileHandle_t f ) {
	int block, remaining;
	int read;
	byte    *buf;
	int tries;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !f ) {
		return 0;
	}

	buf = (byte *)buffer;
	fs_readCount += len;

	if ( fsh[f].zipFile == qfalse ) {
		remaining = len;
		tries = 0;
		while ( remaining ) {
			block = remaining;
			read = fread( buf, 1, block, fsh[f].handleFiles.file.o );
			if ( read == 0 ) {
				// we might have been trying to read from a CD, which
				// sometimes returns a 0 read on windows
				if ( !tries ) {
					tries = 1;
				} else {
					return len - remaining;   //Com_Error (ERR_FATAL, "FS_Read: 0 bytes read");
				}
			}

			if ( read == -1 ) {
				Com_Error( ERR_FATAL, "FS_Read: -1 bytes read" );
			}

			remaining -= read;
			buf += read;
		}
		return len;
	} else {
		return unzReadCurrentFile( fsh[f].handleFiles.file.z, buffer, len );
	}
}

/*
=================
FS_Write

Properly handles partial writes
=================
*/
int FS_Write( const void *buffer, int len, fileHandle_t h ) {
	int block, remaining;
	int written;
	byte    *buf;
	int tries;
	FILE    *f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !h ) {
		return 0;
	}

	f = FS_FileForHandle( h );
	buf = (byte *)buffer;

	remaining = len;
	tries = 0;
	while ( remaining ) {
		block = remaining;
		written = fwrite( buf, 1, block, f );
		if ( written == 0 ) {
			if ( !tries ) {
				tries = 1;
			} else {
				Com_Printf( "FS_Write: 0 bytes written\n" );
				return 0;
			}
		}

		if ( written == -1 ) {
			Com_Printf( "FS_Write: -1 bytes written\n" );
			return 0;
		}

		remaining -= written;
		buf += written;
	}
	if ( fsh[h].handleSync ) {
		fflush( f );
	}
	return len;
}

void QDECL FS_Printf( fileHandle_t h, const char *fmt, ... ) {
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start( argptr,fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	FS_Write( msg, strlen( msg ), h );
}

#define PK3_SEEK_BUFFER_SIZE 65536

/*
=================
FS_Seek

=================
*/
int FS_Seek( fileHandle_t f, long offset, int origin ) {
	int		_origin;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
		return -1;
	}

	if (fsh[f].zipFile == qtrue) {
		//FIXME: this is really, really crappy
		//(but better than what was here before)
		byte	buffer[PK3_SEEK_BUFFER_SIZE];
		int		remainder;
		int		currentPosition = FS_FTell( f );

		// change negative offsets into FS_SEEK_SET
		if ( offset < 0 ) {
			switch( origin ) {
				case FS_SEEK_END:
					remainder = fsh[f].zipFileLen + offset;
					break;

				case FS_SEEK_CUR:
					remainder = currentPosition + offset;
					break;

				case FS_SEEK_SET:
				default:
					remainder = 0;
					break;
			}

			if ( remainder < 0 ) {
				remainder = 0;
			}

			origin = FS_SEEK_SET;
		} else {
			if ( origin == FS_SEEK_END ) {
				remainder = fsh[f].zipFileLen - currentPosition + offset;
			} else {
				remainder = offset;
			}
		}

		switch( origin ) {
			case FS_SEEK_SET:
				if ( remainder == currentPosition ) {
					return offset;
				}
				unzSetOffset(fsh[f].handleFiles.file.z, fsh[f].zipFilePos);
				unzOpenCurrentFile(fsh[f].handleFiles.file.z);
				//fallthrough

			case FS_SEEK_END:
			case FS_SEEK_CUR:
				while( remainder > PK3_SEEK_BUFFER_SIZE ) {
					FS_Read( buffer, PK3_SEEK_BUFFER_SIZE, f );
					remainder -= PK3_SEEK_BUFFER_SIZE;
				}
				FS_Read( buffer, remainder, f );
				return offset;

			default:
				Com_Error( ERR_FATAL, "Bad origin in FS_Seek" );
				return -1;
		}
	} else {
		FILE *file;
		file = FS_FileForHandle(f);
		switch( origin ) {
		case FS_SEEK_CUR:
			_origin = SEEK_CUR;
			break;
		case FS_SEEK_END:
			_origin = SEEK_END;
			break;
		case FS_SEEK_SET:
			_origin = SEEK_SET;
			break;
		default:
			Com_Error( ERR_FATAL, "Bad origin in FS_Seek" );
			break;
		}

		return fseek( file, offset, _origin );
	}
}


/*
======================================================================================

CONVENIENCE FUNCTIONS FOR ENTIRE FILES

======================================================================================
*/

int FS_FileIsInPAK( const char *filename, int *pChecksum ) {
	searchpath_t    *search;
	pack_t          *pak;
	fileInPack_t    *pakFile;
	long hash = 0;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !filename ) {
		Com_Error( ERR_FATAL, "FS_FOpenFileRead: NULL 'filename' parameter passed" );
	}

	// qpaths are not supposed to have a leading slash
	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	// make absolutely sure that it can't back up the path.
	// The searchpaths do guarantee that something will always
	// be prepended, so we don't need to worry about "c:" or "//limbo"
	if ( strstr( filename, ".." ) || strstr( filename, "::" ) ) {
		return -1;
	}

	//
	// search through the path, one element at a time
	//

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		//
		if ( search->pack ) {
			hash = FS_HashFileName( filename, search->pack->hashSize );
		}
		// is the element a pak file?
		if ( search->pack && search->pack->hashTable[hash] ) {
			// disregard if it doesn't match one of the allowed pure pak files
			if ( !FS_PakIsPure( search->pack ) ) {
				continue;
			}

			// look through all the pak file elements
			pak = search->pack;
			pakFile = pak->hashTable[hash];
			do {
				// case and separator insensitive comparisons
				if ( !FS_FilenameCompare( pakFile->name, filename ) ) {
					if ( pChecksum ) {
						*pChecksum = pak->pure_checksum;
					}
					return 1;
				}
				pakFile = pakFile->next;
			} while ( pakFile != NULL );
		}
	}
	return -1;
}

/*
============
FS_ReadFileDir

Filename are relative to the quake search path
a null buffer will just return the file length without loading
If searchPath is non-NULL search only in that specific search path
============
*/
long FS_ReadFileDir(const char *qpath, void *searchPath, qboolean unpure, void **buffer)
{
	fileHandle_t	h;
	searchpath_t	*search;
	byte*			buf;
	qboolean		isConfig;
	long				len;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !qpath || !qpath[0] ) {
		Com_Error( ERR_FATAL, "FS_ReadFile with empty name" );
	}

	buf = NULL;	// quiet compiler warning

	// if this is a .cfg file and we are playing back a journal, read
	// it from the journal file
	if ( strstr( qpath, ".cfg" ) ) {
		isConfig = qtrue;
		if ( com_journal && com_journal->integer == 2 ) {
			int		r;

			Com_DPrintf( "Loading %s from journal file.\n", qpath );
			r = FS_Read( &len, sizeof( len ), com_journalDataFile );
			if ( r != sizeof( len ) ) {
				if (buffer != NULL) *buffer = NULL;
				return -1;
			}
			// if the file didn't exist when the journal was created
			if (!len) {
				if (buffer == NULL) {
					return 1;			// hack for old journal files
				}
				*buffer = NULL;
				return -1;
			}
			if (buffer == NULL) {
				return len;
			}

			buf = Hunk_AllocateTempMemory(len+1);
			*buffer = buf;

			r = FS_Read( buf, len, com_journalDataFile );
			if ( r != len ) {
				Com_Error( ERR_FATAL, "Read from journalDataFile failed" );
			}

			fs_loadCount++;
			fs_loadStack++;

			// guarantee that it will have a trailing 0 for string operations
			buf[len] = 0;

			return len;
		}
	} else {
		isConfig = qfalse;
	}

	search = searchPath;

	if(search == NULL)
	{
		// look for it in the filesystem or pack files
		len = FS_FOpenFileRead(qpath, &h, qfalse);
        }
	else
	{
		// look for it in a specific search path only
		len = FS_FOpenFileReadDir(qpath, search, &h, qfalse, unpure);
	}

	if ( h == 0 ) {
		if ( buffer ) {
			*buffer = NULL;
		}
		// if we are journalling and it is a config file, write a zero to the journal file
		if ( isConfig && com_journal && com_journal->integer == 1 ) {
			Com_DPrintf( "Writing zero for %s to journal file.\n", qpath );
			len = 0;
			FS_Write( &len, sizeof( len ), com_journalDataFile );
			FS_Flush( com_journalDataFile );
		}
		return -1;
	}
	
	if ( !buffer ) {
		if ( isConfig && com_journal && com_journal->integer == 1 ) {
			Com_DPrintf( "Writing len for %s to journal file.\n", qpath );
			FS_Write( &len, sizeof( len ), com_journalDataFile );
			FS_Flush( com_journalDataFile );
		}
		FS_FCloseFile( h);
		return len;
	}

	fs_loadCount++;
	fs_loadStack++;

	buf = Hunk_AllocateTempMemory(len+1);
	*buffer = buf;

	FS_Read (buf, len, h);

	// guarantee that it will have a trailing 0 for string operations
	buf[len] = 0;
	FS_FCloseFile( h );

	// if we are journalling and it is a config file, write it to the journal file
	if ( isConfig && com_journal && com_journal->integer == 1 ) {
		Com_DPrintf( "Writing %s to journal file.\n", qpath );
		FS_Write( &len, sizeof( len ), com_journalDataFile );
		FS_Write( buf, len, com_journalDataFile );
		FS_Flush( com_journalDataFile );
	}
	return len;
}

/*
============
FS_ReadFile

Filename are relative to the quake search path
a null buffer will just return the file length without loading
============
*/
long FS_ReadFile(const char *qpath, void **buffer)
{
	return FS_ReadFileDir(qpath, NULL, qfalse, buffer);
}

/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile( void *buffer ) {
	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}
	if ( !buffer ) {
		Com_Error( ERR_FATAL, "FS_FreeFile( NULL )" );
	}
	fs_loadStack--;

	Hunk_FreeTempMemory( buffer );

	// if all of our temp files are free, clear all of our space
	if ( fs_loadStack == 0 ) {
		Hunk_ClearTempMemory();
	}
}

/*
============
FS_WriteFile

Filenames are relative to the quake search path
============
*/
void FS_WriteFile( const char *qpath, const void *buffer, int size ) {
	fileHandle_t f;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !qpath || !buffer ) {
		Com_Error( ERR_FATAL, "FS_WriteFile: NULL parameter" );
	}

	f = FS_FOpenFileWrite( qpath );
	if ( !f ) {
		Com_Printf( "Failed to open %s\n", qpath );
		return;
	}

	FS_Write( buffer, size, f );

	FS_FCloseFile( f );
}



/*
==========================================================================

ZIP FILE LOADING

==========================================================================
*/

/*
=================
FS_LoadZipFile

Creates a new pak_t in the search chain for the contents
of a zip file.
=================
*/
static pack_t *FS_LoadZipFile(const char *zipfile, const char *basename ) {
	fileInPack_t    *buildBuffer;
	pack_t          *pack;
	unzFile uf;
	int err;
	unz_global_info gi;
	char filename_inzip[MAX_ZPATH];
	unz_file_info file_info;
	int i, len;
	long hash;
	int fs_numHeaderLongs;
	int             *fs_headerLongs;
	char            *namePtr;

	fs_numHeaderLongs = 0;

	uf = unzOpen( zipfile );
	err = unzGetGlobalInfo( uf,&gi );

	if ( err != UNZ_OK ) {
		return NULL;
	}

	fs_packFiles += gi.number_entry;

	len = 0;
	unzGoToFirstFile( uf );
	for ( i = 0; i < gi.number_entry; i++ )
	{
		err = unzGetCurrentFileInfo( uf, &file_info, filename_inzip, sizeof( filename_inzip ), NULL, 0, NULL, 0 );
		if ( err != UNZ_OK ) {
			break;
		}
		len += strlen( filename_inzip ) + 1;
		unzGoToNextFile( uf );
	}

	buildBuffer = Z_Malloc( ( gi.number_entry * sizeof( fileInPack_t ) ) + len );
	namePtr = ( (char *) buildBuffer ) + gi.number_entry * sizeof( fileInPack_t );
	fs_headerLongs = Z_Malloc( ( gi.number_entry + 1 ) * sizeof(int) );
	fs_headerLongs[ fs_numHeaderLongs++ ] = LittleLong( fs_checksumFeed );

	// get the hash table size from the number of files in the zip
	// because lots of custom pk3 files have less than 32 or 64 files
	for ( i = 1; i <= MAX_FILEHASH_SIZE; i <<= 1 ) {
		if ( i > gi.number_entry ) {
			break;
		}
	}

	pack = Z_Malloc( sizeof( pack_t ) + i * sizeof( fileInPack_t * ) );
	pack->hashSize = i;
	pack->hashTable = ( fileInPack_t ** )( ( (char *) pack ) + sizeof( pack_t ) );
	for ( i = 0; i < pack->hashSize; i++ ) {
		pack->hashTable[i] = NULL;
	}

	Q_strncpyz( pack->pakFilename, zipfile, sizeof( pack->pakFilename ) );
	Q_strncpyz( pack->pakBasename, basename, sizeof( pack->pakBasename ) );

	// strip .pk3 if needed
	if ( strlen( pack->pakBasename ) > 4 && !Q_stricmp( pack->pakBasename + strlen( pack->pakBasename ) - 4, ".pk3" ) ) {
		pack->pakBasename[strlen( pack->pakBasename ) - 4] = 0;
	}

	pack->handle = uf;
	pack->numfiles = gi.number_entry;
	unzGoToFirstFile( uf );

	for ( i = 0; i < gi.number_entry; i++ )
	{
		err = unzGetCurrentFileInfo( uf, &file_info, filename_inzip, sizeof( filename_inzip ), NULL, 0, NULL, 0 );
		if ( err != UNZ_OK ) {
			break;
		}
		if ( file_info.uncompressed_size > 0 ) {
			fs_headerLongs[fs_numHeaderLongs++] = LittleLong( file_info.crc );
		}
		Q_strlwr( filename_inzip );
		hash = FS_HashFileName( filename_inzip, pack->hashSize );
		buildBuffer[i].name = namePtr;
		strcpy( buildBuffer[i].name, filename_inzip );
		namePtr += strlen( filename_inzip ) + 1;
		// store the file position in the zip
		buildBuffer[i].pos = unzGetOffset(uf);
		buildBuffer[i].len = file_info.uncompressed_size;
		buildBuffer[i].next = pack->hashTable[hash];
		pack->hashTable[hash] = &buildBuffer[i];
		unzGoToNextFile( uf );
	}

	pack->checksum = Com_BlockChecksum( &fs_headerLongs[ 1 ], sizeof(*fs_headerLongs) * ( fs_numHeaderLongs - 1 ) );
	pack->pure_checksum = Com_BlockChecksum( fs_headerLongs, sizeof(*fs_headerLongs) * fs_numHeaderLongs );
	pack->checksum = LittleLong( pack->checksum );
	pack->pure_checksum = LittleLong( pack->pure_checksum );

	Z_Free( fs_headerLongs );

	pack->buildBuffer = buildBuffer;
	return pack;
}

/*
=================
FS_FreePak

Frees a pak structure and releases all associated resources
=================
*/

static void FS_FreePak(pack_t *thepak)
{
	unzClose(thepak->handle);
	Z_Free(thepak->buildBuffer);
	Z_Free(thepak);
}

/*
=================
FS_GetZipChecksum

Compares whether the given pak file matches a referenced checksum
=================
*/
qboolean FS_CompareZipChecksum(const char *zipfile)
{
	pack_t *thepak;
	int index, checksum;
	
	thepak = FS_LoadZipFile(zipfile, "");
	
	if(!thepak)
		return qfalse;
	
	checksum = thepak->checksum;
	FS_FreePak(thepak);
	
	for(index = 0; index < fs_numServerReferencedPaks; index++)
	{
		if(checksum == fs_serverReferencedPaks[index])
			return qtrue;
	}
	
	return qfalse;
}

/*
=================================================================================

DIRECTORY SCANNING FUNCTIONS

=================================================================================
*/

#define MAX_FOUND_FILES 0x1000

static int FS_ReturnPath( const char *zname, char *zpath, int *depth ) {
	int len, at, newdep;

	newdep = 0;
	zpath[0] = 0;
	len = 0;
	at = 0;

	while ( zname[at] != 0 )
	{
		if ( zname[at] == '/' || zname[at] == '\\' ) {
			len = at;
			newdep++;
		}
		at++;
	}
	strcpy( zpath, zname );
	zpath[len] = 0;
	*depth = newdep;

	return len;
}

/*
==================
FS_AddFileToList
==================
*/
static int FS_AddFileToList( char *name, char *list[MAX_FOUND_FILES], int nfiles ) {
	int i;

	if ( nfiles == MAX_FOUND_FILES - 1 ) {
		return nfiles;
	}
	for ( i = 0 ; i < nfiles ; i++ ) {
		if ( !Q_stricmp( name, list[i] ) ) {
			return nfiles;      // already in list
		}
	}
	list[nfiles] = CopyString( name );
	nfiles++;

	return nfiles;
}

/*
===============
FS_ListFilteredFiles

Returns a uniqued list of files that match the given criteria
from all search paths
===============
*/
char **FS_ListFilteredFiles( const char *path, const char *extension, char *filter, int *numfiles, qboolean allowNonPureFilesOnDisk ) {
	int nfiles;
	char            **listCopy;
	char            *list[MAX_FOUND_FILES];
	searchpath_t    *search;
	int i;
	int pathLength;
	int extensionLength;
	int length, pathDepth, temp;
	pack_t          *pak;
	fileInPack_t    *buildBuffer;
	char zpath[MAX_ZPATH];

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !path ) {
		*numfiles = 0;
		return NULL;
	}
	if ( !extension ) {
		extension = "";
	}

	pathLength = strlen( path );
	if ( path[pathLength - 1] == '\\' || path[pathLength - 1] == '/' ) {
		pathLength--;
	}
	extensionLength = strlen( extension );
	nfiles = 0;
	FS_ReturnPath( path, zpath, &pathDepth );

	//
	// search through the path, one element at a time, adding to list
	//
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack ) {

			//ZOID:  If we are pure, don't search for files on paks that
			// aren't on the pure list
			if ( !FS_PakIsPure( search->pack ) ) {
				continue;
			}

			// look through all the pak file elements
			pak = search->pack;
			buildBuffer = pak->buildBuffer;
			for ( i = 0; i < pak->numfiles; i++ ) {
				char    *name;
				int zpathLen, depth;

				// check for directory match
				name = buildBuffer[i].name;
				//
				if ( filter ) {
					// case insensitive
					if ( !Com_FilterPath( filter, name, qfalse ) ) {
						continue;
					}
					// unique the match
					nfiles = FS_AddFileToList( name, list, nfiles );
				} else {

					zpathLen = FS_ReturnPath( name, zpath, &depth );

					if ( ( depth - pathDepth ) > 2 || pathLength > zpathLen || Q_stricmpn( name, path, pathLength ) ) {
						continue;
					}

					// check for extension match
					length = strlen( name );
					if ( length < extensionLength ) {
						continue;
					}

					if ( Q_stricmp( name + length - extensionLength, extension ) ) {
						continue;
					}
					// unique the match

					temp = pathLength;
					if ( pathLength ) {
						temp++;     // include the '/'
					}
					nfiles = FS_AddFileToList( name + temp, list, nfiles );
				}
			}
		} else if ( search->dir ) { // scan for files in the filesystem
			char    *netpath;
			int numSysFiles;
			char    **sysFiles;
			char    *name;

			// don't scan directories for files if we are pure or restricted
			if ( fs_numServerPaks && !allowNonPureFilesOnDisk ) {
				continue;
			} else {
				netpath = FS_BuildOSPath( search->dir->path, search->dir->gamedir, path );
				sysFiles = Sys_ListFiles( netpath, extension, filter, &numSysFiles, qfalse );
				for ( i = 0 ; i < numSysFiles ; i++ ) {
					// unique the match
					name = sysFiles[i];
					nfiles = FS_AddFileToList( name, list, nfiles );
				}
				Sys_FreeFileList( sysFiles );
			}
		}
	}

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
=================
FS_ListFiles
=================
*/
char **FS_ListFiles( const char *path, const char *extension, int *numfiles ) {
	return FS_ListFilteredFiles( path, extension, NULL, numfiles, qfalse );
}

/*
=================
FS_FreeFileList
=================
*/
void FS_FreeFileList( char **list ) {
	int i;

	if ( !fs_searchpaths ) {
		Com_Error( ERR_FATAL, "Filesystem call made without initialization" );
	}

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


/*
================
FS_GetFileList
================
*/
int FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize ) {
	int nFiles, i, nTotal, nLen;
	char **pFiles = NULL;

	*listbuf = 0;
	nFiles = 0;
	nTotal = 0;

	if ( Q_stricmp( path, "$modlist" ) == 0 ) {
		return FS_GetModList( listbuf, bufsize );
	}

	pFiles = FS_ListFiles( path, extension, &nFiles );

	for ( i = 0; i < nFiles; i++ ) {
		nLen = strlen( pFiles[i] ) + 1;
		if ( nTotal + nLen + 1 < bufsize ) {
			strcpy( listbuf, pFiles[i] );
			listbuf += nLen;
			nTotal += nLen;
		} else {
			nFiles = i;
			break;
		}
	}

	FS_FreeFileList( pFiles );

	return nFiles;
}

/*
=======================
Sys_ConcatenateFileLists

mkv: Naive implementation. Concatenates three lists into a
     new list, and frees the old lists from the heap.

FIXME TTimo those two should move to common.c next to Sys_ListFiles
=======================
 */
static unsigned int Sys_CountFileList(char **list)
{
	int i = 0;

	if (list)
	{
		while (*list)
		{
			list++;
			i++;
		}
	}
	return i;
}

static char** Sys_ConcatenateFileLists( char **list0, char **list1 )
{
	int totalLength = 0;
	char** cat = NULL, **dst, **src;

	totalLength += Sys_CountFileList(list0);
	totalLength += Sys_CountFileList(list1);

	/* Create new list. */
	dst = cat = Z_Malloc( ( totalLength + 1 ) * sizeof( char* ) );

	/* Copy over lists. */
	if (list0)
	{
		for (src = list0; *src; src++, dst++)
			*dst = *src;
	}
	if (list1)
	{
		for (src = list1; *src; src++, dst++)
			*dst = *src;
	}

	// Terminate the list
	*dst = NULL;

	// Free our old lists.
	// NOTE: not freeing their content, it's been merged in dst and still being used
	if (list0) Z_Free( list0 );
	if (list1) Z_Free( list1 );

	return cat;
}

/*
================
FS_GetModDescription
================
*/
void FS_GetModDescription( const char *modDir, char *description, int descriptionLen ) {
	fileHandle_t	descHandle;
	char			descPath[MAX_QPATH];
	int				nDescLen;
	FILE			*file;

	Com_sprintf( descPath, sizeof ( descPath ), "%s%cdescription.txt", modDir, PATH_SEP );
	nDescLen = FS_SV_FOpenFileRead( descPath, &descHandle );

	if ( nDescLen > 0 ) {
		file = FS_FileForHandle(descHandle);
		Com_Memset( description, 0, descriptionLen );
		nDescLen = fread(description, 1, descriptionLen, file);
		if (nDescLen >= 0) {
			description[nDescLen] = '\0';
		}
	} else {
		Q_strncpyz( description, modDir, descriptionLen );
	}

	if ( descHandle ) {
		FS_FCloseFile( descHandle );
	}
}

/*
================
FS_GetModList

Returns a list of mod directory names
A mod directory is a peer to baseq3 with a pk3 in it
The directories are searched in base path, cd path and home path
================
*/
int	FS_GetModList( char *listbuf, int bufsize ) {
	int		nMods, i, j, nTotal, nLen, nPaks, nPotential, nDescLen;
	char **pFiles = NULL;
	char **pPaks = NULL;
	char *name, *path;
	char description[MAX_OSPATH];

	int dummy;
	char **pFiles0 = NULL;
	char **pFiles1 = NULL;
#ifndef STANDALONE
	char **pFiles2 = NULL;
	char **pFiles3 = NULL;
	char **pFiles4 = NULL;
	char **pFiles5 = NULL;
#endif

	qboolean bDrop = qfalse;

	*listbuf = 0;
	nMods = nTotal = 0;

	pFiles0 = Sys_ListFiles( fs_homepath->string, NULL, NULL, &dummy, qtrue );
	pFiles1 = Sys_ListFiles( fs_basepath->string, NULL, NULL, &dummy, qtrue );
#ifndef STANDALONE
	pFiles2 = Sys_ListFiles( fs_steampath->string, NULL, NULL, &dummy, qtrue );
	pFiles3 = Sys_ListFiles( fs_gogpath->string, NULL, NULL, &dummy, qtrue );
#endif
	// we searched for mods in up to four paths
	// it is likely that we have duplicate names now, which we will cleanup below
#ifndef STANDALONE
	pFiles4 = Sys_ConcatenateFileLists( pFiles0, pFiles1 );
	pFiles5 = Sys_ConcatenateFileLists( pFiles2, pFiles3 );
	pFiles = Sys_ConcatenateFileLists( pFiles4, pFiles5 );
#else
	pFiles = Sys_ConcatenateFileLists( pFiles0, pFiles1 );
#endif

	nPotential = Sys_CountFileList(pFiles);

	for ( i = 0 ; i < nPotential ; i++ ) {
		name = pFiles[i];
		// NOTE: cleaner would involve more changes
		// ignore duplicate mod directories
		if (i!=0) {
			bDrop = qfalse;
			for(j=0; j<i; j++)
			{
				if (Q_stricmp(pFiles[j],name)==0) {
					// this one can be dropped
					bDrop = qtrue;
					break;
				}
			}
		}
		if (bDrop) {
			continue;
		}
		// we drop "baseq3" "." and ".."
		if (Q_stricmp(name, com_basegame->string) && Q_stricmpn(name, ".", 1)) {
			// now we need to find some .pk3 files to validate the mod
			// NOTE TTimo: (actually I'm not sure why .. what if it's a mod under developement with no .pk3?)
			// we didn't keep the information when we merged the directory names, as to what OS Path it was found under
			//   so it could be in base path, cd path or home path
			//   we will try each three of them here (yes, it's a bit messy)
			path = FS_BuildOSPath( fs_basepath->string, name, "" );
			nPaks = 0;
			pPaks = Sys_ListFiles(path, ".pk3", NULL, &nPaks, qfalse); 
			Sys_FreeFileList( pPaks ); // we only use Sys_ListFiles to check wether .pk3 files are present

			/* try on home path */
			if ( nPaks <= 0 )
			{
				path = FS_BuildOSPath( fs_homepath->string, name, "" );
				nPaks = 0;
				pPaks = Sys_ListFiles( path, ".pk3", NULL, &nPaks, qfalse );
				Sys_FreeFileList( pPaks );
			}

#ifndef STANDALONE
			/* try on steam path */
			if ( nPaks <= 0 )
			{
				path = FS_BuildOSPath( fs_steampath->string, name, "" );
				nPaks = 0;
				pPaks = Sys_ListFiles( path, ".pk3", NULL, &nPaks, qfalse );
				Sys_FreeFileList( pPaks );
			}

			/* try on gog path */
			if ( nPaks <= 0 )
			{
				path = FS_BuildOSPath( fs_gogpath->string, name, "" );
				nPaks = 0;
				pPaks = Sys_ListFiles( path, ".pk3", NULL, &nPaks, qfalse );
				Sys_FreeFileList( pPaks );
			}
#endif

			if (nPaks > 0) {
				nLen = strlen(name) + 1;
				// nLen is the length of the mod path
				// we need to see if there is a description available
				FS_GetModDescription( name, description, sizeof( description ) );
				nDescLen = strlen(description) + 1;

				if (nTotal + nLen + 1 + nDescLen + 1 < bufsize) {
					strcpy(listbuf, name);
					listbuf += nLen;
					strcpy(listbuf, description);
					listbuf += nDescLen;
					nTotal += nLen + nDescLen;
					nMods++;
				}
				else {
					break;
				}
			}
		}
	}
	Sys_FreeFileList( pFiles );

	return nMods;
}




//============================================================================

/*
================
FS_Dir_f
================
*/
void FS_Dir_f( void ) {
	char    *path;
	char    *extension;
	char    **dirnames;
	int ndirs;
	int i;

	if ( Cmd_Argc() < 2 || Cmd_Argc() > 3 ) {
		Com_Printf( "usage: dir <directory> [extension]\n" );
		return;
	}

	if ( Cmd_Argc() == 2 ) {
		path = Cmd_Argv( 1 );
		extension = "";
	} else {
		path = Cmd_Argv( 1 );
		extension = Cmd_Argv( 2 );
	}

	Com_Printf( "Directory of %s %s\n", path, extension );
	Com_Printf( "---------------\n" );

	dirnames = FS_ListFiles( path, extension, &ndirs );

	for ( i = 0; i < ndirs; i++ ) {
		Com_Printf( "%s\n", dirnames[i] );
	}
	FS_FreeFileList( dirnames );
}

/*
===========
FS_ConvertPath
===========
*/
void FS_ConvertPath( char *s ) {
	while ( *s ) {
		if ( *s == '\\' || *s == ':' ) {
			*s = '/';
		}
		s++;
	}
}

/*
===========
FS_PathCmp

Ignore case and seprator char distinctions
===========
*/
int FS_PathCmp( const char *s1, const char *s2 ) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( c1 >= 'a' && c1 <= 'z' ) {
			c1 -= ( 'a' - 'A' );
		}
		if ( c2 >= 'a' && c2 <= 'z' ) {
			c2 -= ( 'a' - 'A' );
		}

		if ( c1 == '\\' || c1 == ':' ) {
			c1 = '/';
		}
		if ( c2 == '\\' || c2 == ':' ) {
			c2 = '/';
		}

		if ( c1 < c2 ) {
			return -1;      // strings not equal
		}
		if ( c1 > c2 ) {
			return 1;
		}
	} while ( c1 );

	return 0;       // strings are equal
}

/*
================
FS_SortFileList
================
*/
void FS_SortFileList( char **filelist, int numfiles ) {
	int i, j, k, numsortedfiles;
	char **sortedlist;

	sortedlist = Z_Malloc( ( numfiles + 1 ) * sizeof( *sortedlist ) );
	sortedlist[0] = NULL;
	numsortedfiles = 0;
	for ( i = 0; i < numfiles; i++ ) {
		for ( j = 0; j < numsortedfiles; j++ ) {
			if ( FS_PathCmp( filelist[i], sortedlist[j] ) < 0 ) {
				break;
			}
		}
		for ( k = numsortedfiles; k > j; k-- ) {
			sortedlist[k] = sortedlist[k - 1];
		}
		sortedlist[j] = filelist[i];
		numsortedfiles++;
	}
	Com_Memcpy( filelist, sortedlist, numfiles * sizeof( *filelist ) );
	Z_Free( sortedlist );
}

/*
================
FS_NewDir_f
================
*/
void FS_NewDir_f( void ) {
	char    *filter;
	char    **dirnames;
	int ndirs;
	int i;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: fdir <filter>\n" );
		Com_Printf( "example: fdir *q3dm*.bsp\n" );
		return;
	}

	filter = Cmd_Argv( 1 );

	Com_Printf( "---------------\n" );

	dirnames = FS_ListFilteredFiles( "", "", filter, &ndirs, qfalse );

	FS_SortFileList( dirnames, ndirs );

	for ( i = 0; i < ndirs; i++ ) {
		FS_ConvertPath( dirnames[i] );
		Com_Printf( "%s\n", dirnames[i] );
	}
	Com_Printf( "%d files listed\n", ndirs );
	FS_FreeFileList( dirnames );
}

/*
============
FS_Path_f

============
*/
void FS_Path_f( void ) {
	searchpath_t    *s;
	int i;

	Com_Printf( "Current search path:\n" );
	for ( s = fs_searchpaths; s; s = s->next ) {
		if ( s->pack ) {
			Com_Printf( "%s (%i files)\n", s->pack->pakFilename, s->pack->numfiles );
			if ( fs_numServerPaks ) {
				if ( !FS_PakIsPure( s->pack ) ) {
					Com_Printf( "    not on the pure list\n" );
				} else {
					Com_Printf( "    on the pure list\n" );
				}
			}
		} else {
			Com_Printf ("%s%c%s\n", s->dir->path, PATH_SEP, s->dir->gamedir );
		}
	}

	Com_Printf( "\n" );
	for ( i = 1 ; i < MAX_FILE_HANDLES ; i++ ) {
		if ( fsh[i].handleFiles.file.o ) {
			Com_Printf( "handle %i: %s\n", i, fsh[i].name );
		}
	}
}

/*
============
FS_TouchFile_f
============
*/
void FS_TouchFile_f( void ) {
	fileHandle_t f;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: touchFile <file>\n" );
		return;
	}

	FS_FOpenFileRead( Cmd_Argv( 1 ), &f, qfalse );
	if ( f ) {
		FS_FCloseFile( f );
	}
}

/*
============
FS_Which
============
*/

qboolean FS_Which(const char *filename, void *searchPath)
{
	searchpath_t *search = searchPath;

	if(FS_FOpenFileReadDir(filename, search, NULL, qfalse, qfalse) > 0)
	{
		if(search->pack)
		{
			Com_Printf("File \"%s\" found in \"%s\"\n", filename, search->pack->pakFilename);
			return qtrue;
		}
		else if(search->dir)
		{
			Com_Printf( "File \"%s\" found at \"%s\"\n", filename, search->dir->fullpath);
			return qtrue;
		}
	}

	return qfalse;
}

/*
============
FS_Which_f
============
*/
void FS_Which_f( void ) {
	searchpath_t	*search;
	char		*filename;

	filename = Cmd_Argv(1);

	if ( !filename[0] ) {
		Com_Printf( "Usage: which <file>\n" );
		return;
	}

	// qpaths are not supposed to have a leading slash
	if ( filename[0] == '/' || filename[0] == '\\' ) {
		filename++;
	}

	// just wants to see if file is there
	for(search = fs_searchpaths; search; search = search->next)
	{
		if(FS_Which(filename, search))
			return;
	}

	Com_Printf("File not found: \"%s\"\n", filename);
}

//===========================================================================


static int QDECL paksort( const void *a, const void *b ) {
	char    *aa, *bb;

	aa = *(char **)a;
	bb = *(char **)b;

	return FS_PathCmp( aa, bb );
}

/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads the zip headers
================
*/
#define MAX_PAKFILES    1024
void FS_AddGameDirectory( const char *path, const char *dir, qboolean allowUnzippedDLLs ) {
	searchpath_t    *sp;
	int i;
	searchpath_t    *search;
	pack_t          *pak;
	char            curpath[MAX_OSPATH + 1], *pakfile;
	int numfiles;
	char            **pakfiles;
	char            *sorted[MAX_PAKFILES];

	// find all pak files in this directory
	Q_strncpyz(curpath, FS_BuildOSPath(path, dir, ""), sizeof(curpath));
	curpath[strlen(curpath) - 1] = '\0';	// strip the trailing slash

	// this fixes the case where fs_basepath is the same as fs_cdpath
	// which happens on full installs
	for ( sp = fs_searchpaths ; sp ; sp = sp->next ) {
		if ( sp->dir && !Q_stricmp( sp->dir->path, path ) && !Q_stricmp( sp->dir->gamedir, dir ) ) {
			return;         // we've already got this one
		}
	}

	Q_strncpyz( fs_gamedir, dir, sizeof( fs_gamedir ) );

	// find all pak files in this directory
	pakfile = FS_BuildOSPath( path, dir, "" );
	pakfile[ strlen( pakfile ) - 1 ] = 0; // strip the trailing slash

	pakfiles = Sys_ListFiles( pakfile, ".pk3", NULL, &numfiles, qfalse );

	// sort them so that later alphabetic matches override
	// earlier ones.  This makes pak1.pk3 override pak0.pk3
	if ( numfiles > MAX_PAKFILES ) {
		numfiles = MAX_PAKFILES;
	}
	for ( i = 0 ; i < numfiles ; i++ ) {
		sorted[i] = pakfiles[i];
// JPW NERVE KLUDGE: sorry, temp mod mp_* to _p_* so "mp_pak*" gets alphabetically sorted before "pak*"

		if ( !Q_strncmp( sorted[i],"mp_",3 ) ) {
			memcpy( sorted[i],"zz",2 );
		}

// jpw
	}

	qsort( sorted, numfiles, sizeof(char*), paksort );

	for ( i = 0 ; i < numfiles ; i++ ) {
		if ( Q_strncmp( sorted[i],"sp_",3 ) ) { // JPW NERVE -- exclude sp_*
// JPW NERVE KLUDGE: fix filenames broken in mp/sp/pak sort above

			if ( !Q_strncmp( sorted[i],"zz_",3 ) ) {
				memcpy( sorted[i],"mp",2 );
			}

// jpw
			pakfile = FS_BuildOSPath( path, dir, sorted[i] );
			if ( ( pak = FS_LoadZipFile( pakfile, sorted[i] ) ) == 0 ) {
				continue;
			}
			Q_strncpyz(pak->pakPathname, curpath, sizeof(pak->pakPathname));
			// store the game name for downloading
			strcpy( pak->pakGamename, dir );

			search = Z_Malloc( sizeof( searchpath_t ) );
			search->pack = pak;
			search->next = fs_searchpaths;
			fs_searchpaths = search;
		}
	}

	// done
	Sys_FreeFileList( pakfiles );

	//
	// add the directory to the search path
	//
	search = Z_Malloc (sizeof(searchpath_t));
	search->dir = Z_Malloc( sizeof( *search->dir ) );

	Q_strncpyz(search->dir->path, path, sizeof(search->dir->path));
	Q_strncpyz(search->dir->fullpath, curpath, sizeof(search->dir->fullpath));
	Q_strncpyz(search->dir->gamedir, dir, sizeof(search->dir->gamedir));
	search->dir->allowUnzippedDLLs = allowUnzippedDLLs;

	search->next = fs_searchpaths;
	fs_searchpaths = search;
}

/*
================
FS_idPak
================
*/
qboolean FS_idPak(char *pak, char *base, int numPaks) {
	int i;

	if ( !FS_FilenameCompare( pak, va( "%s/mp_bin", base ) ) ) {
		return qtrue;
	}

	for ( i = 0; i < NUM_ID_PAKS; i++ ) {
		if ( !FS_FilenameCompare( pak, va( "%s/mp_bin%d", base, i ) ) ) {
			break;
		}
		if ( !FS_FilenameCompare( pak, va( "%s/pak%d", base, i ) ) ) {
			break;
		}
// JPW NERVE -- this fn prevents external sources from downloading/overwriting official files, so exclude both SP and MP files from this list as well
		if ( !FS_FilenameCompare( pak, va( "%s/mp_pak%d",base,i ) ) ) {
			break;
		}
		if ( !FS_FilenameCompare( pak, va( "%s/sp_pak%d",base,i + 1) ) ) {
			break;
		}
// jpw
	}
	if ( i < numPaks ) {
		return qtrue;
	}
	return qfalse;
}

/*
================
FS_CheckDirTraversal

Check whether the string contains stuff like "../" to prevent directory traversal bugs
and return qtrue if it does.
================
*/

qboolean FS_CheckDirTraversal(const char *checkdir)
{
	if(strstr(checkdir, "../") || strstr(checkdir, "..\\"))
		return qtrue;
	
	return qfalse;
}

/*
================
FS_InvalidGameDir

return true if path is a reference to current directory or directory traversal
or a sub-directory
================
*/
qboolean FS_InvalidGameDir( const char *gamedir ) {
	if ( !strcmp( gamedir, "." ) || !strcmp( gamedir, ".." )
		|| strchr( gamedir, '/' ) || strchr( gamedir, '\\' ) ) {
		return qtrue;
	}

	return qfalse;
}

/*
================
FS_ComparePaks

----------------
dlstring == qtrue

Returns a list of pak files that we should download from the server. They all get stored
in the current gamedir and an FS_Restart will be fired up after we download them all.

The string is the format:

@remotename@localname [repeat]

static int		fs_numServerReferencedPaks;
static int		fs_serverReferencedPaks[MAX_SEARCH_PATHS];
static char		*fs_serverReferencedPakNames[MAX_SEARCH_PATHS];

----------------
dlstring == qfalse

we are not interested in a download string format, we want something human-readable
(this is used for diagnostics while connecting to a pure server)

================
*/
qboolean FS_ComparePaks( char *neededpaks, int len, qboolean dlstring ) {
	searchpath_t    *sp;
	qboolean havepak;
	char *origpos = neededpaks;
	int i;

	if ( !fs_numServerReferencedPaks )
		return qfalse; // Server didn't send any pack information along

	*neededpaks = 0;

	for ( i = 0 ; i < fs_numServerReferencedPaks ; i++ )
	{
		// Ok, see if we have this pak file
		havepak = qfalse;

		// never autodownload any of the id paks
		if(FS_idPak(fs_serverReferencedPakNames[i], BASEGAME, NUM_ID_PAKS))
		{
			continue;
		}

		// Make sure the server cannot make us write to non-quake3 directories.
		if(FS_CheckDirTraversal(fs_serverReferencedPakNames[i]))
		{
			Com_Printf("WARNING: Invalid download name %s\n", fs_serverReferencedPakNames[i]);
			continue;
		}

		for ( sp = fs_searchpaths ; sp ; sp = sp->next ) {
			if ( sp->pack && sp->pack->checksum == fs_serverReferencedPaks[i] ) {
				havepak = qtrue; // This is it!
				break;
			}
		}

		if ( !havepak && fs_serverReferencedPakNames[i] && *fs_serverReferencedPakNames[i] ) {
			// Don't got it

			if ( dlstring ) {
				// We need this to make sure we won't hit the end of the buffer or the server could
				// overwrite non-pk3 files on clients by writing so much crap into neededpaks that
				// Q_strcat cuts off the .pk3 extension.

				origpos += strlen(origpos);

				// Remote name
				Q_strcat( neededpaks, len, "@" );
				Q_strcat( neededpaks, len, fs_serverReferencedPakNames[i] );
				Q_strcat( neededpaks, len, ".pk3" );

				// Local name
				Q_strcat( neededpaks, len, "@" );
				// Do we have one with the same name?
				if ( FS_SV_FileExists( va( "%s.pk3", fs_serverReferencedPakNames[i] ) ) ) {
					char st[MAX_ZPATH];
					// We already have one called this, we need to download it to another name
					// Make something up with the checksum in it
					Com_sprintf( st, sizeof( st ), "%s.%08x.pk3", fs_serverReferencedPakNames[i], fs_serverReferencedPaks[i] );
					Q_strcat( neededpaks, len, st );
				} else
				{
					Q_strcat( neededpaks, len, fs_serverReferencedPakNames[i] );
					Q_strcat( neededpaks, len, ".pk3" );
				}

				// Find out whether it might have overflowed the buffer and don't add this file to the
				// list if that is the case.
				if(strlen(origpos) + (origpos - neededpaks) >= len - 1)
				{
					*origpos = '\0';
					break;
				}
			} else
			{
				Q_strcat( neededpaks, len, fs_serverReferencedPakNames[i] );
				Q_strcat( neededpaks, len, ".pk3" );
				// Do we have one with the same name?
				if ( FS_SV_FileExists( va( "%s.pk3", fs_serverReferencedPakNames[i] ) ) ) {
					Q_strcat( neededpaks, len, " (local file exists with wrong checksum)" );
				}
				Q_strcat( neededpaks, len, "\n" );
			}
		}
	}

	if ( *neededpaks ) {
		Com_Printf( "Need paks: %s\n", neededpaks );
		return qtrue;
	}

	return qfalse; // We have them all
}

/*
================
FS_Shutdown

Frees all resources.
================
*/
void FS_Shutdown( qboolean closemfp ) {
	searchpath_t    *p, *next;
	int i;

	for ( i = 0; i < MAX_FILE_HANDLES; i++ ) {
		if ( fsh[i].fileSize ) {
			FS_FCloseFile( i );
		}
	}

	// free everything
	for ( p = fs_searchpaths ; p ; p = next ) {
		next = p->next;

		if(p->pack)
			FS_FreePak(p->pack);
		if (p->dir)
			Z_Free(p->dir);

		Z_Free(p);
	}

	// any FS_ calls will now be an error until reinitialized
	fs_searchpaths = NULL;

	Cmd_RemoveCommand( "path" );
	Cmd_RemoveCommand( "dir" );
	Cmd_RemoveCommand( "fdir" );
	Cmd_RemoveCommand( "touchFile" );
	Cmd_RemoveCommand( "which" );

#ifdef FS_MISSING
	if ( closemfp ) {
		fclose( missingFiles );
	}
#endif
}

/*
================
FS_ReorderPurePaks
NOTE TTimo: the reordering that happens here is not reflected in the cvars (\cvarlist *pak*)
  this can lead to misleading situations, see show_bug.cgi?id=540
================
*/
static void FS_ReorderPurePaks( void ) {
	searchpath_t *s;
	int i;
	searchpath_t **p_insert_index, // for linked list reordering
	**p_previous;     // when doing the scan

	fs_reordered = qfalse;

	// only relevant when connected to pure server
	if ( !fs_numServerPaks ) {
		return;
	}

	p_insert_index = &fs_searchpaths; // we insert in order at the beginning of the list
	for ( i = 0 ; i < fs_numServerPaks ; i++ ) {
		p_previous = p_insert_index; // track the pointer-to-current-item
		for ( s = *p_insert_index; s; s = s->next ) { // the part of the list before p_insert_index has been sorted already
			if ( s->pack && fs_serverPaks[i] == s->pack->checksum ) {
				fs_reordered = qtrue;
				// move this element to the insert list
				*p_previous = s->next;
				s->next = *p_insert_index;
				*p_insert_index = s;
				// increment insert list
				p_insert_index = &s->next;
				break; // iterate to next server pack
			}
			p_previous = &s->next;
		}
	}

}

/*
================
FS_Startup
================
*/
static void FS_Startup( const char *gameName ) {
	const char *homePath;

	Com_Printf( "----- FS_Startup -----\n" );

	fs_packFiles = 0;

	fs_debug = Cvar_Get( "fs_debug", "0", 0 );
	fs_basepath = Cvar_Get ("fs_basepath", Sys_DefaultInstallPath(), CVAR_INIT|CVAR_PROTECTED );
	fs_basegame = Cvar_Get( "fs_basegame", "", CVAR_INIT );
	homePath = Sys_DefaultHomePath();
	if (!homePath || !homePath[0]) {
		homePath = fs_basepath->string;
	}
	fs_homepath = Cvar_Get ("fs_homepath", homePath, CVAR_INIT|CVAR_PROTECTED );
	fs_gamedirvar = Cvar_Get( "fs_game", "", CVAR_INIT | CVAR_SYSTEMINFO );

	if (!gameName[0]) {
		Cvar_ForceReset( "com_basegame" );
	}

	if (!FS_FilenameCompare(fs_gamedirvar->string, gameName)) {
		// This is the standard base game. Servers and clients should
		// use "" and not the standard basegame name because this messes
		// up pak file negotiation and lots of other stuff
		Cvar_ForceReset( "fs_game" );
	}

	if (FS_InvalidGameDir(gameName)) {
		Com_Error( ERR_DROP, "Invalid com_basegame '%s'", gameName );
	}
	if (FS_InvalidGameDir(fs_basegame->string)) {
		Com_Error( ERR_DROP, "Invalid fs_basegame '%s'", fs_basegame->string );
	}
	if (FS_InvalidGameDir(fs_gamedirvar->string)) {
		Com_Error( ERR_DROP, "Invalid fs_game '%s'", fs_gamedirvar->string );
	}

	// add search path elements in reverse priority order
#ifndef STANDALONE
	fs_gogpath = Cvar_Get ("fs_gogpath", Sys_GogPath(), CVAR_INIT|CVAR_PROTECTED );
	if (fs_gogpath->string[0]) {
		FS_AddGameDirectory( fs_gogpath->string, gameName, qtrue );
	}

	fs_steampath = Cvar_Get ("fs_steampath", Sys_SteamPath(), CVAR_INIT|CVAR_PROTECTED );
	if (fs_steampath->string[0]) {
		FS_AddGameDirectory( fs_steampath->string, gameName, qtrue );
	}
#endif

	if ( fs_basepath->string[0] ) {
		FS_AddGameDirectory( fs_basepath->string, gameName, qtrue );
	}

#ifdef __APPLE__
	fs_apppath = Cvar_Get ("fs_apppath", Sys_DefaultAppPath(), CVAR_INIT|CVAR_PROTECTED );
	// Make MacOSX also include the base path included with the .app bundle
	if (fs_apppath->string[0])
		FS_AddGameDirectory(fs_apppath->string, gameName, qtrue);
#endif
	
	// NOTE: same filtering below for mods and basegame
	if (fs_homepath->string[0] && Q_stricmp(fs_homepath->string,fs_basepath->string)) {
		FS_CreatePath ( fs_homepath->string );
		FS_AddGameDirectory( fs_homepath->string, gameName, qtrue );
	}

	// check for additional base game so mods can be based upon other mods
	if ( fs_basegame->string[0] && Q_stricmp( fs_basegame->string, gameName ) ) {
#ifndef STANDALONE
		if (fs_gogpath->string[0]) {
			FS_AddGameDirectory( fs_gogpath->string, fs_basegame->string, qtrue );
		}

		if ( fs_steampath->string[0] ) {
			FS_AddGameDirectory( fs_steampath->string, fs_basegame->string, qtrue );
		}
#endif

		if ( fs_basepath->string[0] ) {
			FS_AddGameDirectory( fs_basepath->string, fs_basegame->string, qtrue );
		}

		if ( fs_homepath->string[0] && Q_stricmp( fs_homepath->string,fs_basepath->string ) ) {
			FS_AddGameDirectory( fs_homepath->string, fs_basegame->string, qtrue );
		}
	}

	// check for additional game folder for mods
	if ( fs_gamedirvar->string[0] && Q_stricmp( fs_gamedirvar->string, gameName ) ) {
#ifndef STANDALONE
		if (fs_gogpath->string[0]) {
			FS_AddGameDirectory( fs_gogpath->string, fs_gamedirvar->string, qtrue );
		}

		if ( fs_steampath->string[0] ) {
			FS_AddGameDirectory( fs_steampath->string, fs_gamedirvar->string, qtrue );
		}
#endif

		if ( fs_basepath->string[0] ) {
			FS_AddGameDirectory( fs_basepath->string, fs_gamedirvar->string, qtrue );
		}

		if ( fs_homepath->string[0] && Q_stricmp( fs_homepath->string,fs_basepath->string ) ) {
			FS_AddGameDirectory( fs_homepath->string, fs_gamedirvar->string, qtrue );
		}
	}

#ifndef STANDALONE
	if (!com_standalone->integer) {
		Com_ReadCDKey(BASEGAME);
		if (fs_gamedirvar->string[0]) {
			Com_AppendCDKey(fs_gamedirvar->string);
		}
	}
#endif

	// add our commands
	Cmd_AddCommand( "path", FS_Path_f );
	Cmd_AddCommand( "dir", FS_Dir_f );
	Cmd_AddCommand( "fdir", FS_NewDir_f );
	Cmd_AddCommand( "touchFile", FS_TouchFile_f );
	Cmd_AddCommand ("which", FS_Which_f );

	// show_bug.cgi?id=506
	// reorder the pure pk3 files according to server order
	FS_ReorderPurePaks();

	// print the current search paths
	FS_Path_f();

	fs_gamedirvar->modified = qfalse; // We just loaded, it's not modified

	Com_Printf( "----------------------\n" );

#ifdef FS_MISSING
	if ( missingFiles == NULL ) {
		missingFiles = Sys_FOpen( "\\missing.txt", "ab" );
	}
#endif
	Com_Printf( "%d files in pk3 files\n", fs_packFiles );
}

#ifndef STANDALONE
/*
===================
FS_CheckMPPaks

Check whether any of the original id pak files is present,
and start up in standalone mode, if there are none and a
different com_basegame was set.
Note: If you're building a game that doesn't depend on the
RTCW media mp_pak0.pk3, you'll want to remove this by defining
STANDALONE in q_shared.h
===================
*/
static void FS_CheckMPPaks( void )
{
	searchpath_t	*path;
	pack_t		*curpack;
	const char	*pakBasename;
	unsigned int foundPak = 0;

	for( path = fs_searchpaths; path; path = path->next )
	{
		if(!path->pack)
			continue;
		
		curpack = path->pack;
		pakBasename = curpack->pakBasename;

		if(!Q_stricmpn( curpack->pakGamename, BASEGAME, MAX_OSPATH )
			&& strlen(pakBasename) == 7 && !Q_stricmpn( pakBasename, "mp_pak", 6 )
			&& pakBasename[6] >= '0' && pakBasename[6] <= '0' + NUM_MP_PAKS - 1)
		{
			if( curpack->checksum != mppak_checksums[pakBasename[6]-'0'] )
			{
				if(pakBasename[6] == '0')
				{
					Com_Printf("\n\n"
						"**************************************************\n"
						"WARNING: " BASEGAME "/mp_pak0.pk3 is present but its checksum (%u)\n"
						"is not correct. Please re-copy mp_pak0.pk3 from your\n"
						"legitimate RTCW CDROM.\n"
						"**************************************************\n\n\n",
						curpack->checksum );
				}
				else
				{
					Com_Printf("\n\n"
						"**************************************************\n"
						"WARNING: " BASEGAME "/mp_pak%d.pk3 is present but its checksum (%u)\n"
						"is not correct. Please re-install the point release\n"
						"**************************************************\n\n\n",
						pakBasename[6]-'0', curpack->checksum );
				}
			}

			foundPak |= 1<<(pakBasename[6]-'0');
		}
		else
		{
			int index;
			
			// Finally check whether this pak's checksum is listed because the user tried
			// to trick us by renaming the file, and set foundPak's highest bit to indicate this case.
			
			for(index = 0; index < ARRAY_LEN(mppak_checksums); index++)
			{
				if(curpack->checksum == mppak_checksums[index])
				{
					Com_Printf("\n\n"
						"**************************************************\n"
						"WARNING: %s is renamed pak file %s%cmp_pak%d.pk3\n"
						"Running in standalone mode won't work\n"
						"Please rename, or remove this file\n"
						"**************************************************\n\n\n",
						curpack->pakFilename, BASEGAME, PATH_SEP, index);


					foundPak |= 0x80000000;
				}
			}

		}
	}

	if(!foundPak && Q_stricmp(com_basegame->string, BASEGAME))
	{
		Cvar_Set("com_standalone", "1");
	}
	else
		Cvar_Set("com_standalone", "0");


	if(!com_standalone->integer && (foundPak & 0x3f) != 0x3f)
	{
		char errorText[MAX_STRING_CHARS] = "";
		char missingPaks[MAX_STRING_CHARS] = "";
		int i = 0;

		if((foundPak & 0x3f) != 0x3f)
		{
			for( i = 0; i < NUM_MP_PAKS; i++ ) {
				if ( !( foundPak & ( 1 << i ) ) ) {
					Q_strcat( missingPaks, sizeof( missingPaks ), va( "mp_pak%d.pk3 ", i ) );
				}
			}

			Q_strcat( errorText, sizeof( errorText ),
				va( "\n\nPoint Release files are missing: %s \n"
				"Please re-install the 1.41 point release.\n\n", missingPaks ) );
		}

		Com_Error(ERR_FATAL, "%s", errorText);
	}
}


/*
===================
FS_CheckPak0

Check whether any of the original id pak files is present,
and start up in standalone mode, if there are none and a
different com_basegame was set.
Note: If you're building a game that doesn't depend on the
RTCW media pak0.pk3, you'll want to remove this by defining
STANDALONE in q_shared.h
===================
*/
static void FS_CheckPak0( void )
{
	searchpath_t	*path;
	pack_t		*curpack;
	const char	*pakBasename;
	qboolean founddemo = qfalse;
	unsigned int foundPak = 0;

	for( path = fs_searchpaths; path; path = path->next )
	{
		if(!path->pack)
			continue;
		
		curpack = path->pack;
		pakBasename = curpack->pakBasename;

		if(!Q_stricmpn( curpack->pakGamename, "demomain", MAX_OSPATH )
			&& !Q_stricmpn( pakBasename, "pak0", MAX_OSPATH ))
		{
			if(curpack->checksum == DEMO_PAK0_CHECKSUM)
				founddemo = qtrue;
		}

		else if(!Q_stricmpn( curpack->pakGamename, BASEGAME, MAX_OSPATH )
			&& strlen(pakBasename) == 4 && !Q_stricmpn( pakBasename, "pak", 3 )
			&& pakBasename[3] >= '0' && pakBasename[3] <= '0' + NUM_ID_PAKS - 1)
		{
			if( curpack->checksum != pak_checksums[pakBasename[3]-'0'] )
			{
				if(pakBasename[3] == '0')
				{
					Com_Printf("\n\n"
						"**************************************************\n"
						"WARNING: " BASEGAME "/pak0.pk3 is present but its checksum (%u)\n"
						"is not correct. Please re-copy pak0.pk3 from your\n"
						"legitimate RTCW CDROM.\n"
						"**************************************************\n\n\n",
						curpack->checksum );

						Com_Error(ERR_FATAL, NULL);
				}
				/*
				else
				{
					Com_Printf("\n\n"
						"**************************************************\n"
						"WARNING: " BASEGAME "/pak%d.pk3 is present but its checksum (%u)\n"
						"is not correct. Please re-install the point release\n"
						"**************************************************\n\n\n",
						pakBasename[3]-'0', curpack->checksum );
				}
				*/
			}

			foundPak |= 1<<(pakBasename[3]-'0');
		}
		else
		{
			int index;
			
			// Finally check whether this pak's checksum is listed because the user tried
			// to trick us by renaming the file, and set foundPak's highest bit to indicate this case.
			
			for(index = 0; index < ARRAY_LEN(pak_checksums); index++)
			{
				if(curpack->checksum == pak_checksums[index])
				{
					Com_Printf("\n\n"
						"**************************************************\n"
						"WARNING: %s is renamed pak file %s%cpak%d.pk3\n"
						"Running in standalone mode won't work\n"
						"Please rename, or remove this file\n"
						"**************************************************\n\n\n",
						curpack->pakFilename, BASEGAME, PATH_SEP, index);


					foundPak |= 0x80000000;
				}
			}
		}
	}

	if(!foundPak && Q_stricmp(com_basegame->string, BASEGAME))
	{
		Cvar_Set("com_standalone", "1");
	}
	else
		Cvar_Set("com_standalone", "0");

	if(!com_standalone->integer)
	{
		if(!(foundPak & 0x01))
		{
			if(founddemo)
			{
				Com_Printf( "\n\n"
					"**************************************************\n"
					"WARNING: It looks like you're using pak0.pk3\n"
					"from the demo. This may work fine, but it is not\n"
					"guaranteed or supported.\n"
					"**************************************************\n\n\n" );
				
				foundPak |= 0x01;
			}
		}
	}


	if(!com_standalone->integer && (foundPak & 0x01) != 0x01)
	{
		char errorText[MAX_STRING_CHARS] = "";

		if((foundPak & 0x01) != 0x01)
		{
			Q_strcat(errorText, sizeof(errorText),
				"\n\n\"pak0.pk3\" is missing. Please copy it\n"
				"from your legitimate RTCW CDROM.\n\n");
		}

		Q_strcat(errorText, sizeof(errorText),
			va("Also check that your iortcw executable is in\n"
				"the correct place and that every file\n"
				"in the \"%s\" directory is present and readable.\n\n", BASEGAME));

		Com_Error(ERR_FATAL, "%s", errorText);
	}

	if(!founddemo)
		FS_CheckMPPaks();
	
}
#endif

/*
=====================
FS_LoadedPakChecksums

Returns a space separated string containing the checksums of all loaded pk3 files.
Servers with sv_pure set will get this string and pass it to clients.
=====================
*/
const char *FS_LoadedPakChecksums( void ) {
	static char info[BIG_INFO_STRING];
	searchpath_t    *search;

	info[0] = 0;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( !search->pack ) {
			continue;
		}

		Q_strcat( info, sizeof( info ), va( "%i ", search->pack->checksum ) );
	}

	return info;
}

/*
=====================
FS_LoadedPakNames

Returns a space separated string containing the names of all loaded pk3 files.
Servers with sv_pure set will get this string and pass it to clients.
=====================
*/
const char *FS_LoadedPakNames( void ) {
	static char info[BIG_INFO_STRING];
	searchpath_t    *search;

	info[0] = 0;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( !search->pack ) {
			continue;
		}

		if ( *info ) {
			Q_strcat( info, sizeof( info ), " " );
		}
		Q_strcat( info, sizeof( info ), search->pack->pakBasename );
	}

	return info;
}

/*
=====================
FS_LoadedPakPureChecksums

Returns a space separated string containing the pure checksums of all loaded pk3 files.
Servers with sv_pure use these checksums to compare with the checksums the clients send
back to the server.
=====================
*/
const char *FS_LoadedPakPureChecksums( void ) {
	static char info[BIG_INFO_STRING];
	searchpath_t    *search;

	info[0] = 0;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( !search->pack ) {
			continue;
		}

		Q_strcat( info, sizeof( info ), va( "%i ", search->pack->pure_checksum ) );
	}

	return info;
}

/*
=====================
FS_ReferencedPakChecksums

Returns a space separated string containing the checksums of all referenced pk3 files.
The server will send this to the clients so they can check which files should be auto-downloaded.
=====================
*/
const char *FS_ReferencedPakChecksums( void ) {
	static char info[BIG_INFO_STRING];
	searchpath_t *search;

	info[0] = 0;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack ) {
			if (search->pack->referenced || Q_stricmpn(search->pack->pakGamename, com_basegame->string, strlen(com_basegame->string))) {
				Q_strcat( info, sizeof( info ), va( "%i ", search->pack->checksum ) );
			}
		}
	}

	return info;
}

/*
=====================
FS_ReferencedPakNames

Returns a space separated string containing the names of all referenced pk3 files.
The server will send this to the clients so they can check which files should be auto-downloaded.
=====================
*/
const char *FS_ReferencedPakNames( void ) {
	static char info[BIG_INFO_STRING];
	searchpath_t    *search;

	info[0] = 0;

	// we want to return ALL pk3's from the fs_game path
	// and referenced one's from baseq3
	for ( search = fs_searchpaths ; search ; search = search->next ) {
		// is the element a pak file?
		if ( search->pack ) {
			if (search->pack->referenced || Q_stricmpn(search->pack->pakGamename, com_basegame->string, strlen(com_basegame->string))) {
				if (*info) {
					Q_strcat(info, sizeof( info ), " " );
				}
				Q_strcat( info, sizeof( info ), search->pack->pakGamename );
				Q_strcat( info, sizeof( info ), "/" );
				Q_strcat( info, sizeof( info ), search->pack->pakBasename );
			}
		}
	}

	return info;
}


/*
=====================
FS_ReferencedPakPureChecksums

Returns a space separated string containing the pure checksums of all referenced pk3 files.
Servers with sv_pure set will get this string back from clients for pure validation

The string has a specific order, "cgame ui @ ref1 ref2 ref3 ..."
=====================
*/
const char *FS_ReferencedPakPureChecksums( void ) {
	static char info[BIG_INFO_STRING];
	searchpath_t    *search;
	int nFlags, numPaks, checksum;

	info[0] = 0;

	checksum = fs_checksumFeed;

	numPaks = 0;
	for ( nFlags = FS_CGAME_REF; nFlags; nFlags = nFlags >> 1 ) {
		if ( nFlags & FS_GENERAL_REF ) {
			// add a delimter between must haves and general refs
			//Q_strcat(info, sizeof(info), "@ ");
			info[strlen( info ) + 1] = '\0';
			info[strlen( info ) + 2] = '\0';
			info[strlen( info )] = '@';
			info[strlen( info )] = ' ';
		}
		for ( search = fs_searchpaths ; search ; search = search->next ) {
			// is the element a pak file and has it been referenced based on flag?
			if ( search->pack && ( search->pack->referenced & nFlags ) ) {
				Q_strcat( info, sizeof( info ), va( "%i ", search->pack->pure_checksum ) );
				if ( nFlags & ( FS_CGAME_REF | FS_UI_REF ) ) {
					break;
				}
				checksum ^= search->pack->pure_checksum;
				numPaks++;
			}
		}
	}
	// last checksum is the encoded number of referenced pk3s
	checksum ^= numPaks;
	Q_strcat( info, sizeof( info ), va( "%i ", checksum ) );

	return info;
}


/*
=====================
FS_ClearPakReferences
=====================
*/
void FS_ClearPakReferences( int flags ) {
	searchpath_t *search;

	if ( !flags ) {
		flags = -1;
	}
	for ( search = fs_searchpaths; search; search = search->next ) {
		// is the element a pak file and has it been referenced?
		if ( search->pack ) {
			search->pack->referenced &= ~flags;
		}
	}
}


/*
=====================
FS_PureServerSetLoadedPaks

If the string is empty, all data sources will be allowed.
If not empty, only pk3 files that match one of the space
separated checksums will be checked for files, with the
exception of .cfg and .dat files.
=====================
*/
void FS_PureServerSetLoadedPaks( const char *pakSums, const char *pakNames ) {
	int i, c, d;

	Cmd_TokenizeString( pakSums );

	c = Cmd_Argc();
	if ( c > MAX_SEARCH_PATHS ) {
		c = MAX_SEARCH_PATHS;
	}

	fs_numServerPaks = c;

	for ( i = 0 ; i < c ; i++ ) {
		fs_serverPaks[i] = atoi( Cmd_Argv( i ) );
	}

	if ( fs_numServerPaks ) {
		Com_DPrintf( "Connected to a pure server.\n" );
	} else
	{
		if ( fs_reordered ) {
			// show_bug.cgi?id=540
			// force a restart to make sure the search order will be correct
			Com_DPrintf( "FS search reorder is required\n" );
			FS_Restart( fs_checksumFeed );
			return;
		}
	}

	for ( i = 0 ; i < c ; i++ ) {
		if ( fs_serverPakNames[i] ) {
			Z_Free( fs_serverPakNames[i] );
		}
		fs_serverPakNames[i] = NULL;
	}
	if ( pakNames && *pakNames ) {
		Cmd_TokenizeString( pakNames );

		d = Cmd_Argc();
		if ( d > MAX_SEARCH_PATHS ) {
			d = MAX_SEARCH_PATHS;
		}

		for ( i = 0 ; i < d ; i++ ) {
			fs_serverPakNames[i] = CopyString( Cmd_Argv( i ) );
		}
	}
}

/*
=====================
FS_PureServerSetReferencedPaks

The checksums and names of the pk3 files referenced at the server
are sent to the client and stored here. The client will use these
checksums to see if any pk3 files need to be auto-downloaded.
=====================
*/
void FS_PureServerSetReferencedPaks( const char *pakSums, const char *pakNames ) {
	int i, c, d = 0;

	Cmd_TokenizeString( pakSums );

	c = Cmd_Argc();
	if ( c > MAX_SEARCH_PATHS ) {
		c = MAX_SEARCH_PATHS;
	}

	for ( i = 0 ; i < c ; i++ ) {
		fs_serverReferencedPaks[i] = atoi( Cmd_Argv( i ) );
	}

	for (i = 0 ; i < ARRAY_LEN(fs_serverReferencedPakNames); i++)
	{
		if(fs_serverReferencedPakNames[i])
			Z_Free( fs_serverReferencedPakNames[i] );

		fs_serverReferencedPakNames[i] = NULL;
	}
	if ( pakNames && *pakNames ) {
		Cmd_TokenizeString( pakNames );

		d = Cmd_Argc();

		if(d > c)
			d = c;

		for ( i = 0 ; i < d ; i++ ) {
			fs_serverReferencedPakNames[i] = CopyString( Cmd_Argv( i ) );
		}
	}

	// ensure that there are as many checksums as there are pak names.
	if(d < c)
		c = d;
	
	fs_numServerReferencedPaks = c;
}

/*
================
FS_InitFilesystem

Called only at inital startup, not when the filesystem
is resetting due to a game change
================
*/
void FS_InitFilesystem( void ) {
	// allow command line parms to override our defaults
	// we have to specially handle this, because normal command
	// line variable sets don't happen until after the filesystem
	// has already been initialized
	Com_StartupVariable("fs_basepath");
	Com_StartupVariable("fs_homepath");
	Com_StartupVariable("fs_game");

	if(!FS_FilenameCompare(Cvar_VariableString("fs_game"), com_basegame->string))
		Cvar_Set("fs_game", "");

	// try to start up normally
	FS_Startup(com_basegame->string);

#ifndef STANDALONE
#ifndef UPDATE_SERVER
	FS_CheckPak0( );
#endif
#endif

#ifndef UPDATE_SERVER
	// if we can't find default.cfg, assume that the paths are
	// busted and error out now, rather than getting an unreadable
	// graphics screen when the font fails to load
	if ( FS_ReadFile( "default.cfg", NULL ) <= 0 ) {
		// TTimo - added some verbosity, 'couldn't load default.cfg' confuses the hell out of users
		Com_Error( ERR_FATAL, "Couldn't load default.cfg - I am missing essential files - verify your installation?" );
	}
#endif

	Q_strncpyz( lastValidBase, fs_basepath->string, sizeof( lastValidBase ) );
	Q_strncpyz( lastValidComBaseGame, com_basegame->string, sizeof( lastValidComBaseGame ) );
	Q_strncpyz( lastValidFsBaseGame, fs_basegame->string, sizeof( lastValidFsBaseGame ) );
	Q_strncpyz( lastValidGame, fs_gamedirvar->string, sizeof( lastValidGame ) );
}


/*
================
FS_Restart
================
*/
void FS_Restart( int checksumFeed ) {
	const char *lastGameDir;

	// free anything we currently have loaded
	FS_Shutdown( qfalse );

	// set the checksum feed
	fs_checksumFeed = checksumFeed;

	// clear pak references
	FS_ClearPakReferences( 0 );

	// try to start up normally
	FS_Startup(com_basegame->string);

#ifndef STANDALONE
	FS_CheckPak0( );
#endif

	// if we can't find default.cfg, assume that the paths are
	// busted and error out now, rather than getting an unreadable
	// graphics screen when the font fails to load
	if ( FS_ReadFile( "default.cfg", NULL ) <= 0 ) {
		// this might happen when connecting to a pure server not using BASEGAME/pak0.pk3
		// (for instance a TA demo server)
		if ( lastValidBase[0] ) {
			FS_PureServerSetLoadedPaks( "", "" );
			Cvar_Set( "fs_basepath", lastValidBase );
			Cvar_Set( "com_basegame", lastValidComBaseGame );
			Cvar_Set( "fs_basegame", lastValidFsBaseGame );
			Cvar_Set( "fs_game", lastValidGame );
			lastValidBase[0] = '\0';
			lastValidComBaseGame[0] = '\0';
			lastValidFsBaseGame[0] = '\0';
			lastValidGame[0] = '\0';
			FS_Restart( checksumFeed );
			Com_Error( ERR_DROP, "Invalid game folder" );
			return;
		}
		Com_Error( ERR_FATAL, "Couldn't load default.cfg" );
	}

	lastGameDir = ( lastValidGame[0] ) ? lastValidGame : lastValidComBaseGame;

	if ( Q_stricmp( FS_GetCurrentGameDir(), lastGameDir ) ) {
		Sys_RemovePIDFile( lastGameDir );
		Sys_InitPIDFile( FS_GetCurrentGameDir() );

		// skip the wolfconfig.cfg if "safe" is on the command line
		if ( !Com_SafeMode() ) {
			Cbuf_AddText("exec " Q3CONFIG_CFG "\n");
		}
	}

	Q_strncpyz( lastValidBase, fs_basepath->string, sizeof( lastValidBase ) );
	Q_strncpyz( lastValidComBaseGame, com_basegame->string, sizeof( lastValidComBaseGame ) );
	Q_strncpyz( lastValidFsBaseGame, fs_basegame->string, sizeof( lastValidFsBaseGame ) );
	Q_strncpyz( lastValidGame, fs_gamedirvar->string, sizeof( lastValidGame ) );

}

/*
=================
FS_ConditionalRestart

Restart if necessary
Return qtrue if restarting due to game directory changed, qfalse otherwise
=================
*/
qboolean FS_ConditionalRestart(int checksumFeed, qboolean disconnect)
{
	if(fs_gamedirvar->modified)
	{
		if(FS_FilenameCompare(lastValidGame, fs_gamedirvar->string) &&
		   (*lastValidGame || FS_FilenameCompare(fs_gamedirvar->string, com_basegame->string)) &&
		   (*fs_gamedirvar->string || FS_FilenameCompare(lastValidGame, com_basegame->string)))
		{
			Com_GameRestart(checksumFeed, disconnect);
			return qtrue;
		}
		else
			fs_gamedirvar->modified = qfalse;
	}
	
	if(checksumFeed != fs_checksumFeed)
		FS_Restart(checksumFeed);
	else if(fs_numServerPaks && !fs_reordered)
		FS_ReorderPurePaks();
	
	return qfalse;
}

/*
========================================================================================

Handle based file calls for virtual machines

========================================================================================
*/

int	FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode ) {
	int r;
	qboolean sync;

	sync = qfalse;

	switch( mode ) {
		case FS_READ:
			r = FS_FOpenFileRead( qpath, f, qtrue );
			break;
		case FS_WRITE:
			*f = FS_FOpenFileWrite( qpath );
			r = 0;
			if (*f == 0) {
				r = -1;
			}
			break;
		case FS_APPEND_SYNC:
			sync = qtrue;
		case FS_APPEND:
			*f = FS_FOpenFileAppend( qpath );
			r = 0;
			if (*f == 0) {
				r = -1;
			}
			break;
		default:
			Com_Error( ERR_FATAL, "FS_FOpenFileByMode: bad mode" );
			return -1;
	}

	if (!f) {
		return r;
	}

	if ( *f ) {
		fsh[*f].fileSize = r;
	}
	fsh[*f].handleSync = sync;

	return r;
}

int     FS_FTell( fileHandle_t f ) {
	int pos;
	if ( fsh[f].zipFile == qtrue ) {
		pos = unztell( fsh[f].handleFiles.file.z );
	} else {
		pos = ftell( fsh[f].handleFiles.file.o );
	}
	return pos;
}

void    FS_Flush( fileHandle_t f ) {
	fflush( fsh[f].handleFiles.file.o );
}

void	FS_FilenameCompletion( const char *dir, const char *ext,
		qboolean stripExt, void(*callback)(const char *s), qboolean allowNonPureFilesOnDisk ) {
	char	**filenames;
	int		nfiles;
	int		i;
	char	filename[ MAX_STRING_CHARS ];

	filenames = FS_ListFilteredFiles( dir, ext, NULL, &nfiles, allowNonPureFilesOnDisk );

	FS_SortFileList( filenames, nfiles );

	for( i = 0; i < nfiles; i++ ) {
		FS_ConvertPath( filenames[ i ] );
		Q_strncpyz( filename, filenames[ i ], MAX_STRING_CHARS );

		if( stripExt ) {
			COM_StripExtension(filename, filename, sizeof(filename));
		}

		callback( filename );
	}
	FS_FreeFileList( filenames );
}

const char *FS_GetCurrentGameDir(void)
{
	if(fs_gamedirvar->string[0])
		return fs_gamedirvar->string;

	return com_basegame->string;
}

// CVE-2006-2082
// compared requested pak against the names as we built them in FS_ReferencedPakNames
qboolean FS_VerifyPak( const char *pak ) {
	char teststring[ BIG_INFO_STRING ];
	searchpath_t    *search;

	for ( search = fs_searchpaths ; search ; search = search->next ) {
		if ( search->pack ) {
			Q_strncpyz( teststring, search->pack->pakGamename, sizeof( teststring ) );
			Q_strcat( teststring, sizeof( teststring ), "/" );
			Q_strcat( teststring, sizeof( teststring ), search->pack->pakBasename );
			Q_strcat( teststring, sizeof( teststring ), ".pk3" );
			if ( !Q_stricmp( teststring, pak ) ) {
				return qtrue;
			}
		}
	}
	return qfalse;
}


