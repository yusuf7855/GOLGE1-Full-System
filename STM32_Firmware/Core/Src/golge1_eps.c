// GOLGE-1 EPS - Guc Yonetimi

#include "golge1_eps.h"
#include <string.h>

static uint32_t eps_sim_tick = 0;
static float    sim_orbit_angle = 0.0f;

// MPPT state
static float    mppt_duty_cycle = 0.5f;
static float    mppt_last_power = 0.0f;
static float    mppt_step = 0.01f;

void EPS_Init(void)
{
    eps_sim_tick = 0;
    sim_orbit_angle = 0.0f;
    mppt_duty_cycle = 0.5f;
    mppt_last_power = 0.0f;

    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_JETSON_POWER, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_SDR_POWER, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_CAMERA_POWER, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_HEATER_ENABLE, GPIO_PIN_RESET);
}

// Coulomb sayaci modeli
void EPS_UpdateBatteryStatus(EPSData_t *eps)
{
    eps_sim_tick++;

    float charge_power = eps->solar_power_mw * EPS_MPPT_EFFICIENCY;
    float charge_current = (eps->bat_voltage > 0.1f) ?
                           (charge_power / eps->bat_voltage) : 0.0f;

    float discharge_current = (eps->bat_voltage > 0.1f) ?
                              (eps->total_draw_mw / eps->bat_voltage) : 0.0f;

    eps->bat_current = charge_current - discharge_current;

    float dt_hours = (float)TASK_EPS_PERIOD_MS / 3600000.0f;
    float delta_energy = (eps->bat_current * eps->bat_voltage * dt_hours) / 1000.0f;
    eps->bat_energy_wh += delta_energy;

    if (eps->bat_energy_wh > EPS_BATTERY_CAPACITY_WH)
        eps->bat_energy_wh = EPS_BATTERY_CAPACITY_WH;
    if (eps->bat_energy_wh < 0.0f)
        eps->bat_energy_wh = 0.0f;

    eps->bat_percent = (eps->bat_energy_wh / EPS_BATTERY_CAPACITY_WH) * 100.0f;
    eps->bat_percent = GOLGE1_CLAMP(eps->bat_percent, 0.0f, 100.0f);

    float soc_ratio = eps->bat_percent / 100.0f;
    eps->bat_voltage = EPS_BATTERY_VOLTAGE_MIN +
                       soc_ratio * (EPS_BATTERY_VOLTAGE_MAX - EPS_BATTERY_VOLTAGE_MIN);
}

// MPPT: Perturb & Observe
void EPS_UpdateSolarPower(EPSData_t *eps)
{
    // 90dk periyot, 0.01667 derece/tick
    sim_orbit_angle += 0.01667f;
    if (sim_orbit_angle >= 360.0f)
        sim_orbit_angle -= 360.0f;

    // 240-360 arasi tutulma
    if (sim_orbit_angle >= 240.0f) {
        eps->in_eclipse = 1;
        eps->solar_voltage = 0.0f;
        eps->solar_current = 0.0f;
        eps->solar_power_mw = 0.0f;
        return;
    }

    eps->in_eclipse = 0;

    float sun_angle_factor;
    if (sim_orbit_angle < 120.0f)
        sun_angle_factor = sim_orbit_angle / 120.0f;
    else
        sun_angle_factor = (240.0f - sim_orbit_angle) / 120.0f;
    sun_angle_factor = GOLGE1_CLAMP(sun_angle_factor, 0.0f, 1.0f);

    float raw_power = (float)EPS_SOLAR_MAX_POWER_MW * sun_angle_factor;

    // P&O adimi
    float current_power = raw_power * mppt_duty_cycle;
    if (current_power > mppt_last_power)
        mppt_duty_cycle += mppt_step;
    else {
        mppt_step = -mppt_step;
        mppt_duty_cycle += mppt_step;
    }
    mppt_duty_cycle = GOLGE1_CLAMP(mppt_duty_cycle, 0.1f, 0.95f);
    mppt_last_power = current_power;

    eps->solar_power_mw = current_power * EPS_MPPT_EFFICIENCY;
    eps->solar_voltage = 9.0f * sun_angle_factor;
    eps->solar_current = (eps->solar_voltage > 0.1f) ?
                         (eps->solar_power_mw / eps->solar_voltage) : 0.0f;
}

float EPS_CalculatePowerDraw(const SubsysStatus_t *subsys)
{
    float total = (float)PWR_OBC_ACTIVE;

    switch (subsys->jetson) {
        case SUBSYS_ACTIVE:  total += PWR_JETSON_ACTIVE; break;
        case SUBSYS_STANDBY: total += PWR_JETSON_IDLE;   break;
        default: break;
    }
    switch (subsys->sdr) {
        case SUBSYS_ACTIVE:  total += PWR_SDR_TX;         break;
        case SUBSYS_STANDBY: total += PWR_SDR_RX_PASSIVE; break;
        default: break;
    }
    if (subsys->camera == SUBSYS_ACTIVE) total += PWR_CAMERA_ACTIVE;
    if (subsys->ir_sensor == SUBSYS_ACTIVE) total += PWR_IR_ACTIVE;
    switch (subsys->adcs) {
        case SUBSYS_ACTIVE:  total += PWR_ADCS_ACTIVE; break;
        case SUBSYS_STANDBY: total += PWR_ADCS_IDLE;   break;
        default: break;
    }
    if (subsys->heater == SUBSYS_ACTIVE) total += PWR_HEATER;

    return total;
}

