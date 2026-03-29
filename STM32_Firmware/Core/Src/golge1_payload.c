// GOLGE-1 Payload - Goren Goz + Dinleyen Kulak

#include "golge1_payload.h"
#include "golge1_eps.h"
#include <string.h>
#include <stdlib.h>

static uint32_t sim_scan_counter = 0;
static uint8_t  sim_target_id_counter = 0;

typedef struct {
    const char *id;
    const char *type;
    const char *classification;
    float lat, lon, alt;
    uint16_t heading, speed;
    float confidence;
    uint8_t sensor_id;
    float signal_strength;
} SimScenario_t;

// Optik hedef senaryolari
static const SimScenario_t optical_scenarios[] = {
    { "TGT-OPT-01", "OPTICAL", "TANK",    39.9334f, 32.8597f, 850.0f,  45, 35, 0.89f, SENSOR_ID_OPTICAL, 0.0f },
    { "TGT-OPT-02", "IR_ANOMALY", "APC",  39.9256f, 32.8701f, 830.0f, 120, 50, 0.78f, SENSOR_ID_IR, 0.0f },
    { "TGT-OPT-03", "OPTICAL", "CONVOY",  39.9412f, 32.8489f, 870.0f, 270, 40, 0.92f, SENSOR_ID_OPTICAL, 0.0f },
};

// RF golge senaryolari
static const SimScenario_t rf_scenarios[] = {
    { "TGT-RF-01", "RF_SHADOW",    "STEALTH_AC", 41.5012f, 36.1120f, 12000.0f, 180, 850, 0.87f, SENSOR_ID_RF_FSR, -42.5f },
    { "TGT-RF-02", "MICRO_DOPPLER","AIRCRAFT",   41.4923f, 36.0987f,  8500.0f, 225, 720, 0.93f, SENSOR_ID_DOPPLER, -38.2f },
    { "TGT-RF-03", "RF_SHADOW",    "UAV",        41.5134f, 36.1345f,  3000.0f,  90, 180, 0.71f, SENSOR_ID_RF_FSR, -55.0f },
};

#define NUM_OPTICAL_SCENARIOS  (sizeof(optical_scenarios) / sizeof(optical_scenarios[0]))
#define NUM_RF_SCENARIOS       (sizeof(rf_scenarios) / sizeof(rf_scenarios[0]))

void PAYLOAD_Init(void)
{
    sim_scan_counter = 0;
    sim_target_id_counter = 0;
}

void PAYLOAD_StartOpticalScan(SubsysStatus_t *subsys)
{
    EPS_SetSubsystemPower(PIN_CAMERA_POWER, SUBSYS_ACTIVE);
    subsys->camera = SUBSYS_ACTIVE;
    subsys->ir_sensor = SUBSYS_ACTIVE;
    EPS_SetSubsystemPower(PIN_JETSON_POWER, SUBSYS_ACTIVE);
    subsys->jetson = SUBSYS_ACTIVE;
    subsys->adcs = SUBSYS_ACTIVE;
}

void PAYLOAD_StopOpticalScan(SubsysStatus_t *subsys)
{
    EPS_SetSubsystemPower(PIN_CAMERA_POWER, SUBSYS_OFF);
    subsys->camera = SUBSYS_OFF;
    subsys->ir_sensor = SUBSYS_OFF;
    subsys->jetson = SUBSYS_STANDBY;
}

