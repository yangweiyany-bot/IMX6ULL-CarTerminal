#!/usr/bin/env bash
set -euo pipefail

unit_name=imx6ull-car-terminal.service
root=/
prefix=/usr/local

usage() {
    cat <<'USAGE'
Usage: uninstall-systemd.sh [--root <staging-root>] [--prefix <prefix>]

Disables and removes the systemd activation unit and optional SocketCAN
drop-in. Configuration, executable, logs, CSV files, and service account are
intentionally preserved.
USAGE
}

while [[ $# -gt 0 ]]; do
    case $1 in
    --root)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        root=$2
        shift 2
        ;;
    --prefix)
        [[ $# -ge 2 ]] || { usage >&2; exit 2; }
        prefix=$2
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

[[ $root == /* && $prefix == /* ]] || {
    printf 'Root and prefix must be absolute paths\n' >&2
    exit 2
}
[[ $root != *..* && $prefix != *..* ]] || {
    printf 'Root and prefix must not contain .. components\n' >&2
    exit 2
}
root=${root%/}
[[ -n $root ]] || root=/

root_path() {
    if [[ $root == / ]]; then
        printf '%s' "$1"
    else
        printf '%s%s' "$root" "$1"
    fi
}

unit_target=$(root_path "/etc/systemd/system/$unit_name")
dropin_directory=$(root_path "/etc/systemd/system/$unit_name.d")
dropin_target="$dropin_directory/20-socketcan.conf"

if [[ $root == / ]]; then
    [[ $(id -u) -eq 0 ]] || {
        printf 'Live systemd uninstall must run as root\n' >&2
        exit 1
    }
    systemctl disable --now "$unit_name" 2>/dev/null || true
fi

rm -f -- "$dropin_target" "$unit_target"
rmdir -- "$dropin_directory" 2>/dev/null || true

if [[ $root == / ]]; then
    systemctl daemon-reload
fi

printf 'Removed systemd activation files; configuration and data remain\n'
