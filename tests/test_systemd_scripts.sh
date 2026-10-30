#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
    printf 'Usage: %s <install-script> <uninstall-script> <unit> <drop-in>\n' \
        "$0" >&2
    exit 2
fi

install_script=$1
uninstall_script=$2
unit_file=$3
dropin_file=$4
temp_root=$(mktemp -d)
cleanup() {
    if [[ $temp_root == /tmp/* ]]; then
        rm -rf -- "$temp_root"
    fi
}
trap cleanup EXIT

prefix=/usr/local
install -d "$temp_root$prefix/bin" \
    "$temp_root$prefix/etc/imx6ull-car-terminal" \
    "$temp_root$prefix/lib/systemd/system" \
    "$temp_root$prefix/share/imx6ull-car-terminal/systemd"
install -m 0755 /bin/true "$temp_root$prefix/bin/car_terminal"
printf 'run_mode=continuous\n' \
    >"$temp_root$prefix/etc/imx6ull-car-terminal/systemd.conf"
install -m 0644 "$unit_file" \
    "$temp_root$prefix/lib/systemd/system/imx6ull-car-terminal.service"
install -m 0644 "$dropin_file" \
    "$temp_root$prefix/share/imx6ull-car-terminal/systemd/socketcan-capability.conf"

"$install_script" --root "$temp_root" --prefix "$prefix" --socketcan

test -f "$temp_root/etc/systemd/system/imx6ull-car-terminal.service"
test -f "$temp_root/etc/systemd/system/imx6ull-car-terminal.service.d/20-socketcan.conf"
test -d "$temp_root/var/log/imx6ull-car-terminal"
test -d "$temp_root/var/lib/imx6ull-car-terminal"

"$uninstall_script" --root "$temp_root" --prefix "$prefix"

test ! -e "$temp_root/etc/systemd/system/imx6ull-car-terminal.service"
test ! -e "$temp_root/etc/systemd/system/imx6ull-car-terminal.service.d/20-socketcan.conf"
test -d "$temp_root/var/log/imx6ull-car-terminal"
test -d "$temp_root/var/lib/imx6ull-car-terminal"
test -f "$temp_root$prefix/etc/imx6ull-car-terminal/systemd.conf"

# GNUInstallDirs places configuration under /etc when the prefix is /usr.
usr_root="$temp_root/usr-layout"
install -d "$usr_root/usr/bin" "$usr_root/etc/imx6ull-car-terminal" \
    "$usr_root/usr/lib/systemd/system" \
    "$usr_root/usr/share/imx6ull-car-terminal/systemd"
install -m 0755 /bin/true "$usr_root/usr/bin/car_terminal"
printf 'run_mode=continuous\n' \
    >"$usr_root/etc/imx6ull-car-terminal/systemd.conf"
sed -e 's#/usr/local/bin/#/usr/bin/#g' \
    -e 's#/usr/local/etc/#/etc/#g' \
    -e 's#/usr/local/share/#/usr/share/#g' "$unit_file" \
    >"$usr_root/usr/lib/systemd/system/imx6ull-car-terminal.service"
install -m 0644 "$dropin_file" \
    "$usr_root/usr/share/imx6ull-car-terminal/systemd/socketcan-capability.conf"

"$install_script" --root "$usr_root" --prefix /usr
test -f "$usr_root/etc/systemd/system/imx6ull-car-terminal.service"
test -d "$usr_root/var/log/imx6ull-car-terminal"
test -d "$usr_root/var/lib/imx6ull-car-terminal"
"$uninstall_script" --root "$usr_root" --prefix /usr
test ! -e "$usr_root/etc/systemd/system/imx6ull-car-terminal.service"
test -f "$usr_root/etc/imx6ull-car-terminal/systemd.conf"
