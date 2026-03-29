// GOLGE-1 FDIR + TMR

#include "golge1_fdir.h"
#include "golge1_eps.h"
#include "iwdg.h"
#include <string.h>

extern IWDG_HandleTypeDef hiwdg;

static FDIR_AnomalyLog_t anomaly_log;

#define MAX_TASKS 7
static uint32_t task_heartbeats[MAX_TASKS];
static uint8_t  task_alive_flags[MAX_TASKS];

/* --- TMR --- */

void TMR_Init(TMR_CriticalData_t *tmr)
{
    for (int i = 0; i < TMR_COPY_COUNT; i++) {
        tmr->state[i] = STATE_BOOT;
        tmr->battery[i] = 50.0f;
        tmr->timestamp[i] = 0;
        tmr->boot_count[i] = 0;
        tmr->sequence[i] = 0;
        tmr->checksum[i] = 0;
    }
}

void TMR_WriteState(TMR_CriticalData_t *tmr, Golge1_State_t state)
{
    for (int i = 0; i < TMR_COPY_COUNT; i++)
        tmr->state[i] = state;
}

Golge1_State_t TMR_ReadState(TMR_CriticalData_t *tmr)
{
    // 2/3 cogunluk oylamasi
    if (tmr->state[0] == tmr->state[1]) {
        tmr->state[2] = tmr->state[0];
        return tmr->state[0];
    }
    if (tmr->state[0] == tmr->state[2]) {
        tmr->state[1] = tmr->state[0];
        return tmr->state[0];
    }
    if (tmr->state[1] == tmr->state[2]) {
        tmr->state[0] = tmr->state[1];
        return tmr->state[1];
    }

    // 3 kopya da farkli -> safe mode
    Golge1_State_t safe = STATE_SAFE_MODE;
    for (int i = 0; i < TMR_COPY_COUNT; i++)
        tmr->state[i] = safe;
    return safe;
}

void TMR_WriteBattery(TMR_CriticalData_t *tmr, float battery)
{
    for (int i = 0; i < TMR_COPY_COUNT; i++)
        tmr->battery[i] = battery;
}

float TMR_ReadBattery(TMR_CriticalData_t *tmr)
{
    // en yakin 2 degerin ortalamasi
    float diff_01 = tmr->battery[0] - tmr->battery[1];
    float diff_02 = tmr->battery[0] - tmr->battery[2];
    float diff_12 = tmr->battery[1] - tmr->battery[2];
    if (diff_01 < 0) diff_01 = -diff_01;
    if (diff_02 < 0) diff_02 = -diff_02;
    if (diff_12 < 0) diff_12 = -diff_12;

    float result;
    if (diff_01 <= diff_02 && diff_01 <= diff_12) {
        result = (tmr->battery[0] + tmr->battery[1]) / 2.0f;
        tmr->battery[2] = result;
    } else if (diff_02 <= diff_01 && diff_02 <= diff_12) {
        result = (tmr->battery[0] + tmr->battery[2]) / 2.0f;
        tmr->battery[1] = result;
    } else {
        result = (tmr->battery[1] + tmr->battery[2]) / 2.0f;
        tmr->battery[0] = result;
    }
    return result;
}

void TMR_WriteTimestamp(TMR_CriticalData_t *tmr, uint32_t timestamp)
{
    for (int i = 0; i < TMR_COPY_COUNT; i++)
        tmr->timestamp[i] = timestamp;
}

uint32_t TMR_ReadTimestamp(TMR_CriticalData_t *tmr)
{
    if (tmr->timestamp[0] == tmr->timestamp[1]) {
        tmr->timestamp[2] = tmr->timestamp[0];
        return tmr->timestamp[0];
    }
    if (tmr->timestamp[0] == tmr->timestamp[2]) {
        tmr->timestamp[1] = tmr->timestamp[0];
        return tmr->timestamp[0];
    }
    if (tmr->timestamp[1] == tmr->timestamp[2]) {
        tmr->timestamp[0] = tmr->timestamp[1];
        return tmr->timestamp[1];
    }
    // hepsi farkli: en buyugu al
    uint32_t max = tmr->timestamp[0];
    if (tmr->timestamp[1] > max) max = tmr->timestamp[1];
    if (tmr->timestamp[2] > max) max = tmr->timestamp[2];
    for (int i = 0; i < TMR_COPY_COUNT; i++)
        tmr->timestamp[i] = max;
    return max;
}

