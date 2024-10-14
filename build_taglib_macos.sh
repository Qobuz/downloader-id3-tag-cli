#!/bin/bash

# Compile macOS dependencies on Mac M1
# This script should be run under Terminal
# Previously, you should install :
# - Xcode
# - CMake

# Build Taglib universal
function build_taglib {
    echo "Building TagLib"
    
    if [ ! -d "taglib" ]; then
        git clone -b v2.0.2 --single-branch --depth 1 https://github.com/taglib/taglib.git
    fi

    pushd taglib
    mkdir build
    pushd build

    cmake \
        -DCMAKE_INSTALL_PREFIX=${LOCAL_DIR}/taglib \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
        -DWITH_ZLIB=ON -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=ON \
        ..
    rm -rf ${LOCAL_DIR}/taglib
    make -j 12
    make install

    popd build
    popd taglib
}

echo "Cleaning tmp directory"
rm -rf tmp
if [ $? -ne 0 ]; then
    echo "Unable to remove tmp directory"
    exit
fi
mkdir tmp
pushd tmp

# Where we install all our dependencies
export LOCAL_DIR=`pwd`/local
mkdir -p ${LOCAL_DIR}

export MACOSX_DEPLOYMENT_TARGET="10.13"

build_taglib

