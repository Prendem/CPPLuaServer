#!/bin/bash

# Check if vcpkg is bootstrapped, if not bootstrap it
if [ ! -f "./vcpkg/vcpkg" ]; then
    ./vcpkg/bootstrap-vcpkg.sh
fi

# Run the cmake command
cmake -S . -B ./build -DCMAKE_TOOLCHAIN_FILE="./vcpkg/scripts/buildsystems/vcpkg.cmake"