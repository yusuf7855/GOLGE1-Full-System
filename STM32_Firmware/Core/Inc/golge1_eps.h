// GOLGE-1 EPS - Guc Yonetimi

#ifndef __GOLGE1_EPS_H
#define __GOLGE1_EPS_H

#include "main.h"

void EPS_Init(void);
void EPS_UpdateBatteryStatus(EPSData_t *eps);
void EPS_UpdateSolarPower(EPSData_t *eps);

float EPS_CalculatePowerDraw(const SubsysStatus_t *subsys);
float EPS_CalculatePowerMargin(const EPSData_t *eps, const SubsysStatus_t *subsys);

void EPS_SetSubsystemPower(uint16_t subsystem_pin, SubsysPower_t state);
void EPS_ThermalManagement(const ThermalData_t *thermal, EPSData_t *eps);
void EPS_UpdateThermalSensors(ThermalData_t *thermal, const SubsysStatus_t *subsys);

// Batarya + state'e gore profil onerisi
SubsysStatus_t EPS_GetRecommendedProfile(float bat_percent, Golge1_State_t state);

Golge1_Event_t EPS_CheckAnomalies(const EPSData_t *eps, const ThermalData_t *thermal);
void EPS_EmergencyShutdown(void);

#endif /* __GOLGE1_EPS_H */
