#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: build-imx6ull.sh [options]

Options:
  --build-type Debug|Release       Default: Release
  --cc <absolute-path>             Generic ARM C compiler
  --cxx <absolute-path>            Generic ARM C++ compiler
  --sysroot <absolute-path>        Matching target sysroot
  --arch-flags <flags>             BSP/SDK supplied CPU/FPU/ABI flags
  --toolchain-file <path>          External CMake toolchain (for an SDK)
  --yocto-env <environment-setup>  Source a Yocto SDK environment first
  --jobs <positive-number>         Parallel build jobs
  --help                           Show this help

Environment alternatives:
  IMX6ULL_C_COMPILER, IMX6ULL_CXX_COMPILER, IMX6ULL_SYSROOT,
  IMX6ULL_ARCH_FLAGS, IMX6ULL_TOOLCHAIN_FILE, CC, CXX,
  SDKTARGETSYSROOT, CMAKE_TOOLCHAIN_FILE.

Outputs are isolated in build_imx6ull_debug/install_imx6ull_debug or
build_imx6ull_release/install_imx6ull_release under the project directory.
USAGE
}

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_type=Release
cc_value=${IMX6ULL_C_COMPILER:-}
cxx_value=${IMX6ULL_CXX_COMPILER:-}
sysroot_value=${IMX6ULL_SYSROOT:-}
arch_flags=${IMX6ULL_ARCH_FLAGS:-}
toolchain_value=${IMX6ULL_TOOLCHAIN_FILE:-}
yocto_environment=
jobs=

while [[ $# -gt 0 ]]; do
    case $1 in
    --build-type)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        build_type=$2
        shift 2
        ;;
    --cc)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        cc_value=$2
        shift 2
        ;;
    --cxx)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        cxx_value=$2
        shift 2
        ;;
    --sysroot)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        sysroot_value=$2
        shift 2
        ;;
    --arch-flags)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        arch_flags=$2
        shift 2
        ;;
    --toolchain-file)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        toolchain_value=$2
        shift 2
        ;;
    --yocto-env)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        yocto_environment=$2
        shift 2
        ;;
    --jobs)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        jobs=$2
        shift 2
        ;;
    --help)
        usage
        exit 0
        ;;
    *)
        printf 'Unknown option: %s\n' "$1" >&2
        usage >&2
        exit 2
        ;;
    esac
done

case $build_type in
Debug | Release) ;;
*)
    printf 'Build type must be Debug or Release: %s\n' "$build_type" >&2
    exit 2
    ;;
esac
if [[ -n $jobs && ! $jobs =~ ^[1-9][0-9]*$ ]]; then
    printf 'Jobs must be a positive integer: %s\n' "$jobs" >&2
    exit 2
fi

for tool in cmake ninja file readelf objdump; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'Required host tool is missing: %s\n' "$tool" >&2
        exit 1
    }
done

external_toolchain=0
if [[ -n $yocto_environment ]]; then
    [[ -f $yocto_environment ]] || {
        printf 'Yocto environment file not found: %s\n' \
            "$yocto_environment" >&2
        exit 1
    }
    set +u
    # shellcheck disable=SC1090
    source "$yocto_environment"
    set -u
    external_toolchain=1
    cc_value=${cc_value:-${CC:-}}
    cxx_value=${cxx_value:-${CXX:-}}
    sysroot_value=${sysroot_value:-${SDKTARGETSYSROOT:-}}
    if [[ -z $toolchain_value ]]; then
        toolchain_value=${CMAKE_TOOLCHAIN_FILE:-}
    fi
    if [[ -z $toolchain_value && -n ${OECORE_NATIVE_SYSROOT:-} ]]; then
        candidate="$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake"
        [[ -f $candidate ]] && toolchain_value=$candidate
    fi
fi
if [[ -n ${CMAKE_TOOLCHAIN_FILE:-} && -z $toolchain_value ]]; then
    toolchain_value=$CMAKE_TOOLCHAIN_FILE
    external_toolchain=1
fi
if [[ -n $toolchain_value ]]; then
    external_toolchain=1
else
    toolchain_value="$project_root/cmake/toolchains/imx6ull-linux-gnu.cmake"
fi

[[ -f $toolchain_value ]] || {
    printf 'CMake toolchain file not found: %s\n' "$toolchain_value" >&2
    exit 1
}
toolchain_value=$(realpath "$toolchain_value")

if [[ -z $cc_value ]]; then
    cc_value=${CC:-}
