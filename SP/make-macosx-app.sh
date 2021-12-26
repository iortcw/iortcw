#!/bin/bash

# Let's make the user give us a target to work with.
# architecture is assumed universal if not specified, and is optional.
# if arch is defined, it we will store the .app bundle in the target arch build directory
if [ $# == 0 ] || [ $# -gt 2 ]; then
	echo "Usage:   $0 target <arch>"
	echo "Example: $0 release x86"
	echo "Valid targets are:"
	echo " release"
	echo " debug"
	echo
	echo "Optional architectures are:"
	echo " x86"
	echo " x86_64"
	echo " ppc"
	echo " arm64"
	echo
	exit 1
fi

# validate target name
if [ "$1" == "release" ]; then
	TARGET_NAME="release"
elif [ "$1" == "debug" ]; then
	TARGET_NAME="debug"
else
	echo "Invalid target: $1"
	echo "Valid targets are:"
	echo " release"
	echo " debug"
	exit 1
fi

CURRENT_ARCH=""

# validate the architecture if it was specified
if [ "$2" != "" ]; then
	if [ "$2" == "x86" ]; then
		CURRENT_ARCH="x86"
	elif [ "$2" == "x86_64" ]; then
		CURRENT_ARCH="x86_64"
	elif [ "$2" == "ppc" ]; then
		CURRENT_ARCH="ppc"
	elif [ "$2" == "arm64" ]; then
		CURRENT_ARCH="arm64"
	else
		echo "Invalid architecture: $2"
		echo "Valid architectures are:"
		echo " x86"
		echo " x86_64"
		echo " ppc"
		echo " arm64"
		echo
		exit 1
	fi
fi

# symlinkArch() creates a symlink with the architecture suffix.
# meant for universal binaries, but also handles the way this script generates
# application bundles for a single architecture as well.
function symlinkArch()
{
    EXT="dylib"
    SEP="${3}"
    SRCFILE="${1}"
    DSTFILE="${2}${SEP}"
    DSTPATH="${4}"

    if [ ! -e "${DSTPATH}/${SRCFILE}.${EXT}" ]; then
        echo "**** ERROR: missing ${SRCFILE}.${EXT} from ${MACOS}"
        exit 1
    fi

    if [ ! -d "${DSTPATH}" ]; then
        echo "**** ERROR: path not found ${DSTPATH}"
        exit 1
    fi

    pushd "${DSTPATH}" > /dev/null

    IS32=`file "${SRCFILE}.${EXT}" | grep "i386"`
    IS64=`file "${SRCFILE}.${EXT}" | grep "x86_64"`
    ISPPC=`file "${SRCFILE}.${EXT}" | grep "ppc"`
    ISARM=`file "${SRCFILE}.${EXT}" | grep "arm64"`

    if [ "${IS32}" != "" ]; then
        if [ ! -L "${DSTFILE}i386.${EXT}" ]; then
            ln -s "${SRCFILE}.${EXT}" "${DSTFILE}i386.${EXT}"
        fi
    elif [ -L "${DSTFILE}i386.${EXT}" ]; then
        rm "${DSTFILE}i386.${EXT}"
    fi

    if [ "${IS64}" != "" ]; then
        if [ ! -L "${DSTFILE}x86_64.${EXT}" ]; then
            ln -s "${SRCFILE}.${EXT}" "${DSTFILE}x86_64.${EXT}"
        fi
    elif [ -L "${DSTFILE}x86_64.${EXT}" ]; then
        rm "${DSTFILE}x86_64.${EXT}"
    fi

    if [ "${ISPPC}" != "" ]; then
        if [ ! -L "${DSTFILE}ppc.${EXT}" ]; then
            ln -s "${SRCFILE}.${EXT}" "${DSTFILE}ppc.${EXT}"
        fi
    elif [ -L "${DSTFILE}ppc.${EXT}" ]; then
        rm "${DSTFILE}ppc.${EXT}"
    fi

    if [ "${ISARM}" != "" ]; then
        if [ ! -L "${DSTFILE}arm64.${EXT}" ]; then
            ln -s "${SRCFILE}.${EXT}" "${DSTFILE}arm64.${EXT}"
        fi
    elif [ -L "${DSTFILE}arm64.${EXT}" ]; then
        rm "${DSTFILE}arm64.${EXT}"
    fi

    popd > /dev/null
}

SEARCH_ARCHS="	\
	x86	\
	x86_64	\
	ppc	\
	arm64 \
"

HAS_LIPO=`command -v lipo`
HAS_CP=`command -v cp`

# if lipo is not available, we cannot make a universal binary, print a warning
if [ ! -x "${HAS_LIPO}" ] && [ "${CURRENT_ARCH}" == "" ]; then
	CURRENT_ARCH=`uname -m`
	if [ "${CURRENT_ARCH}" == "i386" ]; then CURRENT_ARCH="x86"; fi
	echo "$0 cannot make a universal binary, falling back to architecture ${CURRENT_ARCH}"
fi

# if the optional arch parameter is used, replace SEARCH_ARCHS to only work with one
if [ "${CURRENT_ARCH}" != "" ]; then
	SEARCH_ARCHS="${CURRENT_ARCH}"
fi

AVAILABLE_ARCHS=""

IORTCW_VERSION=`grep '^VERSION=' Makefile | sed -e 's/.*=\(.*\)/\1/'`
IORTCW_CLIENT_ARCHS=""
IORTCW_RENDERER_GL1_ARCHS=""
IORTCW_RENDERER_GL2_ARCHS=""
IORTCW_CGAME_ARCHS=""
IORTCW_GAME_ARCHS=""
IORTCW_UI_ARCHS=""

BASEDIR="main"

CGAME="cgame.sp"
GAME="qagame.sp"
UI="ui.sp"

RENDERER_OPENGL="renderer_sp_opengl1"
RENDERER_OPENGL2="renderer_sp_rend2"

CGAME_NAME="${CGAME}.dylib"
GAME_NAME="${GAME}.dylib"
UI_NAME="${UI}.dylib"

RENDERER_OPENGL1_NAME="renderer_sp_opengl1.dylib"
RENDERER_OPENGL2_NAME="renderer_sp_rend2.dylib"

ICNSDIR="misc"
ICNS="iortcw.icns"
PKGINFO="APPLIORTCW"

OBJROOT="build"
#BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${CURRENT_ARCH}"
PRODUCT_NAME="iowolfsp"
WRAPPER_EXTENSION="app"
WRAPPER_NAME="${PRODUCT_NAME}.${WRAPPER_EXTENSION}"
CONTENTS_FOLDER_PATH="${WRAPPER_NAME}/Contents"
UNLOCALIZED_RESOURCES_FOLDER_PATH="${CONTENTS_FOLDER_PATH}/Resources"
EXECUTABLE_FOLDER_PATH="${CONTENTS_FOLDER_PATH}/MacOS"
EXECUTABLE_NAME="${PRODUCT_NAME}"

# loop through the architectures to build string lists for each universal binary
for ARCH in $SEARCH_ARCHS; do
	CURRENT_ARCH=${ARCH}

	if [ ${CURRENT_ARCH} == "x86" ]; then FILE_ARCH="i386"; fi
	if [ ${CURRENT_ARCH} == "x86_64" ]; then FILE_ARCH="x86_64"; fi
	if [ ${CURRENT_ARCH} == "ppc" ]; then FILE_ARCH="ppc"; fi
	if [ ${CURRENT_ARCH} == "arm64" ]; then FILE_ARCH="arm64"; fi

	BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${CURRENT_ARCH}"
	IORTCW_CLIENT="${EXECUTABLE_NAME}.${CURRENT_ARCH}"
	IORTCW_RENDERER_GL1="${RENDERER_OPENGL}_${FILE_ARCH}.dylib"
	IORTCW_RENDERER_GL2="${RENDERER_OPENGL2}_${FILE_ARCH}.dylib"
	IORTCW_CGAME="${CGAME}.${FILE_ARCH}.dylib"
	IORTCW_GAME="${GAME}.${FILE_ARCH}.dylib"
	IORTCW_UI="${UI}.${FILE_ARCH}.dylib"

	if [ ! -d ${BUILT_PRODUCTS_DIR} ]; then
		CURRENT_ARCH=""
		BUILT_PRODUCTS_DIR=""
		continue
	fi

	# executables
	if [ -e ${BUILT_PRODUCTS_DIR}/${IORTCW_CLIENT} ]; then
		IORTCW_CLIENT_ARCHS="${BUILT_PRODUCTS_DIR}/${IORTCW_CLIENT} ${IORTCW_CLIENT_ARCHS}"
		VALID_ARCHS="${ARCH} ${VALID_ARCHS}"
	else
		continue
	fi

	# renderers
	if [ -e ${BUILT_PRODUCTS_DIR}/${IORTCW_RENDERER_GL1} ]; then
		IORTCW_RENDERER_GL1_ARCHS="${BUILT_PRODUCTS_DIR}/${IORTCW_RENDERER_GL1} ${IORTCW_RENDERER_GL1_ARCHS}"
	fi
	if [ -e ${BUILT_PRODUCTS_DIR}/${IORTCW_RENDERER_GL2} ]; then
		IORTCW_RENDERER_GL2_ARCHS="${BUILT_PRODUCTS_DIR}/${IORTCW_RENDERER_GL2} ${IORTCW_RENDERER_GL2_ARCHS}"
	fi

	# game
	if [ -e ${BUILT_PRODUCTS_DIR}/${BASEDIR}/${IORTCW_CGAME} ]; then
		IORTCW_CGAME_ARCHS="${BUILT_PRODUCTS_DIR}/${BASEDIR}/${IORTCW_CGAME} ${IORTCW_CGAME_ARCHS}"
	fi
	if [ -e ${BUILT_PRODUCTS_DIR}/${BASEDIR}/${IORTCW_GAME} ]; then
		IORTCW_GAME_ARCHS="${BUILT_PRODUCTS_DIR}/${BASEDIR}/${IORTCW_GAME} ${IORTCW_GAME_ARCHS}"
	fi
	if [ -e ${BUILT_PRODUCTS_DIR}/${BASEDIR}/${IORTCW_UI} ]; then
		IORTCW_UI_ARCHS="${BUILT_PRODUCTS_DIR}/${BASEDIR}/${IORTCW_UI} ${IORTCW_UI_ARCHS}"
	fi

	#echo "valid arch: ${ARCH}"
done

# final preparations and checks before attempting to make the application bundle
cd `dirname $0`

if [ ! -f Makefile ]; then
	echo "$0 must be run from the iortcw build directory"
	exit 1
fi

if [ "${IORTCW_CLIENT_ARCHS}" == "" ]; then
	echo "$0: no iortcw binary architectures were found for target '${TARGET_NAME}'"
	exit 1
fi

# set the final application bundle output directory
if [ "${2}" == "" ]; then
	BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-universal"
	if [ ! -d ${BUILT_PRODUCTS_DIR} ]; then
		mkdir -p ${BUILT_PRODUCTS_DIR} || exit 1;
	fi
else
	BUILT_PRODUCTS_DIR="${OBJROOT}/${TARGET_NAME}-darwin-${CURRENT_ARCH}"
fi

BUNDLEBINDIR="${BUILT_PRODUCTS_DIR}/${EXECUTABLE_FOLDER_PATH}"


# here we go
echo "Creating bundle '${BUILT_PRODUCTS_DIR}/${WRAPPER_NAME}'"
echo "with architectures:"
for ARCH in ${VALID_ARCHS}; do
	echo " ${ARCH}"
done
echo ""

# make the application bundle directories
if [ ! -d "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_FOLDER_PATH}/$BASEDIR" ]; then
	mkdir -p "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_FOLDER_PATH}/$BASEDIR" || exit 1;
fi
if [ ! -d "${BUILT_PRODUCTS_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}" ]; then
	mkdir -p "${BUILT_PRODUCTS_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}" || exit 1;
fi

# copy and generate some application bundle resources
cp code/libs/macosx/*.dylib "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_FOLDER_PATH}"
cp ${ICNSDIR}/${ICNS} "${BUILT_PRODUCTS_DIR}/${UNLOCALIZED_RESOURCES_FOLDER_PATH}/$ICNS" || exit 1;
echo -n ${PKGINFO} > "${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/PkgInfo" || exit 1;

# create Info.Plist
PLIST="<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>${EXECUTABLE_NAME}</string>
    <key>CFBundleIconFile</key>
    <string>iortcw</string>
    <key>CFBundleIdentifier</key>
    <string>org.iortcw.${PRODUCT_NAME}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${PRODUCT_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${IORTCW_VERSION}</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>${IORTCW_VERSION}</string>
    <key>CGDisableCoalescedUpdates</key>
    <true/>
    <key>LSMinimumSystemVersion</key>
    <string>${MACOSX_DEPLOYMENT_TARGET}</string>"

if [ -n "${MACOSX_DEPLOYMENT_TARGET_PPC}" ] || [ -n "${MACOSX_DEPLOYMENT_TARGET_X86}" ] || [ -n "${MACOSX_DEPLOYMENT_TARGET_X86_64}" ] || [ -n "${MACOSX_DEPLOYMENT_TARGET_ARM64}" ]; then
	PLIST="${PLIST}
    <key>LSMinimumSystemVersionByArchitecture</key>
    <dict>"

	if [ -n "${MACOSX_DEPLOYMENT_TARGET_PPC}" ]; then
	PLIST="${PLIST}
        <key>ppc</key>
        <string>${MACOSX_DEPLOYMENT_TARGET_PPC}</string>"
	fi

	if [ -n "${MACOSX_DEPLOYMENT_TARGET_X86}" ]; then
	PLIST="${PLIST}
        <key>i386</key>
        <string>${MACOSX_DEPLOYMENT_TARGET_X86}</string>"
	fi

	if [ -n "${MACOSX_DEPLOYMENT_TARGET_X86_64}" ]; then
	PLIST="${PLIST}
        <key>x86_64</key>
        <string>${MACOSX_DEPLOYMENT_TARGET_X86_64}</string>"
	fi

	if [ -n "${MACOSX_DEPLOYMENT_TARGET_ARM64}" ]; then
	PLIST="${PLIST}
        <key>arm64</key>
        <string>${MACOSX_DEPLOYMENT_TARGET_ARM64}</string>"
	fi

	PLIST="${PLIST}
    </dict>"
fi

PLIST="${PLIST}
    <key>NSHumanReadableCopyright</key>
    <string>Return to Castle Wolfenstein single player Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <key>NSHighResolutionCapable</key>
    <false/>
</dict>
</plist>
"
echo -e "${PLIST}" > "${BUILT_PRODUCTS_DIR}/${CONTENTS_FOLDER_PATH}/Info.plist"

# action takes care of generating universal binaries if lipo is available
# otherwise, it falls back to using a simple copy, expecting the first item in
# the second parameter list to be the desired architecture
function action()
{
	COMMAND=""

	if [ -x "${HAS_LIPO}" ]; then
		COMMAND="${HAS_LIPO} -create -o"
		$HAS_LIPO -create -o "${1}" ${2} # make sure $2 is treated as a list of files
	elif [ -x ${HAS_CP} ]; then
		COMMAND="${HAS_CP}"
		SRC="${2// */}" # in case there is a list here, use only the first item
		$HAS_CP "${SRC}" "${1}"
	else
		"$0 cannot create an application bundle."
		exit 1
	fi

	#echo "${COMMAND}" "${1}" "${2}"
}

