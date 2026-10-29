# Example only: the board BSP/toolchain must supply both values explicitly.
# No CPU, FPU, float ABI, libc, or tuning flags are guessed here.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(IMX6ULL_CXX_COMPILER "" CACHE FILEPATH
    "Absolute path to the BSP-provided target C++ compiler")
set(IMX6ULL_SYSROOT "" CACHE PATH
    "Absolute path to the matching target sysroot")

if(IMX6ULL_CXX_COMPILER STREQUAL "")
    message(FATAL_ERROR
        "Set -DIMX6ULL_CXX_COMPILER=/absolute/path/to/<target>-g++")
endif()
if(NOT EXISTS "${IMX6ULL_CXX_COMPILER}")
    message(FATAL_ERROR
        "IMX6ULL_CXX_COMPILER does not exist: ${IMX6ULL_CXX_COMPILER}")
endif()
if(IMX6ULL_SYSROOT STREQUAL "")
    message(FATAL_ERROR
        "Set -DIMX6ULL_SYSROOT=/absolute/path/to/matching/sysroot")
endif()
if(NOT IS_DIRECTORY "${IMX6ULL_SYSROOT}")
    message(FATAL_ERROR
        "IMX6ULL_SYSROOT is not a directory: ${IMX6ULL_SYSROOT}")
endif()

set(CMAKE_CXX_COMPILER "${IMX6ULL_CXX_COMPILER}")
set(CMAKE_SYSROOT "${IMX6ULL_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${IMX6ULL_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