fi
if [[ -z $cxx_value ]]; then
    cxx_value=${CXX:-}
fi
if [[ -z $sysroot_value ]]; then
    sysroot_value=${SDKTARGETSYSROOT:-}
fi

compiler_command() {
    local specification=$1
    local token
    local -a tokens
    read -r -a tokens <<<"$specification"
    for token in "${tokens[@]}"; do
        if [[ $(basename "$token") == *gcc* ||
              $(basename "$token") == *g++* ||
              $(basename "$token") == *c++* ]]; then
            command -v "$token" 2>/dev/null || true
            return
        fi
    done
}

cc_command=$(compiler_command "$cc_value")
cxx_command=$(compiler_command "$cxx_value")
[[ -n $cc_command ]] || {
    printf 'ARM C compiler is missing or not executable: %s\n' \
        "${cc_value:-<not specified>}" >&2
    exit 1
}
[[ -n $cxx_command ]] || {
    printf 'ARM C++ compiler is missing or not executable: %s\n' \
        "${cxx_value:-<not specified>}" >&2
    exit 1
}
cc_target=$($cc_command -dumpmachine 2>/dev/null || true)
cxx_target=$($cxx_command -dumpmachine 2>/dev/null || true)
[[ $cc_target == arm*linux* ]] || {
    printf 'C compiler target is not 32-bit ARM Linux: %s\n' \
        "${cc_target:-unknown}" >&2
    exit 1
}
[[ $cxx_target == arm*linux* ]] || {
    printf 'C++ compiler target is not 32-bit ARM Linux: %s\n' \
        "${cxx_target:-unknown}" >&2
    exit 1
}

[[ -n $sysroot_value && -d $sysroot_value && $sysroot_value != / ]] || {
    printf 'Target sysroot is missing or invalid: %s\n' "$sysroot_value" >&2
    exit 1
}
sysroot_value=$(realpath "$sysroot_value")
if [[ ! -d $sysroot_value/usr/include && ! -d $sysroot_value/include ]]; then
    printf 'Target sysroot has no include directory: %s\n' \
        "$sysroot_value" >&2
    exit 1
fi
if [[ ! -d $sysroot_value/usr/lib && ! -d $sysroot_value/lib ]]; then
    printf 'Target sysroot has no library directory: %s\n' \
        "$sysroot_value" >&2
    exit 1
fi

type_lower=${build_type,,}
build_directory="$project_root/build_imx6ull_$type_lower"
install_directory="$project_root/install_imx6ull_$type_lower"
mkdir -p "$build_directory" "$install_directory"

cmake_arguments=(
    -S "$project_root"
    -B "$build_directory"
    -G Ninja
    "-DCMAKE_BUILD_TYPE=$build_type"
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain_value"
    -DCMAKE_INSTALL_PREFIX=/usr
    -DBUILD_TESTING=OFF
)
if [[ $external_toolchain -eq 0 ]]; then
    cmake_arguments+=(
        "-DIMX6ULL_C_COMPILER=$cc_command"
        "-DIMX6ULL_CXX_COMPILER=$cxx_command"
        "-DIMX6ULL_SYSROOT=$sysroot_value"
        "-DIMX6ULL_ARCH_FLAGS=$arch_flags"
    )
fi

printf 'Target C compiler: %s\n' "$cc_target"
printf 'Target C++ compiler: %s\n' "$cxx_target"
printf 'Target sysroot: %s\n' "$sysroot_value"
printf 'Toolchain file: %s\n' "$toolchain_value"
printf 'Build directory: %s\n' "$build_directory"
printf 'Install directory: %s\n' "$install_directory"

cmake "${cmake_arguments[@]}"
if [[ -n $jobs ]]; then
    cmake --build "$build_directory" --parallel "$jobs"
else
    cmake --build "$build_directory" --parallel
fi
DESTDIR="$install_directory" cmake --install "$build_directory"

target_readelf=$(command -v "$cxx_target-readelf" 2>/dev/null ||
    command -v readelf)
target_objdump=$(command -v "$cxx_target-objdump" 2>/dev/null ||
    command -v objdump)
READELF=$target_readelf OBJDUMP=$target_objdump \
    "$project_root/scripts/check-arm-elf.sh" \
    "$install_directory/usr/bin/car_terminal" "$sysroot_value" |
    tee "$install_directory/arm-elf-report.txt"

printf 'Cross build and ARM ELF validation completed successfully\n'
