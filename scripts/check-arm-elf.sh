#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: check-arm-elf.sh <ARM-binary> <target-sysroot>

Optional environment overrides: FILE_TOOL, READELF, OBJDUMP.
USAGE
}

if [[ $# -ne 2 ]]; then
    usage >&2
    exit 2
fi

binary=$1
sysroot=$2
[[ -f $binary ]] || { printf 'Binary not found: %s\n' "$binary" >&2; exit 1; }
[[ -d $sysroot && $sysroot != / ]] || {
    printf 'Invalid target sysroot: %s\n' "$sysroot" >&2
    exit 1
}
binary=$(realpath "$binary")
sysroot=$(realpath "$sysroot")

file_tool=${FILE_TOOL:-file}
readelf_tool=${READELF:-readelf}
objdump_tool=${OBJDUMP:-objdump}
for tool in "$file_tool" "$readelf_tool" "$objdump_tool"; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'Required ELF inspection tool not found: %s\n' "$tool" >&2
        exit 1
    }
done

file_output=$($file_tool -L "$binary")
printf 'file: %s\n' "$file_output"
[[ $file_output == *"ELF 32-bit"* && $file_output == *ARM* ]] || {
    printf 'ELF validation failed: binary is not 32-bit ARM\n' >&2
    exit 1
}
[[ $file_output != *x86-64* && $file_output != *X86-64* ]] || {
    printf 'ELF validation failed: x86-64 binary detected\n' >&2
    exit 1
}

header=$($readelf_tool -h "$binary")
printf '%s\n' "$header" | grep -Eq 'Class:[[:space:]]+ELF32' || {
    printf 'readelf validation failed: ELF class is not ELF32\n' >&2
    exit 1
}
printf '%s\n' "$header" | grep -Eq 'Machine:[[:space:]]+ARM' || {
    printf 'readelf validation failed: ELF machine is not ARM\n' >&2
    exit 1
}

objdump_header=$($objdump_tool -f "$binary")
printf '%s\n' "$objdump_header" | grep -Eqi 'architecture:[[:space:]]*arm' || {
    printf 'objdump validation failed: architecture is not ARM\n' >&2
    exit 1
}

interpreter=$($readelf_tool -l "$binary" |
    sed -n 's@.*Requesting program interpreter: \(.*\)]@\1@p')
if [[ -n $interpreter ]]; then
    printf 'interpreter: %s\n' "$interpreter"
    interpreter_path="$sysroot$interpreter"
    [[ -e $interpreter_path ]] || {
        printf 'Dynamic interpreter is absent from sysroot: %s\n' \
            "$interpreter_path" >&2
        exit 1
    }
    resolved_interpreter=$(realpath -e "$interpreter_path")
    [[ $resolved_interpreter == "$sysroot"/* ]] || {
        printf 'Dynamic interpreter escapes the target sysroot: %s\n' \
            "$resolved_interpreter" >&2
        exit 1
    }
    interpreter_type=$($file_tool -L "$interpreter_path")
    [[ $interpreter_type == *"ELF 32-bit"* &&
       $interpreter_type == *ARM* &&
       $interpreter_type != *x86-64* && $interpreter_type != *X86-64* ]] || {
        printf 'Dynamic interpreter is not an ARM ELF: %s\n' \
            "$interpreter_path" >&2
        exit 1
    }
else
    printf 'interpreter: none (static or non-dynamic ELF)\n'
fi

mapfile -t dependencies < <(
    $readelf_tool -d "$binary" 2>/dev/null |
        sed -n 's/.*Shared library: \[\(.*\)\]/\1/p'
)
for dependency in "${dependencies[@]}"; do
    match=
    while IFS= read -r candidate; do
        resolved_candidate=$(realpath -e "$candidate" 2>/dev/null || true)
        [[ -n $resolved_candidate ]] || continue
        [[ $resolved_candidate == "$sysroot"/* ]] || {
            printf 'Dependency path escapes the target sysroot: %s\n' \
                "$candidate" >&2
            exit 1
        }
        candidate_type=$($file_tool -L "$candidate")
        [[ $candidate_type != *x86-64* &&
           $candidate_type != *X86-64* ]] || {
            printf 'Host x86-64 dependency found in target sysroot: %s\n' \
                "$candidate" >&2
            exit 1
        }
        if [[ $candidate_type == *ELF* ]]; then
            [[ $candidate_type == *"ELF 32-bit"* &&
               $candidate_type == *ARM* ]] || {
                printf 'Non-ARM runtime dependency found in target sysroot: %s\n' \
                    "$candidate" >&2
                exit 1
            }
            match=$candidate
            break
        fi
    done < <(find "$sysroot" \( -type f -o -type l \) \
        -name "$dependency" -print 2>/dev/null)
    [[ -n $match ]] || {
        printf 'ARM ELF dependency is absent from sysroot: %s\n' \
            "$dependency" >&2
        exit 1
    }
    printf 'dependency: %s -> %s\n' "$dependency" "$match"
done

printf '%s\n' "$header" | grep -E 'Class:|Machine:|Flags:'
$readelf_tool -A "$binary" 2>/dev/null |
    grep -E 'Tag_CPU_name|Tag_CPU_arch|Tag_ABI_VFP_args' || true
printf 'ARM ELF validation passed: %s\n' "$binary"
