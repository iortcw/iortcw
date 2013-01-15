#!/bin/sh
#

# Lets make the user give us a target build system

if [ $# -ne 1 ]; then
	echo "Usage:   $0 target_architecture"
	echo "Example: $0 i386"
	echo "other valid options are x86_64 or ppc"
	echo
	echo "If you don't know or care about architectures please consider using make-macosx-ub.sh instead of this script."
	exit 1
fi

if [ "$1" == "i386" ]; then
	BUILDARCH=i386
elif [ "$1" == "x86_64" ]; then
	BUILDARCH=x86_64
elif [ "$1" == "ppc" ]; then
	BUILDARCH=ppc
else
	echo "Invalid architecture: $1"
	echo "Valid architectures are i386, x86_64 or ppc"
	exit 1
fi

CC=gcc-4.0
APPBUNDLE=iortcw.app
BINARY=iortcw.${BUILDARCH}
DEDBIN=iortcwded.${BUILDARCH}
PKGINFO=APPLIOTCW
ICNS=misc/iortcw.icns
DESTDIR=build/release-darwin-${BUILDARCH}
BASEDIR=main

BIN_OBJ="
	build/release-darwin-${BUILDARCH}/iortcw.${BUILDARCH}
"
BIN_DEDOBJ="
	build/release-darwin-${BUILDARCH}/iortcwded.${BUILDARCH}
"
BASE_OBJ="
	build/release-darwin-${BUILDARCH}/$BASEDIR/cgame.mp.${BUILDARCH}.dylib
	build/release-darwin-${BUILDARCH}/$BASEDIR/ui.mp.${BUILDARCH}.dylib
	build/release-darwin-${BUILDARCH}/$BASEDIR/qagame.mp.${BUILDARCH}.dylib
"
RENDER_OBJ="
	build/release-darwin-${BUILDARCH}/renderer_opengl1_smp_${BUILDARCH}.dylib
	build/release-darwin-${BUILDARCH}/renderer_opengl1_${BUILDARCH}.dylib
	build/release-darwin-${BUILDARCH}/renderer_rend2_smp_${BUILDARCH}.dylib
	build/release-darwin-${BUILDARCH}/renderer_rend2_${BUILDARCH}.dylib
"

cd `dirname $0`
if [ ! -f Makefile ]; then
	echo "This script must be run from the iortcw build directory"
	exit 1
fi

Q3_VERSION=`grep '^VERSION=' Makefile | sed -e 's/.*=\(.*\)/\1/'`

# We only care if we're >= 10.4, not if we're specifically Tiger.
# "8" is the Darwin major kernel version.
TIGERHOST=`uname -r |perl -w -p -e 's/\A(\d+)\..*\Z/$1/; $_ = (($_ >= 8) ? "1" : "0");'`

# we want to use the oldest available SDK for max compatiblity. However 10.4 and older
# can not build 64bit binaries, making 10.5 the minimum version.   This has been tested 
# with xcode 3.1 (xcode31_2199_developerdvd.dmg).  It contains the 10.5 SDK and a decent
# enough gcc to actually compile iortcw
# For PPC macs, G4's or better are required to run iortcw.

unset ARCH_SDK
unset ARCH_CFLAGS
unset ARCH_LDFLAGS

if [ -d /Developer/SDKs/MacOSX10.5.sdk ]; then
	ARCH_SDK=/Developer/SDKs/MacOSX10.5.sdk
	ARCH_CFLAGS="-arch ${BUILDARCH} -isysroot /Developer/SDKs/MacOSX10.5.sdk \
			-DMAC_OS_X_VERSION_MIN_REQUIRED=1050"
	ARCH_LDFLAGS=" -mmacosx-version-min=10.5"
fi


echo "Building ${BUILDARCH} Client/Dedicated Server against \"$ARCH_SDK\""
sleep 3

if [ ! -d $DESTDIR ]; then
	mkdir -p $DESTDIR
fi

# For parallel make on multicore boxes...
NCPU=`sysctl -n hw.ncpu`


# intel client and server
if [ -d build/release-darwin-${BUILDARCH} ]; then
	rm -r build/release-darwin-${BUILDARCH}
fi
(ARCH=${BUILDARCH} CFLAGS=$ARCH_CFLAGS LDFLAGS=$ARCH_LDFLAGS make -j$NCPU) || exit 1;

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
		<string>iortcw $Q3_VERSION</string>
		<key>CFBundleIconFile</key>
		<string>iortcw.icns</string>
		<key>CFBundleIdentifier</key>
		<string>org.ioquake.iortcw</string>
		<key>CFBundleInfoDictionaryVersion</key>
		<string>6.0</string>
		<key>CFBundleName</key>
		<string>iortcw</string>
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


cp $BIN_OBJ $DESTDIR/$APPBUNDLE/Contents/MacOS/$BINARY
cp $BIN_DEDOBJ $DESTDIR/$APPBUNDLE/Contents/MacOS/$DEDBIN
cp $RENDER_OBJ $DESTDIR/$APPBUNDLE/Contents/MacOS/
cp $BASE_OBJ $DESTDIR/$APPBUNDLE/Contents/MacOS/$BASEDIR/
cp code/libs/macosx/*.dylib $DESTDIR/$APPBUNDLE/Contents/MacOS/
cp code/libs/macosx/*.dylib $DESTDIR
