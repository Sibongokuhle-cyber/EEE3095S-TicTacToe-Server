"""Measure how long the board takes to answer a MOVE, to estimate the AI's search time.

    python tools/time_ai.py COM14 [repeats]        (close the GUI / pytest first)

For each first move (cells 0-8) it times
  * a two-player game:  NEW  then MOVE:<i>       (no AI, this is the serial round-trip), and
  * an AI game:         NEW:AI then MOVE:<i>     (round-trip + the AI's search).
The AI game's first reply is the AI's most expensive decision. The difference
between the two columns estimates the search time on the board. USB-serial jitter
is a few milliseconds, so look at the mean and max over several repeats.
"""
import statistics
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM14"
repeats = int(sys.argv[2]) if len(sys.argv) > 2 else 5


def cmd(s, text, wait=True):
    s.write((text + "\n").encode())
    s.flush()
    return s.readline().decode(errors="replace").strip() if wait else None


with serial.Serial(port, 115200, timeout=3) as s:
    for _ in range(20):                       # sync: the board may still be booting
        if cmd(s, "CONNECT") == "OK":
            break
    time.sleep(0.5)                           # let any extra replies to repeated CONNECTs arrive
    s.reset_input_buffer()
    assert cmd(s, "CONNECT") == "OK", "board does not answer CONNECT"

    def timed_first_move(new_cmd, idx):
        cmd(s, "RESIGN")                      # end any game in progress (reply ignored)
        reply = cmd(s, new_cmd)
        assert reply.startswith("OK:BOARD:"), f"{new_cmd} -> {reply!r}"
        t = time.perf_counter()
        reply = cmd(s, f"MOVE:{idx}")
        ms = (time.perf_counter() - t) * 1000
        assert reply.startswith("OK:BOARD:"), f"MOVE:{idx} -> {reply!r}"
        return ms

    print(f"{'first move':>10} | {'2-player ms (min/mean/max)':>28} | {'AI game ms (min/mean/max)':>28}")
    worst_ai = 0.0
    for idx in range(9):
        plain = [timed_first_move("NEW", idx) for _ in range(repeats)]
        ai = [timed_first_move("NEW:AI", idx) for _ in range(repeats)]
        worst_ai = max(worst_ai, max(ai))
        f = lambda v: f"{min(v):6.1f} /{statistics.mean(v):6.1f} /{max(v):6.1f}"
        print(f"{idx:>10} | {f(plain):>28} | {f(ai):>28}")
    cmd(s, "RESIGN")
    cmd(s, "DISCONNECT")
    print(f"\nslowest AI game reply: {worst_ai:.1f} ms (search time = this minus the 2-player round trip)")
