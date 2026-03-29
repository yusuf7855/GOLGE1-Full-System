/* USER CODE BEGIN Header */
// GOLGE-1 Ana Baslik Dosyasi
/* USER CODE END Header */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdbool.h>
#include "golge1_config.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* Durum makinesi */
typedef enum {
    STATE_BOOT          = 0x00,
    STATE_DETUMBLE      = 0x01,
    STATE_IDLE_CHARGE   = 0x02,
    STATE_OPTICAL_SCAN  = 0x10,  // Optik/IR tarama
    STATE_RF_SCAN       = 0x11,  // Pasif FSR
    STATE_COMMS_RELAY   = 0x20,  // Sifreli TX
    STATE_STORE_FWD     = 0x21,  // Kaydet-ilet
    STATE_SAFE_MODE     = 0xFF   // Minimum guc
} Golge1_State_t;

typedef enum {
    SUBSYS_OFF     = 0,
    SUBSYS_STANDBY = 1,
    SUBSYS_ACTIVE  = 2
} SubsysPower_t;

/* Sistem olaylari */
typedef enum {
    EVT_NONE                = 0x00,
    EVT_POST_COMPLETE       = 0x01,
    EVT_DETUMBLE_DONE       = 0x02,
    EVT_CHARGE_SUFFICIENT   = 0x10,
    EVT_TARGET_DETECTED     = 0x11,
    EVT_SCAN_COMPLETE       = 0x12,
    EVT_COMMS_WINDOW_OPEN   = 0x13,
    EVT_COMMS_WINDOW_CLOSE  = 0x14,
    EVT_TX_COMPLETE         = 0x15,
    EVT_ALL_PACKETS_SENT    = 0x16,
    EVT_BATTERY_LOW         = 0x20,
    EVT_BATTERY_CRITICAL    = 0x21,
    EVT_THERMAL_WARNING     = 0x22,
    EVT_THERMAL_CRITICAL    = 0x23,
    EVT_COMM_TIMEOUT        = 0x30,
    EVT_WATCHDOG_ALERT      = 0x31,
    EVT_RADIATION_SEU       = 0x32,
    EVT_SUBSYS_FAILURE      = 0x33,
    EVT_TMR_MISMATCH        = 0x34,
    EVT_GROUND_CMD          = 0x40,
    EVT_FORCE_SAFE_MODE     = 0x41,
    EVT_FORCE_REBOOT        = 0x42,
    EVT_EXIT_SAFE_MODE      = 0x43
} Golge1_Event_t;

/* Yer istasyonu komutlari */
typedef enum {
    CMD_NOP             = 0x00,
    CMD_SET_STATE       = 0x01,
    CMD_START_SCAN      = 0x02,
    CMD_ABORT_SCAN      = 0x03,
    CMD_DOWNLINK_DATA   = 0x04,
    CMD_UPDATE_KEY      = 0x05,
    CMD_SET_FREQ_TABLE  = 0x06,
    CMD_REBOOT          = 0xFE,
    CMD_SAFE_MODE       = 0xFF
} CmdCode_t;

/* Hedef istihbarat verisi */
typedef struct {
    char     id[12];
    char     type[16];             // OPTICAL, IR_ANOMALY, RF_SHADOW, MICRO_DOPPLER
    char     classification[16];   // TANK, APC, AIRCRAFT, SHIP, UNKNOWN
    float    lat;
    float    lon;
    float    alt;
    uint16_t heading;              // derece (0-359)
    uint16_t speed;                // km/h
    float    confidence;           // 0.0 - 1.0
    uint32_t detect_time;          // uptime (s)
    uint8_t  priority;             // 0: dusuk, 3: kritik
    uint8_t  sensor_id;
    float    signal_strength;      // dBm veya SNR
    uint8_t  is_active;
} IntelData_t;

/* EPS durumu */
typedef struct {
    float    bat_voltage;
    float    bat_current;          // mA, + sarj / - desarj
    float    bat_percent;
    float    bat_temp;
    float    bat_energy_wh;
    float    solar_voltage;
    float    solar_current;
    float    solar_power_mw;
    uint8_t  in_eclipse;
    float    total_draw_mw;
    float    power_margin_mw;
    uint8_t  heater_active;
} EPSData_t;

