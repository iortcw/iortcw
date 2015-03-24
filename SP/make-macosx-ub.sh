#!/bin/sh
CC=gcc-4.0
APPBUNDLE=iowolfsp.app
BINARY=iowolfsp.ub
PKGINFO=APPLIORTCW
ICNS=misc/iortcw.icns
DESTDIR=build/release-darwin-ub
BASEDIR=main

BIN_OBJ="
	build/release-darwin-x86_64/iowolfsp.x86_64
	build/release-darwin-x86/iowolfsp.x86
	build/release-darwin-ppc/iowolfsp.ppc
"
BASE_OBJ="
	build/release-darwin-x86_64/$BASEDIR/cgame.sp.x86_64.dylib
	build/release-darwin-x86/$BASEDIR/cgame.sp.i386.dylib
	build/release-darwin-ppc/$BASEDIR/cgame.sp.ppc.dylib
	build/release-darwin-x86_64/$BASEDIR/ui.sp.x86_64.dylib
	build/release-darwin-x86/$BASEDIR/ui.sp.i386.dylib
	build/release-darwin-ppc/$BASEDIR/ui.sp.ppc.dylib
	build/release-darwin-x86_64/$BASEDIR/qagame.sp.x86_64.dylib
	build/release-darwin-x86/$BASEDIR/qagame.sp.i386.dylib
	build/release-darwin-ppc/$BASEDIR/qagame.sp.ppc.dylib
"
RENDER_OBJ="
	build/release-darwin-x86_64/renderer_sp_opengl1_x86_64.dylib
	build/release-darwin-x86/renderer_sp_opengl1_i386.dylib
	build/release-darwin-ppc/renderer_sp_opengl1_ppc.dylib
	build/release-darwin-x86_64/renderer_sp_rend2_x86_64.dylib
	build/release-darwin-x86/renderer_sp_rend2_i386.dylib
	build/release-darwin-ppc/renderer_sp_rend2_ppc.dylib
"

cd `dirname $0`
if [ ! -f Makefile ]; then
	echo "This script must be run from the iowolfsp build directory"
	exit 1
fi

Q3_VERSION=`grep '^VERSION=' Makefile | sed -e 's/.*=\(.*\)/\1/'`

# We only care if we're >= 10.4, not if we're specifically Tiger.
# "8" is the Darwin major kernel version.
TIGERHOST=`uname -r |perl -w -p -e 's/\A(\d+)\..*\Z/$1/; $_ = (($_ >= 8) ? "1" : "0");'`

# we want to use the oldest available SDK for max compatiblity. However 10.4 and older
# can not build 64bit binaries, making 10.5 the minimum version.   This has been tested 
# with xcode 3.1 (xcode31_2199_developerdvd.dmg).  It contains the 10.5 SDK and a decent
# enough gcc to actually compile iowolfsp
# For PPC macs, G4's or better are required to run iowolfsp.

unset X86_64_SDK
unset X86_64_CFLAGS
unset X86_SDK
unset X86_CFLAGS
unset PPC_64_SDK
unset PPC_CFLAGS

if [ -d /Developer/SDKs/MacOSX10.5.sdk ]; then
	X86_64_SDK=/Developer/SDKs/MacOSX10.5.sdk
	X86_64_CFLAGS="-arch x86_64 -isysroot /Developer/SDKs/MacOSX10.5.sdk"

	X86_SDK=/Developer/SDKs/MacOSX10.5.sdk
	X86_CFLAGS="-arch i386 -isysroot /Developer/SDKs/MacOSX10.5.sdk"

	PPC_SDK=/Developer/SDKs/MacOSX10.5.sdk
	PPC_CFLAGS="-arch ppc -isysroot /Developer/SDKs/MacOSX10.5.sdk"
fi

if [ -z $X86_64_SDK ] || [ -z $X86_SDK ] || [ -z $PPC_SDK ]; then
	echo "\
ERROR: This script is for building a Universal Binary.  You cannot build
       for a different architecture unless you have the proper Mac OS X SDKs
       installed.  If you just want to to compile for your own system run
       'make' instead of this script."
	exit 1
fi

