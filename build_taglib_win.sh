#!/bin/bash

# Compile Windows dependencies
# This script should be run under Terminal
# Previously, you should install :
# - VisualStudio
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

    CFLAGS="-MP" CXXFLAGS="-MP" cmake \
        -DCMAKE_INSTALL_PREFIX=${LOCAL_DIR}/taglib \
        -DWITH_ZLIB=OFF -DBUILD_TESTING=OFF \
        -DBUILD_EXAMPLES=ON \
        ..
    rm -rf ${LOCAL_DIR}/taglib
    cmake --build . --config Release --target install

    popd
    popd
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

build_taglib

