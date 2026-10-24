"""Run the pytest suite against the PC build of the firmware (./fwsim) through a pseudo-terminal.
Usage: python3 run_tests.py [NAME=VALUE ...]     e.g.  python3 run_tests.py USE_CRC=1
Linux / macOS / WSL only (uses pty)."""
import os, pty, sys, subprocess, threading, tty

here = os.path.dirname(os.path.abspath(__file__))
extra = dict(a.split("=", 1) for a in sys.argv[1:])
master, slave = pty.openpty()
tty.setraw(slave)
port = os.ttyname(slave)
fw = subprocess.Popen([os.path.join(here, "fwsim")], stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=0)

def to_fw():
    while True:
        try:
            d = os.read(master, 256)
        except OSError:
            break
        if not d:
            break
        fw.stdin.write(d); fw.stdin.flush()

def from_fw():
    while True:
        d = os.read(fw.stdout.fileno(), 256)
        if not d:
            break
        os.write(master, d)

threading.Thread(target=to_fw, daemon=True).start()
threading.Thread(target=from_fw, daemon=True).start()
env = dict(os.environ, SERIAL_PORT=port, **extra)
tests = os.path.join(here, "..", "..", "tests")
rc = subprocess.call([sys.executable, "-m", "pytest", "-q", "-p", "no:cacheprovider", tests], env=env)
fw.kill()
sys.exit(rc)
