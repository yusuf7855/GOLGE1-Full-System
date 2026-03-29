# GOLGE-1 Goren Goz - Optik/IR Hedef Tespiti

import cv2
import numpy as np
import time
import logging
from config import *

log = logging.getLogger("OPTICAL")

# YOLOv8 import (ultralytics)
try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    log.warning("ultralytics yuklu degil, sim modunda calisacak")


class ShadowAnalyzer:
    """Alt-piksel golge analizi ile hedef tespiti"""

    def __init__(self, sun_azimuth=45.0, sun_elevation=30.0):
        self.sun_azimuth = sun_azimuth    # derece
        self.sun_elevation = sun_elevation

    def detect_shadows(self, frame):
        """Frame'deki uzun golgeleri tespit et"""
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)

        # golge maskeleme: karanlik pikseller
        _, shadow_mask = cv2.threshold(blurred, 60, 255, cv2.THRESH_BINARY_INV)

        # morfolojik temizlik
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (3, 3))
        shadow_mask = cv2.morphologyEx(shadow_mask, cv2.MORPH_CLOSE, kernel)
        shadow_mask = cv2.morphologyEx(shadow_mask, cv2.MORPH_OPEN, kernel)

        # kontur bul
        contours, _ = cv2.findContours(shadow_mask, cv2.RETR_EXTERNAL,
                                        cv2.CHAIN_APPROX_SIMPLE)

        shadows = []
        for cnt in contours:
            if len(cnt) < 5:
                continue

            # minimum alan kutusu
            rect = cv2.minAreaRect(cnt)
            (cx, cy), (w, h), angle = rect

            # uzun golge filtresi
            length = max(w, h)
            width = min(w, h)
            if length < SHADOW_MIN_LENGTH_PX:
                continue
            aspect_ratio = length / (width + 1e-6)
            if aspect_ratio < 3.0:
                continue

            # gunes acisi tutarlilik kontrolu
            shadow_angle = angle % 180
            expected_angle = self.sun_azimuth % 180
            angle_diff = abs(shadow_angle - expected_angle)
            if angle_diff > SHADOW_ANGLE_TOLERANCE and (180 - angle_diff) > SHADOW_ANGLE_TOLERANCE:
                continue

            # boyuttan hedef buyuklugu tahmini (GSD)
            estimated_length_m = length * GSD_METERS
            shadows.append({
                "center": (int(cx), int(cy)),
                "length_px": length,
                "length_m": estimated_length_m,
                "angle": angle,
                "area": cv2.contourArea(cnt),
                "contour": cnt
            })

        return shadows

    def classify_by_shadow(self, shadow):
        """Golge uzunlugundan hedef siniflandirma"""
        length_m = shadow["length_m"]
        if length_m > 30:
            return "CONVOY", 0.6
        elif length_m > 15:
            return "TANK", 0.7
        elif length_m > 8:
            return "APC", 0.65
        elif length_m > 5:
            return "TRUCK", 0.6
        return "UNKNOWN", 0.5


class IRDetector:
    """IR (kizilotesi) anomali tespiti"""

    def __init__(self):
        self.baseline = None
        self.frame_count = 0

    def update_baseline(self, thermal_frame):
        """Arka plan isi modelini guncelle"""
        if self.baseline is None:
            self.baseline = thermal_frame.astype(np.float32)
        else:
            # hareketli ortalama
            alpha = 0.05
            self.baseline = alpha * thermal_frame + (1 - alpha) * self.baseline

    def detect_anomalies(self, thermal_frame):
        """Sicaklik anomalilerini tespit et (+400C egzoz izi)"""
        if self.baseline is None:
            self.update_baseline(thermal_frame)
            return []

        diff = thermal_frame.astype(np.float32) - self.baseline

        # sicak anomaliler (egzoz, motor)
        hot_mask = diff > IR_HOT_THRESH
        # soguk anomaliler (golge, kamuflaj)
        cold_mask = diff < IR_COLD_THRESH

        anomalies = []

        for mask, atype in [(hot_mask, "HOT"), (cold_mask, "COLD")]:
            mask_u8 = (mask * 255).astype(np.uint8)
            contours, _ = cv2.findContours(mask_u8, cv2.RETR_EXTERNAL,
                                            cv2.CHAIN_APPROX_SIMPLE)
            for cnt in contours:
                area = cv2.contourArea(cnt)
                if area < 10:
                    continue
                M = cv2.moments(cnt)
                if M["m00"] == 0:
                    continue
                cx = int(M["m10"] / M["m00"])
                cy = int(M["m01"] / M["m00"])

                # bolgedeki maks sicaklik farki
                rect = cv2.boundingRect(cnt)
                roi = diff[rect[1]:rect[1]+rect[3], rect[0]:rect[0]+rect[2]]
                peak = float(np.max(np.abs(roi)))

                anomalies.append({
                    "center": (cx, cy),
                    "type": atype,
                    "peak_temp_diff": peak,
                    "area_px": area,
                    "confidence": min(0.95, 0.5 + peak / 1000.0)
                })

        self.update_baseline(thermal_frame)
        return anomalies


