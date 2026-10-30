#!/usr/bin/env bash
set -euo pipefail

unit_name=imx6ull-car-terminal.service
service_user=car-terminal
root=/
prefix=/usr/local
enable_service=0
socketcan_capability=0

usage() {
    cat <<'USAGE'
Usage: install-systemd.sh [--root <staging-root>] [--prefix <prefix>]
                          [--enable] [--socketcan]

Run after `cmake --install`. The default installs the service activation unit,
creates persistent data directories, and does not start the service.
--socketcan installs the minimal CAP_NET_RAW systemd drop-in; simulator mode
does not receive that capability.
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
    --enable)
        enable_service=1
        shift
        ;;
    --socketcan)
        socketcan_capability=1
        shift
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
prefix=${prefix%/}
[[ -n $prefix ]] || prefix=/

root_path() {
    if [[ $root == / ]]; then
        printf '%s' "$1"
    else
        printf '%s%s' "$root" "$1"
    fi
}

installed_prefix=$(root_path "$prefix")
unit_source="$installed_prefix/lib/systemd/system/$unit_name"
dropin_source="$installed_prefix/share/imx6ull-car-terminal/systemd/socketcan-capability.conf"
unit_target=$(root_path "/etc/systemd/system/$unit_name")
dropin_target=$(root_path "/etc/systemd/system/$unit_name.d/20-socketcan.conf")
log_directory=$(root_path "/var/log/imx6ull-car-terminal")
data_directory=$(root_path "/var/lib/imx6ull-car-terminal")

[[ -f $unit_source ]] || {
    printf 'Installed systemd unit is missing: %s\n' "$unit_source" >&2
    exit 1
}
exec_line=$(grep -m1 '^ExecStart=' "$unit_source" || true)
binary_absolute=${exec_line#ExecStart=}
binary_absolute=${binary_absolute%% *}
config_absolute=${exec_line##* --config }
[[ $binary_absolute == /* && $config_absolute == /* &&
   $config_absolute != "$exec_line" &&
   $binary_absolute != *..* && $config_absolute != *..* ]] || {
    printf 'Installed unit has an unsafe or unsupported ExecStart line\n' >&2
    exit 1
}
binary_path=$(root_path "$binary_absolute")
config_path=$(root_path "$config_absolute")
[[ -x $binary_path ]] || {
    printf 'Installed executable is missing: %s\n' "$binary_path" >&2
    exit 1
}
[[ -f $config_path ]] || {
    printf 'Installed systemd configuration is missing: %s\n' "$config_path" >&2
    exit 1
}

if [[ $root == / ]]; then
    [[ $(id -u) -eq 0 ]] || {
        printf 'Live systemd installation must run as root\n' >&2
        exit 1
    }
    getent group "$service_user" >/dev/null ||
        groupadd --system "$service_user"
    id -u "$service_user" >/dev/null 2>&1 ||
        useradd --system --gid "$service_user" --home-dir /nonexistent \
            --shell /usr/sbin/nologin "$service_user"
fi

install -d -m 0755 "$(dirname "$unit_target")"
install -d -m 0750 "$log_directory" "$data_directory"
install -m 0644 "$unit_source" "$unit_target"

if [[ $socketcan_capability -eq 1 ]]; then
    [[ -f $dropin_source ]] || {
        printf 'SocketCAN capability drop-in is missing: %s\n' \
            "$dropin_source" >&2
        exit 1
    }
    install -d -m 0755 "$(dirname "$dropin_target")"
    install -m 0644 "$dropin_source" "$dropin_target"
fi

if [[ $root == / ]]; then
    chown "$service_user:$service_user" "$log_directory" "$data_directory"
    systemctl daemon-reload
    if [[ $enable_service -eq 1 ]]; then
        systemctl enable "$unit_name"
    fi
fi

printf 'Installed %s (data directories preserved across uninstall)\n' \
    "$unit_target"
