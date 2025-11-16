#!/bin/bash
# ============================================================
# Validator Runner - Long Dataset Evaluator
# ============================================================

INPUT_DIR="../data/long"
OUTPUT_DIR="../output/nga/long"
LOG_DIR="validate_results/nga/long"

# Create necessary directories
mkdir -p "$OUTPUT_DIR"
mkdir -p "$LOG_DIR"

# Four dataset prefixes
PREFIXES=(35 42 49 56)

echo "============================================================"
echo "Running validator on long dataset test cases"
echo "============================================================"

# Loop over prefixes
for prefix in "${PREFIXES[@]}"; do

    # Each prefix has 6 instances
    for idx in {1..6}; do

        input_file="${INPUT_DIR}/l${prefix}_${idx}.json"
        output_file="${OUTPUT_DIR}/sol_ln_ga${prefix}_${idx}.json"
        log_file="${LOG_DIR}/log_${prefix}_${idx}.txt"

        echo "[Long ${prefix}_${idx}] Processing: $input_file -> $output_file"

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
done

echo "============================================================"
echo "All long dataset validations completed"
echo "Logs location: $LOG_DIR"
echo "============================================================"
