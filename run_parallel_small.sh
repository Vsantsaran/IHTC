#!/bin/bash
# =======================================================
# Parallel runner for C GA executable (compile + nohup)
# =======================================================

SRC="oanga.c cJSON.c"             # FIXED: Added cJSON.c
EXEC="./oanga_small"                    # executable to create
INPUT_DIR="../data/short"         # input folder
OUTPUT_DIR="output/oanga/small" # solution folder
LOG_DIR="logs/oanga/small"           # logs folder
TOTAL_RUNS=9                     # number of input files

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
gcc -O3 -march=native -mtune=native -fopenmp oanga.c cJSON.o -o oanga_small -lm

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

echo "🚀 Launching $TOTAL_RUNS GA runs in parallel..."
echo "📂 Inputs:  $INPUT_DIR"
echo "📂 Outputs: $OUTPUT_DIR"
echo "📂 Logs:    $LOG_DIR"

# =======================================================
# RUN ALL INSTANCES IN PARALLEL
# =======================================================
for ((i=1; i<=TOTAL_RUNS; i++)); do
    num=$(printf "%02d" $i)

    INPUT_FILE="${INPUT_DIR}/small${num}.json"
    OUTPUT_FILE="${OUTPUT_DIR}/sol_sm_ga${num}.json"
    LOG_FILE="${LOG_DIR}/out${num}.log"

    if [[ -f "$INPUT_FILE" ]]; then
        echo "▶️  Starting run $num → $INPUT_FILE"
        nohup "$EXEC" "$INPUT_FILE" "$OUTPUT_FILE" > "$LOG_FILE" 2>&1 &
        sleep 0.5   # avoid process storms
    else
        echo "⚠️  Skipping run $num — file not found: $INPUT_FILE"
    fi
done

echo "✅ All runs launched in background!"
echo "📊 Check logs in: $LOG_DIR"
echo "🔍 Check running jobs with: ps -u $USER | grep oanga_small"
