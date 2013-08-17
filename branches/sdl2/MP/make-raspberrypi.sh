#!/bin/bash

make \
	BR=build \
	BD=build \
	V=1 \
	USE_CODEC_VORBIS=0 \
	USE_CODEC_OPUS=0 \
	USE_CURL=1 \
	USE_CURL_DLOPEN=1 \
	USE_OPENAL=1 \
	USE_OPENAL_DLOPEN=1 \
	USE_RENDERER_DLOPEN=1 \
	USE_VOIP=1 \
	USE_LOCAL_HEADERS=1 \
	USE_INTERNAL_JPEG=1 \
	USE_INTERNAL_SPEEX=1 \
	USE_INTERNAL_ZLIB=1 \
	USE_OPENGLES=1 \
	USE_BLOOM=0 \
	BUILD_GAME_SO=1 \
	BUILD_GAME_QVM=0 \
	BUILD_RENDERER_REND2=0 \
	ARCH=arm \
	PLATFORM=linux \
	COMPILE_ARCH=arm \
	COMPILE_PLATFORM=linux \
	COPYDIR=/usr/local/games/wolfenstein/ \
	CFLAGS="-DVCMODS_MISC -DVCMODS_OPENGLES -DVCMODS_DEPTH -DVCMODS_REPLACETRIG -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux" \
	LDFLAGS="-L/opt/vc/lib -lbcm_host" \
	release