class MotionDetector:
    """Hareket vektoru analizi (pes pese kareler)"""

    def __init__(self):
        self.prev_gray = None

    def detect_motion(self, frame):
        """Optik akis ile hareket eden nesneleri bul"""
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        if self.prev_gray is None:
            self.prev_gray = gray
            return []

        # Farneback optik akis
        flow = cv2.calcOpticalFlowFarneback(
            self.prev_gray, gray, None,
            pyr_scale=0.5, levels=3, winsize=15,
            iterations=3, poly_n=5, poly_sigma=1.2, flags=0
        )

        # akis buyuklugu
        mag, ang = cv2.cartToPolar(flow[..., 0], flow[..., 1])

        # hareket esigi
        motion_mask = (mag > 2.0).astype(np.uint8) * 255

        # morfolojik temizlik
        kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (7, 7))
        motion_mask = cv2.morphologyEx(motion_mask, cv2.MORPH_CLOSE, kernel)

        contours, _ = cv2.findContours(motion_mask, cv2.RETR_EXTERNAL,
                                        cv2.CHAIN_APPROX_SIMPLE)

        movers = []
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area < 100:
                continue

            M = cv2.moments(cnt)
            if M["m00"] == 0:
                continue
            cx = int(M["m10"] / M["m00"])
            cy = int(M["m01"] / M["m00"])

            # ortalama hiz ve yon
            rect = cv2.boundingRect(cnt)
            roi_mag = mag[rect[1]:rect[1]+rect[3], rect[0]:rect[0]+rect[2]]
            roi_ang = ang[rect[1]:rect[1]+rect[3], rect[0]:rect[0]+rect[2]]

            avg_speed = float(np.mean(roi_mag)) * GSD_METERS * CAM_FPS  # m/s
            avg_heading = float(np.mean(roi_ang)) * 180 / np.pi

            movers.append({
                "center": (cx, cy),
                "speed_ms": avg_speed,
                "speed_kmh": avg_speed * 3.6,
                "heading_deg": avg_heading % 360,
                "area_px": area,
            })

        self.prev_gray = gray
        return movers


