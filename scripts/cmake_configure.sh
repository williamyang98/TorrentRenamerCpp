#!/bin/sh
if [[ -z "$BUILD_OUT" ]]; then
    BUILD_OUT="build"
fi

if [[ -z "$BUILD_TYPE" ]]; then
    BUILD_TYPE="Release"
fi

cmake .\
 -B $BUILD_OUT\
 -G Ninja\
 -DCMAKE_BUILD_TYPE:STRING=$BUILD_TYPE\
 -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE
