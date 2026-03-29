/* USER CODE BEGIN Header */
/**
  * @file           : freertos.c
  * @brief          : GOLGE-1 FreeRTOS Task tanimlari
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>
#include "golge1_config.h"
#include "golge1_comms.h"
#include "golge1_eps.h"
#include "golge1_payload.h"
#include "golge1_fdir.h"
#include "golge1_adcs.h"
#include "golge1_command.h"
#include "golge1_ecc.h"
#include "golge1_orbit.h"
#include "golge1_flash.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

extern UART_HandleTypeDef   huart2;
extern RNG_HandleTypeDef    hrng;
extern Golge1_Telemetry_t   satellite;
extern TMR_CriticalData_t   tmr_data;
extern SF_Packet_t           sf_buffer[];
extern uint8_t               sf_write_idx;
extern uint8_t               sf_read_idx;
extern uint8_t               sf_count;

#define TASK_ID_SM          0
#define TASK_ID_TELEM       1
#define TASK_ID_COMMS       2
#define TASK_ID_PAYLOAD     3
#define TASK_ID_EPS         4
#define TASK_ID_FDIR        5
#define TASK_ID_SF          6

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

static osMutexId_t dataMutex;
static osMutexId_t uartMutex;

static osMessageQueueId_t commsQueue;
static osMessageQueueId_t eventQueue;

static osEventFlagsId_t systemFlags;

#define FLAG_TARGET_FOUND       (1 << 0)
#define FLAG_SCAN_COMPLETE      (1 << 1)
#define FLAG_COMMS_DONE         (1 << 2)
#define FLAG_CHARGE_READY       (1 << 3)
#define FLAG_SAFE_MODE          (1 << 4)
#define FLAG_NEW_TELEMETRY      (1 << 5)

static uint32_t state_entry_tick = 0;
static uint32_t scan_start_tick = 0;

/* USER CODE END Variables */

/* Definitions for tasks */
osThreadId_t smTaskHandle;
osThreadId_t telemTaskHandle;
osThreadId_t commsTaskHandle;
osThreadId_t payloadTaskHandle;
osThreadId_t epsTaskHandle;
osThreadId_t fdirTaskHandle;
osThreadId_t sfTaskHandle;

