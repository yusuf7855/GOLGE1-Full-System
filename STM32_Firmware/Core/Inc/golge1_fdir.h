// GOLGE-1 FDIR + TMR

#ifndef __GOLGE1_FDIR_H
#define __GOLGE1_FDIR_H

#include "main.h"

/* TMR */

void TMR_Init(TMR_CriticalData_t *tmr);
void TMR_WriteState(TMR_CriticalData_t *tmr, Golge1_State_t state);
Golge1_State_t TMR_ReadState(TMR_CriticalData_t *tmr);
void TMR_WriteBattery(TMR_CriticalData_t *tmr, float battery);
float TMR_ReadBattery(TMR_CriticalData_t *tmr);
void TMR_WriteTimestamp(TMR_CriticalData_t *tmr, uint32_t timestamp);
uint32_t TMR_ReadTimestamp(TMR_CriticalData_t *tmr);
void TMR_WriteSequence(TMR_CriticalData_t *tmr, uint16_t seq);
uint16_t TMR_ReadSequence(TMR_CriticalData_t *tmr);
bool TMR_ScrubAndVerify(TMR_CriticalData_t *tmr, StatsData_t *stats);

/* FDIR */

typedef struct {
    Golge1_Event_t events[FDIR_ANOMALY_WINDOW];
    uint8_t        head;
    uint8_t        count;
    uint32_t       last_anomaly;
} FDIR_AnomalyLog_t;

void FDIR_Init(void);
void FDIR_LogAnomaly(Golge1_Event_t event);
bool FDIR_IsAnomalyThresholdExceeded(void);

Golge1_Event_t FDIR_SystemHealthCheck(const EPSData_t *eps,
                                       const ThermalData_t *thermal,
                                       const StatsData_t *stats,
                                       TMR_CriticalData_t *tmr);

bool FDIR_CheckCommTimeout(uint32_t last_contact, uint32_t current_time);
void FDIR_FeedWatchdog(void);
void FDIR_TaskHeartbeat(uint8_t task_id);
bool FDIR_AllTasksHealthy(uint32_t current_time);
Golge1_State_t FDIR_ExecuteRecovery(Golge1_Event_t event, StatsData_t *stats);
uint32_t FDIR_CRC32(const uint8_t *data, uint32_t length);

#endif /* __GOLGE1_FDIR_H */
