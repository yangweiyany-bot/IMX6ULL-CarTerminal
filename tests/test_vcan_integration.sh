#!/usr/bin/env bash
set -uo pipefail

if [[ $# -ne 2 ]]; then
    printf 'Usage: %s <car-terminal-binary> <sender-binary>\n' "$0" >&2
    exit 2
fi

if ! command -v ip >/dev/null 2>&1; then
    printf 'SKIP: ip command is unavailable; vcan integration was not run\n'
    exit 77
fi
if ! ip link show vcan0 >/dev/null 2>&1; then
    printf 'SKIP: vcan0 is not present; vcan integration was not run\n'
    exit 77
fi

app_binary=$1
sender_binary=$2
temp_dir=$(mktemp -d)
cleanup() {
    if [[ -n "${app_pid:-}" ]] && kill -0 "$app_pid" 2>/dev/null; then
        kill -TERM "$app_pid" 2>/dev/null || true
        wait "$app_pid" 2>/dev/null || true
    fi
    if [[ "$temp_dir" == /tmp/* ]]; then
        rm -rf -- "$temp_dir"
    fi
}
trap cleanup EXIT

config_file="$temp_dir/socketcan.conf"
output_file="$temp_dir/program.out"
csv_file="$temp_dir/runtime/data.csv"
log_file="$temp_dir/runtime/app.log"
mkdir -p "$temp_dir/runtime"
cat >"$config_file" <<'CONFIG'
data_source=socketcan
low_voltage_threshold_v=12.55
high_temperature_threshold_c=40.0
overspeed_threshold_kmh=120.0
sample_count=2
sampling_interval_ms=1
queue_capacity=2
can_interface=vcan0
can_receive_timeout_ms=50
can_filters=standard:0x123:0x7FF
can_speed_mapping=standard:0x123:0:2:little:unsigned:0.01:0
can_temperature_mapping=standard:0x123:2:2:little:signed:0.1:0
can_voltage_mapping=standard:0x123:4:2:big:unsigned:0.01:0
log_file=runtime/app.log
csv_file=runtime/data.csv
CONFIG

"$app_binary" --config "$config_file" >"$output_file" 2>&1 &
app_pid=$!
sleep 0.15

# Synthetic test frames only. 0x124 must be rejected by the exact 0x123 filter.
"$sender_binary" vcan0 0x124 FFFFFFFFFFFF
"$sender_binary" vcan0 0x123 D204FA000500
sleep 0.03
"$sender_binary" vcan0 0x123 DC05FA000500

set +e
wait "$app_pid"
status=$?
set -e
app_pid=
if [[ "$status" -ne 0 ]]; then
    printf 'SocketCAN application exited with %s\n' "$status" >&2
    sed -n '1,200p' "$output_file" >&2
    exit 1
fi
if [[ ! -f "$csv_file" ]] || [[ $(wc -l <"$csv_file") -ne 3 ]]; then
    printf 'Expected CSV header plus two filtered samples\n' >&2
    sed -n '1,100p' "$csv_file" 2>/dev/null >&2 || true
    exit 1
fi
if ! grep -Fq ',12.34,25.00,12.80,' "$csv_file"; then
    printf 'First decoded synthetic sample is missing\n' >&2
    exit 1
fi
if ! grep -Fq ',15.00,25.00,12.80,' "$csv_file"; then
    printf 'Second decoded synthetic sample is missing\n' >&2
    exit 1
fi
if ! grep -Fq 'Monitoring completed successfully' "$log_file"; then
    printf 'Completion log entry is missing\n' >&2
    exit 1
fi