void TMR_WriteSequence(TMR_CriticalData_t *tmr, uint16_t seq)
{
    for (int i = 0; i < TMR_COPY_COUNT; i++)
        tmr->sequence[i] = seq;
}

uint16_t TMR_ReadSequence(TMR_CriticalData_t *tmr)
{
    if (tmr->sequence[0] == tmr->sequence[1]) {
        tmr->sequence[2] = tmr->sequence[0];
        return tmr->sequence[0];
    }
    if (tmr->sequence[0] == tmr->sequence[2]) {
        tmr->sequence[1] = tmr->sequence[0];
        return tmr->sequence[0];
    }
    if (tmr->sequence[1] == tmr->sequence[2]) {
        tmr->sequence[0] = tmr->sequence[1];
        return tmr->sequence[1];
    }
    uint16_t max = tmr->sequence[0];
    if (tmr->sequence[1] > max) max = tmr->sequence[1];
    if (tmr->sequence[2] > max) max = tmr->sequence[2];
    for (int i = 0; i < TMR_COPY_COUNT; i++)
        tmr->sequence[i] = max;
    return max;
}

bool TMR_ScrubAndVerify(TMR_CriticalData_t *tmr, StatsData_t *stats)
{
    bool corrected = false;

    Golge1_State_t voted_state = TMR_ReadState(tmr);
    for (int i = 0; i < TMR_COPY_COUNT; i++) {
        if (tmr->state[i] != voted_state) {
            tmr->state[i] = voted_state;
            corrected = true;
        }
    }

    uint32_t voted_ts = TMR_ReadTimestamp(tmr);
    for (int i = 0; i < TMR_COPY_COUNT; i++) {
        if (tmr->timestamp[i] != voted_ts) {
            tmr->timestamp[i] = voted_ts;
            corrected = true;
        }
    }

    (void)TMR_ReadBattery(tmr);
    (void)TMR_ReadSequence(tmr);

    if (corrected && stats != NULL) {
        stats->tmr_corrections++;
    }

    return corrected;
}

/* --- FDIR --- */

void FDIR_Init(void)
{
    memset(&anomaly_log, 0, sizeof(anomaly_log));
    memset(task_heartbeats, 0, sizeof(task_heartbeats));
    memset(task_alive_flags, 0, sizeof(task_alive_flags));
}

void FDIR_LogAnomaly(Golge1_Event_t event)
{
    anomaly_log.events[anomaly_log.head] = event;
    anomaly_log.head = (anomaly_log.head + 1) % FDIR_ANOMALY_WINDOW;
    if (anomaly_log.count < FDIR_ANOMALY_WINDOW)
        anomaly_log.count++;
    anomaly_log.last_anomaly = HAL_GetTick();
}

bool FDIR_IsAnomalyThresholdExceeded(void)
{
    uint8_t error_count = 0;
    for (uint8_t i = 0; i < anomaly_log.count; i++) {
        Golge1_Event_t evt = anomaly_log.events[i];
        if (evt >= EVT_BATTERY_LOW && evt <= EVT_TMR_MISMATCH)
            error_count++;
    }
    return (error_count >= FDIR_ANOMALY_THRESHOLD);
}

