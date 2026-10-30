#!/usr/bin/env bash
set -uo pipefail

if [[ $# -ne 1 ]]; then
    printf 'Usage: %s <car-terminal-binary>\n' "$0" >&2
    exit 2
fi

binary=$1
temp_dir=$(mktemp -d)
cleanup() {
    if [[ "$temp_dir" == /tmp/* ]]; then
        rm -rf -- "$temp_dir"
    fi
}
trap cleanup EXIT

config_file="$temp_dir/default.conf"
output_file="$temp_dir/program.out"
log_file="$temp_dir/runtime/car_terminal.log"
csv_file="$temp_dir/runtime/vehicle_data.csv"

mkdir -p "$temp_dir/runtime"
cat >"$config_file" <<'CONFIG'
low_voltage_threshold_v=12.55
high_temperature_threshold_c=40.0
overspeed_threshold_kmh=120.0
sample_count=1000
sampling_interval_ms=20
queue_capacity=2
log_file=runtime/car_terminal.log
csv_file=runtime/vehicle_data.csv
CONFIG

"$binary" --config "$config_file" >"$output_file" 2>&1 &
program_pid=$!

ready=0
for _ in $(seq 1 200); do
    if ! kill -0 "$program_pid" 2>/dev/null; then
        break
    fi
    if [[ -f "$csv_file" ]] &&
       [[ $(wc -l <"$csv_file") -ge 3 ]]; then
        ready=1
        break
    fi
    sleep 0.01
done

if [[ "$ready" -ne 1 ]]; then
    printf 'Program did not produce samples before timeout\n' >&2
    wait "$program_pid" 2>/dev/null || true
    sed -n '1,160p' "$output_file" >&2
    exit 1
fi

kill -TERM "$program_pid"
set +e
wait "$program_pid"
status=$?
set -e

if [[ "$status" -ne 143 ]]; then
    printf 'Expected exit 143 after SIGTERM, got %s\n' "$status" >&2
    sed -n '1,160p' "$output_file" >&2
    exit 1
fi

if [[ $(wc -l <"$csv_file") -lt 3 ]]; then
    printf 'CSV does not contain the header and acquired samples\n' >&2
    exit 1
fi

if ! grep -Fq 'Monitoring stopped by signal 15 after draining queued samples' \
        "$log_file"; then
    printf 'Graceful-stop log entry is missing\n' >&2
    sed -n '1,160p' "$log_file" >&2
    exit 1
fi
