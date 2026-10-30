#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'Usage: %s <configured-unit>\n' "$0" >&2
    exit 2
fi

unit_file=$1
grep -Fq 'User=car-terminal' "$unit_file"
grep -Fq 'Restart=on-failure' "$unit_file"
grep -Fq 'KillSignal=SIGTERM' "$unit_file"
grep -Fq 'NoNewPrivileges=true' "$unit_file"
grep -Fq 'ProtectSystem=strict' "$unit_file"
if grep -Fq 'AmbientCapabilities=' "$unit_file"; then
    printf 'Default simulator unit must not grant ambient capabilities\n' >&2
    exit 1
fi

if command -v systemd-analyze >/dev/null 2>&1; then
    temp_unit=$(mktemp --suffix=.service)
    cleanup() {
        rm -f -- "$temp_unit"
    }
    trap cleanup EXIT
    sed 's#^ExecStart=.*#ExecStart=/bin/true#' "$unit_file" >"$temp_unit"
    systemd-analyze verify "$temp_unit"
else
    printf 'systemd-analyze unavailable; directive checks completed only\n'
fi
