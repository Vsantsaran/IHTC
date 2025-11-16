#!/bin/bash
# ============================================================
# Validator Runner - Process multiple test instances
# ============================================================
# Usage: ./run_all_tests.sh <start_index> <end_index> <padding>
# Example: ./run_all_tests.sh 0 9 2
# Input:  ../data/test/test00.json ... test09.json
# Output: ../output/nga/test/sol_ga0.json ... sol_ga9.json
# ============================================================

START=${1:-1}
END=${2:-10}
PADDING=${3:-2}

INPUT_DIR="../data/test"
OUTPUT_DIR="../output/nga/test"
LOG_DIR="validate_logs"

# Create necessary directories
mkdir -p "$OUTPUT_DIR"
mkdir -p "$LOG_DIR"

echo "============================================================"
echo "Running validator on test cases ${START} to ${END}"
echo "============================================================"

for ((i=$START; i<=$END; i++)); do
    padded_num=$(printf "%0${PADDING}d" $i)
    input_file="${INPUT_DIR}/test${padded_num}.json"
    output_file="${OUTPUT_DIR}/sol_ga${i}.json"
    log_file="${LOG_DIR}/log${padded_num}.txt"

    echo "[Test ${padded_num}] Processing: $input_file -> $output_file"

    if [[ ! -f "$input_file" ]]; then
        echo "  WARNING: Input file not found - skipping"
        continue
    fi

    if [[ ! -f "./IHTP_Validator" ]]; then
        echo "  ERROR: IHTP_Validator executable not found"
        exit 1
    fi

    ./IHTP_Validator "$input_file" "$output_file" >"$log_file" 2>&1
    exit_code=$?

    if [[ $exit_code -eq 0 ]]; then
        echo "  COMPLETED - log: $log_file"
    else
        echo "  FAILED (exit code: $exit_code) - check: $log_file"
    fi

    echo ""
done

echo "============================================================"
echo "All validation runs completed"
echo "Logs location: $LOG_DIR"
echo "============================================================"
