#!/usr/bin/env bash
# Builds Core/Src/main.c for the PC (HAL stubbed out), prints the AI statistics,
# then runs the pytest suite against it twice: plain and with USE_CRC=1.
# Needs gcc, python3, pytest, pyserial. Linux / macOS / WSL.
set -e
cd "$(dirname "$0")"
sed 's/\r$//' ../../Core/Src/main.c > build_main.c
gcc -O1 -Wall -Wno-unused -Dmain=fw_main -c build_main.c -Istub -o main_sim.o
gcc -O1 harness.c main_sim.o -Istub -lpthread -o fwsim
sed '1i extern unsigned long ai_nodes;' build_main.c > build_main_cnt.c
gcc -O1 -Wno-unused -Dmain=fw_main '-DAI_NODE_HOOK()=(ai_nodes++)' -c build_main_cnt.c -Istub -o main_cnt.o
gcc -O1 ai_count.c main_cnt.o -Istub -o ai_count
echo "== AI statistics (every possible human strategy vs the AI)"
./ai_count
echo "== pytest, plain frames"
python3 run_tests.py
echo "== pytest, checksum on every request"
python3 run_tests.py USE_CRC=1