const osThreadAttr_t smTask_attr = {
  .name = "StateMachine",
  .stack_size = STACK_SIZE_SM,
  .priority = (osPriority_t) osPriorityHigh,
};
const osThreadAttr_t telemTask_attr = {
  .name = "Telemetry",
  .stack_size = STACK_SIZE_TELEM,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
const osThreadAttr_t commsTask_attr = {
  .name = "Comms",
  .stack_size = STACK_SIZE_COMMS,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t payloadTask_attr = {
  .name = "Payload",
  .stack_size = STACK_SIZE_PAYLOAD,
  .priority = (osPriority_t) osPriorityNormal,
};
const osThreadAttr_t epsTask_attr = {
  .name = "EPS",
  .stack_size = STACK_SIZE_EPS,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
const osThreadAttr_t fdirTask_attr = {
  .name = "FDIR",
  .stack_size = STACK_SIZE_FDIR,
  .priority = (osPriority_t) osPriorityRealtime,
};
const osThreadAttr_t sfTask_attr = {
  .name = "StoreFwd",
  .stack_size = STACK_SIZE_SF,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void Task_StateMachine(void *argument);
void Task_Telemetry(void *argument);
void Task_Comms(void *argument);
void Task_Payload(void *argument);
void Task_EPS(void *argument);
void Task_FDIR(void *argument);
void Task_StoreForward(void *argument);
/* USER CODE END FunctionPrototypes */

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */

  const osMutexAttr_t dataMutex_attr = { .name = "dataMutex" };
  const osMutexAttr_t uartMutex_attr = { .name = "uartMutex" };
  dataMutex = osMutexNew(&dataMutex_attr);
  uartMutex = osMutexNew(&uartMutex_attr);

  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */

  commsQueue = osMessageQueueNew(QUEUE_COMMS_DEPTH, sizeof(CommsPacket_t), NULL);
  eventQueue = osMessageQueueNew(QUEUE_EVENT_DEPTH, sizeof(EventMessage_t), NULL);
  systemFlags = osEventFlagsNew(NULL);

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  smTaskHandle      = osThreadNew(Task_StateMachine, NULL, &smTask_attr);
  telemTaskHandle   = osThreadNew(Task_Telemetry, NULL, &telemTask_attr);
  commsTaskHandle   = osThreadNew(Task_Comms, NULL, &commsTask_attr);
  payloadTaskHandle = osThreadNew(Task_Payload, NULL, &payloadTask_attr);
  epsTaskHandle     = osThreadNew(Task_EPS, NULL, &epsTask_attr);
  fdirTaskHandle    = osThreadNew(Task_FDIR, NULL, &fdirTask_attr);
  sfTaskHandle      = osThreadNew(Task_StoreForward, NULL, &sfTask_attr);

  /* USER CODE BEGIN RTOS_THREADS */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_Task_StateMachine */
/* Master durum makinesi, 100ms periyot */
/* USER CODE END Header_Task_StateMachine */
void Task_StateMachine(void *argument)
{
  /* USER CODE BEGIN Task_StateMachine */

  Golge1_State_t current_state = STATE_BOOT;
  Golge1_State_t next_state = STATE_BOOT;
  EventMessage_t event_msg;
  uint8_t scan_cycle = 0; /* 0: optik, 1: RF */

  state_entry_tick = HAL_GetTick();
  TMR_WriteState(&tmr_data, STATE_BOOT);

  for(;;)
  {
    FDIR_TaskHeartbeat(TASK_ID_SM);
    uint32_t now = HAL_GetTick();

    current_state = TMR_ReadState(&tmr_data);

    while (osMessageQueueGet(eventQueue, &event_msg, NULL, 0) == osOK) {
        switch (event_msg.event) {
            case EVT_BATTERY_CRITICAL:
                next_state = STATE_SAFE_MODE;
                break;
            case EVT_BATTERY_LOW:
                if (current_state != STATE_SAFE_MODE)
                    next_state = STATE_IDLE_CHARGE;
                break;
            case EVT_THERMAL_CRITICAL:
                next_state = STATE_IDLE_CHARGE;
                break;
            case EVT_TARGET_DETECTED:
                osEventFlagsSet(systemFlags, FLAG_TARGET_FOUND);
                break;
            case EVT_TX_COMPLETE:
            case EVT_ALL_PACKETS_SENT:
                osEventFlagsSet(systemFlags, FLAG_COMMS_DONE);
                break;
            case EVT_FORCE_SAFE_MODE:
                next_state = STATE_SAFE_MODE;
                break;
            case EVT_EXIT_SAFE_MODE:
                next_state = STATE_IDLE_CHARGE;
                break;
            default:
                break;
        }
    }

    switch (current_state) {

        case STATE_BOOT:
            if ((now - state_entry_tick) >= SM_BOOT_DELAY_MS) {
                next_state = STATE_DETUMBLE;
            }
            break;

        case STATE_DETUMBLE:
        {
            osMutexAcquire(dataMutex, osWaitForever);
            float ang_rate = satellite.adcs.angular_rate[0] *
                            satellite.adcs.angular_rate[0] +
                            satellite.adcs.angular_rate[1] *
                            satellite.adcs.angular_rate[1] +
                            satellite.adcs.angular_rate[2] *
                            satellite.adcs.angular_rate[2];
            osMutexRelease(dataMutex);

            if (ang_rate < (SM_DETUMBLE_RATE_THRESH * SM_DETUMBLE_RATE_THRESH) ||
                (now - state_entry_tick) >= SM_DETUMBLE_TIMEOUT_MS) {
                osMutexAcquire(dataMutex, osWaitForever);
                satellite.adcs.is_stable = 1;
                osMutexRelease(dataMutex);
                next_state = STATE_IDLE_CHARGE;
            }
            break;
        }

        case STATE_IDLE_CHARGE:
        {
            float bat;
            osMutexAcquire(dataMutex, osWaitForever);
            bat = satellite.eps.bat_percent;
            osMutexRelease(dataMutex);

            if (bat >= SM_CHARGE_TO_SCAN_THRESH) {
                osEventFlagsSet(systemFlags, FLAG_CHARGE_READY);
                if (scan_cycle % 2 == 0)
                    next_state = STATE_OPTICAL_SCAN;
                else
                    next_state = STATE_RF_SCAN;
                scan_cycle++;
            }
            break;
        }

        case STATE_OPTICAL_SCAN:
        {
            uint32_t flags = osEventFlagsGet(systemFlags);

            if (flags & FLAG_TARGET_FOUND) {
                osEventFlagsClear(systemFlags, FLAG_TARGET_FOUND);
                next_state = STATE_COMMS_RELAY;
            }
            else if ((now - state_entry_tick) >= SM_SCAN_OPTICAL_MS) {
                next_state = STATE_IDLE_CHARGE;
            }
            else {
                osMutexAcquire(dataMutex, osWaitForever);
                float bat = satellite.eps.bat_percent;
                osMutexRelease(dataMutex);
                if (bat <= SM_SCAN_TO_CHARGE_THRESH)
                    next_state = STATE_IDLE_CHARGE;
            }
            break;
        }

        case STATE_RF_SCAN:
        {
            uint32_t flags = osEventFlagsGet(systemFlags);

            if (flags & FLAG_TARGET_FOUND) {
                osEventFlagsClear(systemFlags, FLAG_TARGET_FOUND);
                next_state = STATE_COMMS_RELAY;
            }
            else if ((now - state_entry_tick) >= SM_SCAN_RF_MS) {
                next_state = STATE_IDLE_CHARGE;
            }
            else {
                osMutexAcquire(dataMutex, osWaitForever);
                float bat = satellite.eps.bat_percent;
                osMutexRelease(dataMutex);
                if (bat <= SM_SCAN_TO_CHARGE_THRESH)
                    next_state = STATE_IDLE_CHARGE;
            }
            break;
        }

        case STATE_COMMS_RELAY:
        {
            uint32_t flags = osEventFlagsGet(systemFlags);

            if (flags & FLAG_COMMS_DONE) {
                osEventFlagsClear(systemFlags, FLAG_COMMS_DONE);
                next_state = STATE_IDLE_CHARGE;
            }
            else if ((now - state_entry_tick) >= SM_COMMS_TIMEOUT_MS) {
                next_state = STATE_STORE_FWD;
            }
            break;
        }

        case STATE_STORE_FWD:
        {
            if ((now - state_entry_tick) >= 5000) {
                next_state = STATE_IDLE_CHARGE;
            }
            break;
        }

        case STATE_SAFE_MODE:
        {
            osMutexAcquire(dataMutex, osWaitForever);
            float bat = satellite.eps.bat_percent;
            osMutexRelease(dataMutex);

            /* oto cikis: bat>%50 ve 5dk gecti */
            if (bat >= EPS_BAT_NOMINAL &&
                (now - state_entry_tick) >= 300000) {
                next_state = STATE_IDLE_CHARGE;
            }
            break;
        }
    }

    if (next_state != current_state) {
        osMutexAcquire(dataMutex, osWaitForever);

        SubsysStatus_t profile = EPS_GetRecommendedProfile(
            satellite.eps.bat_percent, next_state);
        satellite.subsystems = profile;
        satellite.header.state = (uint8_t)next_state;

        osMutexRelease(dataMutex);

        TMR_WriteState(&tmr_data, next_state);
        state_entry_tick = now;

        EPS_SetSubsystemPower(PIN_JETSON_POWER, profile.jetson);
        EPS_SetSubsystemPower(PIN_SDR_POWER, profile.sdr);
        EPS_SetSubsystemPower(PIN_CAMERA_POWER, profile.camera);

        if (next_state == STATE_SAFE_MODE) {
            EPS_EmergencyShutdown();
        }
    }

    osDelay(TASK_SM_PERIOD_MS);
  }
  /* USER CODE END Task_StateMachine */
}

/* USER CODE BEGIN Header_Task_Telemetry */
/* Telemetri toplama, JSON olustur, 2s periyot */
/* USER CODE END Header_Task_Telemetry */
void Task_Telemetry(void *argument)
{
  /* USER CODE BEGIN Task_Telemetry */

  static char json_buffer[COMMS_MAX_PAYLOAD_SIZE];
  static uint16_t packet_sequence = 0;

  for(;;)
  {
    FDIR_TaskHeartbeat(TASK_ID_TELEM);

    osMutexAcquire(dataMutex, osWaitForever);

    satellite.header.timestamp = HAL_GetTick() / 1000;
    satellite.header.sequence = packet_sequence;
    satellite.header.packet_type = (satellite.target_count > 0) ? 1 : 0;
    satellite.stats.uptime_sec = satellite.header.timestamp;

    TMR_WriteTimestamp(&tmr_data, satellite.header.timestamp);
    TMR_WriteSequence(&tmr_data, packet_sequence);
    TMR_WriteBattery(&tmr_data, satellite.eps.bat_percent);

    int json_len = COMMS_TelemetryToJSON(&satellite, json_buffer,
                                          sizeof(json_buffer));

    osMutexRelease(dataMutex);

    if (json_len > 0) {
        CommsPacket_t pkt;
        memcpy(pkt.data, json_buffer, GOLGE1_MIN(json_len, COMMS_MAX_PACKET_SIZE));
        pkt.length = (uint16_t)json_len;
        pkt.priority = SF_PRIORITY_NORMAL;
        pkt.encrypted = 0;

        osMutexAcquire(dataMutex, osWaitForever);
        if (satellite.target_count > 0)
            pkt.priority = SF_PRIORITY_CRITICAL;
        osMutexRelease(dataMutex);

        osMessageQueuePut(commsQueue, &pkt, 0, 100);
        osEventFlagsSet(systemFlags, FLAG_NEW_TELEMETRY);
    }

    packet_sequence++;

    osDelay(TASK_TELEM_PERIOD_MS);
  }
  /* USER CODE END Task_Telemetry */
}

/* USER CODE BEGIN Header_Task_Comms */
/* TX/RX haberlesme, sifreleme, beacon */
/* USER CODE END Header_Task_Comms */
void Task_Comms(void *argument)
{
  /* USER CODE BEGIN Task_Comms */

  static uint8_t tx_frame[COMMS_MAX_PACKET_SIZE];
  static uint16_t tx_frame_len = 0;
  static uint32_t last_beacon_tick = 0;
  static uint16_t comms_seq = 0;

  CommsPacket_t rx_pkt;

  for(;;)
  {
    FDIR_TaskHeartbeat(TASK_ID_COMMS);

    Golge1_State_t state = TMR_ReadState(&tmr_data);
    uint32_t now = HAL_GetTick();

    if (state == STATE_COMMS_RELAY) {
        if (osMessageQueueGet(commsQueue, &rx_pkt, NULL, 100) == osOK) {

            uint8_t channel = FHSS_NextChannel();
            (void)channel; /* SDR'de frekans degisir */

            COMMS_PrepareSecurePacket(
                (const char *)rx_pkt.data,
                tx_frame,
                &tx_frame_len,
                comms_seq++
            );

            osMutexAcquire(uartMutex, osWaitForever);
            HAL_UART_Transmit(&huart2, tx_frame, tx_frame_len, HAL_MAX_DELAY);
            osMutexRelease(uartMutex);

            osMutexAcquire(dataMutex, osWaitForever);
            satellite.stats.packets_sent++;
            osMutexRelease(dataMutex);

            if (osMessageQueueGetCount(commsQueue) == 0) {
                EventMessage_t evt = {
                    .event = EVT_ALL_PACKETS_SENT,
                    .source = TASK_ID_COMMS,
                    .timestamp = now / 1000
                };
                osMessageQueuePut(eventQueue, &evt, 0, 0);
            }
        }
    }

    else if (state == STATE_SAFE_MODE || state == STATE_IDLE_CHARGE) {
        if ((now - last_beacon_tick) >= COMMS_BEACON_INTERVAL_MS) {
            last_beacon_tick = now;

            float bat;
            osMutexAcquire(dataMutex, osWaitForever);
            bat = satellite.eps.bat_percent;
            osMutexRelease(dataMutex);

            COMMS_CreateBeacon(bat, (uint8_t)state, tx_frame, &tx_frame_len);

            osMutexAcquire(uartMutex, osWaitForever);
            HAL_UART_Transmit(&huart2, tx_frame, tx_frame_len, HAL_MAX_DELAY);
            osMutexRelease(uartMutex);

            osMutexAcquire(dataMutex, osWaitForever);
            satellite.stats.packets_sent++;
            osMutexRelease(dataMutex);
        }
    }

    /* Uplink komut alimi (her durumda) */
    if (CMD_IsPacketReady()) {
        uint8_t cmd_raw[CMD_MAX_PACKET_SIZE];
        uint16_t cmd_len;
        CMD_GetReceivedPacket(cmd_raw, &cmd_len);

        ParsedCommand_t parsed;
        CmdResult_t parse_result = CMD_Parse(cmd_raw, cmd_len, &parsed);

        if (parse_result == CMD_RESULT_OK) {
            CmdResponse_t response;
            CMD_Execute(&parsed, &response);

            uint8_t resp_buf[64];
            uint16_t resp_len;
            CMD_BuildResponse(&response, resp_buf, &resp_len);

            osMutexAcquire(uartMutex, osWaitForever);
            HAL_UART_Transmit(&huart2, resp_buf, resp_len, HAL_MAX_DELAY);
            osMutexRelease(uartMutex);

            osMutexAcquire(dataMutex, osWaitForever);
            satellite.stats.last_ground_contact = HAL_GetTick() / 1000;
            osMutexRelease(dataMutex);
        }
    }

    osDelay(TASK_COMMS_PERIOD_MS);
  }
  /* USER CODE END Task_Comms */
}

/* USER CODE BEGIN Header_Task_Payload */
/* Gorev yuku: optik/IR/RF tarama */
/* USER CODE END Header_Task_Payload */
void Task_Payload(void *argument)
{
  /* USER CODE BEGIN Task_Payload */

  IntelData_t new_targets[4];

  for(;;)
  {
    FDIR_TaskHeartbeat(TASK_ID_PAYLOAD);

    Golge1_State_t state = TMR_ReadState(&tmr_data);

    switch (state) {

        case STATE_OPTICAL_SCAN:
        {
            uint8_t opt_count = PAYLOAD_SimulateOpticalDetection(
                new_targets, 3);

            uint8_t ir_count = PAYLOAD_SimulateIRDetection(
                &new_targets[opt_count], 1);

            uint8_t total = opt_count + ir_count;

            if (total > 0) {
                osMutexAcquire(dataMutex, osWaitForever);

                for (uint8_t i = 0; i < total; i++) {
                    PAYLOAD_AddTarget(satellite.targets,
                                     &satellite.target_count,
                                     &new_targets[i],
                                     PAYLOAD_MAX_TARGETS);
                }
                satellite.stats.targets_detected += total;

                PAYLOAD_SortTargetsByPriority(
                    satellite.targets, satellite.target_count);

                osMutexRelease(dataMutex);

                EventMessage_t evt = {
                    .event = EVT_TARGET_DETECTED,
                    .source = TASK_ID_PAYLOAD,
                    .timestamp = HAL_GetTick() / 1000,
                    .param = total
                };
                osMessageQueuePut(eventQueue, &evt, 0, 0);
            }
            break;
        }

        case STATE_RF_SCAN:
        {
            uint8_t rf_count = PAYLOAD_SimulateRFShadowDetection(
                new_targets, 2);

            if (rf_count > 0) {
                osMutexAcquire(dataMutex, osWaitForever);

                for (uint8_t i = 0; i < rf_count; i++) {
                    PAYLOAD_AddTarget(satellite.targets,
                                     &satellite.target_count,
                                     &new_targets[i],
                                     PAYLOAD_MAX_TARGETS);
                }
                satellite.stats.targets_detected += rf_count;

                PAYLOAD_SortTargetsByPriority(
                    satellite.targets, satellite.target_count);

                osMutexRelease(dataMutex);

                EventMessage_t evt = {
                    .event = EVT_TARGET_DETECTED,
                    .source = TASK_ID_PAYLOAD,
                    .timestamp = HAL_GetTick() / 1000,
                    .param = rf_count
                };
                osMessageQueuePut(eventQueue, &evt, 0, 0);
            }
            break;
        }

        default:
        {
            /* Eski hedefleri temizle (5dk) */
            osMutexAcquire(dataMutex, osWaitForever);
            PAYLOAD_PurgeStaleTargets(satellite.targets,
                                      &satellite.target_count,
                                      HAL_GetTick() / 1000, 300);
            osMutexRelease(dataMutex);
            break;
        }
    }

    osDelay(TASK_PAYLOAD_PERIOD_MS);
  }
  /* USER CODE END Task_Payload */
}

/* USER CODE BEGIN Header_Task_EPS */
/* Guc yonetimi, ADCS, yorunge */
/* USER CODE END Header_Task_EPS */
void Task_EPS(void *argument)
{
  /* USER CODE BEGIN Task_EPS */

  for(;;)
  {
    FDIR_TaskHeartbeat(TASK_ID_EPS);

    osMutexAcquire(dataMutex, osWaitForever);

    EPS_UpdateSolarPower(&satellite.eps);

    satellite.eps.total_draw_mw = EPS_CalculatePowerDraw(&satellite.subsystems);
    satellite.eps.power_margin_mw = satellite.eps.solar_power_mw -
                                     satellite.eps.total_draw_mw;

    EPS_UpdateBatteryStatus(&satellite.eps);
    EPS_UpdateThermalSensors(&satellite.thermal, &satellite.subsystems);
    EPS_ThermalManagement(&satellite.thermal, &satellite.eps);

    Golge1_Event_t anomaly = EPS_CheckAnomalies(&satellite.eps, &satellite.thermal);

    osMutexRelease(dataMutex);

    if (anomaly != EVT_NONE) {
        EventMessage_t evt = {
            .event = anomaly,
            .source = TASK_ID_EPS,
            .timestamp = HAL_GetTick() / 1000
        };
        osMessageQueuePut(eventQueue, &evt, 0, 0);
    }

    /* ADCS guncelle */
    Golge1_State_t state = TMR_ReadState(&tmr_data);
    uint8_t adcs_mode;
    switch (state) {
        case STATE_BOOT:
        case STATE_DETUMBLE:     adcs_mode = ADCS_MODE_DETUMBLE; break;
        case STATE_OPTICAL_SCAN:
        case STATE_RF_SCAN:      adcs_mode = ADCS_MODE_TARGET;   break;
        case STATE_COMMS_RELAY:  adcs_mode = ADCS_MODE_GROUND_STATION; break;
        case STATE_IDLE_CHARGE:  adcs_mode = ADCS_MODE_SUN;      break;
        default:                 adcs_mode = ADCS_MODE_NADIR;    break;
    }

    osMutexAcquire(dataMutex, osWaitForever);
    ADCS_Update(&satellite.adcs, adcs_mode, ADCS_DT);
    osMutexRelease(dataMutex);

    /* Yorunge propagasyonu */
    static SatPosition_t sat_pos;
    ORBIT_Propagate(HAL_GetTick() / 1000, &sat_pos);

    EclipseInfo_t eclipse;
    ORBIT_CalculateEclipse(&sat_pos, HAL_GetTick() / 1000, &eclipse);

    osMutexAcquire(dataMutex, osWaitForever);
    satellite.eps.in_eclipse = eclipse.in_eclipse ? 1 : 0;
    if (eclipse.in_eclipse)
        satellite.eps.solar_power_mw = 0;
    osMutexRelease(dataMutex);

    if (sat_pos.gs_visible) {
        EventMessage_t evt = {
            .event = EVT_COMMS_WINDOW_OPEN,
            .source = TASK_ID_EPS,
            .timestamp = HAL_GetTick() / 1000
        };
        osMessageQueuePut(eventQueue, &evt, 0, 0);
    }

    osDelay(TASK_EPS_PERIOD_MS);
  }
  /* USER CODE END Task_EPS */
}

/* USER CODE BEGIN Header_Task_FDIR */
/* Watchdog, TMR scrub, saglik kontrolu */
/* USER CODE END Header_Task_FDIR */
void Task_FDIR(void *argument)
{
  /* USER CODE BEGIN Task_FDIR */

  uint32_t scrub_tick = 0;

  for(;;)
  {
    FDIR_TaskHeartbeat(TASK_ID_FDIR);
    uint32_t now = HAL_GetTick();

    /* TMR scrub (5s periyot) */
    if ((now - scrub_tick) >= TMR_SCRUB_INTERVAL_MS) {
        scrub_tick = now;

        osMutexAcquire(dataMutex, osWaitForever);
        bool corrected = TMR_ScrubAndVerify(&tmr_data, &satellite.stats);
        osMutexRelease(dataMutex);

        if (corrected) {
            EventMessage_t evt = {
                .event = EVT_TMR_MISMATCH,
                .source = TASK_ID_FDIR,
                .timestamp = now / 1000
            };
            osMessageQueuePut(eventQueue, &evt, 0, 0);
        }
    }

    /* Sistem saglik kontrolu */
    osMutexAcquire(dataMutex, osWaitForever);
    Golge1_Event_t health_event = FDIR_SystemHealthCheck(
        &satellite.eps, &satellite.thermal,
        &satellite.stats, &tmr_data);
    osMutexRelease(dataMutex);

    if (health_event != EVT_NONE) {
        osMutexAcquire(dataMutex, osWaitForever);
        Golge1_State_t recovery_state = FDIR_ExecuteRecovery(
            health_event, &satellite.stats);
        osMutexRelease(dataMutex);

        if (health_event == EVT_BATTERY_CRITICAL ||
            health_event == EVT_RADIATION_SEU) {
            TMR_WriteState(&tmr_data, recovery_state);
        }

        EventMessage_t evt = {
            .event = health_event,
            .source = TASK_ID_FDIR,
            .timestamp = now / 1000
        };
        osMessageQueuePut(eventQueue, &evt, 0, 0);
    }

    FDIR_FeedWatchdog();

    /* Flash telemetri logu (30s) */
    static uint32_t last_log_tick = 0;
    if ((now - last_log_tick) >= 30000) {
        last_log_tick = now;

        osMutexAcquire(dataMutex, osWaitForever);
        FlashTelemLog_t log_entry = {
            .magic = 0x544C4F47,  /* "TLOG" */
            .timestamp = satellite.stats.uptime_sec,
            .state = satellite.header.state,
            .battery = satellite.eps.bat_percent,
            .temp_mcu = satellite.thermal.mcu,
            .temp_ai = satellite.thermal.ai,
            .target_count = satellite.target_count,
            .seq = satellite.header.sequence,
        };
        log_entry.crc = FDIR_CRC32((const uint8_t *)&log_entry,
                                    sizeof(FlashTelemLog_t) - sizeof(uint32_t));
        osMutexRelease(dataMutex);

        FLASH_WriteTelemLog(&log_entry);
    }

    /* ECC bellek scrub (10s) */
    static uint32_t last_ecc_tick = 0;
    if ((now - last_ecc_tick) >= 10000) {
        last_ecc_tick = now;

        static uint8_t tmr_shadow[sizeof(TMR_CriticalData_t) * 2];
        osMutexAcquire(dataMutex, osWaitForever);
        ECC_ProtectMemory((const uint8_t *)&tmr_data,
                          sizeof(TMR_CriticalData_t),
                          tmr_shadow);
        osMutexRelease(dataMutex);
    }

    osDelay(TASK_FDIR_PERIOD_MS);
  }
  /* USER CODE END Task_FDIR */
}

/* USER CODE BEGIN Header_Task_StoreForward */
/* Kaydet-ilet dairesel tampon */
/* USER CODE END Header_Task_StoreForward */
void Task_StoreForward(void *argument)
{
  /* USER CODE BEGIN Task_StoreForward */

  for(;;)
  {
    FDIR_TaskHeartbeat(TASK_ID_SF);

    Golge1_State_t state = TMR_ReadState(&tmr_data);

    if (state == STATE_STORE_FWD) {

        CommsPacket_t pending_pkt;
        while (osMessageQueueGet(commsQueue, &pending_pkt, NULL, 0) == osOK) {

            SF_Packet_t *slot = &sf_buffer[sf_write_idx];
            slot->is_valid = 1;
            slot->priority = pending_pkt.priority;
            slot->retry_count = 0;
            slot->timestamp = HAL_GetTick() / 1000;
            slot->data_length = GOLGE1_MIN(pending_pkt.length, SF_PACKET_DATA_SIZE);
            memcpy(slot->data, pending_pkt.data, slot->data_length);

            sf_write_idx = (sf_write_idx + 1) % SF_MAX_PACKETS;
            if (sf_count < SF_MAX_PACKETS)
                sf_count++;

            osMutexAcquire(dataMutex, osWaitForever);
            satellite.stats.packets_stored++;
            osMutexRelease(dataMutex);

            FLASH_WriteSFPacket(slot, (sf_write_idx == 0) ?
                                SF_MAX_PACKETS - 1 : sf_write_idx - 1);
        }
    }

    else if (state == STATE_COMMS_RELAY && sf_count > 0) {

        uint8_t best_idx = sf_read_idx;
        uint8_t best_pri = 0;

        for (uint8_t i = 0; i < sf_count; i++) {
            uint8_t idx = (sf_read_idx + i) % SF_MAX_PACKETS;
            if (sf_buffer[idx].is_valid && sf_buffer[idx].priority > best_pri) {
                best_pri = sf_buffer[idx].priority;
                best_idx = idx;
            }
        }

        if (sf_buffer[best_idx].is_valid) {
            CommsPacket_t pkt;
            memcpy(pkt.data, sf_buffer[best_idx].data,
                   sf_buffer[best_idx].data_length);
            pkt.length = sf_buffer[best_idx].data_length;
            pkt.priority = sf_buffer[best_idx].priority;
            pkt.encrypted = 0;

            if (osMessageQueuePut(commsQueue, &pkt, 0, 100) == osOK) {
                sf_buffer[best_idx].is_valid = 0;
                sf_count--;
            } else {
                sf_buffer[best_idx].retry_count++;
                if (sf_buffer[best_idx].retry_count >= SF_MAX_RETRY) {
                    sf_buffer[best_idx].is_valid = 0;
                    sf_count--;
                }
            }
        }
    }

    osDelay(TASK_SF_PERIOD_MS);
  }
  /* USER CODE END Task_StoreForward */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/* USER CODE END Application */