// Jetson YOLOv8 + golge analizi sim
uint8_t PAYLOAD_SimulateOpticalDetection(IntelData_t *targets, uint8_t max_targets)
{
    sim_scan_counter++;
    uint8_t detected = 0;

    if (sim_scan_counter % 3 != 0) return 0;

    uint8_t scenario_idx = (sim_scan_counter / 3) % NUM_OPTICAL_SCENARIOS;
    uint8_t num_to_detect = (scenario_idx == 2) ? 2 : 1;

    for (uint8_t i = 0; i < num_to_detect && detected < max_targets; i++) {
        const SimScenario_t *sc = &optical_scenarios[(scenario_idx + i) % NUM_OPTICAL_SCENARIOS];
        IntelData_t *tgt = &targets[detected];

        strncpy(tgt->id, sc->id, sizeof(tgt->id) - 1);
        strncpy(tgt->type, sc->type, sizeof(tgt->type) - 1);
        strncpy(tgt->classification, sc->classification, sizeof(tgt->classification) - 1);

        // konum jitter
        float jitter_lat = ((float)(sim_scan_counter % 7) - 3.0f) * 0.0001f;
        float jitter_lon = ((float)(sim_scan_counter % 5) - 2.0f) * 0.0001f;
        tgt->lat = sc->lat + jitter_lat;
        tgt->lon = sc->lon + jitter_lon;
        tgt->alt = sc->alt;
        tgt->heading = sc->heading;
        tgt->speed = sc->speed;
        tgt->confidence = sc->confidence;
        tgt->detect_time = HAL_GetTick() / 1000;
        tgt->priority = (tgt->confidence >= 0.85f) ? SF_PRIORITY_CRITICAL : SF_PRIORITY_HIGH;
        tgt->sensor_id = sc->sensor_id;
        tgt->signal_strength = sc->signal_strength;
        tgt->is_active = 1;
        detected++;
    }
    return detected;
}

// +400C ustu IR anomali
uint8_t PAYLOAD_SimulateIRDetection(IntelData_t *targets, uint8_t max_targets)
{
    if (sim_scan_counter % 5 != 0 || max_targets == 0) return 0;

    IntelData_t *tgt = &targets[0];
    strncpy(tgt->id, "TGT-IR-01", sizeof(tgt->id) - 1);
    strncpy(tgt->type, "IR_ANOMALY", sizeof(tgt->type) - 1);
    strncpy(tgt->classification, "UNKNOWN", sizeof(tgt->classification) - 1);
    tgt->lat = 39.9380f + ((float)(sim_scan_counter % 3)) * 0.001f;
    tgt->lon = 32.8650f;
    tgt->alt = 0.0f;
    tgt->heading = 0;
    tgt->speed = 0;
    tgt->confidence = 0.72f;
    tgt->detect_time = HAL_GetTick() / 1000;
    tgt->priority = SF_PRIORITY_HIGH;
    tgt->sensor_id = SENSOR_ID_IR;
    tgt->signal_strength = 0.0f;
    tgt->is_active = 1;
    return 1;
}

void PAYLOAD_StartRFScan(SubsysStatus_t *subsys)
{
    EPS_SetSubsystemPower(PIN_SDR_POWER, SUBSYS_ACTIVE);
    subsys->sdr = SUBSYS_STANDBY;  // RX pasif
    EPS_SetSubsystemPower(PIN_JETSON_POWER, SUBSYS_ACTIVE);
    subsys->jetson = SUBSYS_ACTIVE;
    subsys->camera = SUBSYS_OFF;
    subsys->ir_sensor = SUBSYS_OFF;
}

void PAYLOAD_StopRFScan(SubsysStatus_t *subsys)
{
    EPS_SetSubsystemPower(PIN_SDR_POWER, SUBSYS_OFF);
    subsys->sdr = SUBSYS_OFF;
    subsys->jetson = SUBSYS_STANDBY;
}

// Pasif FSR: RF golge + mikro-Doppler tespiti
uint8_t PAYLOAD_SimulateRFShadowDetection(IntelData_t *targets, uint8_t max_targets)
{
    sim_scan_counter++;
    uint8_t detected = 0;

    if (sim_scan_counter % 4 != 0) return 0;

    uint8_t scenario_idx = (sim_scan_counter / 4) % NUM_RF_SCENARIOS;

    for (uint8_t i = 0; i < 1 && detected < max_targets; i++) {
        const SimScenario_t *sc = &rf_scenarios[(scenario_idx + i) % NUM_RF_SCENARIOS];
        IntelData_t *tgt = &targets[detected];

        strncpy(tgt->id, sc->id, sizeof(tgt->id) - 1);
        strncpy(tgt->type, sc->type, sizeof(tgt->type) - 1);
        strncpy(tgt->classification, sc->classification, sizeof(tgt->classification) - 1);

        float jitter = ((float)(sim_scan_counter % 11) - 5.0f) * 0.001f;
        tgt->lat = sc->lat + jitter;
        tgt->lon = sc->lon + jitter * 0.7f;
        tgt->alt = sc->alt;
        tgt->heading = sc->heading;
        tgt->speed = sc->speed;
        tgt->confidence = sc->confidence;
        tgt->detect_time = HAL_GetTick() / 1000;
        tgt->priority = SF_PRIORITY_CRITICAL;
        tgt->sensor_id = sc->sensor_id;
        tgt->signal_strength = sc->signal_strength;
        tgt->is_active = 1;

        PAYLOAD_AnalyzeMicroDoppler(tgt);
        detected++;
    }
    return detected;
}