class OpticalDetector:
    """Goren Goz: tum optik/IR tespit birlesik"""

    def __init__(self):
        self.model = None
        self.shadow = ShadowAnalyzer()
        self.ir = IRDetector()
        self.motion = MotionDetector()
        self.cap = None
        self.target_counter = 0

        if YOLO_AVAILABLE:
            try:
                self.model = YOLO(YOLO_MODEL_PATH)
                log.info("YOLOv8 yuklendi")
            except Exception as e:
                log.warning(f"YOLOv8 yuklenemedi: {e}, sim modunda")
                self.model = None

    def open_camera(self):
        """Kamera ac (CSI veya USB)"""
        # Jetson CSI pipeline
        gst_pipeline = (
            f"nvarguscamerasrc ! "
            f"video/x-raw(memory:NVMM), width={CAM_WIDTH}, height={CAM_HEIGHT}, "
            f"framerate={CAM_FPS}/1 ! "
            f"nvvidconv flip-method={CAM_FLIP} ! "
            f"video/x-raw, format=BGRx ! videoconvert ! "
            f"video/x-raw, format=BGR ! appsink"
        )

        try:
            self.cap = cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)
            if not self.cap.isOpened():
                # fallback: USB kamera
                self.cap = cv2.VideoCapture(CAM_DEVICE)
                self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAM_WIDTH)
                self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAM_HEIGHT)

            if self.cap.isOpened():
                log.info(f"Kamera acildi: {CAM_WIDTH}x{CAM_HEIGHT}@{CAM_FPS}")
                return True
        except Exception as e:
            log.error(f"Kamera hatasi: {e}")

        return False

    def close_camera(self):
        if self.cap and self.cap.isOpened():
            self.cap.release()
            self.cap = None

    def process_frame(self, frame, sat_lat=0, sat_lon=0):
        """Tek frame isle, hedef listesi dondur"""
        targets = []

        # 1. YOLOv8 inference
        yolo_targets = self._run_yolo(frame)
        targets.extend(yolo_targets)

        # 2. golge analizi
        if SHADOW_ANALYSIS:
            shadow_targets = self._run_shadow_analysis(frame)
            targets.extend(shadow_targets)

        # 3. hareket tespiti
        motion_targets = self._run_motion_detection(frame)
        targets.extend(motion_targets)

        # koordinat ata (uydu pozisyonundan piksel -> enlem/boylam)
        for t in targets:
            px, py = t.get("center", (CAM_WIDTH//2, CAM_HEIGHT//2))
            t["lat"] = sat_lat + (py - CAM_HEIGHT/2) * GSD_METERS / 111320.0
            t["lon"] = sat_lon + (px - CAM_WIDTH/2) * GSD_METERS / (111320.0 * np.cos(np.radians(sat_lat + 0.001)))
            t["alt"] = 0.0

        return targets

    def _run_yolo(self, frame):
        """YOLOv8 hedef tespiti"""
        targets = []

        if self.model is None:
            # sim modu: rastgele hedef uret
            return self._simulate_yolo()

        results = self.model.predict(
            frame,
            conf=YOLO_CONF_THRESH,
            iou=YOLO_IOU_THRESH,
            imgsz=YOLO_IMG_SIZE,
            device=YOLO_DEVICE,
            verbose=False
        )

        for r in results:
            for box in r.boxes:
                cls_id = int(box.cls[0])
                conf = float(box.conf[0])
                x1, y1, x2, y2 = box.xyxy[0].tolist()
                cx = int((x1 + x2) / 2)
                cy = int((y1 + y2) / 2)

                self.target_counter += 1
                targets.append({
                    "id": f"TGT-OPT-{self.target_counter:03d}",
                    "type": "OPTICAL",
                    "classification": YOLO_CLASSES.get(cls_id, "UNKNOWN"),
                    "center": (cx, cy),
                    "bbox": (int(x1), int(y1), int(x2), int(y2)),
                    "confidence": conf,
                    "sensor_id": 0,
                    "heading": 0,
                    "speed": 0,
                    "signal_db": 0,
                })

        return targets

    def _simulate_yolo(self):
        """Model yokken simulasyon hedefi"""
        if np.random.random() > 0.3:
            return []

        self.target_counter += 1
        cls = np.random.choice(list(YOLO_CLASSES.values()))
        return [{
            "id": f"TGT-OPT-{self.target_counter:03d}",
            "type": "OPTICAL",
            "classification": cls,
            "center": (
                np.random.randint(100, CAM_WIDTH - 100),
                np.random.randint(100, CAM_HEIGHT - 100)
            ),
            "confidence": round(np.random.uniform(0.55, 0.95), 2),
            "sensor_id": 0,
            "heading": np.random.randint(0, 360),
            "speed": np.random.randint(0, 80),
            "signal_db": 0,
        }]

    def _run_shadow_analysis(self, frame):
        """Golge tespiti ve siniflandirma"""
        targets = []
        shadows = self.shadow.detect_shadows(frame)

        for s in shadows:
            cls, conf = self.shadow.classify_by_shadow(s)
            self.target_counter += 1
            targets.append({
                "id": f"TGT-SHD-{self.target_counter:03d}",
                "type": "SHADOW",
                "classification": cls,
                "center": s["center"],
                "confidence": conf,
                "sensor_id": 0,
                "heading": int(s["angle"]) % 360,
                "speed": 0,
                "signal_db": 0,
            })

        return targets

    def _run_motion_detection(self, frame):
        """Hareket tespiti"""
        targets = []
        movers = self.motion.detect_motion(frame)

        for m in movers:
            self.target_counter += 1
            targets.append({
                "id": f"TGT-MOV-{self.target_counter:03d}",
                "type": "MOTION",
                "classification": "UNKNOWN",
                "center": m["center"],
                "confidence": 0.6,
                "sensor_id": 0,
                "heading": int(m["heading_deg"]),
                "speed": int(m["speed_kmh"]),
                "signal_db": 0,
            })

        return targets

    def scan_once(self, sat_lat=39.9334, sat_lon=32.8597):
        """Tek tarama: frame yakala + isle"""
        if self.cap is None or not self.cap.isOpened():
            if not self.open_camera():
                return self._simulate_yolo()  # kamera yoksa sim

        ret, frame = self.cap.read()
        if not ret:
            log.warning("Frame yakalanamadi")
            return []

        return self.process_frame(frame, sat_lat, sat_lon)
