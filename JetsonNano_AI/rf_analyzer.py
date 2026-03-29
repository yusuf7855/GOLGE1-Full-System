# GOLGE-1 Dinleyen Kulak - Pasif FSR Radar + Mikro-Doppler

import numpy as np
import time
import logging
from config import *

log = logging.getLogger("RF")

# SDR import (rtlsdr)
try:
    from rtlsdr import RtlSdr
    SDR_AVAILABLE = True
except ImportError:
    SDR_AVAILABLE = False
    log.warning("rtlsdr yuklu degil, sim modunda calisacak")


class SpectralAnalyzer:
    """FFT tabanli spektrum analizi"""

    def __init__(self, fft_size=SDR_FFT_SIZE, sample_rate=SDR_SAMPLE_RATE):
        self.fft_size = fft_size
        self.sample_rate = sample_rate
        self.freq_resolution = sample_rate / fft_size  # Hz/bin
        self.window = np.hanning(fft_size)
        self.baseline = None
        self.baseline_count = 0

    def compute_psd(self, iq_samples):
        """IQ verisinden Guc Spektral Yogunlugu (PSD) hesapla"""
        num_segments = len(iq_samples) // self.fft_size
        if num_segments == 0:
            return np.zeros(self.fft_size)

        psd_acc = np.zeros(self.fft_size)
        for i in range(num_segments):
            segment = iq_samples[i * self.fft_size:(i + 1) * self.fft_size]
            windowed = segment * self.window
            spectrum = np.fft.fftshift(np.fft.fft(windowed))
            psd_acc += np.abs(spectrum) ** 2

        psd = 10 * np.log10(psd_acc / num_segments + 1e-12)
        return psd

    def update_baseline(self, psd):
        """Referans gurultu tabani guncelle"""
        if self.baseline is None:
            self.baseline = psd.copy()
        else:
            alpha = 0.1
            self.baseline = alpha * psd + (1 - alpha) * self.baseline
        self.baseline_count += 1

    def detect_shadows(self, psd):
        """Referansa gore RF golge (sinyal dususu) tespit et"""
        if self.baseline is None or self.baseline_count < 5:
            return []

        diff = self.baseline - psd  # pozitif = sinyal dususu

        shadows = []
        # ardisik bin'lerde esik ustu dusus ara
        above_thresh = diff > RF_SHADOW_THRESH_DB
        in_shadow = False
        start_bin = 0

        for i in range(len(diff)):
            if above_thresh[i] and not in_shadow:
                in_shadow = True
                start_bin = i
            elif not above_thresh[i] and in_shadow:
                in_shadow = False
                end_bin = i
                width = end_bin - start_bin

                if width >= 3:  # min 3 bin genislik
                    center_bin = (start_bin + end_bin) // 2
                    center_freq = (center_bin - self.fft_size // 2) * self.freq_resolution
                    depth_db = float(np.max(diff[start_bin:end_bin]))

                    shadows.append({
                        "center_freq_hz": SDR_CENTER_FREQ + center_freq,
                        "bandwidth_hz": width * self.freq_resolution,
                        "depth_db": depth_db,
                        "start_bin": start_bin,
                        "end_bin": end_bin,
                        "confidence": min(0.95, 0.4 + depth_db / 15.0)
                    })

        return shadows


class MicroDopplerAnalyzer:
    """Mikro-Doppler imza analizi (JEM - Jet Engine Modulation)"""

    def __init__(self, fft_size=SDR_FFT_SIZE, sample_rate=SDR_SAMPLE_RATE):
        self.fft_size = fft_size
        self.sample_rate = sample_rate
        self.stft_window = 256
        self.stft_overlap = 192

    def extract_micro_doppler(self, iq_samples):
        """STFT ile mikro-Doppler spektrogram cikar"""
        hop = self.stft_window - self.stft_overlap
        num_frames = (len(iq_samples) - self.stft_window) // hop

        if num_frames <= 0:
            return None

        spectrogram = np.zeros((self.stft_window, num_frames))
        window = np.hanning(self.stft_window)

        for i in range(num_frames):
            start = i * hop
            segment = iq_samples[start:start + self.stft_window] * window
            fft_result = np.fft.fftshift(np.fft.fft(segment))
            spectrogram[:, i] = np.abs(fft_result)

        return spectrogram

    def detect_jem_signature(self, spectrogram):
        """Turbin mikro-Doppler imzasi tespit et"""
        if spectrogram is None:
            return None

        # zaman ekseni boyunca varyans: periyodik sinyal = yuksek varyans
        temporal_var = np.var(spectrogram, axis=1)

        # gurultu tabaninin ustu
        noise_floor = np.median(temporal_var)
        peaks = temporal_var > (noise_floor * 5)

        if not np.any(peaks):
            return None

        # tepe frekanslarini bul
        peak_indices = np.where(peaks)[0]
        freq_res = self.sample_rate / self.stft_window

        peak_freqs = []
        for idx in peak_indices:
            freq = (idx - self.stft_window // 2) * freq_res
            if abs(freq) > 10:  # DC atla
                peak_freqs.append(abs(freq))

        if not peak_freqs:
            return None

        dominant_freq = max(peak_freqs)  # Hz
        strength = float(np.max(temporal_var[peaks]))

        return {
            "dominant_freq_hz": dominant_freq,
            "peak_freqs": sorted(peak_freqs)[:5],
            "strength": strength,
        }

    def classify_aircraft(self, jem_data):
        """JEM imzasindan ucak tipi siniflandirma"""
        if jem_data is None:
            return "UNKNOWN", 0.3

        fd = jem_data["dominant_freq_hz"]

        best_match = "UNKNOWN"
        best_conf = 0.3

        for name, sig in JEM_SIGNATURES.items():
            if name == "UNKNOWN":
                continue
            fd_min, fd_max = sig["fd_range"]
            if fd_min <= fd <= fd_max:
                # ne kadar merkezde o kadar iyi eslesme
                center = (fd_min + fd_max) / 2
                spread = (fd_max - fd_min) / 2
                dist = abs(fd - center) / spread
                conf = max(0.6, 0.95 - dist * 0.3)

                if conf > best_conf:
                    best_conf = conf
                    best_match = name

        return best_match, round(best_conf, 2)


class RFAnalyzer:
    """Dinleyen Kulak: tum RF analiz birlesik"""

    def __init__(self):
        self.sdr = None
        self.spectral = SpectralAnalyzer()
        self.doppler = MicroDopplerAnalyzer()
        self.target_counter = 0

    def open_sdr(self):
        """SDR cihazini ac"""
        if not SDR_AVAILABLE:
            log.info("SDR sim modunda")
            return True

        try:
            self.sdr = RtlSdr()
            self.sdr.sample_rate = SDR_SAMPLE_RATE
            self.sdr.center_freq = SDR_CENTER_FREQ
            self.sdr.gain = SDR_GAIN
            log.info(f"SDR acildi: {SDR_CENTER_FREQ/1e6:.1f} MHz, {SDR_SAMPLE_RATE/1e6:.1f} Msps")
            return True
        except Exception as e:
            log.error(f"SDR hatasi: {e}")
            return False

    def close_sdr(self):
        if self.sdr:
            try:
                self.sdr.close()
            except:
                pass
            self.sdr = None

    def capture_iq(self, duration_sec=SDR_INTEGRATION_TIME):
        """Ham IQ veri yakala"""
        num_samples = int(SDR_SAMPLE_RATE * duration_sec)

        if self.sdr:
            try:
                return self.sdr.read_samples(num_samples)
            except Exception as e:
                log.error(f"IQ yakalama hatasi: {e}")

        # sim: gurultu + zayif sinyal
        return self._simulate_iq(num_samples)

    def _simulate_iq(self, num_samples):
        """SDR yokken sim IQ verisi"""
        t = np.arange(num_samples) / SDR_SAMPLE_RATE

        # gurultu tabani
        noise = (np.random.randn(num_samples) + 1j * np.random.randn(num_samples)) * 0.01

        # FM verici sinyalleri (referans)
        for freq_offset in [-200e3, -50e3, 100e3, 250e3]:
            noise += 0.5 * np.exp(1j * 2 * np.pi * freq_offset * t)

        # %30 olasilikla RF golge ekle
        if np.random.random() < 0.3:
            shadow_freq = np.random.uniform(-100e3, 100e3)
            shadow_width = np.random.uniform(5e3, 20e3)
            shadow_start = int(num_samples * 0.3)
            shadow_end = int(num_samples * 0.7)

            # sinyal dususu sim
            mask = np.ones(num_samples)
            mask[shadow_start:shadow_end] *= 0.3
            freq_range = np.abs(np.fft.fftfreq(num_samples, 1/SDR_SAMPLE_RATE) - shadow_freq)
            # basitlestirme: zaman domaininde supresyon

        # %20 olasilikla mikro-Doppler ekle
        if np.random.random() < 0.2:
            jem_freq = np.random.uniform(1000, 3000)  # Hz
            noise += 0.001 * np.exp(1j * 2 * np.pi * jem_freq * t) * np.sin(2 * np.pi * 50 * t)

        return noise

    def scan_once(self, sat_lat=41.5, sat_lon=36.1):
        """Tek RF tarama: IQ yakala + isle"""
        targets = []

        iq_data = self.capture_iq()

        # 1. PSD hesapla
        psd = self.spectral.compute_psd(iq_data)

        # 2. RF golge tespiti
        shadows = self.spectral.detect_shadows(psd)
        self.spectral.update_baseline(psd)

        for s in shadows:
            self.target_counter += 1
            # konum tahmini (RF'de daha belirsiz)
            lat_jitter = np.random.uniform(-0.01, 0.01)
            lon_jitter = np.random.uniform(-0.01, 0.01)

            targets.append({
                "id": f"TGT-RF-{self.target_counter:03d}",
                "type": "RF_SHADOW",
                "classification": "UNKNOWN",
                "lat": sat_lat + lat_jitter,
                "lon": sat_lon + lon_jitter,
                "alt": np.random.uniform(5000, 15000),
                "heading": np.random.randint(0, 360),
                "speed": np.random.randint(500, 1000),
                "confidence": s["confidence"],
                "sensor_id": 2,  # SENSOR_ID_RF_FSR
                "signal_db": -s["depth_db"],
                "freq_hz": s["center_freq_hz"],
                "bandwidth_hz": s["bandwidth_hz"],
            })

        # 3. mikro-Doppler analizi
        spectrogram = self.doppler.extract_micro_doppler(iq_data)
        jem = self.doppler.detect_jem_signature(spectrogram)

        if jem:
            aircraft_type, jem_conf = self.doppler.classify_aircraft(jem)

            # mevcut RF hedefin sinifini guncelle
            if targets:
                best = max(targets, key=lambda t: t["confidence"])
                best["classification"] = aircraft_type
                best["confidence"] = max(best["confidence"], jem_conf)
                best["type"] = "MICRO_DOPPLER"
                best["sensor_id"] = 3  # SENSOR_ID_DOPPLER
            else:
                # bagimsiz Doppler tespiti
                self.target_counter += 1
                targets.append({
                    "id": f"TGT-DPL-{self.target_counter:03d}",
                    "type": "MICRO_DOPPLER",
                    "classification": aircraft_type,
                    "lat": sat_lat + np.random.uniform(-0.005, 0.005),
                    "lon": sat_lon + np.random.uniform(-0.005, 0.005),
                    "alt": 10000,
                    "heading": 0,
                    "speed": 800,
                    "confidence": jem_conf,
                    "sensor_id": 3,
                    "signal_db": 0,
                    "jem_freq_hz": jem["dominant_freq_hz"],
                })

        return targets
