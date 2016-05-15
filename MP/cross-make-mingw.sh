#!/bin/sh

# Note: This works in Linux and cygwin

if [ "$ARCH" = "x86_64" ];
then
    CMD_PREFIX="x86_64-w64-mingw32 amd64-mingw32msvc"
else
    CMD_PREFIX="i686-w64-mingw32 i586-mingw32msvc i686-pc-mingw32"
    export ARCH=x86
fi

if [ "$CC" = "cc" ] || [ "$CC" = "gcc" ];
then
    CC=
fi

if [ "X$CC" = "X" ]; then
    for check in $CMD_PREFIX; do
        full_check="${check}-gcc"
        which "$full_check" > /dev/null 2>&1
        if [ "$?" = "0" ]; then
            export CC="$full_check"
        fi
    done
fi

if [ "X$CXX" = "X" ]; then
    for check in $CMD_PREFIX; do
        full_check="${check}-g++"
        which "$full_check" > /dev/null 2>&1
        if [ "$?" = "0" ]; then
            export CXX="$full_check"
        fi
    done
fi

if [ "X$WINDRES" = "X" ]; then
    for check in $CMD_PREFIX; do
        full_check="${check}-windres"
        which "$full_check" > /dev/null 2>&1
        if [ "$?" = "0" ]; then
            export WINDRES="$full_check"
        fi
    done
fi

if [ "X$WINDRES" = "X" -o "X$CC" = "X" -o "X$CXX" = "X" ]; then
    echo "Error: Must define or find WINDRES, CC, and CXX"
    exit 1
fi

export PLATFORM=mingw32

exec make $*
