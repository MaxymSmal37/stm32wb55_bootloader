# ----------------------------------------------------------------------------
# CMake toolchain file for the GNU Arm Embedded toolchain (arm-none-eabi-gcc).
#
# Usage:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
#
# The toolchain binaries are located via TOOLCHAIN_PATH. Override it on the
# command line if your installation lives elsewhere, e.g.:
#   cmake ... -DTOOLCHAIN_PATH=/opt/arm-gnu-toolchain/bin
# ----------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# We are cross-compiling for bare metal: skip the compiler test that tries to
# link a full executable (it would fail without our startup/linker script).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Default install location of the Arm GNU Toolchain on this machine. Override
# with -DTOOLCHAIN_PATH=... if needed.
if(NOT DEFINED TOOLCHAIN_PATH)
    set(TOOLCHAIN_PATH "/Applications/ArmGNUToolchain/14.3.rel1/arm-none-eabi/bin")
endif()

set(TOOLCHAIN_PREFIX arm-none-eabi-)

# Allow the bare command names to be found if the toolchain is on PATH.
find_program(ARM_GCC ${TOOLCHAIN_PREFIX}gcc HINTS ${TOOLCHAIN_PATH})
if(ARM_GCC)
    get_filename_component(TOOLCHAIN_PATH ${ARM_GCC} DIRECTORY)
endif()

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}objcopy CACHE INTERNAL "")
set(CMAKE_SIZE         ${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}size    CACHE INTERNAL "")

# Search for programs only in the host paths, and for libraries/headers only
# in the target (cross) paths.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
