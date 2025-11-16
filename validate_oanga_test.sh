#!/bin/bash

# Usage: ./run_all_tests.sh <start_index> <end_index> <padding>
# Example: ./run_all_tests.sh 0 9 2
# Input:  ../data/test/test00.json ... test09.json
# Output: ../output/nga/test/sol_ga0.json ... sol_ga9.json

START=${1:-0}
END=${2:-10}
PADDING=${3:-2}

LOG_DIR="validate_results/oanga/test"
mkdir -p "$LOG_DIR"  # create log folder if not exist

for ((i=$START; i<=$END; i++)); do
    padded_num=$(printf "%0${PADDING}d" $i)
    input_file="../data/test/test${padded_num}.json"
    output_file="output/oanga/test/sol_test_ga${padded_num}.json"
    log_file="${LOG_DIR}/log${padded_num}.txt"

    echo "=== Running test ${padded_num} (output -> sol_new_ga${i}.json) ==="
    
    if [[ -f "$input_file" ]]; then
        ./IHTP_Validator "$input_file" "$output_file" >"$log_file" 2>&1
        echo "✅ Finished test ${padded_num} — log saved at $log_file"
    else
        echo "⚠️  Input file not found: $input_file — skipping."
    fi
    
    echo ""
done

echo "🎯 All runs completed. Check logs in: $LOG_DIR"
