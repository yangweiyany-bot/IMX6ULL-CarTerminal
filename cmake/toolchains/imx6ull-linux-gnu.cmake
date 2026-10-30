# Generic i.MX6ULL Linux cross-compilation toolchain.
#
# The BSP/SDK must provide the compiler paths, sysroot, and any ABI/FPU flags.
# This file deliberately does not guess hard-float vs soft-float, libc, FPU,
# or Linux distribution details.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT DEFINED IMX6ULL_C_COMPILER)
    set(IMX6ULL_C_COMPILER "$ENV{IMX6ULL_C_COMPILER}" CACHE FILEPATH
        "Absolute path to the BSP-matched ARM C compiler")
endif()
if(NOT DEFINED IMX6ULL_CXX_COMPILER)
    set(IMX6ULL_CXX_COMPILER "$ENV{IMX6ULL_CXX_COMPILER}" CACHE FILEPATH
        "Absolute path to the BSP-matched ARM C++ compiler")
endif()
if(NOT DEFINED IMX6ULL_SYSROOT)
    set(IMX6ULL_SYSROOT "$ENV{IMX6ULL_SYSROOT}" CACHE PATH
        "Absolute path to the BSP-matched target sysroot")
endif()
if(NOT DEFINED IMX6ULL_ARCH_FLAGS)
    set(IMX6ULL_ARCH_FLAGS "$ENV{IMX6ULL_ARCH_FLAGS}" CACHE STRING
        "Optional CPU/FPU/ABI flags supplied by the BSP or SDK")
endif()

foreach(variable IMX6ULL_C_COMPILER IMX6ULL_CXX_COMPILER)
    if("${${variable}}" STREQUAL "")
        message(FATAL_ERROR "${variable} must be an absolute compiler path")
    endif()
    if(NOT IS_ABSOLUTE "${${variable}}" OR
       NOT EXISTS "${${variable}}")
        message(FATAL_ERROR
            "${variable} is not an existing absolute path: ${${variable}}")
    endif()
endforeach()

if(IMX6ULL_SYSROOT STREQUAL "" OR
   NOT IS_ABSOLUTE "${IMX6ULL_SYSROOT}" OR
   NOT IS_DIRECTORY "${IMX6ULL_SYSROOT}")
    message(FATAL_ERROR
        "IMX6ULL_SYSROOT must be an existing absolute target sysroot")
endif()
if(IMX6ULL_SYSROOT STREQUAL "/")
    message(FATAL_ERROR "The host root directory cannot be used as a sysroot")
endif()
if(NOT IS_DIRECTORY "${IMX6ULL_SYSROOT}/usr/include" AND
   NOT IS_DIRECTORY "${IMX6ULL_SYSROOT}/include")
    message(FATAL_ERROR
        "IMX6ULL_SYSROOT has no target include directory: ${IMX6ULL_SYSROOT}")
endif()
if(NOT IS_DIRECTORY "${IMX6ULL_SYSROOT}/usr/lib" AND
   NOT IS_DIRECTORY "${IMX6ULL_SYSROOT}/lib")
    message(FATAL_ERROR
        "IMX6ULL_SYSROOT has no target library directory: ${IMX6ULL_SYSROOT}")
endif()

foreach(language C CXX)
    execute_process(
        COMMAND "${IMX6ULL_${language}_COMPILER}" -dumpmachine
        RESULT_VARIABLE IMX6ULL_${language}_DUMPMACHINE_RESULT
        OUTPUT_VARIABLE IMX6ULL_${language}_COMPILER_TARGET
        ERROR_VARIABLE IMX6ULL_${language}_DUMPMACHINE_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT IMX6ULL_${language}_DUMPMACHINE_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Cannot query the ${language} compiler target: "
            "${IMX6ULL_${language}_DUMPMACHINE_ERROR}")
    endif()
    if(NOT IMX6ULL_${language}_COMPILER_TARGET MATCHES "^arm.*linux")
        message(FATAL_ERROR
            "${language} compiler does not target 32-bit ARM Linux: "
            "${IMX6ULL_${language}_COMPILER_TARGET}")
    endif()
endforeach()

set(CMAKE_C_COMPILER "${IMX6ULL_C_COMPILER}")
set(CMAKE_CXX_COMPILER "${IMX6ULL_CXX_COMPILER}")
set(CMAKE_SYSROOT "${IMX6ULL_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${IMX6ULL_SYSROOT}")

if(NOT IMX6ULL_ARCH_FLAGS STREQUAL "")
    set(CMAKE_C_FLAGS_INIT "${IMX6ULL_ARCH_FLAGS}")
    set(CMAKE_CXX_FLAGS_INIT "${IMX6ULL_ARCH_FLAGS}")
endif()

# Build tools run on the host; headers, libraries, and packages must come only
# from the target sysroot. This prevents accidental x86-64 linkage.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
