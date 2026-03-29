// GOLGE-1 Orbit - Yorunge Propagatoru ve Gecis Hesaplayici

#ifndef __GOLGE1_ORBIT_H
#define __GOLGE1_ORBIT_H

#include "main.h"
#include <math.h>

#define ORBIT_MU_EARTH          398600.4418f   // km3/s2
#define ORBIT_RE_EARTH          6371.0f        // km
#define ORBIT_J2                0.00108263f
#define ORBIT_SOLAR_RADIUS      696340.0f      // km
#define ORBIT_AU                149597870.7f   // km
#define ORBIT_EARTH_ROT_RATE    7.2921159e-5f  // rad/s
#define ORBIT_DEG_TO_RAD        0.01745329252f
#define ORBIT_RAD_TO_DEG        57.2957795131f
#define ORBIT_PI                3.14159265359f
#define ORBIT_TWO_PI            6.28318530718f

// GOLGE-1 yorunge parametreleri (LEO SSO)
#define GOLGE1_ALTITUDE_KM      500.0f
#define GOLGE1_INCLINATION_DEG  97.4f
#define GOLGE1_ECCENTRICITY     0.001f
#define GOLGE1_RAAN_DEG         0.0f

// Yer istasyonu (Ankara)
#define GS_LATITUDE             39.9334f
#define GS_LONGITUDE            32.8597f
#define GS_ALTITUDE_KM          0.9f
#define GS_MIN_ELEVATION_DEG    5.0f

typedef struct {
    float semi_major_axis;     // a (km)
    float eccentricity;        // e
    float inclination;         // i (rad)
    float raan;                // omega_buyuk (rad)
    float arg_perigee;         // omega (rad)
    float mean_anomaly;        // M (rad)
    float mean_motion;         // n (rad/s)
    float period;              // T (s)
} OrbitalElements_t;

typedef struct {
    float x_eci, y_eci, z_eci;        // km
    float vx_eci, vy_eci, vz_eci;     // km/s

    float latitude;                     // derece
    float longitude;                    // derece
    float altitude;                     // km

    float gs_elevation;                 // derece
    float gs_azimuth;                   // derece
    float gs_range;                     // km
    bool  gs_visible;
} SatPosition_t;

typedef struct {
    uint32_t aos_time;          // AOS (s)
    uint32_t los_time;          // LOS (s)
    uint32_t max_el_time;
    float    max_elevation;     // derece
    uint32_t duration;          // s
    bool     is_valid;
} PassWindow_t;

typedef struct {
    bool     in_eclipse;
    uint32_t eclipse_entry;
    uint32_t eclipse_exit;
    float    sunlit_fraction;   // 0.0-1.0
} EclipseInfo_t;

void  ORBIT_Init(void);
void  ORBIT_Propagate(uint32_t elapsed_sec, SatPosition_t *pos);
float ORBIT_SolveKepler(float mean_anomaly, float eccentricity);

void ORBIT_ECItoGeo(float x_eci, float y_eci, float z_eci,
                    uint32_t elapsed_sec,
                    float *lat, float *lon, float *alt);

bool ORBIT_CalculateVisibility(const SatPosition_t *sat_pos,
                                float gs_lat, float gs_lon, float gs_alt,
                                float *elevation, float *azimuth, float *range);

void ORBIT_PredictNextPass(uint32_t current_time, PassWindow_t *window);

void ORBIT_CalculateEclipse(const SatPosition_t *sat_pos,
                             uint32_t elapsed_sec,
                             EclipseInfo_t *eclipse);

float ORBIT_GroundDistance(float lat1, float lon1, float lat2, float lon2);

#endif /* __GOLGE1_ORBIT_H */
