// GOLGE-1 ADCS - Yonelim Belirleme ve Kontrol

#ifndef __GOLGE1_ADCS_H
#define __GOLGE1_ADCS_H

#include "main.h"
#include <math.h>

#define ADCS_DT                     0.25f    // kontrol periyodu (s)
#define ADCS_BDOT_GAIN              5e5f     // Am²/T/s
#define ADCS_STABLE_RATE_THRESH     0.01f    // rad/s
#define ADCS_POINTING_ACCURACY      1.0f     // derece

#define ADCS_MAG_FIELD_MIN          20.0f    // uT
#define ADCS_MAG_FIELD_MAX          60.0f    // uT

#define ADCS_RW_MAX_TORQUE          0.001f   // Nm
#define ADCS_RW_MAX_MOMENTUM        0.01f    // Nms
#define ADCS_RW_GAIN_P              0.5f
#define ADCS_RW_GAIN_D              0.8f

#define ADCS_MTQ_MAX_DIPOLE         0.2f     // Am²

#define ADCS_MODE_DETUMBLE          0
#define ADCS_MODE_NADIR             1
#define ADCS_MODE_TARGET            2
#define ADCS_MODE_SUN               3
#define ADCS_MODE_GROUND_STATION    4

typedef struct {
    float w;
    float x;
    float y;
    float z;
} Quaternion_t;

typedef struct {
    float x, y, z;
} Vector3f_t;

Quaternion_t Quat_Multiply(const Quaternion_t *q1, const Quaternion_t *q2);
Quaternion_t Quat_Conjugate(const Quaternion_t *q);
Quaternion_t Quat_Normalize(const Quaternion_t *q);
Vector3f_t   Quat_RotateVector(const Quaternion_t *q, const Vector3f_t *v);
Quaternion_t Quat_Error(const Quaternion_t *q_target, const Quaternion_t *q_current);
Quaternion_t Quat_FromEuler(float roll, float pitch, float yaw);
void         Quat_ToEuler(const Quaternion_t *q, float *roll, float *pitch, float *yaw);

float      Vec3_Norm(const Vector3f_t *v);
Vector3f_t Vec3_Cross(const Vector3f_t *a, const Vector3f_t *b);
float      Vec3_Dot(const Vector3f_t *a, const Vector3f_t *b);

void ADCS_Init(void);

// m = -k * dB/dt
void ADCS_BdotDetumble(const Vector3f_t *mag_current,
                       const Vector3f_t *mag_previous,
                       float dt,
                       Vector3f_t *dipole_cmd);

// tau = -Kp * q_err_vec - Kd * w
void ADCS_PDController(const Quaternion_t *q_error,
                       const Vector3f_t *omega,
                       Vector3f_t *torque_cmd);

void ADCS_NadirTarget(const Quaternion_t *current_q, Quaternion_t *target_q);

void ADCS_TargetPointing(float sat_lat, float sat_lon,
                         float target_lat, float target_lon,
                         Quaternion_t *target_q);

void ADCS_SunPointing(const Vector3f_t *sun_vector, Quaternion_t *target_q);

void ADCS_Update(ADCSData_t *adcs, uint8_t mode, float dt);
bool ADCS_IsStable(const Vector3f_t *omega);
float ADCS_GetPointingError(const Quaternion_t *q_current,
                            const Quaternion_t *q_target);

void ADCS_SimulateMagField(float lat, float lon, float alt, Vector3f_t *mag);

#endif /* __GOLGE1_ADCS_H */
