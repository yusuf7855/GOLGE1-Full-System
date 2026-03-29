#!/usr/bin/env python3
# GOLGE-1 Jetson Nano Ana Uygulama

import time
import signal
import sys
import logging
from config import *
from obc_comm import OBCLink
from optical_detector import OpticalDetector
from rf_analyzer import RFAnalyzer
from target_manager import TargetManager

# loglama
logging.basicConfig(
    level=getattr(logging, LOG_LEVEL),
    format="[%(asctime)s] %(name)-8s %(levelname)-5s %(message)s",
    datefmt="%H:%M:%S"
)
log = logging.getLogger("MAIN")

# ctrl+c
running = True
def signal_handler(sig, frame):
    global running
    log.info("Kapatiliyor...")
    running = False
signal.signal(signal.SIGINT, signal_handler)


class GolgeJetson:
    """GOLGE-1 Jetson Nano orkestrator"""

    def __init__(self):
        self.obc = OBCLink()
        self.optical = OpticalDetector()
        self.rf = RFAnalyzer()
        self.targets = TargetManager()
        self.mode = "IDLE"
        self.scan_count = 0

    def init(self):
        log.info("=" * 40)
        log.info("GOLGE-1 JETSON NANO v2.0")
        log.info("TUA Astro Hackathon 2026")
        log.info("=" * 40)

        # OBC baglantisi
        if self.obc.connect():
            log.info("STM32 OBC baglantisi OK")
        else:
            log.warning("OBC baglanti yok, standalone mod")

        # GPU kontrolu
        try:
            import torch
            if torch.cuda.is_available():
                gpu_name = torch.cuda.get_device_name(0)
                log.info(f"GPU: {gpu_name}")
            else:
                log.info("GPU yok, CPU modunda")
        except ImportError:
            log.info("PyTorch yuklu degil")

        log.info("Sistem hazir, komut bekleniyor...")

    def run(self):
        """Ana dongu"""
        self.init()

        while running:
            # OBC'den komut kontrol
            cmd = self.obc.read_command()
            if cmd:
                self.mode = self.obc.current_mode

            # heartbeat gonder
            self.obc.send_heartbeat()

            # mod bazli islem
            if self.mode == "OPTICAL":
                self._optical_scan()
            elif self.mode == "RF":
                self._rf_scan()
            elif self.mode == "SLEEP":
                time.sleep(POWER_MODE_SLEEP_SEC)
                continue
            else:
                # IDLE: OBC komutu bekle
                time.sleep(0.5)
                continue

            time.sleep(0.1)

        self._shutdown()

    def _optical_scan(self):
        """Goren Goz taramasi"""
        detections = self.optical.scan_once()
        self.scan_count += 1

        if detections:
            self.targets.add_detections(detections)

            # STM32'ye gonder
            priority_targets = self.targets.get_priority_targets()
            if priority_targets:
                self.obc.send_targets(priority_targets)
                log.info(f"[OPTIK] Tarama #{self.scan_count}: "
                        f"{len(detections)} tespit, "
                        f"{len(priority_targets)} hedef gonderildi")

    def _rf_scan(self):
        """Dinleyen Kulak taramasi"""
        detections = self.rf.scan_once()
        self.scan_count += 1

        if detections:
            self.targets.add_detections(detections)

            priority_targets = self.targets.get_priority_targets()
            if priority_targets:
                self.obc.send_targets(priority_targets)
                log.info(f"[RF] Tarama #{self.scan_count}: "
                        f"{len(detections)} tespit, "
                        f"{len(priority_targets)} hedef gonderildi")

    def _shutdown(self):
        log.info("Kapatiliyor...")
        self.optical.close_camera()
        self.rf.close_sdr()
        self.obc.disconnect()
        stats = self.targets.get_stats()
        log.info(f"Toplam: {stats['total_detected']} hedef tespit edildi")
        log.info("GOLGE-1 Jetson kapandi.")


def main():
    app = GolgeJetson()

    if len(sys.argv) > 1:
        # CLI modu: dogrudan tarama
        mode = sys.argv[1].upper()
        if mode == "OPTICAL":
            app.init()
            app.mode = "OPTICAL"
            app.obc.current_mode = "OPTICAL"
            log.info("Optik tarama basladi (CLI)")
            while running:
                app._optical_scan()
                time.sleep(1)
        elif mode == "RF":
            app.init()
            app.mode = "RF"
            app.obc.current_mode = "RF"
            log.info("RF tarama basladi (CLI)")
            while running:
                app._rf_scan()
                time.sleep(SDR_INTEGRATION_TIME)
        elif mode == "DEMO":
            app.init()
            log.info("Demo mod: 5 optik + 5 RF tarama")
            app.mode = "OPTICAL"
            for i in range(5):
                app._optical_scan()
                time.sleep(0.5)
            app.mode = "RF"
            for i in range(5):
                app._rf_scan()
                time.sleep(0.5)
            stats = app.targets.get_stats()
            log.info(f"Demo tamamlandi: {stats}")
        else:
            print(f"Kullanim: python3 main.py [OPTICAL|RF|DEMO]")
    else:
        # normal mod: OBC komutlariyla calis
        app.run()


if __name__ == "__main__":
    main()
