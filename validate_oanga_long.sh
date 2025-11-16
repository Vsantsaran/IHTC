#!/bin/bash
# ============================================================
# Validator Runner - Long Dataset Evaluator
# ============================================================

INPUT_DIR="../data/long"
OUTPUT_DIR="output/oanga/long"
LOG_DIR="validate_results/oanga/long"

# Create necessary directories
mkdir -p "$OUTPUT_DIR"
mkdir -p "$LOG_DIR"

# Dataset prefixes
PREFIXES=(35 42 49 56)

echo "============================================================"
echo "Running validator on long dataset test cases"
echo "============================================================"

counter=1

# Loop over prefixes
for prefix in "${PREFIXES[@]}"; do

    # Each prefix has 6 instances
    for idx in {1..6}; do

        padded_counter=$(printf "%02d" $counter)

        input_file="${INPUT_DIR}/l${prefix}_${idx}.json"
        output_file="${OUTPUT_DIR}/sol_ln_ga${padded_counter}.json"
        log_file="${LOG_DIR}/log_${padded_counter}.txt"

        echo "[Long ${prefix}_${idx}] -> sol_ln_ga${padded_counter}.json"

        if [[ ! -f "$input_file" ]]; then
            echo "  WARNING: Input file not found - skipping"
            counter=$((counter+1))
            continue
        fi

        if [[ ! -f "./IHTP_Validator" ]]; then
            echo "  ERROR: IHTP_Validator executable not found"
            exit 1
        fi

        ./IHTP_Validator "$input_file" "$output_file" > "$log_file" 2>&1
        exit_code=$?

        if [[ $exit_code -eq 0 ]]; then
            echo "  COMPLETED - log: $log_file"
        else
            echo "  FAILED (exit code: $exit_code) - check: $log_file"
        fi

        echo ""

        counter=$((counter+1))
    done
done

echo "============================================================"
echo "All long dataset validations completed"
echo "Output: sol_ln_ga01.json ... sol_ln_ga24.json"
echo "Logs: $LOG_DIR"
echo "============================================================"
