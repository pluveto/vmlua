#!/bin/bash
BUILD_DIR=build
PROJECT_NAME=`pwd | sed 's#.*/##'`

cmake -G Ninja -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Debug

if [ $? -ne 0 ]; then
    echo "cmake configure failed"
    exit 1
fi

cmake --build $BUILD_DIR --parallel 4

if [ $? -ne 0 ]; then
    echo "cmake build failed"
    exit 1
fi