Golge1_Event_t FDIR_SystemHealthCheck(const EPSData_t *eps,
                                       const ThermalData_t *thermal,
                                       const StatsData_t *stats,
                                       TMR_CriticalData_t *tmr)
{
    // oncelik: TMR > batarya > termal > haberlesme > anomali

    Golge1_State_t voted_state = TMR_ReadState(tmr);
    for (int i = 0; i < TMR_COPY_COUNT; i++) {
        if (tmr->state[i] != voted_state)
            return EVT_TMR_MISMATCH;
    }

    if (eps->bat_percent <= EPS_BAT_CRITICAL)
        return EVT_BATTERY_CRITICAL;

    if (eps->bat_percent <= EPS_BAT_LOW)
        return EVT_BATTERY_LOW;

    if (thermal->mcu >= THERMAL_MCU_CRITICAL ||
        thermal->ai >= THERMAL_AI_CRITICAL ||
        thermal->sdr >= THERMAL_SDR_CRITICAL)
        return EVT_THERMAL_CRITICAL;

    if (thermal->mcu >= THERMAL_MCU_WARNING ||
        thermal->ai >= THERMAL_AI_WARNING ||
        thermal->sdr >= THERMAL_SDR_WARNING)
        return EVT_THERMAL_WARNING;

    uint32_t current_sec = HAL_GetTick() / 1000;
    if (stats->last_ground_contact > 0 &&
        (current_sec - stats->last_ground_contact) > (FDIR_COMM_TIMEOUT_MS / 1000))
        return EVT_COMM_TIMEOUT;

    if (FDIR_IsAnomalyThresholdExceeded())
        return EVT_SAFE_MODE_ENTER;

    return EVT_NONE;
}

bool FDIR_CheckCommTimeout(uint32_t last_contact, uint32_t current_time)
{
    if (last_contact == 0)
        return false; // henuz temas yok

    return (current_time - last_contact) > (FDIR_COMM_TIMEOUT_MS / 1000);
}

void FDIR_TaskHeartbeat(uint8_t task_id)
{
    if (task_id < MAX_TASKS) {
        task_heartbeats[task_id] = HAL_GetTick();
        task_alive_flags[task_id] = 1;
    }
}

bool FDIR_AllTasksHealthy(uint32_t current_time)
{
    for (uint8_t i = 0; i < MAX_TASKS; i++) {
        if (task_alive_flags[i] &&
            (current_time - task_heartbeats[i]) > 5000) { // 5s timeout
            return false;
        }
    }
    return true;
}

void FDIR_FeedWatchdog(void)
{
    uint32_t now = HAL_GetTick();

    if (FDIR_AllTasksHealthy(now)) {
        HAL_IWDG_Refresh(&hiwdg);
    }
    // beslemezse IWDG reset atar
}

Golge1_State_t FDIR_ExecuteRecovery(Golge1_Event_t event, StatsData_t *stats)
{
    stats->error_count++;

    switch (event) {
        case EVT_THERMAL_WARNING:
            FDIR_LogAnomaly(event);
            return STATE_IDLE_CHARGE;

        case EVT_BATTERY_LOW:
            FDIR_LogAnomaly(event);
            EPS_EmergencyShutdown();
            return STATE_IDLE_CHARGE;

        case EVT_THERMAL_CRITICAL:
            FDIR_LogAnomaly(event);
            EPS_EmergencyShutdown();
            return STATE_IDLE_CHARGE;

        case EVT_BATTERY_CRITICAL:
            FDIR_LogAnomaly(event);
            EPS_EmergencyShutdown();
            return STATE_SAFE_MODE;

        case EVT_TMR_MISMATCH:
            FDIR_LogAnomaly(event);
            return STATE_IDLE_CHARGE;

        case EVT_COMM_TIMEOUT:
            FDIR_LogAnomaly(event);
            return STATE_STORE_FWD;

        case EVT_SUBSYS_FAILURE:
        case EVT_RADIATION_SEU:
            FDIR_LogAnomaly(event);
            stats->reboot_count++;
            if (stats->reboot_count >= FDIR_MAX_CONSECUTIVE_REBOOT) {
                EPS_EmergencyShutdown();
            }
            return STATE_SAFE_MODE;

        case EVT_FORCE_SAFE_MODE:
            EPS_EmergencyShutdown();
            return STATE_SAFE_MODE;

        case EVT_FORCE_REBOOT:
            while (1) { /* IWDG timeout -> reset */ }

        default:
            FDIR_LogAnomaly(event);
            return STATE_IDLE_CHARGE;
    }
}

/* CRC-32 */

uint32_t FDIR_CRC32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320; // CRC-32 polinomu
            else
                crc >>= 1;
        }
    }

    return ~crc;
}