// JEM (Jet Engine Modulation) siniflandirma
void PAYLOAD_AnalyzeMicroDoppler(IntelData_t *target)
{
    if (target->speed > 500) {
        if (target->signal_strength < -40.0f) {
            strncpy(target->classification, "STEALTH_AC", sizeof(target->classification) - 1);
            target->confidence = GOLGE1_MIN(target->confidence + 0.05f, 0.98f);
        } else {
            strncpy(target->classification, "AIRCRAFT", sizeof(target->classification) - 1);
        }
    } else if (target->speed > 100 && target->speed < 300) {
        strncpy(target->classification, "UAV", sizeof(target->classification) - 1);
    }
}

bool PAYLOAD_AddTarget(IntelData_t *list, uint8_t *count,
                       const IntelData_t *new_target, uint8_t max_count)
{
    // ayni ID varsa guncelle
    for (uint8_t i = 0; i < *count; i++) {
        if (strncmp(list[i].id, new_target->id, sizeof(list[i].id)) == 0) {
            memcpy(&list[i], new_target, sizeof(IntelData_t));
            return true;
        }
    }

    if (*count < max_count) {
        memcpy(&list[*count], new_target, sizeof(IntelData_t));
        (*count)++;
        return true;
    }

    // dolu: en dusuk oncelikliyle degistir
    uint8_t min_pri_idx = 0;
    for (uint8_t i = 1; i < *count; i++) {
        if (list[i].priority < list[min_pri_idx].priority)
            min_pri_idx = i;
    }

    if (new_target->priority > list[min_pri_idx].priority) {
        memcpy(&list[min_pri_idx], new_target, sizeof(IntelData_t));
        return true;
    }
    return false;
}

// insertion sort (kucuk liste)
void PAYLOAD_SortTargetsByPriority(IntelData_t *list, uint8_t count)
{
    for (uint8_t i = 1; i < count; i++) {
        IntelData_t key;
        memcpy(&key, &list[i], sizeof(IntelData_t));
        int j = i - 1;
        while (j >= 0 && list[j].priority < key.priority) {
            memcpy(&list[j + 1], &list[j], sizeof(IntelData_t));
            j--;
        }
        memcpy(&list[j + 1], &key, sizeof(IntelData_t));
    }
}

void PAYLOAD_PurgeStaleTargets(IntelData_t *list, uint8_t *count,
                               uint32_t current_time, uint32_t max_age_sec)
{
    uint8_t write_idx = 0;
    for (uint8_t read_idx = 0; read_idx < *count; read_idx++) {
        if ((current_time - list[read_idx].detect_time) < max_age_sec &&
            list[read_idx].is_active) {
            if (write_idx != read_idx)
                memcpy(&list[write_idx], &list[read_idx], sizeof(IntelData_t));
            write_idx++;
        }
    }
    *count = write_idx;
}

void PAYLOAD_AllSensorsOff(SubsysStatus_t *subsys)
{
    EPS_SetSubsystemPower(PIN_JETSON_POWER, SUBSYS_OFF);
    EPS_SetSubsystemPower(PIN_SDR_POWER, SUBSYS_OFF);
    EPS_SetSubsystemPower(PIN_CAMERA_POWER, SUBSYS_OFF);
    subsys->jetson = SUBSYS_OFF;
    subsys->sdr = SUBSYS_OFF;
    subsys->camera = SUBSYS_OFF;
    subsys->ir_sensor = SUBSYS_OFF;
}
