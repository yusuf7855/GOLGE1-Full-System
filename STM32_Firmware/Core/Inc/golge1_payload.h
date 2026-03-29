/**
  ******************************************************************************
  * @file           : golge1_payload.h
  * @brief          : GÖLGE-1 Görev Yükü Yönetim Başlık Dosyası
  * @description    : Optik/IR kamera (Gören Göz), SDR pasif radar (Dinleyen
  *                   Kulak), Jetson Nano AI arayüzü, hedef yönetimi
  ******************************************************************************
  */

#ifndef __GOLGE1_PAYLOAD_H
#define __GOLGE1_PAYLOAD_H

#include "main.h"

/* ========================================================================== */
/*                           BAŞLATMA                                         */
/* ========================================================================== */

/**
 * @brief Görev yükü alt sistemini başlat
 *        Jetson UART hattını, sensör GPIO'ları konfigüre et
 */
void PAYLOAD_Init(void);

/* ========================================================================== */
/*                    GÖREN GÖZ (OPTİK / IR)                                */
/* ========================================================================== */

/**
 * @brief Optik tarama başlat
 *        Kamerayı aç, Jetson'u inference moduna al
 * @param subsys  Alt sistem durumları (güncellenir)
 */
void PAYLOAD_StartOpticalScan(SubsysStatus_t *subsys);

/**
 * @brief Optik tarama durdur ve sonuçları topla
 * @param subsys  Alt sistem durumları (güncellenir)
 */
void PAYLOAD_StopOpticalScan(SubsysStatus_t *subsys);

/**
 * @brief Optik/IR hedef tespit simülasyonu (Jetson Nano -> STM32)
 *        Gerçekte: Jetson UART üzerinden hedef JSON gönderir.
 *        Simülasyonda: Sahte hedefler üretilir.
 * @param targets      Hedef dizisi çıktısı
 * @param max_targets  Dizi kapasitesi
 * @return Tespit edilen hedef sayısı
 */
uint8_t PAYLOAD_SimulateOpticalDetection(IntelData_t *targets, uint8_t max_targets);

/**
 * @brief IR anomali analizi
 *        +400°C üzeri ısı anomalilerini (egzoz izi) tespit eder
 * @param targets      Hedef dizisi çıktısı
 * @param max_targets  Dizi kapasitesi
 * @return Tespit edilen anomali sayısı
 */
uint8_t PAYLOAD_SimulateIRDetection(IntelData_t *targets, uint8_t max_targets);

/* ========================================================================== */
/*                  DİNLEYEN KULAK (PASİF FSR RADAR)                         */
/* ========================================================================== */

/**
 * @brief RF tarama başlat (SDR pasif mod)
 *        SDR'yi alıcı moduna al, geniş bant dinleme başlat
 * @param subsys  Alt sistem durumları
 */
void PAYLOAD_StartRFScan(SubsysStatus_t *subsys);

/**
 * @brief RF tarama durdur
 * @param subsys  Alt sistem durumları
 */
void PAYLOAD_StopRFScan(SubsysStatus_t *subsys);

/**
 * @brief RF gölge analizi simülasyonu
 *        Sivil/askeri vericilerin RF sinyallerindeki kesinti ve
 *        mikro-Doppler imzasından stealth hedef tespiti yapar.
 *
 *        Gerçekte: Jetson, SDR'den gelen ham IQ verisine FFT uygular,
 *        referans sinyal profiliyle karşılaştırarak anomali bulur.
 *
 * @param targets      Hedef dizisi çıktısı
 * @param max_targets  Dizi kapasitesi
 * @return Tespit edilen RF gölge sayısı
 */
uint8_t PAYLOAD_SimulateRFShadowDetection(IntelData_t *targets, uint8_t max_targets);

/**
 * @brief Mikro-Doppler imza analizi simülasyonu
 *        Uçak türbin/pervane imzasından hedef sınıflandırma
 * @param target  Mevcut hedef (classification alanı güncellenir)
 */
void PAYLOAD_AnalyzeMicroDoppler(IntelData_t *target);

/* ========================================================================== */
/*                       HEDEF YÖNETİMİ                                      */
/* ========================================================================== */

/**
 * @brief Hedef listesine yeni hedef ekle (duplikasyon kontrolü ile)
 * @param list       Mevcut hedef listesi
 * @param count      Mevcut hedef sayısı (güncellenir)
 * @param new_target Yeni tespit edilen hedef
 * @param max_count  Liste kapasitesi
 * @return true ise başarıyla eklendi
 */
bool PAYLOAD_AddTarget(IntelData_t *list, uint8_t *count,
                       const IntelData_t *new_target, uint8_t max_count);

/**
 * @brief Hedef listesini önceliğe göre sırala (yüksek öncelik başta)
 * @param list   Hedef listesi
 * @param count  Hedef sayısı
 */
void PAYLOAD_SortTargetsByPriority(IntelData_t *list, uint8_t count);

/**
 * @brief Süresi dolmuş hedefleri temizle
 * @param list         Hedef listesi
 * @param count        Hedef sayısı (güncellenir)
 * @param current_time Mevcut zaman (uptime saniye)
 * @param max_age_sec  Maksimum hedef yaşı (saniye)
 */
void PAYLOAD_PurgeStaleTargets(IntelData_t *list, uint8_t *count,
                               uint32_t current_time, uint32_t max_age_sec);

/**
 * @brief Tüm sensörleri kapat (güç tasarrufu)
 */
void PAYLOAD_AllSensorsOff(SubsysStatus_t *subsys);

#endif /* __GOLGE1_PAYLOAD_H */
