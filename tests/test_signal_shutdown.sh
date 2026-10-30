#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'Usage: %s <car-terminal-binary>\n' "$0" >&2
    exit 2
fi

binary=$1
temp_root=$(mktemp -d)
cleanup() {
    if [[ "$temp_root" == /tmp/* ]]; then
        rm -rf -- "$temp_root"
    fi
}
trap cleanup EXIT

run_signal_case() {
    local signal_name=$1
    local signal_number=$2
    local expected_status=$3
    local case_dir="$temp_root/$signal_name"
    local config_file="$case_dir/continuous.conf"
    local output_file="$case_dir/program.out"
    local log_file="$case_dir/runtime/car_terminal.log"
    local csv_file="$case_dir/runtime/vehicle_data.csv"

    mkdir -p "$case_dir/runtime"
    cat >"$config_file" <<'CONFIG'
data_source=simulator
run_mode=continuous
low_voltage_threshold_v=12.55
high_temperature_threshold_c=40.0
overspeed_threshold_kmh=120.0
sample_count=2
sampling_interval_ms=10
queue_capacity=2
log_file=runtime/car_terminal.log
csv_file=runtime/vehicle_data.csv
CONFIG

    "$binary" --config "$config_file" >"$output_file" 2>&1 &
    local program_pid=$!
    local ready=0
    for _ in $(seq 1 300); do
        if ! kill -0 "$program_pid" 2>/dev/null; then
            break
        fi
        if [[ -f "$csv_file" ]] &&
           [[ $(wc -l <"$csv_file") -ge 4 ]]; then
            ready=1
            break
        fi
        sleep 0.01
    done

    if [[ "$ready" -ne 1 ]]; then
        printf '%s case did not continue past sample_count=2\n' \
            "$signal_name" >&2
        wait "$program_pid" 2>/dev/null || true
        sed -n '1,180p' "$output_file" >&2
        return 1
    fi

    kill -s "$signal_name" "$program_pid"
    set +e
    wait "$program_pid"
    local status=$?
    set -e

    if [[ "$status" -ne "$expected_status" ]]; then
        printf 'Expected exit %s after %s, got %s\n' \
            "$expected_status" "$signal_name" "$status" >&2
        sed -n '1,180p' "$output_file" >&2
        return 1
    fi

    local csv_lines
    local data_rows
    local logged_samples
    local last_sample
    csv_lines=$(wc -l <"$csv_file")
    data_rows=$((csv_lines - 1))
    logged_samples=$(grep -Ec '\[(INFO|WARNING)\] Sample ' "$log_file")
    last_sample=$(tail -n 1 "$csv_file" | cut -d, -f1)
    if [[ "$data_rows" -lt 3 ]] ||
       [[ "$logged_samples" -ne "$data_rows" ]] ||
       [[ "$last_sample" -ne "$data_rows" ]]; then
        printf '%s CSV/log drain mismatch: rows=%s logs=%s last=%s\n' \
            "$signal_name" "$data_rows" "$logged_samples" \
            "$last_sample" >&2
        return 1
    fi

    if ! grep -Fq "Monitoring stopped by signal $signal_number after draining queued samples" \
            "$log_file"; then
        printf '%s graceful-stop log entry is missing\n' "$signal_name" >&2
        sed -n '1,180p' "$log_file" >&2
        return 1
    fi
    if ! grep -Fq 'Run mode: continuous (until SIGINT/SIGTERM)' \
            "$output_file"; then
        printf '%s continuous-mode output is missing\n' "$signal_name" >&2
        return 1
    fi
}

run_signal_case INT 2 130
run_signal_case TERM 15 143
