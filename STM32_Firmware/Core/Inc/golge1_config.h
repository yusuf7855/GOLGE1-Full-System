// GOLGE-1 Sistem Konfigurasyonu

#ifndef __GOLGE1_CONFIG_H
#define __GOLGE1_CONFIG_H

/* Uydu Kimligi */
#define GOLGE1_SAT_ID               "GLG1"
#define GOLGE1_CALLSIGN             "GLG1\x40\x40"
#define GROUND_CALLSIGN             "YERIST"
#define GOLGE1_CCSDS_APID           0x042
#define GOLGE1_PROTOCOL_VERSION     0x02

/* EPS - Batarya */
#define EPS_BATTERY_CAPACITY_WH     40.0f
#define EPS_BATTERY_VOLTAGE_NOM     7.4f
#define EPS_BATTERY_VOLTAGE_MIN     6.0f
#define EPS_BATTERY_VOLTAGE_MAX     8.4f

/* Batarya esikleri (%) */
#define EPS_BAT_CRITICAL            15.0f
#define EPS_BAT_LOW                 30.0f
#define EPS_BAT_NOMINAL             50.0f
#define EPS_BAT_HIGH                80.0f
#define EPS_BAT_FULL                95.0f

/* Gunes paneli */
#define EPS_SOLAR_MAX_POWER_MW      30000
#define EPS_SOLAR_ECLIPSE_POWER_MW  0
#define EPS_MPPT_EFFICIENCY         0.95f

/* Alt sistem guc tuketimi (mW) */
#define PWR_OBC_ACTIVE              500
#define PWR_OBC_SLEEP               50
#define PWR_JETSON_ACTIVE           5000
#define PWR_JETSON_IDLE             1500
#define PWR_JETSON_OFF              0
#define PWR_SDR_TX                  3000
#define PWR_SDR_RX_PASSIVE          1500
#define PWR_SDR_OFF                 0
#define PWR_CAMERA_ACTIVE           2000
#define PWR_CAMERA_OFF              0
#define PWR_IR_ACTIVE               1000
#define PWR_IR_OFF                  0
#define PWR_ADCS_ACTIVE             800
#define PWR_ADCS_IDLE               200
#define PWR_HEATER                  500

/* Termal limitler (C) */
#define THERMAL_MCU_WARNING         70
#define THERMAL_MCU_CRITICAL        85
#define THERMAL_AI_WARNING          65
#define THERMAL_AI_CRITICAL         80
#define THERMAL_SDR_WARNING         60
#define THERMAL_SDR_CRITICAL        75
#define THERMAL_BATTERY_MAX         45
#define THERMAL_BATTERY_MIN         (-10)
#define THERMAL_HEATER_ON_THRESH    (-5)
#define THERMAL_HEATER_OFF_THRESH   5

/* Durum makinesi zamanlayicilari */
#define SM_BOOT_DELAY_MS            5000
#define SM_DETUMBLE_TIMEOUT_MS      300000
#define SM_DETUMBLE_RATE_THRESH     0.5f     // rad/s
#define SM_SCAN_OPTICAL_MS          45000
#define SM_SCAN_RF_MS               60000
#define SM_COMMS_WINDOW_MS          30000
#define SM_COMMS_TIMEOUT_MS         120000
#define SM_CHARGE_TO_SCAN_THRESH    80.0f
#define SM_SCAN_TO_CHARGE_THRESH    40.0f

/* Haberlesme */
#define COMMS_UART_BAUDRATE         115200
#define COMMS_MAX_PACKET_SIZE       1024
#define COMMS_MAX_PAYLOAD_SIZE      768

/* AX.25 */
#define AX25_FLAG                   0x7E
#define AX25_CONTROL_UI             0x03
#define AX25_PID_NO_L3              0xF0
#define AX25_SSID_COMMAND           0x60
#define AX25_SSID_RESPONSE          0xE0
#define AX25_ADDR_LEN               7

/* CCSDS */
#define CCSDS_VERSION               0
#define CCSDS_TYPE_TLM              0
#define CCSDS_TYPE_CMD              1
#define CCSDS_SEC_HDR_PRESENT       1
#define CCSDS_SEQ_STANDALONE        3
#define CCSDS_PRIMARY_HDR_SIZE      6
#define CCSDS_SEC_HDR_SIZE          4

