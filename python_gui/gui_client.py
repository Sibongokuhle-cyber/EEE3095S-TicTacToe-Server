import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import queue

class TicTacToeClient:
    def __init__(self, root):
        self.root = root
        self.root.title("Tic-Tac-Toe Serial Client")
        self.ser = None
        self.rx_queue = queue.Queue()
        self.game_over = False
        self.board = ['.'] * 9

        self._build_ui()
        self._refresh_ports()
        self.root.after(50, self._poll_queue)

    def _build_ui(self):
        top = ttk.Frame(self.root, padding=5)
        top.pack(fill=tk.X)

        ttk.Label(top, text="Port:").pack(side=tk.LEFT)
        self.port_cb = ttk.Combobox(top, width=12, state="readonly")
        self.port_cb.pack(side=tk.LEFT, padx=5)
        ttk.Button(top, text="Refresh", command=self._refresh_ports).pack(side=tk.LEFT)
        self.connect_btn = ttk.Button(top, text="Connect", command=self._connect)
        self.connect_btn.pack(side=tk.LEFT, padx=5)
        self.disconnect_btn = ttk.Button(top, text="Disconnect", command=self._disconnect, state=tk.DISABLED)
        self.disconnect_btn.pack(side=tk.LEFT)

        self.status = tk.StringVar(value="Disconnected")
        ttk.Label(self.root, textvariable=self.status, relief=tk.SUNKEN, anchor=tk.W).pack(fill=tk.X)

        board_frame = ttk.Frame(self.root, padding=10)
        board_frame.pack()
        self.buttons = []
        for i in range(9):
            b = tk.Button(board_frame, text="", font=("Arial", 24), width=3, height=1,
                          command=lambda idx=i: self._send_move(idx))
            b.grid(row=i//3, column=i%3, padx=2, pady=2)
            self.buttons.append(b)

        ctrl = ttk.Frame(self.root, padding=5)
        ctrl.pack()
        ttk.Button(ctrl, text="New Game", command=self._new_game).pack(side=tk.LEFT, padx=5)
        ttk.Button(ctrl, text="Resign", command=self._resign).pack(side=tk.LEFT, padx=5)

    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cb['values'] = ports
        if ports:
            self.port_cb.current(0)

    def _connect(self):
        port = self.port_cb.get()
        if not port:
            messagebox.showerror("Error", "No serial port selected")
            return
        try:
            self.ser = serial.Serial(port, 115200, timeout=1)
            self.status.set(f"Connected to {port}")
            self.connect_btn.config(state=tk.DISABLED)
            self.disconnect_btn.config(state=tk.NORMAL)
            threading.Thread(target=self._read_thread, daemon=True).start()
            self._send("CONNECT")
        except Exception as e:
            messagebox.showerror("Serial error", str(e))

    def _disconnect(self):
        if self.ser:
            try:
                self._send("DISCONNECT")
                self.ser.close()
            except Exception:
                pass
        self.ser = None
        self.status.set("Disconnected")
        self.connect_btn.config(state=tk.NORMAL)
        self.disconnect_btn.config(state=tk.DISABLED)

    def _read_thread(self):
        while self.ser and self.ser.is_open:
            try:
                line = self.ser.readline().decode(errors='ignore').strip()
                if line:
                    self.rx_queue.put(line)
            except Exception:
                break

    def _poll_queue(self):
        while not self.rx_queue.empty():
            line = self.rx_queue.get()
            self._handle_line(line)
        self.root.after(50, self._poll_queue)

    def _handle_line(self, line):
        if line.startswith("OK: BOARD: "):
            self._set_board(line[10:])
            self.game_over = False
        elif line.startswith("BOARD: "):
            self._set_board(line[7:])
        elif line.startswith("WIN: "):
            winner = line[5:]
            self.status.set(f"Winner: {winner}")
            self._set_board(self.board)
            self.game_over = True
            self._disable_board()
        elif line == "DRAW":
            self.status.set("Draw")
            self.game_over = True
            self._disable_board()
        elif line.startswith("NACK: "):
            messagebox.showwarning("Rejected", line)
        elif line == "OK":
            self.status.set("OK")
        else:
            self.status.set(line)

    def _set_board(self, board_str):
        if len(board_str) != 9:
            return
        self.board = list(board_str)
        for i, ch in enumerate(self.board):
            self.buttons[i].config(text="" if ch == '.' else ch)

    def _disable_board(self):
        for b in self.buttons:
            b.config(state=tk.DISABLED)

    def _enable_board(self):
        for b in self.buttons:
            b.config(state=tk.NORMAL)

    def _send(self, cmd):
        if self.ser and self.ser.is_open:
            self.ser.write((cmd + "\n").encode())

    def _send_move(self, idx):
        if self.game_over:
            return
        self._send(f"MOVE:{idx}")

    def _new_game(self):
        self._send("NEW")
        self.game_over = False
        self._enable_board()
        self.status.set("New game")

    def _resign(self):
        self._send("RESIGN")
        self.game_over = True
        self._disable_board()
        self.status.set("Resigned")

if __name__ == "__main__":
    root = tk.Tk()
    app = TicTacToeClient(root)
    root.mainloop()