echo "Building X86_64 Client/Dedicated Server against \"$X86_64_SDK\""
echo "Building X86 Client/Dedicated Server against \"$X86_SDK\""
echo "Building PPC Client/Dedicated Server against \"$PPC_SDK\""
echo

if [ "$X86_64_SDK" != "/Developer/SDKs/MacOSX10.5.sdk" ] || \
        [ "$X86_SDK" != "/Developer/SDKs/MacOSX10.5.sdk" ]; then
	echo "\
WARNING: in order to build a binary with maximum compatibility you must
         build on Mac OS X 10.5 using Xcode 3.1 and have the MacOSX10.5
         SDKs installed from the Xcode install disk Packages folder."
sleep 3
fi

if [ ! -d $DESTDIR ]; then
	mkdir -p $DESTDIR
fi

# For parallel make on multicore boxes...
NCPU=`sysctl -n hw.ncpu`

# x86_64 client and server
if [ -d build/release-release-x86_64 ]; then
	rm -r build/release-darwin-x86_64
fi
(ARCH=x86_64 CC=gcc-4.0 CFLAGS=$X86_64_CFLAGS make -j$NCPU) || exit 1;

echo;echo

# x86 client and server
if [ -d build/release-darwin-x86 ]; then
	rm -r build/release-darwin-x86
fi
(ARCH=x86 CC=gcc-4.0 CFLAGS=$X86_CFLAGS make -j$NCPU) || exit 1;

echo;echo

# PPC client and server
if [ -d build/release-darwin-ppc ]; then
	rm -r build/release-darwin-ppc
fi
(ARCH=ppc CC=gcc-4.0 CFLAGS=$PPC_CFLAGS make -j$NCPU) || exit 1;

echo;echo

echo "Creating .app bundle $DESTDIR/$APPBUNDLE"
if [ ! -d $DESTDIR/$APPBUNDLE/Contents/MacOS/$BASEDIR ]; then
	mkdir -p $DESTDIR/$APPBUNDLE/Contents/MacOS/$BASEDIR || exit 1;
fi
if [ ! -d $DESTDIR/$APPBUNDLE/Contents/Resources ]; then
	mkdir -p $DESTDIR/$APPBUNDLE/Contents/Resources
fi
cp $ICNS $DESTDIR/$APPBUNDLE/Contents/Resources/iortcw.icns || exit 1;
echo $PKGINFO > $DESTDIR/$APPBUNDLE/Contents/PkgInfo
echo "
	<?xml version=\"1.0\" encoding=\"UTF-8\"?>
	<!DOCTYPE plist
		PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\"
		\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
	<plist version=\"1.0\">
	<dict>
		<key>CFBundleDevelopmentRegion</key>
		<string>English</string>
		<key>CFBundleExecutable</key>
		<string>$BINARY</string>
		<key>CFBundleGetInfoString</key>
		<string>iowolfsp $Q3_VERSION</string>
		<key>CFBundleIconFile</key>
		<string>iortcw.icns</string>
		<key>CFBundleIdentifier</key>
		<string>org.ioquake.iortcw</string>
		<key>CFBundleInfoDictionaryVersion</key>
		<string>6.0</string>
		<key>CFBundleName</key>
		<string>iowolfsp</string>
		<key>CFBundlePackageType</key>
		<string>APPL</string>
		<key>CFBundleShortVersionString</key>
		<string>$Q3_VERSION</string>
		<key>CFBundleSignature</key>
		<string>$PKGINFO</string>
		<key>CFBundleVersion</key>
		<string>$Q3_VERSION</string>
		<key>NSExtensions</key>
		<dict/>
		<key>NSPrincipalClass</key>
		<string>NSApplication</string>
	</dict>
	</plist>
	" > $DESTDIR/$APPBUNDLE/Contents/Info.plist


# Make UB's from previous builds of x86, x86_64 and ppc binaries
lipo -create -o $DESTDIR/$APPBUNDLE/Contents/MacOS/$BINARY $BIN_OBJ

cp $RENDER_OBJ $DESTDIR/$APPBUNDLE/Contents/MacOS/
cp $BASE_OBJ $DESTDIR/$APPBUNDLE/Contents/MacOS/$BASEDIR/
cp code/libs/macosx/*.dylib $DESTDIR/$APPBUNDLE/Contents/MacOS/