/* FHSS */
#define FHSS_CHANNEL_COUNT          16
#define FHSS_DWELL_TIME_MS          50
#define FHSS_SYNC_WORD              0xDEAD

/* Beacon */
#define COMMS_BEACON_INTERVAL_MS    10000

/* Sifreleme (AES-256) */
#define CRYPTO_AES_KEY_BITS         256
#define CRYPTO_AES_KEY_BYTES        32
#define CRYPTO_AES_BLOCK_BYTES      16
#define CRYPTO_AES_ROUNDS           14
#define CRYPTO_AES_EXPANDED_SIZE    240      // (14+1)*16
#define CRYPTO_IV_BYTES             16
#define CRYPTO_HMAC_SIZE            4

/* Store & Forward */
#define SF_MAX_PACKETS              64
#define SF_PACKET_DATA_SIZE         256
#define SF_PRIORITY_CRITICAL        3
#define SF_PRIORITY_HIGH            2
#define SF_PRIORITY_NORMAL          1
#define SF_PRIORITY_LOW             0
#define SF_MAX_RETRY                5

/* Gorev yuku */
#define PAYLOAD_MAX_TARGETS         8
#define PAYLOAD_CONFIDENCE_THRESH   0.65f
#define PAYLOAD_GSD_METERS          3.5f
#define PAYLOAD_IR_TEMP_THRESH      400.0f
#define PAYLOAD_IR_COLD_THRESH      (-40.0f)
#define PAYLOAD_DOPPLER_THRESH      0.02f
#define PAYLOAD_SHADOW_DB_THRESH    3.0f

/* Sensor ID */
#define SENSOR_ID_OPTICAL           0
#define SENSOR_ID_IR                1
#define SENSOR_ID_RF_FSR            2
#define SENSOR_ID_DOPPLER           3

/* FDIR */
#define FDIR_MAX_CONSECUTIVE_REBOOT 3
#define FDIR_COMM_TIMEOUT_MS        600000   // 10dk sessizlik -> safe mode
#define FDIR_HEARTBEAT_MS           1000
#define FDIR_ANOMALY_WINDOW         10
#define FDIR_ANOMALY_THRESHOLD      5

/* TMR */
#define TMR_COPY_COUNT              3
#define TMR_SCRUB_INTERVAL_MS       5000
#define TMR_VOTE_MAJORITY           2

/* FreeRTOS stack boyutlari (word) */
#define STACK_SIZE_SM               (256 * 4)
#define STACK_SIZE_TELEM            (512 * 4)
#define STACK_SIZE_COMMS            (512 * 4)
#define STACK_SIZE_PAYLOAD          (256 * 4)
#define STACK_SIZE_EPS              (256 * 4)
#define STACK_SIZE_FDIR             (256 * 4)
#define STACK_SIZE_SF               (256 * 4)

/* Queue boyutlari */
#define QUEUE_TELEMETRY_DEPTH       8
#define QUEUE_COMMS_DEPTH           16
#define QUEUE_EVENT_DEPTH           32
#define QUEUE_CMD_DEPTH             8

/* Task periyotlari (ms) */
#define TASK_SM_PERIOD_MS           100
#define TASK_TELEM_PERIOD_MS        2000
#define TASK_COMMS_PERIOD_MS        100
#define TASK_PAYLOAD_PERIOD_MS      500
#define TASK_EPS_PERIOD_MS          250
#define TASK_FDIR_PERIOD_MS         200
#define TASK_SF_PERIOD_MS           1000

/* GPIO */
#define PIN_JETSON_POWER            GPIO_PIN_1
#define PIN_SDR_POWER               GPIO_PIN_2
#define PIN_CAMERA_POWER            GPIO_PIN_3
#define PIN_HEATER_ENABLE           GPIO_PIN_4
#define PIN_DEPLOY_PANEL            GPIO_PIN_5
#define PIN_STATUS_LED              GPIO_PIN_6
#define PORT_SUBSYS_POWER           GPIOA
#define PORT_DEPLOY                 GPIOB
#define PORT_LED                    GPIOC

#endif /* __GOLGE1_CONFIG_H */