/* Termal veriler */
typedef struct {
    int8_t   mcu;
    int8_t   ai;
    int8_t   sdr;
    int8_t   battery;
    int8_t   external;
    int8_t   camera;
} ThermalData_t;

/* ADCS verisi */
typedef struct {
    float    quaternion[4];        // [w, x, y, z]
    float    angular_rate[3];      // rad/s
    float    mag_field[3];         // uT
    float    sun_vector[3];
    uint8_t  pointing_mode;        // 0:Nadir 1:Target 2:Sun 3:GS
    uint8_t  is_stable;
    float    pointing_error;       // derece
} ADCSData_t;

/* Alt sistem durumu */
typedef struct {
    SubsysPower_t jetson;
    SubsysPower_t sdr;
    SubsysPower_t camera;
    SubsysPower_t ir_sensor;
    SubsysPower_t adcs;
    SubsysPower_t heater;
} SubsysStatus_t;

/* Sistem istatistikleri */
typedef struct {
    uint32_t uptime_sec;
    uint32_t boot_count;
    uint32_t error_count;
    uint16_t reboot_count;
    uint32_t packets_sent;
    uint32_t packets_stored;
    uint32_t targets_detected;
    uint32_t tmr_corrections;
    uint32_t crc_errors;
    uint32_t last_ground_contact;
} StatsData_t;

/* Paket basligi */
typedef struct {
    char     sat_id[5];
    uint8_t  version;
    uint8_t  state;
    uint16_t sequence;
    uint32_t timestamp;
    uint16_t packet_type;          // 0:tlm 1:intel 2:beacon
} HeaderData_t;

/* Master telemetri yapisi */
typedef struct {
    HeaderData_t   header;
    EPSData_t      eps;
    ThermalData_t  thermal;
    ADCSData_t     adcs;
    SubsysStatus_t subsystems;
    StatsData_t    stats;
    IntelData_t    targets[PAYLOAD_MAX_TARGETS];
    uint8_t        target_count;
} Golge1_Telemetry_t;

/* TMR korunmali kritik veriler */
typedef struct {
    Golge1_State_t state[TMR_COPY_COUNT];
    float          battery[TMR_COPY_COUNT];
    uint32_t       timestamp[TMR_COPY_COUNT];
    uint32_t       boot_count[TMR_COPY_COUNT];
    uint16_t       sequence[TMR_COPY_COUNT];
    uint32_t       checksum[TMR_COPY_COUNT];
} TMR_CriticalData_t;

/* Store & Forward paketi */
typedef struct {
    uint8_t  is_valid;
    uint8_t  priority;
    uint8_t  retry_count;
    uint8_t  reserved;
    uint16_t data_length;
    uint32_t timestamp;
    uint8_t  data[SF_PACKET_DATA_SIZE];
} SF_Packet_t;

/* Yer istasyonu komutu */
typedef struct {
    uint8_t  cmd_code;
    uint8_t  param_count;
    uint8_t  payload[32];
    uint16_t payload_length;
    uint32_t timestamp;
    uint16_t auth_tag;
} GroundCommand_t;

/* Olay mesaji (event queue) */
typedef struct {
    Golge1_Event_t event;
    uint8_t        source;         // kaynak task ID
    uint32_t       timestamp;
    uint32_t       param;
} EventMessage_t;

/* Comms paketi */
typedef struct {
    uint8_t  data[COMMS_MAX_PACKET_SIZE];
    uint16_t length;
    uint8_t  priority;
    uint8_t  encrypted;
} CommsPacket_t;

/* Fonksiyon prototipleri */
void Error_Handler(void);
void GOLGE1_System_Init(void);
void GOLGE1_POST(void);

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

#define GOLGE1_MIN(a, b)  ((a) < (b) ? (a) : (b))
#define GOLGE1_MAX(a, b)  ((a) > (b) ? (a) : (b))
#define GOLGE1_CLAMP(x, lo, hi) (GOLGE1_MIN(GOLGE1_MAX((x), (lo)), (hi)))

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
