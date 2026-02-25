#!/bin/bash
set -e

# Install TrinityCore build dependencies
apt-get update
apt-get install -y --no-install-recommends \
    build-essential gcc g++ clang cmake ninja-build \
    libboost-all-dev libmysqlclient-dev libssl-dev \
    zlib1g-dev libbz2-dev libreadline-dev libncurses-dev \
    git

# Configure the build directory so cmake --build works
cmake -B out/build/x64-Debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DSCRIPTS=static \
    -DELUNA=ON \
    -DTOOLS=ON \
    -DSERVERS=ON \
    -DWITHOUT_METRICS=ON

echo "Build directory configured at out/build/x64-Debug"
