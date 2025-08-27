#!/bin/bash

VELODB_BASE_DIR=$(dirname $(dirname $(readlink -f $0)))
cd $VELODB_BASE_DIR
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON .
