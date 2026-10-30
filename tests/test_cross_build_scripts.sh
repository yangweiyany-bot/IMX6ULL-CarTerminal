#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
    printf 'Usage: %s <build-script> <elf-checker> <toolchain> <host-binary>\n' \
        "$0" >&2
    exit 2
fi

build_script=$1
elf_checker=$2
toolchain_file=$3
host_binary=$4
temp_root=$(mktemp -d)
cleanup() {
    if [[ $temp_root == /tmp/* ]]; then
        rm -rf -- "$temp_root"
    fi
}
trap cleanup EXIT

bash -n "$build_script" "$elf_checker"
"$build_script" --help | grep -Fq 'build_imx6ull_release'

set +e
"$build_script" --build-type Invalid \
    >"$temp_root/invalid-type.out" 2>&1
invalid_type_status=$?
"$build_script" --cc /definitely/missing-arm-gcc \
    --cxx /definitely/missing-arm-g++ --sysroot /definitely/missing-sysroot \
    >"$temp_root/missing-tools.out" 2>&1
missing_tools_status=$?
"$elf_checker" "$host_binary" /usr \
    >"$temp_root/host-elf.out" 2>&1
host_elf_status=$?
cmake -DIMX6ULL_C_COMPILER=/usr/bin/gcc \
    -DIMX6ULL_CXX_COMPILER=/usr/bin/g++ \
    -DIMX6ULL_SYSROOT=/usr -P "$toolchain_file" \
    >"$temp_root/host-toolchain.out" 2>&1
host_toolchain_status=$?
set -e

[[ $invalid_type_status -ne 0 ]]
grep -Fq 'Build type must be Debug or Release' \
    "$temp_root/invalid-type.out"
[[ $missing_tools_status -ne 0 ]]
grep -Fq 'ARM C compiler is missing' "$temp_root/missing-tools.out"
[[ $host_elf_status -ne 0 ]]
grep -Fq 'binary is not 32-bit ARM' "$temp_root/host-elf.out"
[[ $host_toolchain_status -ne 0 ]]
grep -Fq 'does not target 32-bit ARM Linux' \
    "$temp_root/host-toolchain.out"
