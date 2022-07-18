#!/bin/bash
BUILD_DIR=build
PROJECT_NAME=`pwd | sed 's#.*/##'`
if [ $1 == "clean" ]; then
    cmake --build $BUILD_DIR --target clean
    exit 0
fi

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

GREEN="\033[32m"
RESET="\033[0m"
echo -e "${GREEN}build success, now start${RESET}"
export VM_LUA_DEBUG=1
./$BUILD_DIR/${PROJECT_NAME} $1