float EPS_CalculatePowerMargin(const EPSData_t *eps, const SubsysStatus_t *subsys)
{
    return eps->solar_power_mw - EPS_CalculatePowerDraw(subsys);
}

void EPS_SetSubsystemPower(uint16_t subsystem_pin, SubsysPower_t state)
{
    GPIO_PinState pin_state = (state != SUBSYS_OFF) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, subsystem_pin, pin_state);
}

SubsysStatus_t EPS_GetRecommendedProfile(float bat_percent, Golge1_State_t state)
{
    SubsysStatus_t profile = {0};
    profile.adcs = SUBSYS_STANDBY;

    switch (state) {
        case STATE_BOOT:
        case STATE_DETUMBLE:
            profile.adcs = SUBSYS_ACTIVE;
            break;
        case STATE_IDLE_CHARGE:
            break;
        case STATE_OPTICAL_SCAN:
            if (bat_percent >= EPS_BAT_NOMINAL) {
                profile.jetson = SUBSYS_ACTIVE;
                profile.camera = SUBSYS_ACTIVE;
                profile.ir_sensor = SUBSYS_ACTIVE;
                profile.adcs = SUBSYS_ACTIVE;
            }
            break;
        case STATE_RF_SCAN:
            if (bat_percent >= EPS_BAT_NOMINAL) {
                profile.jetson = SUBSYS_ACTIVE;
                profile.sdr = SUBSYS_STANDBY;    // RX pasif
                profile.adcs = SUBSYS_ACTIVE;
            }
            break;
        case STATE_COMMS_RELAY:
            if (bat_percent >= EPS_BAT_LOW) {
                profile.sdr = SUBSYS_ACTIVE;     // TX
                profile.adcs = SUBSYS_ACTIVE;
            }
            break;
        case STATE_STORE_FWD:
        case STATE_SAFE_MODE:
            break;
    }
    return profile;
}

void EPS_UpdateThermalSensors(ThermalData_t *thermal, const SubsysStatus_t *subsys)
{
    thermal->mcu = 38;

    if (subsys->jetson == SUBSYS_ACTIVE)
        thermal->ai = GOLGE1_MIN(thermal->ai + 2, THERMAL_AI_CRITICAL);
    else if (subsys->jetson == SUBSYS_STANDBY)
        thermal->ai = GOLGE1_MIN(thermal->ai + 1, 50);
    else
        thermal->ai = GOLGE1_MAX(thermal->ai - 3, -20);

    if (subsys->sdr == SUBSYS_ACTIVE)
        thermal->sdr = GOLGE1_MIN(thermal->sdr + 2, THERMAL_SDR_CRITICAL);
    else if (subsys->sdr == SUBSYS_STANDBY)
        thermal->sdr = GOLGE1_MIN(thermal->sdr + 1, 40);
    else
        thermal->sdr = GOLGE1_MAX(thermal->sdr - 2, -25);

    thermal->external = GOLGE1_MAX(thermal->external - 1, -40);

    if (subsys->camera == SUBSYS_ACTIVE)
        thermal->camera = GOLGE1_MIN(thermal->camera + 1, 35);
    else
        thermal->camera = GOLGE1_MAX(thermal->camera - 2, -15);
}

void EPS_ThermalManagement(const ThermalData_t *thermal, EPSData_t *eps)
{
    if (thermal->battery <= THERMAL_HEATER_ON_THRESH && !eps->heater_active) {
        HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_HEATER_ENABLE, GPIO_PIN_SET);
        eps->heater_active = 1;
    }
    else if (thermal->battery >= THERMAL_HEATER_OFF_THRESH && eps->heater_active) {
        HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_HEATER_ENABLE, GPIO_PIN_RESET);
        eps->heater_active = 0;
    }
}

Golge1_Event_t EPS_CheckAnomalies(const EPSData_t *eps, const ThermalData_t *thermal)
{
    if (eps->bat_percent <= EPS_BAT_CRITICAL) return EVT_BATTERY_CRITICAL;
    if (eps->bat_percent <= EPS_BAT_LOW) return EVT_BATTERY_LOW;

    if (thermal->ai >= THERMAL_AI_CRITICAL ||
        thermal->sdr >= THERMAL_SDR_CRITICAL ||
        thermal->mcu >= THERMAL_MCU_CRITICAL)
        return EVT_THERMAL_CRITICAL;

    if (thermal->ai >= THERMAL_AI_WARNING ||
        thermal->sdr >= THERMAL_SDR_WARNING ||
        thermal->mcu >= THERMAL_MCU_WARNING)
        return EVT_THERMAL_WARNING;

    if (thermal->battery >= THERMAL_BATTERY_MAX ||
        thermal->battery <= THERMAL_BATTERY_MIN)
        return EVT_THERMAL_CRITICAL;

    return EVT_NONE;
}

void EPS_EmergencyShutdown(void)
{
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_JETSON_POWER, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_SDR_POWER, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_CAMERA_POWER, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(PORT_SUBSYS_POWER, PIN_HEATER_ENABLE, GPIO_PIN_RESET);
}
