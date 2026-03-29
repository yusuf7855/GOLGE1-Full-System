# GOLGE-1 | 3U Taktik Istihbarat ve Cok Maksatli Role Kup Uydusu

**TUA Astro Hackathon 2026 — Spectraloop / Samsun Universitesi**

> Canli Demo: [spectraloop.com](https://spectraloop.com) | Mobil Kontrol: [spectraloop.com/mobile](https://spectraloop.com/mobile)

![GOLGE-1 Yer Istasyonu C2 Arayuzu](screenshot.png)

---

## Proje Hakkinda

GOLGE-1, 3U form faktorunde (10x10x30 cm) otonom bir taktik gozlem ve role kup uydusudur. Uzayda Edge AI ile goruntu isleyip, gigabyte ham veri yerine **2 KB sifreli koordinat** indiren, stealth hedefleri pasif radarla tespit eden ve jammed ortamda frekans atlamali iletisim kuran bir savunma platformudur.

### Trinity Gorev Mimarisi

| Mod | Gorev | Teknoloji |
|-----|-------|-----------|
| Goren Goz | Optik/IR hedef tespiti | YOLOv8n + Jetson Nano, 23 FPS |
| Duyan Kulak | Pasif bistatic radar | SDR + FFT, mikro-Doppler analizi |
| Konusan Dil | Sifreli taktik role | AES-256 + frekans atlamali SDR |

---

## Sistem Mimarisi

```
GOLGE-1 CUBESAT (500 km LEO)
├── STM32F405 OBC ──── FreeRTOS, Durum Makinesi, AES-256, AX.25
├── Jetson Nano ────── YOLOv8n TensorRT, FFT Sinyal Isleme
├── SDR Modul ──────── Pasif Radar + Sifreli Role (cift kullanim)
├── Kamera + IR ────── 5 MP RGB + FLIR Lepton 3.5 Termal
├── ADCS ───────────── Magnetorquer + Star Tracker + BNO055
└── EPS ────────────── 4x GaAs Panel (15W) + 30 Wh Li-Ion
         |
    [AX.25 Sifreli Downlink — 437.5 MHz UHF]
         |
YER ISTASYONU (spectraloop.com)
├── Node.js + Socket.IO Backend
├── React Taktik Arayuz (Leaflet Harita + Three.js 3D)
└── Mobil Kontrol Paneli
```

---

## Repo Yapisi

```
GOLGE1-Full-System/
│
├── STM32_Firmware/           # OBC Ucus Yazilimi (C, FreeRTOS)
│   ├── Core/Src/
│   │   ├── main.c            # Ana program + State Machine
│   │   ├── freertos.c        # RTOS gorev yonetimi
│   │   ├── golge1_adcs.c     # Yonelim kontrol (B-Dot, Quaternion PD)
│   │   ├── golge1_comms.c    # AX.25 + AES-256 haberlesme
│   │   ├── golge1_eps.c      # Guc yonetimi + MPPT
│   │   ├── golge1_fdir.c     # 6 kademeli hata kurtarma
│   │   ├── golge1_payload.c  # Gorev yuku kontrol
│   │   ├── golge1_orbit.c    # Yorunge hesaplama
│   │   ├── golge1_flash.c    # Store & Forward (NAND)
│   │   ├── golge1_ecc.c      # TMR + ECC hata duzeltme
│   │   └── golge1_command.c  # Telekomut isleme
│   ├── Core/Inc/             # Header dosyalari
│   ├── Drivers/              # STM32 HAL surculeri
│   ├── Middlewares/          # FreeRTOS kernel
│   └── GOLGE1_FSW_OBC.ioc   # STM32CubeMX konfigurasyon
│
├── JetsonNano_AI/            # Yapay Zeka Gorev Yuku (Python)
│   ├── main.py               # Ana AI dongusu
│   ├── optical_detector.py   # YOLOv8n optik tespit
│   ├── rf_analyzer.py        # SDR/FFT pasif radar analizi
│   ├── target_manager.py     # Hedef yonetimi + JSON paketleme
│   ├── obc_comm.py           # STM32 UART haberlesme
│   ├── config.py             # Sistem konfigurasyonu
│   └── requirements.txt      # Python bagimliliklar
│
└── YerIstasyonu_C2/          # Yer Istasyonu + Mobil (Node.js)
    ├── server.js             # Express + Socket.IO backend
    ├── satellite_sim.js      # Uydu simulasyon motoru
    ├── package.json          # Node.js bagimliliklar
    └── public/
        ├── index.html        # Taktik C2 Arayuzu (150 KB)
        ├── mobile.html       # Mobil Kontrol Paneli
        ├── icons/            # NATO taktik SVG ikonlar
        ├── sun_texture.jpg   # Gunes 2K doku
        ├── moon_texture.jpg  # Ay 2K doku
        └── siren-alarm.mp3   # Alarm sireni
```

---

## Yer Istasyonu Ozellikleri

### Taktik C2 Arayuzu
- ESRI uydu goruntusu ile gercek zamanli taktik harita
- Fuze tespit ve iz takibi (SVG fuze ikonu + kirmizi trail)
- AI tarama simulasyonu (koordinat girisli, YOLOv8 tespit)
- NATO taktik ikonlar (tank, obus, ZPT, IHA, SAM, radar)
- Simule + gercek hava trafigi (OpenSky ADS-B)
- Canli telemetri paneli (guc, sicaklik, ADCS, link gucu)
- Siren alarm sistemi (MP3, mobil tetiklemeli)

### 3D CubeSat Modeli (Three.js)
- PBR malzemeler + PMREM environment map
- NASA Blue Marble Dunya (gece isiklari + bulutlar + 6 katman atmosfer)
- Gercek Gunes dokusu (8 katman corona + 10 prominens)
- Ay modeli (tidally locked yorunge)
- ADCS gunes takibi gorsellestirmesi (IDLE / AI_SCAN)
- 4 gunes paneli deploy animasyonu
- Anten deploy animasyonu
- Batarya yonetimi (AI_SCAN guc tuketimi, otonom IDLE gecisi)
- Tam ekran modu

### Mobil Kontrol Paneli
- Fuze simulasyonu (Balistik / Cruise / Taktik)
- Hava hedefi (Savas ucagi / Bomber / IHA / IHA Surusu)
- CubeSat mod/panel/anten kontrolu
- Siren ac/kapat
- WebSocket ile gercek zamanli senkronizasyon

---

## Kurulum

### Yer Istasyonu (Lokal)

```bash
cd YerIstasyonu_C2
npm install
node server.js
```

Tarayicida ac:
- Yer Istasyonu: `http://localhost:3000`
- Mobil Kontrol: `http://localhost:3000/mobile`

### STM32 Firmware

1. STM32CubeIDE ile `STM32_Firmware/` klasorunu ac
2. `GOLGE1_FSW_OBC.ioc` dosyasini CubeMX ile yapılandir
3. Build + Flash (STM32F405RG hedef)

### Jetson Nano AI

```bash
cd JetsonNano_AI
pip install -r requirements.txt
python main.py
```

---

## Teknik Ozellikler

| Parametre | Deger |
|-----------|-------|
| Form Faktoru | 3U CubeSat (10x10x30 cm) |
| Kutle | 3.5 kg (marj dahil) |
| Yorunge | 500 km LEO SSO, 97.4 derece inklinasyon |
| Periyot | 94.2 dakika |
| OBC | STM32F405RG (ARM Cortex-M4, 168 MHz) |
| AI Islemci | NVIDIA Jetson Nano (128 CUDA, 472 GFLOPS) |
| AI Model | YOLOv8n TensorRT INT8, 23 FPS, mAP %94.2 |
| Veri Sikistirma | ~500.000:1 (1 GB -> 2 KB) |
| Sifreleme | AES-256-GCM + ECDH P-256 |
| Haberlesme | AX.25, UHF 437.5 MHz, 1W |
| Guc Uretimi | 4x GaAs Panel, 12-15W |
| Batarya | 2S Li-Ion, 30 Wh |
| ADCS | Magnetorquer + Star Tracker + BNO055 IMU |
| FDIR | 6 kademeli otonom kurtarma |

---

## FDIR Senaryolari

| Senaryo | Tetik | Aksiyon |
|---------|-------|---------|
| Iletisim Kaybi | 3 tur ACK yok | Store & Forward + Role modu |
| Radyasyon (SEU) | TMR uyumsuzluk | ECC duzeltme + Watchdog Reset |
| Yonelim Kaybi | Star Tracker kor | B-Dot + Magnetometre kilitleme |
| Gorev Yuku Hasari | Jetson 3x boot fail | Mission Pivot (Pasif Radar + Role) |
| Termal Kacak | >85 derece C | Throttle -> Idle -> Kill Switch |
| Guc Krizi | <%15 batarya | Load Shedding -> Deep Sleep |

---

## Takim

**Spectraloop** — Samsun Universitesi

---

## Lisans

Bu proje TUA Astro Hackathon 2026 icin gelistirilmistir.
