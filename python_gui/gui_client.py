"""Tic-Tac-Toe serial client (Tkinter + pyserial).

Thin client: the STM32 owns all game state and rules. The board is redrawn ONLY
from server replies (OK:BOARD:..., BOARD:...). After a WIN/DRAW the client asks
the server for STATE instead of guessing the final mark.

    pip install pyserial
    python gui_client.py
"""
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

import serial
import serial.tools.list_ports

BAUD = 115200
HANDSHAKE_PERIOD_MS = 600
HANDSHAKE_TRIES = 15           # ~9 s, covers MCU boot / reset on port open
REPLY_TIMEOUT_MS = 2000


class TicTacToeClient:
    def __init__(self, root):
        self.root = root
        self.root.title("Tic-Tac-Toe Serial Client")
        self.ser = None
        self.rx_queue = queue.Queue()
        self.connected = False        # server answered OK to CONNECT
        self.game_over = True         # nothing playable until the server confirms NEW
        self.busy = False             # one request in flight at a time
        self.busy_after = None
        self.handshake_tries = 0
        self.result_text = None       # "Winner: X" / "Draw", shown once STATE arrives
        self.board = ["."] * 9

        self._build_ui()
        self._refresh_ports()
        self._set_controls()
        self.root.after(50, self._poll_queue)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        top = ttk.Frame(self.root, padding=5)
        top.pack(fill=tk.X)
        ttk.Label(top, text="Port:").pack(side=tk.LEFT)
        self.port_cb = ttk.Combobox(top, width=14, state="readonly")
        self.port_cb.pack(side=tk.LEFT, padx=5)
        ttk.Button(top, text="Refresh", command=self._refresh_ports).pack(side=tk.LEFT)
        self.connect_btn = ttk.Button(top, text="Connect", command=self._connect)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        self.disconnect_btn = ttk.Button(top, text="Disconnect", command=self._disconnect)
        self.disconnect_btn.pack(side=tk.LEFT)

        self.status = tk.StringVar(value="Disconnected")
        ttk.Label(self.root, textvariable=self.status, relief=tk.SUNKEN,
                  anchor=tk.W).pack(fill=tk.X)

        board_frame = ttk.Frame(self.root, padding=10)
        board_frame.pack()
        self.buttons = []
        for i in range(9):
            b = tk.Button(board_frame, text="", font=("Arial", 24), width=3, height=1,
                          command=lambda idx=i: self._send_move(idx))
            b.grid(row=i // 3, column=i % 3, padx=2, pady=2)
            self.buttons.append(b)

        ctrl = ttk.Frame(self.root, padding=5)
        ctrl.pack()
        self.new_btn = ttk.Button(ctrl, text="New Game", command=self._new_game)
        self.new_btn.pack(side=tk.LEFT, padx=5)
        self.resign_btn = ttk.Button(ctrl, text="Resign", command=self._resign)
        self.resign_btn.pack(side=tk.LEFT, padx=5)

    def _set_controls(self):
        """Single place that decides what is clickable."""
        playable = self.connected and not self.game_over
        for b in self.buttons:
            b.config(state=tk.NORMAL if playable else tk.DISABLED)
        self.connect_btn.config(state=tk.DISABLED if self.ser else tk.NORMAL)
        self.disconnect_btn.config(state=tk.NORMAL if self.ser else tk.DISABLED)
        self.new_btn.config(state=tk.NORMAL if self.connected else tk.DISABLED)
        self.resign_btn.config(state=tk.NORMAL if playable else tk.DISABLED)

    # -------------------------------------------------------------- serial
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cb["values"] = ports
        if ports and not self.port_cb.get():
            self.port_cb.current(0)

    def _connect(self):
        port = self.port_cb.get()
        if not port:
            messagebox.showerror("Error", "No serial port selected")
            return
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = port, BAUD, 0.2
            s.dtr = False             # do not pulse reset when the port opens (if supported)
            s.rts = False
            s.open()
        except Exception as e:
            messagebox.showerror("Serial error", str(e))
            return

        self.ser = s
        self.connected = False
        self.game_over = True
        self.handshake_tries = 0
        self.status.set(f"Connecting to {port} ...")
        self._set_controls()
        threading.Thread(target=self._read_thread, args=(s,), daemon=True).start()
        self._handshake()

    def _handshake(self):
        """Resend CONNECT until the server answers OK (it may still be booting)."""
        if not self.ser or self.connected:
            return
        if self.handshake_tries >= HANDSHAKE_TRIES:
            self.status.set("No reply from board - check port, power and firmware")
            self._close_port()
            return
        self.handshake_tries += 1
        self._send("CONNECT")
        self.root.after(HANDSHAKE_PERIOD_MS, self._handshake)

    def _disconnect(self):
        if self.ser:
            self._send("DISCONNECT")
        self._close_port()
        self.status.set("Disconnected")

    def _close_port(self):
        ser, self.ser = self.ser, None      # detach first so the reader thread exits
        if ser:
            try:
                ser.flush()
                ser.close()
            except Exception:
                pass
        self.connected = False
        self.game_over = True
        self.busy = False
        self.result_text = None
        self._set_controls()

    def _on_close(self):
        self._disconnect()
        self.root.destroy()

    def _read_thread(self, ser):
        while True:
            try:
                if not ser.is_open:
                    break
                line = ser.readline().decode(errors="replace").strip()
            except Exception:
                break
            if line:
                self.rx_queue.put(line)

    # ------------------------------------------------------------ protocol
    def _poll_queue(self):
        while not self.rx_queue.empty():
            self._handle_line(self.rx_queue.get())
        self.root.after(50, self._poll_queue)

    def _handle_line(self, line):
        # Drop replies that arrive after we closed the port.
        if not self.ser:
            return

        if line == "OK":
            if not self.connected:
                self.connected = True
                self.status.set("Connected - press New Game")
            self._request_done()

        elif line.startswith("OK:BOARD:"):            # NEW, accepted MOVE, or button-initiated new game
            self._set_board(line[len("OK:BOARD:"):])
            self.game_over = False
            self.result_text = None
            self.status.set("Game in progress")
            self._request_done()

        elif line.startswith("BOARD:"):               # reply to STATE
            self._set_board(line[len("BOARD:"):])
            if self.result_text:
                self.status.set(self.result_text)
            self._request_done()

        elif line.startswith("WIN:"):
            self.game_over = True
            self.result_text = f"Winner: {line[4:]}"
            self.status.set(self.result_text)
            self._request_done()
            self._request("STATE")                    # get the authoritative final board

        elif line == "DRAW":
            self.game_over = True
            self.result_text = "Draw"
            self.status.set(self.result_text)
            self._request_done()
            self._request("STATE")

        elif line == "OK:GAMEOVER":
            self.game_over = True
            self.status.set("You resigned - game over")
            self._request_done()

        elif line.startswith("NACK:"):
            reason = line[5:]
            self._request_done()
            if reason == "NOGAME":
                self.game_over = True                 # server says there is no live game
            messagebox.showwarning("Rejected by server", self._explain(reason))

        else:                                         # boot banner etc.
            self.status.set(line)
        self._set_controls()

    @staticmethod
    def _explain(reason):
        return {
            "OCCUPIED": "That cell is already taken.",
            "RANGE": "Cell index out of range.",
            "NOTURN": "It is not that player's turn.",
            "NOGAME": "There is no game in progress. Press New Game.",
            "BADCMD": "The server did not accept that command in its current state.",
            "NOCONN": "Not connected. Press Connect.",
            "TOOLONG": "Message too long.",
            "TIMEOUT": "Message incomplete (timed out).",
        }.get(reason, reason)

    def _set_board(self, board_str):
        if len(board_str) != 9 or any(c not in ".XO" for c in board_str):
            return                                    # not a valid board: ignore, never guess
        self.board = list(board_str)
        for i, ch in enumerate(self.board):
            self.buttons[i].config(text="" if ch == "." else ch)

    # ------------------------------------------------------------ outbound
    def _send(self, cmd):
        if self.ser and self.ser.is_open:
            try:
                self.ser.write((cmd + "\n").encode())
                self.ser.flush()
            except Exception as e:
                self.status.set(f"Write error: {e}")

    def _request(self, cmd):
        """Send one request; ignore further clicks until the reply (or a timeout)."""
        if self.busy:
            return
        self.busy = True
        if self.busy_after:
            self.root.after_cancel(self.busy_after)
        self.busy_after = self.root.after(REPLY_TIMEOUT_MS, self._request_timeout)
        self._send(cmd)

    def _request_done(self):
        self.busy = False
        if self.busy_after:
            self.root.after_cancel(self.busy_after)
            self.busy_after = None

    def _request_timeout(self):
        self.busy = False
        self.busy_after = None
        self.status.set("No reply from board")

    def _send_move(self, idx):
        if self.game_over:
            return
        self._request(f"MOVE:{idx}")      # legality is decided by the server only

    def _new_game(self):
        self._request("NEW")

    def _resign(self):
        self._request("RESIGN")           # UI changes only when the server confirms


if __name__ == "__main__":
    root = tk.Tk()
    TicTacToeClient(root)
    root.mainloop()