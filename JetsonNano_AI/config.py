# GOLGE-1 Jetson Nano Konfigurasyonu

# UART - STM32 haberlesme
UART_PORT = "/dev/ttyTHS1"  # Jetson Nano UART2
UART_BAUD = 115200
UART_TIMEOUT = 1.0

# Kamera
CAM_WIDTH = 1280
CAM_HEIGHT = 720
CAM_FPS = 15
CAM_DEVICE = 0  # /dev/video0 (CSI veya USB)
CAM_FLIP = 2    # CSI kamera icin 180 derece

# YOLOv8 inference
YOLO_MODEL_PATH = "models/yolov8n_golge1.pt"
YOLO_CONF_THRESH = 0.45
YOLO_IOU_THRESH = 0.5
YOLO_IMG_SIZE = 640
YOLO_DEVICE = "cuda"  # Jetson GPU

# siniflar: YOLOv8 egitim sirasina gore
YOLO_CLASSES = {
    0: "TANK",
    1: "APC",
    2: "TRUCK",
    3: "CONVOY",
    4: "AIRCRAFT",
    5: "HELICOPTER",
    6: "SHIP",
    7: "LAUNCHER",
    8: "RADAR_SITE",
    9: "UNKNOWN"
}

# IR anomali
IR_HOT_THRESH = 400.0     # C - egzoz izi
IR_COLD_THRESH = -40.0    # soguk hedef
IR_ENABLED = True

# golge analizi (alt-piksel)
SHADOW_ANALYSIS = True
SHADOW_MIN_LENGTH_PX = 15  # min golge uzunlugu (piksel)
SHADOW_ANGLE_TOLERANCE = 20  # gunes acisi toleransi (derece)
GSD_METERS = 3.5           # yer ornekleme mesafesi

# SDR / Pasif Radar
SDR_SAMPLE_RATE = 2.4e6    # 2.4 MHz
SDR_CENTER_FREQ = 100e6    # 100 MHz (FM bandi)
SDR_GAIN = 40              # dB
SDR_FFT_SIZE = 4096
SDR_OVERLAP = 0.5
SDR_INTEGRATION_TIME = 2.0  # saniye

# RF golge tespiti
RF_SHADOW_THRESH_DB = 3.0   # min sinyal dususu
RF_DOPPLER_THRESH_HZ = 0.02
RF_NOISE_FLOOR_DB = -90.0

# mikro-Doppler JEM (Jet Engine Modulation) referanslari
JEM_SIGNATURES = {
    "F-35":     {"rpm_range": (15000, 20000), "fd_range": (2000, 3000)},
    "F-16":     {"rpm_range": (12000, 16000), "fd_range": (1500, 2500)},
    "Su-57":    {"rpm_range": (12000, 16000), "fd_range": (1500, 2500)},
    "TB-2":     {"rpm_range": (4000, 6000),   "fd_range": (50, 100)},
    "UNKNOWN":  {"rpm_range": (0, 50000),     "fd_range": (0, 5000)},
}

# hedef yonetimi
MAX_TARGETS = 8
TARGET_EXPIRE_SEC = 300    # 5dk sonra sil
TARGET_MERGE_DIST_M = 500  # 500m icindeki hedefler birlestir
MIN_CONFIDENCE = 0.55

# genel
LOG_LEVEL = "INFO"
HEARTBEAT_SEC = 5
POWER_MODE_SLEEP_SEC = 1   # uyku modunda bekleme
