 ![iortcw logo](https://raw.githubusercontent.com/iortcw/iortcw/master/MP/misc/wolf128.png)                       
                               

## iortcw

The intent of this project is to provide a baseline RTCW which may be used
for further development and fun. 
Some of the major features currently implemented are:

  * SDL backend
  * OpenAL sound API support (multiple speaker support and better sound
    quality)
  * Full x86_64 support
  * VoIP support, both in-game and external support through Mumble.
  * MinGW compilation support on Windows and cross compilation support on Linux
  * AVI video capture of demos
  * Much improved console autocompletion
  * Persistent console history
  * Colorized terminal output
  * Optional Ogg Vorbis support
  * Much improved QVM tools
  * Support for various esoteric operating systems
  * cl_guid support
  * HTTP/FTP download redirection (using cURL)
  * Multiuser support on Windows systems (user specific game data
    is stored in "My Documents\RTCW")
  * PNG support
  * Many, many bug fixes

The map editor and associated compiling tools are not included. We suggest you use a modern copy from http://icculus.org/gtkradiant/.

The original id software readme that accompanied the RTCW source release is named README.txt and is contained within the source tree of both MP and SP games.

### Quick Start Guide

  1. If you have not already done so, install Return to Castle Wolfenstein and remember the target installation directory.
  2. Browse to the iortcw project release folder: https://github.com/iortcw/iortcw/releases. 
  3. Download the latest release file for your operating system, as well as patch-data-141.zip.  As of this writing, the latest version of the iortcw release file is v1.51c.
  4. Extract the latest release zip into a location where you would like to have your installation going forward (For example: c:\Games\iortcw\ in Windows or /home/joe/Games/iortcw/ in Linux).
  5. Go to the location of your original existing installation, open the “Main” folder, and copy the following files over to your iortcw's  “Main” folder for the single player campaign: **pak0.pk3, sp_pak1.pk3, sp_pak2.pk3, and sp_pak3.pk3**. For multiplayer, copy the following files to your iortcw's “Main” folder: **mp_bin.pk3, mp_pak0.pk3, mp_pak1.pk3, mp_pak2.pk3, mp_pak3.pk3, mp_pak4.pk3, mp_pak5.pk3, mp_pakmaps0.pk3, mp_pakmaps1.pk3, mp_pakmaps2.pk3, mp_pakmaps3.pk3, mp_pakmaps4.pk3, mp_pakmaps5.pk3, and mp_pakmaps6.pk3**.  
  6. Extract the contents of patch-data-141.zip (or a newer version in the meantime) into your iortcw folder and merge patch-data-141's content into your iortcw folder.  For German, Spanish, French or Italian language support in-game, also extract the contents of one of the respective "patch-data-SP-language" zip files into your iortcw's "Main" folder.
  7. Go to your iortcw installation folder and start either the “iowolfsp*” file for single player or “iowolfmp*” for multiplayer.  If your system uses a 64-bit processor, run the single player or multiplayer file that ends with **x64**. 

### Compilation and installation

##### For *nix
  1. Change to the directory containing this readme.
  2. Run 'make'.

##### For Windows,
  1. Please refer to the HOWTO-Build.txt file contained within this repository.

##### For Mac OS X, building a Universal Binary
  1. Install MacOSX SDK packages from XCode.  For maximum compatibility, use XCode 3.2.6 on 10.6 Snow Leopard with MacOSX10.5sdk.
  2. Change to the directory containing the game source you wish to build.
  3. Run './make-macosx-ub.sh'
  4. Copy the resulting iowolfmp.app or iowolfsp.app in /build/release-darwin-ub to your /Applications/iortcw folder.

##### Installation, for *nix
  1. Set the COPYDIR variable in the shell to be where you installed RTCW
     to. By default it will be /usr/local/games/wolf if you haven't set it.
     This is the path as used by the original Linux RTCW installer and subsequent point releases.
  2. Run 'make copyfiles'.

It is also possible to cross compile for Windows under *nix and Cygwin using MinGW. Your distribution may have mingw32 packages available. You will need to install 'mingw-w64'.

Thereafter, cross compiling is simply a case running:

'PLATFORM=mingw32 ARCH=x86 make' in place of 'make'.

(ARCH may also be set to x86_64.)

The following variables may be set, either on the command line or in
Makefile.local:

  * CFLAGS              - use this for custom CFLAGS
  * V                   - set to show cc command line when building
  * DEFAULT_BASEDIR     - extra path to search for main and such
  * BUILD_SERVER        - build the 'iowolfded' server binary
  * BUILD_CLIENT        - build the 'iowolfmp' or 'iowolfsp' client binary
  * BUILD_BASEGAME      - build the 'main' binaries
  * BUILD_GAME_SO       - build the game shared libraries
  * BUILD_GAME_QVM      - build the game qvms
  * BUILD_STANDALONE    - build binaries suited for stand-alone games
  * SERVERBIN           - rename 'iowolfded' server binary
  * CLIENTBIN           - rename 'iowolfmp' or 'iowolfsp' client binary
  * USE_RENDERER_DLOPEN - build and use the renderer in a library
  * USE_YACC            - use yacc to update code/tools/lcc/lburg/gram.c
  * BASEGAME            - rename 'main'
  * BASEGAME_CFLAGS     - custom CFLAGS for basegame
  * USE_OPENAL          - use OpenAL where available
  * USE_OPENAL_DLOPEN   - link with OpenAL at runtime
  * USE_CURL            - use libcurl for http/ftp download support
  * USE_CURL_DLOPEN     - link with libcurl at runtime
  * USE_CODEC_VORBIS    - enable Ogg Vorbis support
  * USE_CODEC_OPUS      - enable Ogg Opus support
  * USE_MUMBLE          - enable Mumble support
  * USE_VOIP            - enable built-in VoIP support
  * USE_INTERNAL_LIBS   - build internal libraries instead of dynamically linking against system libraries; this just sets the default for USE_INTERNAL_OPUS etc. and USE_LOCAL_HEADERS
  * USE_FREETYPE        - enable FreeType support for rendering fonts
  * USE_INTERNAL_ZLIB   - build and link against internal zlib
  * USE_INTERNAL_JPEG   - build and link against internal JPEG library
  * USE_INTERNAL_OGG    - build and link against internal ogg library
  * USE_INTERNAL_OPUS   - build and link against internal opus/opusfile libraries
  * USE_LOCAL_HEADERS   - use headers contained within this source tree instead of system ones
  * DEBUG_CFLAGS        - C compiler flags to use for building debug version
  * COPYDIR             - the target installation directory
  * TEMPDIR             - specify user defined directory for temp files

The defaults for these variables differ depending on the target platform.


### Console

#### New cvars
* cg_fixedAspect ( 0 ) - Use aspect corrected HUD/UI ( 0 = Off, 1 = 4:3 style, 2 = Widescreen style )
* cg_fixedAspectFOV ( 1 ) - Use aspect correct FOV when using cg_fixedAspect cvar ( 0 = Off - Use cg_fov, 1 = Automatic FOV based on resolution )

* cl_autoRecordDemo ( 0 ) - record a new demo on each map change
* cl_aviFrameRate ( 25 )- the framerate to use when capturing video
* cl_aviMotionJpeg ( 1 ) - use the mjpeg codec when capturing video
* cl_guidServerUniq ( 1 ) - makes cl_guid unique for each server
* cl_cURLLib - filename of cURL library to load
* cl_consoleKeys - space delimited list of key names or characters that toggle the console
* cl_mouseAccelStyle ( 0 )- Set to 1 for QuakeLive mouse acceleration behaviour, 0 for standard
* cl_mouseAccelOffset ( 5 ) - Tuning the acceleration curve, see below

* con_autochat ( 1 ) - Set to 0 to disable sending console input text as chat when there is not a slash at the beginning
* con_autoclear ( 1 ) - Set to 0 to disable clearing console input text when console is closed

* in_availableJoysticks - list of available Joysticks
* in_keyboardDebug - print keyboard debug info
 
* j_forward - Joystick analogue to m_forward, for forward movement speed/direction.
* j_side - Joystick analogue to m_side, for side movement speed/direction.
* j_up - Joystick up movement speed/direction.
* j_pitch - Joystick analogue to m_pitch, for pitch rotation speed/direction.
* j_yaw - Joystick analogue to m_yaw, for yaw rotation speed/direction.
* j_forward_axis - Selects which joystick axis controls forward/back.
* j_side_axis - Selects which joystick axis controls left/right.
* j_up_axis - Selects which joystick axis controls up/down.
* j_pitch_axis - Selects which joystick axis controls pitch.
* j_yaw_axis - Selects which joystick axis controls yaw.

* s_useOpenAL ( 1 ) - use the OpenAL sound backend if available
* s_alPrecache ( 1 ) - cache OpenAL sounds before use
* s_alGain ( 1.0 ) - the value of AL_GAIN for each source
* s_alSources ( 128 ) - the total number of sources to allocate
* s_alDopplerFactor ( 1.0 ) - the value passed to alDopplerFactor
* s_alDopplerSpeed ( 9000 ) - the value passed to alDopplerVelocity
* s_alMinDistance ( 128 ) - the value of AL_REFERENCE_DISTANCE for each source
* s_alMaxDistance ( 1024 )- the maximum distance before sounds starts to become inaudible.
* s_alRolloff ( 2 ) - the value of AL_ROLLOFF_FACTOR for each source
* s_alGraceDistance ( 512 ) - after having passed MaxDistance, length until sounds are completely inaudible
* s_alDriver - which OpenAL library to use
* s_alDevice - which OpenAL device to use
* s_alAvailableDevices - list of available OpenAL devices
* s_alInputDevice - which OpenAL input device to use
* s_alAvailableInputDevices - list of available OpenAL input devices

* s_sdlBits - SDL bit resolution
* s_sdlSpeed - SDL sample rate
* s_sdlChannels - SDL number of channels
* s_sdlDevSamps - SDL DMA buffer size override
* s_sdlMixSamps - SDL mix buffer size override

* s_backend - read only, indicates the current sound backend

* s_muteWhenMinimized - mute sound when minimized
* s_muteWhenUnfocused - mute sound when window is unfocused

* sv_dlRate - bandwidth allotted to PK3 file downloads via UDP, in kbyte/s
* sv_dlURL - the base of the HTTP or FTP site that holds custom pk3 files for your server

* com_ansiColor - enable use of ANSI escape codes in the terminal
* com_altivec - enable use of altivec on PowerPC systems
* com_standalone (read only) - If set to 1, RTCW is running in standalone mode
* com_basegame - Use a different base than main. If no original RTCW pak files are found, this will enable running in standalone mode
* com_homepath - Specify name that is to be appended to the home path
* com_legacyprotocol - Specify protocol version number for legacy RTCW 1.4 protocol, see "Network protocols" section below (startup only)
* com_legacyversion - Use vanilla RTCW 1.41 version string for game server browser visibility
* com_maxfpsUnfocused - Maximum frames per second when unfocused
* com_maxfpsMinimized - Maximum frames per second when minimized
* com_busyWait - Will use a busy loop to wait for rendering next frame when set to non-zero value
* com_pipefile - Specify filename to create a named pipe through which other processes can control the server while it is running. ( Nonfunctional on Windows. )
* com_gamename - Gamename sent to master server in getservers[Ext] query and infoResponse "gamename" infostring value. Also used for filtering local network games.
* com_protocol - Specify protocol version number for current iortcw protocol, see "Network protocols" section below (startup only)

* sv_banFile - Name of the file that is used for storing the server bans

* net_ip6 - IPv6 address to bind to
* net_port6 - port to bind to using the ipv6 address
* net_mcast6addr - multicast address to use for scanning for IPv6 servers on the local network
* net_mcastiface - outgoing interface to use for scan
* net_enabled - enable networking, bitmask. Add up number for option to enable it:

	Enable IPv4 networking:    1

	Enable IPv6 networking:    2

	Prioritize IPv6 over IPv4: 4

	Disable multicast support: 8


* r_allowResize - make window resizable
* r_ext_texture_filter_anisotropic - anisotropic texture filtering
* r_zProj - distance of observer camera to projection plane in quake3 standard units
* r_greyscale - desaturate textures, useful for anaglyph, supports values in the range of 0 to 1
* r_stereoEnabled - enable stereo rendering for techniques like shutter glasses (untested)
* r_anaglyphMode - Enable rendering of anaglyph images

	red-cyan glasses:    1
	red-blue:            2
	red-green:           3
	green-magenta:       4
    
	To swap the colors for left and right eye just add 4 to the value for the wanted color combination. For red-blue and red-green you probably want to enable r_greyscale

* r_stereoSeparation - Control eye separation. Resulting separation is r_zProj divided by this value in quake3 standard units. See also http://wiki.ioquake3.org/Stereo_Rendering for more information
* r_marksOnTriangleMeshes - Support impact marks on md3 models, MOD developers should increase the mark triangle limits in cg_marks.c if they intend to use this.
* r_sdlDriver - read only, indicates the SDL driver backend being used
* r_noborder - Remove window decoration from window managers, like borders and titlebar.
* r_screenshotJpegQuality - Controls quality of jpeg screenshots captured using screenshotJPEG
* r_aviMotionJpegQuality - Controls quality of video capture when cl_aviMotionJpeg is enabled
* r_mode -2 - This new video mode automatically uses the desktop resolution.

#### New commands
* video [filename]- start video capture (use with demo command)
* stopvideo - stop video capture
* stopmusic - stop background music
* minimize - Minimize the game and show desktop
* togglemenu - causes escape key event for opening/closing menu, or going to a previous menu. works in binds, even in UI
* print - print out the contents of a cvar
* unset - unset a user created cvar
* banaddr ( range ) - ban an IP address range from joining a game on this server, valid ( range ) is either playernum or CIDR notation address range.
* exceptaddr ( range ) - exempt an IP address range from a ban.
* bandel ( range ) - delete ban (either range or ban number)
* exceptdel ( range ) - delete exception (either range or exception number)
* listbans - list all currently active bans and exceptions
* rehashbans - reload the banlist from serverbans.dat
* flushbans - delete all bans
* net_restart - restart network subsystem to change latched settings
* game_restart ( fs_game ) - Switch to another mod
* which ( filename/path ) - print out the path on disk to a loaded item
* execq ( filename ) - quiet exec command, doesn't print "execing file.cfg"
* kicknum ( client number ) - kick a client by number, same as clientkick command
* kickall - kick all clients, similar to "kick all" ( but kicks everyone even if someone is named "all" )
* kickbots - kick all bots, similar to "kick allbots" (but kicks all bots even if someone is named "allbots")
* tell ( client num ) msg - send message to a single client (new to server)
* cvar_modified [filter] - list modified cvars, can filter results (such as "r*" for renderer cvars) like cvarlist which lists all cvars
* addbot random - the bot name "random" now selects a random bot

#### README for Users

###### Using shared libraries instead of qvm:

To force RTCW-MP to use shared libraries, run it with the following
  parameters: +set vm_cgame 0 +set vm_game 0 +set vm_ui 0

###### Help! iortcw won't give me an fps of X anymore when setting com_maxfps!

iortcw now uses the select() system call to wait for the rendering of the next frame when com_maxfps was hit. This will improve your CPU load considerably in these cases. However, not all systems may support a granularity for its timing functions that is required to perform this waiting correctly. For instance, iortcw tells select() to wait 2 milliseconds, but really it can only wait for a multiple of 5ms, i.e. 5, 10, 15, 20... ms. In this case you can always revert back to the old behaviour by setting the cvar:

com_busyWait 1

###### Using HTTP/FTP Download Support (Server)
You can enable redirected downloads on your server even if it's not
an iortcw server.  You simply need to use the 'sets' command to put
the sv_dlURL cvar into your SERVERINFO string and ensure sv_allowDownloads is set to 1

sv_dlURL is the base of the URL that contains your custom .pk3 files
the client will append both fs_game and the filename to the end of
this value.  For example, if you have sv_dlURL set to
"http://yoursite.org", fs_game is "main", and the client is
missing "test.pk3", it will attempt to download from the URL
"http://yoursite.org/main/test.pk3"

* sv_allowDownload's value is now a bitmask made up of the following flags:

	1 - ENABLE
    
  	4 - do not use UDP downloads
    
	8 - do not ask the client to disconnect when using HTTP/FTP


Server operators who are concerned about potential "leeching" from their HTTP servers from other iortcw servers can make use of the HTTP_REFERER that iortcw sets which is "ioQ3://{SERVER_IP}:{SERVER_PORT}". For example, Apache's mod_rewrite can restrict access based on HTTP_REFERER.

On a sidenote, downloading via UDP has been improved and yields higher data rates now. You can configure the maximum bandwidth for UDP downloads via the cvar sv_dlRate. Due to system-specific limits the download rate is capped at about 1 Mbyte/s per client, so curl downloading may still be faster.

###### Using HTTP/FTP Download Support (Client)
Simply setting cl_allowDownload to 1 will enable HTTP/FTP downloads
assuming iortcw was compiled with USE_CURL=1 (the default).

* Like sv_allowDownload, cl_allowDownload also uses a bitmask value supporting the following flags:
	1 - ENABLE
	2 - do not use HTTP/FTP downloads
	4 - do not use UDP downloads

When iortcw is built with USE_CURL_DLOPEN=1 (default), it will use the value of the cvar cl_cURLLib as the filename of the cURL library to dynamically load.

###### Multiuser Support on Windows systems
On Windows, all user specific files such as autogenerated configuration, demos, videos, screenshots, and autodownloaded pk3s are now saved in a directory specific to the user who is running iortcw.

On NT-based such as Windows XP, this is usually a directory named:
"C:\Documents and Settings\%USERNAME%\My Docuemtns\RTCW"

On Windows Vista, Windows 7, and Windows 10 will use a directory like:
"C:\Users\%USERNAME%\Documents\RTCW"

You can revert to the old single-user behaviour by setting the fs_homepath cvar to the directory where iortcw is installed.  For example:

	ioWolfMP.exe +set fs_homepath "c:\iortcw"
    
or

	ioWolfSP.exe +set fs_homepath "c:\iortcw"
    
Note that this cvar MUST be set as a command line parameter.

###### SDL Keyboard Differences
iortcw clients have different keyboard behaviour compared to the original RTCW clients.

SDL > 1.2.9 does not support disabling dead key recognition. In order to send dead key characters ( e.g. ~, ', `, and ^ ), you must key a Space ( or sometimes the same character again ) after the character to send it on many international keyboard layouts.

The SDL client supports many more keys than the original RTCW client. For example the keys: "Windows", "SysReq", "ScrollLock", and "Break". For non-US keyboards, all of the so called "World" keys are now supported as well as F13, F14, F15, and the country-specific mode/meta keys.

On many international layouts the default console toggle keys are also dead keys, meaning that dropping the console potentially results in unintentionally initiating the keying of a dead key. Furthermore SDL 1.2's dead key support is broken by design and RTCW doesn't support non-ASCII text entry, so the chances are you won't get the correct character anyway.

If you use such a keyboard layout, you can set the cvar cl_consoleKeys. This is a space delimited list of key names that will toggle the console. The key names are the usual RTCW names e.g. "~", "`", "c", "BACKSPACE", "PAUSE", "WINDOWS" etc. It's also possible to use ASCII characters, by hexadecimal number. Some example values for cl_consoleKeys:

    "~ ` 0x7e 0x60"           Toggle on ~ or ` (the default)
    "WINDOWS"                 Toggle on the Windows key
    "c"                       Toggle on the c key
    "0x43"                    Toggle on the C character (Shift-c)
    "PAUSE F1 PGUP"           Toggle on the Pause, F1 or Page Up keys

Note that when you elect a set of console keys or characters, they cannot then be used for binding, nor will they generate characters when entering text. Also, in addition to the nominated console keys, Shift-ESC is hard coded to always toggle the console.

QuakeLive mouse acceleration ( patch and this text written by TTimo from id )...I've been using an experimental mouse acceleration code for a while, and decided to make it available to everyone. Don't be too worried if you don't understand the explanations below, this is mostly intended for advanced players:


To enable it, set cl_mouseAccelStyle 1 ( 0 is the default/legacy behavior ) New style is controlled with 3 cvars:

* sensitivity
* cl_mouseAccel
* cl_mouseAccelOffset


The old code ( cl_mouseAccelStyle 0 ) can be difficult to calibrate because if you have a base sensitivity setup, as soon as you set a non zero acceleration your base sensitivity at low speeds will change as well. The other problem with style 0 is that you are stuck on a square ( power of two ) acceleration curve.

The new code tries to solve both problems:

Once you setup your sensitivity to feel comfortable and accurate enough for low mouse deltas with no acceleration ( cl_mouseAccel 0 ), you can start increasing cl_mouseAccel and tweaking cl_mouseAccelOffset to get the amplification you want for high deltas with little effect on low mouse deltas.


cl_mouseAccel is a power value. Should be >= 1, 2 will be the same power curve as style 0. The higher the value, the faster the amplification grows with the mouse delta.


cl_mouseAccelOffset sets how much base mouse delta will be doubled by
acceleration. The closer to zero you bring it, the more acceleration will happen at low speeds. This is also very useful if you are changing to a new mouse with higher dpi, if you go from 500 to 1000 dpi, you can divide your cl_mouseAccelOffset by two to keep the same overall 'feel' ( you will likely gain in precision when you do that, but that is not related to mouse acceleration ).


Mouse acceleration is tricky to configure, and when you do you'll have to re-learn your aiming. But you will find that it's very much worth it in the long run.


If you try the new acceleration code and start using it, I'd be very interested by your feedback.


###### README for Developers

* 64-bit mods


If you wish to compile external mods as shared libraries on a 64bit platform, and the mod source is derived from the id RTCW SDK, you will need to modify the interface code a little. Open the files ending in _syscalls.c and change every instance of int to intptr_t in the declaration of the syscall function pointer and the dllEntry function. Also find the vmMain function for each module ( usually in cg_main.c g_main.c etc. ) and similarly replace the return value in the prototype with intptr_t (arg0, arg1, ...stay int).



Add the following code snippet to q_shared.h:

```c
#ifdef Q3_VM
typedef int intptr_t;
#else
#include <stdint.h>
#endif
```

* Creating standalone games


Have you finished the daunting task of removing all dependencies on the RTCW game data? You probably now want to give your users the opportunity to play the game without owning a copy of RTCW, which consequently means removing cd-key and authentication server checks. 


In addition to being a straightforward RTCW client, iortcw also purports to be a reliable and stable code base on which to base your game project.


However, before you start compiling your own version of iortcw, you have to ask yourself: Have we changed or will we need to change anything of importance in the engine?


If your answer to this question is "no", it probably makes no sense to build your own binaries. Instead, you can just use the pre-built binaries on the website. Just make sure the game is called with:

    +set com_basegame <yournewbase>
    
in any links/scripts you install for your users to start the game. The binary must not detect any original RTCW game pak files. If this
condition is met, the game will set com_standalone to 1 and is then running in stand alone mode.
  
  
If you want the engine to use a different directory in your homepath than e.g. "My Documents\RTCW" on Windows or ".wolf" on Linux, then set a new name at startup by adding
  
    +set com_homepath <homedirname>
  
to the command line. You can also control which game name to use when talking to the master server:
  
    +set com_gamename <gamename>
  
So clients requesting a server list will only receive servers that have a matching game name.
  
Example line:
  
    +set com_basegame basefoo +set com_homepath .foo
    +set com_gamename foo


If you really changed parts that would make vanilla iortcw incompatible with your mod, we have included another way to conveniently build a stand-alone binary. Just run make with the option BUILD_STANDALONE=1. Don't forget to edit the PRODUCT_NAME and subsequent #defines in qcommon/q_shared.h with information appropriate for your project.


While a lot of work has been put into iortcw that you can benefit from free of charge, it does not mean that you have no obligations to fulfill. Please be aware that as soon as you start distributing your game with an engine based on our sources we expect you to fully comply with the requirements as stated in the GPL. That includes making sources and modifications you made to the iortcw engine as well as the game-code used to compile the .qvm/.so files for the game logic freely available to everyone. Furthermore, note that the "RTCW Game Source License" prohibits distribution of mods that are intended to operate on a version of RTCW not sanctioned by id software.


This means that if you're creating a standalone game, you cannot use said license on any portion of the product. As the only other license this code has been released under is the GPL, this is the only option.


This does NOT mean that you cannot market this game commercially. The GPL does not prohibit commercial exploitation and all assets (e.g. textures, sounds, maps) created by yourself are your property and can be sold like every other game you find in stores.


###### Network protocols
There are now two cvars that give you some degree of freedom over the reported protocol versions between clients and servers:

	"com_protocol"
and

	"com_legacyprotocol"
    
The reason for this is that some standalone games increased the protocol number even though nothing really changed in their protocol and the iortcw engine is still fully compatible.

In order to harden the network protocol against UDP spoofing attacks a new network protocol was introduced that defends against such attacks.

Unfortunately, this protocol will be incompatible to the original RTCW 1.4 which is the latest official release from id.


Luckily, iortcw has backwards compatibility, on the client as well as on the server. This means iortcw players can play on old servers just as iortcw servers are able to service old clients.

The cvar "com_protocol" denotes the protocol version for the new hardened protocol, whereas the "com_legacyprotocol" cvar denotes the protocol version for the legacy protocol.

If the value for "com_protocol" and "com_legacyprotocol" is identical, then the legacy protocol is always used. If "com_legacyprotocol" is set to 0, then support for the legacy protocol is disabled.
  
Mods that use a standalone engine obviously do not require dual protocol support, and it is turned off if the engine is compiled with STANDALONE per default. If you desire backwards compatibility to older versions of your game you can still enable it in q_shared.h by defining

	LEGACY_PROTOCOL


###### cl_guid Support
cl_guid is a cvar which is part of the client's USERINFO string.  Its value is a 32 character string made up of [a-f] and [0-9] characters. This value is pseudo-unique for every player. Id's RTCW client also sets cl_guid, but only if Punkbuster is enabled on the client. iortcw uses the same method of calculating cl_guid as Punkbuster does by default ( based on cd-key ) So your cl_guid should match what it is in Id's client.

If cl_guidServerUniq is non-zero (the default), then this value is also pseudo-unique for each server a client connects to (based on IP:PORT of the server).

The purpose of cl_guid is to add an identifier for each player on
a server. This value can be reset by the client at any time so it's not useful for blocking access.  However, it can have at least two uses in your mod's game code:

1) improve logging to allow statistical tools to index players by more than just name
2) granting some weak admin rights to players without requiring passwords

###### PNG support
iortcw supports the use of PNG (Portable Network Graphic) images as
textures. It should be noted that the use of such images in a map will result in missing placeholder textures where the map is used with the id RTCW client.

Recent versions of GtkRadiant and q3map2 support PNG images without
modification. However GtkRadiant is not aware that PNG textures are supported by iortcw. To change this behaviour open the file 'q3.game' in the 'games' directory of the GtkRadiant base directory with an editor and change the line:

    texturetypes="tga jpg"

  to

    texturetypes="tga jpg png"

Restart GtkRadiant and PNG textures are now available.

### Contributing

Please send all patches as a pull request on Github:
(https://github.com/iortcw/iortcw)

The focus for iortcw is to develop a stable base suitable for further development and provide players with the same RTCW experience they've had for years. As such iortcw does not have any significant graphical enhancements and none are planned at this time. However, improved graphics and sound patches will be
accepted as long as they are entirely optional, do not require new media and are off by default.


### Credits

Maintainers
  * Donny Springer <M4N4T4RMS@gmail.com>
  * Zack Middleton <zturtleman@gmail.com>
  * James Canete <use.less01@gmail.com>

Significant contributions from
  * Ryan C. Gordon <icculus@icculus.org>
  * Andreas Kohn <andreas@syndrom23.de>
  * Joerg Dietrich <Dietrich_Joerg@t-online.de>
  * Stuart Dalton <badcdev@gmail.com>
  * Vincent S. Cojot <vincent at cojot dot name>
  * optical <alex@rigbo.se>
  * Aaron Gyes <floam@aaron.gy>
  * Ludwig Nussel <ludwig.nussel@suse.de>
  * Thilo Schulz <arny@ats.s.bawue.de>
  * Tim Angus <tim@ngus.net>
  * Tony J. White <tjw@tjw.org>
  * Zachary J. Slater <zachary@ioquake.org>
