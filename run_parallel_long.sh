#!/bin/bash
# =======================================================
# Parallel runner for C GA executable (compile + nohup)
# =======================================================

SRC="oanga.c cJSON.c"             # Source files
EXEC="./oanga_long"               # FIXED: Match compilation output
INPUT_DIR="../data/long"          # input folder
OUTPUT_DIR="output/oanga/long" # solution folder
LOG_DIR="logs/oanga/long"      # logs folder
TOTAL_RUNS=6                      # number of instances per group

# =======================================================
# COMPILE THE C CODE
# =======================================================
echo "🔧 Compiling cJSON.c ..."
gcc -O3 -march=native -mtune=native -c cJSON.c -o cJSON.o

if [[ $? -ne 0 ]]; then
    echo "❌ cJSON.c compilation failed! Aborting."
    exit 1
fi

echo "🔧 Compiling oanga.c and linking ..."
gcc -O3 -march=native -mtune=native -fopenmp oanga.c cJSON.o -o oanga_long -lm

if [[ $? -ne 0 ]]; then
    echo "❌ Compilation failed! Aborting."
    exit 1
fi

echo "✅ Compilation successful → $EXEC"

# =======================================================
# PREPARE FOLDERS
# =======================================================
mkdir -p "$OUTPUT_DIR"
mkdir -p "$LOG_DIR"

echo "🚀 Launching 24 GA runs in parallel (4 groups × 6 instances)..."
echo "📂 Inputs:  $INPUT_DIR"
echo "📂 Outputs: $OUTPUT_DIR"
echo "📂 Logs:    $LOG_DIR"

# =======================================================
# RUN ALL INSTANCES IN PARALLEL
# =======================================================

# Define the 4 file groups
FILE_PREFIXES=("l35_" "l42_" "l49_" "l56_")

# Counter for total runs
total_counter=0

# Loop through each file prefix group
for prefix in "${FILE_PREFIXES[@]}"; do
    echo ""
    echo "📋 Processing group: ${prefix}..."
    
    # Loop through 6 instances in each group
    for ((i=1; i<=TOTAL_RUNS; i++)); do
        total_counter=$((total_counter + 1))
        num=$(printf "%02d" $total_counter)
        
        INPUT_FILE="${INPUT_DIR}/${prefix}${i}.json"
        OUTPUT_FILE="${OUTPUT_DIR}/sol_ln_ga${num}.json"
        LOG_FILE="${LOG_DIR}/out${num}.log"
        
        if [[ -f "$INPUT_FILE" ]]; then
            echo "▶️  Starting run $num → $INPUT_FILE"
            nohup "$EXEC" "$INPUT_FILE" "$OUTPUT_FILE" > "$LOG_FILE" 2>&1 &
            sleep 0.5   # avoid process storms
        else
            echo "⚠️  Skipping run $num — file not found: $INPUT_FILE"
        fi
    done
done

echo ""
echo "✅ All 24 runs launched in background!"
echo "📊 Check logs in: $LOG_DIR"
echo "🔍 Check running jobs with: ps -u $USER | grep oanga_long"
echo "🔍 Count running jobs: ps -u $USER | grep oanga_long | wc -l"
