#!/usr/bin/env python3
"""
Telemetria do projeto de armazenamento de energia por gravidade.

Le as linhas CSV enviadas pela Pico (via HC-06/Bluetooth ou USB serial),
registra tudo em arquivo e plota ao vivo. Tambem permite enviar comandos
(up / down / stop / brake / release / cal) e reprocessar um log salvo.

Protocolo (uma tag por fonte, ver telemetry_task.c):
    D,t_ms,dist_cm,vel_cm_s
    I,t_ms,vel_imu_cm_s,tilt_deg,accel_g
    P,t_ms,tensao_V,corrente_A,potencia_W
    S,t_ms,modo            (0=IDLE 1=SUBINDO 2=DESCENDO 3=FREADO)

Exemplos:
    # Ao vivo, plotando e logando:
    python3 tools/telemetry.py --port /dev/rfcomm0 --log sessao.csv

    # So logar (sem plot), util em maquina sem display:
    python3 tools/telemetry.py --port /dev/ttyACM0 --no-plot

    # Reprocessar/plotar um log salvo, offline:
    python3 tools/telemetry.py --replay sessao.csv

    # Enviar um comando e sair:
    python3 tools/telemetry.py --port /dev/rfcomm0 --send down

Dependencias: pyserial (para ler a serial) e matplotlib (para plotar).
    pip install pyserial matplotlib
"""

import argparse
import sys
import threading
import time
from collections import deque

MODE_NAMES = {0: "IDLE", 1: "SUBINDO", 2: "DESCENDO", 3: "FREADO"}

# Janela deslizante de amostras exibidas no plot ao vivo.
WINDOW = 600


class Telemetry:
    """Buffers por fonte, com timestamp em segundos."""

    def __init__(self, window=WINDOW):
        mk = lambda: deque(maxlen=window)
        # distancia
        self.d_t, self.d_dist, self.d_vel = mk(), mk(), mk()
        # imu
        self.i_t, self.i_vel, self.i_tilt, self.i_acc = mk(), mk(), mk(), mk()
        # potencia
        self.p_t, self.p_v, self.p_i, self.p_w = mk(), mk(), mk(), mk()
        # estado (mudancas de modo)
        self.s_t, self.s_mode = [], []
        self.lock = threading.Lock()

    def parse(self, line):
        """Interpreta uma linha CSV. Retorna True se reconhecida."""
        parts = line.strip().split(",")
        if len(parts) < 2:
            return False
        tag = parts[0]
        try:
            t = float(parts[1]) / 1000.0  # ms -> s
            with self.lock:
                if tag == "D" and len(parts) >= 4:
                    self.d_t.append(t)
                    self.d_dist.append(float(parts[2]))
                    self.d_vel.append(float(parts[3]))
                elif tag == "I" and len(parts) >= 5:
                    self.i_t.append(t)
                    self.i_vel.append(float(parts[2]))
                    self.i_tilt.append(float(parts[3]))
                    self.i_acc.append(float(parts[4]))
                elif tag == "P" and len(parts) >= 5:
                    self.p_t.append(t)
                    self.p_v.append(float(parts[2]))
                    self.p_i.append(float(parts[3]))
                    self.p_w.append(float(parts[4]))
                elif tag == "S" and len(parts) >= 3:
                    self.s_t.append(t)
                    self.s_mode.append(int(float(parts[2])))
                else:
                    return False
            return True
        except (ValueError, IndexError):
            return False


def reader_thread(ser, tlm, logfile, stop):
    """Le linhas da serial, parseia e loga cru."""
    while not stop.is_set():
        try:
            raw = ser.readline()
        except Exception as e:
            print(f"[erro serial] {e}", file=sys.stderr)
            break
        if not raw:
            continue
        line = raw.decode("utf-8", errors="replace").strip()
        if not line:
            continue
        if tlm.parse(line):
            if logfile:
                logfile.write(line + "\n")
                logfile.flush()
        else:
            # Linhas de debug/[CTRL] etc. so ecoam no terminal.
            print("  " + line)


def stdin_command_thread(ser, stop):
    """Le comandos do teclado e envia pela serial (linha + \\n)."""
    print("Comandos: up | down | stop | brake | release | cal   (Ctrl-C p/ sair)")
    while not stop.is_set():
        try:
            cmd = input()
        except (EOFError, KeyboardInterrupt):
            stop.set()
            break
        cmd = cmd.strip()
        if cmd:
            ser.write((cmd + "\n").encode("utf-8"))
            print(f"[enviado] {cmd}")


def run_live(args):
    try:
        import serial
    except ImportError:
        sys.exit("pyserial nao instalado. Rode: pip install pyserial")

    ser = serial.Serial(args.port, args.baud, timeout=1)
    print(f"Conectado em {args.port} @ {args.baud} baud")

    # Modo one-shot: envia um comando e sai.
    if args.send:
        ser.write((args.send + "\n").encode("utf-8"))
        print(f"[enviado] {args.send}")
        ser.close()
        return

    tlm = Telemetry()
    logfile = open(args.log, "w") if args.log else None
    stop = threading.Event()

    t_read = threading.Thread(target=reader_thread,
                              args=(ser, tlm, logfile, stop), daemon=True)
    t_read.start()

    t_cmd = threading.Thread(target=stdin_command_thread,
                             args=(ser, stop), daemon=True)
    t_cmd.start()

    if args.no_plot:
        try:
            while not stop.is_set():
                time.sleep(0.2)
        except KeyboardInterrupt:
            pass
    else:
        live_plot(tlm, stop)

    stop.set()
    ser.close()
    if logfile:
        logfile.close()
    print("Encerrado.")


