# GOLGE-1 Hedef Fuzyonu ve Yonetim

import time
import math
import logging
from config import *

log = logging.getLogger("TARGET")


def haversine(lat1, lon1, lat2, lon2):
    """Iki nokta arasi mesafe (metre)"""
    R = 6371000
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = (math.sin(dlat/2)**2 +
         math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) *
         math.sin(dlon/2)**2)
    return R * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


class TargetManager:
    """Coklu sensor hedef fuzyonu"""

    def __init__(self):
        self.targets = []      # aktif hedefler
        self.history = []      # gecmis hedefler
        self.id_counter = 0

    def add_detections(self, new_targets):
        """Yeni tespit listesini mevcut hedeflerle birlestirir"""
        merged_count = 0
        added_count = 0

        for nt in new_targets:
            if nt.get("confidence", 0) < MIN_CONFIDENCE:
                continue

            match = self._find_match(nt)
            if match:
                self._fuse(match, nt)
                merged_count += 1
            else:
                if len(self.targets) < MAX_TARGETS:
                    nt["first_seen"] = time.time()
                    nt["last_seen"] = time.time()
                    nt["update_count"] = 1
                    self.targets.append(nt)
                    added_count += 1
                else:
                    # dolu: en dusuk guvenli hedefi at
                    worst = min(self.targets, key=lambda t: t.get("confidence", 0))
                    if nt.get("confidence", 0) > worst.get("confidence", 0):
                        self.history.append(worst)
                        self.targets.remove(worst)
                        nt["first_seen"] = time.time()
                        nt["last_seen"] = time.time()
                        nt["update_count"] = 1
                        self.targets.append(nt)
                        added_count += 1

        if merged_count or added_count:
            log.info(f"Hedef guncelleme: +{added_count} yeni, {merged_count} birlesti, "
                     f"toplam {len(self.targets)}")

    def _find_match(self, new_target):
        """Konum ve tipe gore mevcut hedef esle"""
        for t in self.targets:
            dist = haversine(
                t.get("lat", 0), t.get("lon", 0),
                new_target.get("lat", 0), new_target.get("lon", 0)
            )
            if dist < TARGET_MERGE_DIST_M:
                return t
        return None

    def _fuse(self, existing, new):
        """Iki hedefi birlesik guncelle"""
        # konum: agirlikli ortalama (guven oranina gore)
        w1 = existing.get("confidence", 0.5)
        w2 = new.get("confidence", 0.5)
        total_w = w1 + w2

        existing["lat"] = (existing["lat"] * w1 + new["lat"] * w2) / total_w
        existing["lon"] = (existing["lon"] * w1 + new["lon"] * w2) / total_w

        # guven orani: birden fazla sensor dogruladiysa artir
        if new.get("sensor_id") != existing.get("sensor_id"):
            existing["confidence"] = min(0.99, max(w1, w2) + 0.1)
        else:
            existing["confidence"] = max(w1, w2)

        # daha spesifik sinifi al
        if new.get("classification", "UNKNOWN") != "UNKNOWN":
            existing["classification"] = new["classification"]

        # hiz/yon guncelle (yeni veri daha taze)
        if new.get("speed", 0) > 0:
            existing["speed"] = new["speed"]
            existing["heading"] = new.get("heading", existing.get("heading", 0))

        if new.get("alt", 0) > 0:
            existing["alt"] = new["alt"]

        existing["last_seen"] = time.time()
        existing["update_count"] = existing.get("update_count", 1) + 1

    def purge_stale(self):
        """Suresi dolmus hedefleri temizle"""
        now = time.time()
        active = []
        for t in self.targets:
            age = now - t.get("last_seen", now)
            if age < TARGET_EXPIRE_SEC:
                active.append(t)
            else:
                self.history.append(t)
                log.debug(f"Hedef silindi (timeout): {t.get('id')}")
        self.targets = active

    def get_priority_targets(self, max_count=MAX_TARGETS):
        """Oncelik sirasina gore hedef listesi"""
        self.purge_stale()

        # oncelik: confidence * (multi-sensor bonus)
        def priority_score(t):
            base = t.get("confidence", 0)
            if t.get("update_count", 1) > 1:
                base += 0.1  # multi-update bonus
            if t.get("type") in ("MICRO_DOPPLER", "RF_SHADOW"):
                base += 0.05  # stealth tespit bonus
            return base

        sorted_targets = sorted(self.targets, key=priority_score, reverse=True)
        return sorted_targets[:max_count]

    def get_stats(self):
        return {
            "active": len(self.targets),
            "history": len(self.history),
            "total_detected": self.id_counter + len(self.targets) + len(self.history)
        }