#
# the meat of universal binary creation
# destination file names do not have architecture suffix.
# action will handle merging universal binaries if supported.
# symlink appropriate architecture names for universal (fat) binary support.
#

# executables
action "${BUNDLEBINDIR}/${EXECUTABLE_NAME}"				"${IORTCW_CLIENT_ARCHS}"

# renderers
action "${BUNDLEBINDIR}/${RENDERER_OPENGL1_NAME}"		"${IORTCW_RENDERER_GL1_ARCHS}"
action "${BUNDLEBINDIR}/${RENDERER_OPENGL2_NAME}"		"${IORTCW_RENDERER_GL2_ARCHS}"
symlinkArch "${RENDERER_OPENGL}" "${RENDERER_OPENGL}" "_" "${BUNDLEBINDIR}"
symlinkArch "${RENDERER_OPENGL2}" "${RENDERER_OPENGL2}" "_" "${BUNDLEBINDIR}"

# game
action "${BUNDLEBINDIR}/${BASEDIR}/${CGAME_NAME}"		"${IORTCW_CGAME_ARCHS}"
action "${BUNDLEBINDIR}/${BASEDIR}/${GAME_NAME}"		"${IORTCW_GAME_ARCHS}"
action "${BUNDLEBINDIR}/${BASEDIR}/${UI_NAME}"			"${IORTCW_UI_ARCHS}"
symlinkArch "${CGAME}"	"${CGAME}."	""	"${BUNDLEBINDIR}/${BASEDIR}"
symlinkArch "${GAME}"	"${GAME}."	""	"${BUNDLEBINDIR}/${BASEDIR}"
symlinkArch "${UI}"		"${UI}."		""	"${BUNDLEBINDIR}/${BASEDIR}"

