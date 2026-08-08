#!/bin/bash
set -euo pipefail

build_loader=OFF

while getopts ":lh" opt; do
    case "$opt" in
        l)
            build_loader=ON
            ;;
        h)
            echo "Usage: $0 [-l]"
            echo "  -l  also build the host-side serial loader"
            exit 0
            ;;
        :)
            echo "Error: option -$OPTARG requires an argument" >&2
            exit 1
            ;;
        ?)
            echo "Error: unknown option -$OPTARG" >&2
            exit 1
            ;;
    esac
done

if [ -d build ]; then
    if ! rm -rf build 2>/dev/null; then
        echo "Falling back to sudo cleanup for existing build artifacts" >&2
        sudo rm -rf build
    fi
fi

rm -f loader/stm32wb55_loader

cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake -DBUILD_HOST_LOADER=${build_loader}
cmake --build build

if [ "$build_loader" = "ON" ]; then
    echo "Loader executable: $(pwd)/loader/stm32wb55_loader"
fi