def make_figure(tlm):
    import matplotlib.pyplot as plt

    fig, axs = plt.subplots(4, 1, figsize=(10, 9), sharex=True)
    fig.suptitle("Armazenamento de energia por gravidade - telemetria")

    axs[0].set_ylabel("Distancia (cm)")
    axs[1].set_ylabel("Velocidade (cm/s)")
    axs[2].set_ylabel("Inclinacao (deg)")
    axs[3].set_ylabel("Tensao/Corrente/Pot")
    axs[3].set_xlabel("tempo (s)")

    lines = {
        "dist": axs[0].plot([], [], "b-", label="dist")[0],
        "vdist": axs[1].plot([], [], "b-", label="v (distancia)")[0],
        "vimu": axs[1].plot([], [], "r-", label="v (IMU)")[0],
        "tilt": axs[2].plot([], [], "g-", label="tilt")[0],
        "volt": axs[3].plot([], [], "m-", label="V")[0],
        "curr": axs[3].plot([], [], "c-", label="A")[0],
        "pow": axs[3].plot([], [], "k-", label="W")[0],
    }
    axs[2].axhline(20.0, color="r", ls="--", lw=0.8, label="alarme")
    for ax in axs:
        ax.legend(loc="upper left", fontsize=8)
        ax.grid(True, alpha=0.3)
    return fig, axs, lines


def live_plot(tlm, stop):
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    fig, axs, lines = make_figure(tlm)

    def update(_):
        with tlm.lock:
            lines["dist"].set_data(list(tlm.d_t), list(tlm.d_dist))
            lines["vdist"].set_data(list(tlm.d_t), list(tlm.d_vel))
            lines["vimu"].set_data(list(tlm.i_t), list(tlm.i_vel))
            lines["tilt"].set_data(list(tlm.i_t), list(tlm.i_tilt))
            lines["volt"].set_data(list(tlm.p_t), list(tlm.p_v))
            lines["curr"].set_data(list(tlm.p_t), list(tlm.p_i))
            lines["pow"].set_data(list(tlm.p_t), list(tlm.p_w))
        for ax in axs:
            ax.relim()
            ax.autoscale_view()
        return list(lines.values())

    _ani = FuncAnimation(fig, update, interval=200, cache_frame_data=False)
    try:
        plt.show()
    except KeyboardInterrupt:
        pass
    stop.set()


def run_replay(path):
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        sys.exit("matplotlib nao instalado. Rode: pip install matplotlib")

    tlm = Telemetry(window=10 ** 9)  # sem janela: carrega tudo
    with open(path) as f:
        for line in f:
            tlm.parse(line)

    _fig, axs, lines = make_figure(tlm)
    with tlm.lock:
        lines["dist"].set_data(list(tlm.d_t), list(tlm.d_dist))
        lines["vdist"].set_data(list(tlm.d_t), list(tlm.d_vel))
        lines["vimu"].set_data(list(tlm.i_t), list(tlm.i_vel))
        lines["tilt"].set_data(list(tlm.i_t), list(tlm.i_tilt))
        lines["volt"].set_data(list(tlm.p_t), list(tlm.p_v))
        lines["curr"].set_data(list(tlm.p_t), list(tlm.p_i))
        lines["pow"].set_data(list(tlm.p_t), list(tlm.p_w))
    for ax in axs:
        ax.relim()
        ax.autoscale_view()

    # Marca as trocas de modo como linhas verticais.
    for t, m in zip(tlm.s_t, tlm.s_mode):
        for ax in axs:
            ax.axvline(t, color="gray", ls=":", lw=0.6)
        axs[0].text(t, axs[0].get_ylim()[1], MODE_NAMES.get(m, "?"),
                    fontsize=7, rotation=90, va="top")
    print(f"Amostras: D={len(tlm.d_t)} I={len(tlm.i_t)} P={len(tlm.p_t)} "
          f"trocas de modo={len(tlm.s_t)}")
    plt.show()


def main():
    ap = argparse.ArgumentParser(description="Telemetria/plot da Pico (CSV via serial).")
    ap.add_argument("--port", help="porta serial (ex.: /dev/rfcomm0, /dev/ttyACM0)")
    ap.add_argument("--baud", type=int, default=115200, help="baud rate (default 115200)")
    ap.add_argument("--log", help="arquivo para salvar as linhas CSV recebidas")
    ap.add_argument("--no-plot", action="store_true", help="apenas logar, sem plotar")
    ap.add_argument("--send", help="envia um comando e sai (up/down/stop/brake/release/cal)")
    ap.add_argument("--replay", help="plota a partir de um log salvo (offline)")
    args = ap.parse_args()

    if args.replay:
        run_replay(args.replay)
    elif args.port:
        run_live(args)
    else:
        ap.error("informe --port (ao vivo) ou --replay <arquivo>")


if __name__ == "__main__":
    main()
