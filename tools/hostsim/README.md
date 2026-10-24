# Host simulation (PC build of the firmware logic)

Compiles `Core/Src/main.c` for a PC with the ST HAL replaced by tiny stubs
(`stub/main.h`, `harness.c`), connects it to a pseudo-terminal and runs the real
pytest suite against it. It checks the protocol, game logic, AI and checksum code
**before flashing**. It is **not** a board test: it has no real UART timing, so it
cannot find interrupt-latency problems (the overrun we saw on the board only
appeared on hardware).

    bash run_hostsim.sh     # Linux / macOS / WSL; needs gcc, python3, pytest, pyserial

It prints the AI statistics (all possible games against the AI, and the largest
number of search calls in one decision), then the pytest results in plain mode and
with `USE_CRC=1`.
