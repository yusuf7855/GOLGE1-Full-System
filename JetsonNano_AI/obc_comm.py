# GOLGE-1 OBC (STM32) UART Haberlesme

import serial
import json
import time
import threading
import logging
from config import *

log = logging.getLogger("OBC")


class OBCLink:
    """STM32 ile UART haberlesme"""

    def __init__(self):
        self.ser = None
        self.connected = False
        self.rx_buffer = ""
        self.lock = threading.Lock()
        self.last_heartbeat = 0
        self.current_mode = "IDLE"  # IDLE, OPTICAL, RF, SLEEP

    def connect(self):
        try:
            self.ser = serial.Serial(
                port=UART_PORT,
                baudrate=UART_BAUD,
                timeout=UART_TIMEOUT,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )
            self.connected = True
            log.info(f"OBC baglandi: {UART_PORT} @ {UART_BAUD}")
            return True
        except Exception as e:
            log.error(f"OBC baglanti hatasi: {e}")
            self.connected = False
            return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.connected = False

    def send_targets(self, targets):
        """Tespit edilen hedefleri STM32'ye gonder"""
        if not self.connected or not targets:
            return False

        payload = {
            "src": "JETSON",
            "ts": int(time.time()),
            "cnt": len(targets),
            "tgt": []
        }

        for t in targets:
            payload["tgt"].append({
                "id": t.get("id", "UNK"),
                "typ": t.get("type", "UNKNOWN"),
                "cls": t.get("classification", "UNKNOWN"),
                "lat": round(t.get("lat", 0), 6),
                "lon": round(t.get("lon", 0), 6),
                "alt": round(t.get("alt", 0), 1),
                "hdg": t.get("heading", 0),
                "spd": t.get("speed", 0),
                "cnf": round(t.get("confidence", 0), 2),
                "sen": t.get("sensor_id", 0),
                "sig": round(t.get("signal_db", 0), 1),
            })

        return self._send_json(payload)

    def send_heartbeat(self, status="OK"):
        """Periyodik hayatta kalma sinyali"""
        now = time.time()
        if (now - self.last_heartbeat) < HEARTBEAT_SEC:
            return True

        payload = {
            "src": "JETSON",
            "hb": status,
            "mode": self.current_mode,
            "ts": int(now)
        }
        self.last_heartbeat = now
        return self._send_json(payload)

    def send_status(self, gpu_temp, gpu_usage, mem_usage):
        """Jetson durum raporu"""
        payload = {
            "src": "JETSON",
            "stat": {
                "tmp": gpu_temp,
                "gpu": gpu_usage,
                "mem": mem_usage,
                "mode": self.current_mode
            }
        }
        return self._send_json(payload)

    def read_command(self):
        """STM32'den komut oku (non-blocking)"""
        if not self.connected:
            return None

        try:
            with self.lock:
                if self.ser.in_waiting > 0:
                    raw = self.ser.readline().decode("utf-8", errors="ignore").strip()
                    if raw:
                        return self._parse_command(raw)
        except Exception as e:
            log.warning(f"RX hatasi: {e}")

        return None

    def _parse_command(self, raw):
        """STM32 komut parsing"""
        try:
            cmd = json.loads(raw)
            cmd_type = cmd.get("cmd", "")

            if cmd_type == "SET_MODE":
                self.current_mode = cmd.get("mode", "IDLE")
                log.info(f"Mod degisti: {self.current_mode}")
                return cmd

            elif cmd_type == "SCAN_START":
                scan_type = cmd.get("type", "OPTICAL")
                self.current_mode = scan_type
                log.info(f"Tarama basladi: {scan_type}")
                return cmd

            elif cmd_type == "SCAN_STOP":
                self.current_mode = "IDLE"
                log.info("Tarama durduruldu")
                return cmd

            elif cmd_type == "SHUTDOWN":
                self.current_mode = "SLEEP"
                log.info("Uyku moduna geciliyor")
                return cmd

            return cmd

        except json.JSONDecodeError:
            # binary komut olabilir
            log.debug(f"JSON olmayan veri: {raw[:32]}")
            return None

    def _send_json(self, payload):
        """JSON gonder (newline-terminated)"""
        if not self.connected:
            return False

        try:
            data = json.dumps(payload, separators=(",", ":")) + "\n"
            with self.lock:
                self.ser.write(data.encode("utf-8"))
                self.ser.flush()
            return True
        except Exception as e:
            log.error(f"TX hatasi: {e}")
            return False

    def wait_for_mode(self, timeout=30):
        """STM32'den mod komutu bekle"""
        start = time.time()
        while (time.time() - start) < timeout:
            cmd = self.read_command()
            if cmd and cmd.get("cmd") in ("SET_MODE", "SCAN_START"):
                return self.current_mode
            time.sleep(0.1)
        return self.current_mode
