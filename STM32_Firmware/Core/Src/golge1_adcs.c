// GOLGE-1 ADCS - Implementasyon

#include "golge1_adcs.h"
#include <string.h>

static Vector3f_t   mag_prev = {0};
static Quaternion_t target_quaternion = {1.0f, 0, 0, 0};
static float sim_time = 0.0f;

// --- Kuaterniyon Islemleri ---

Quaternion_t Quat_Multiply(const Quaternion_t *q1, const Quaternion_t *q2)
{
    // Hamilton carpimi
    Quaternion_t r;
    r.w = q1->w * q2->w - q1->x * q2->x - q1->y * q2->y - q1->z * q2->z;
    r.x = q1->w * q2->x + q1->x * q2->w + q1->y * q2->z - q1->z * q2->y;
    r.y = q1->w * q2->y - q1->x * q2->z + q1->y * q2->w + q1->z * q2->x;
    r.z = q1->w * q2->z + q1->x * q2->y - q1->y * q2->x + q1->z * q2->w;
    return r;
}

Quaternion_t Quat_Conjugate(const Quaternion_t *q)
{
    Quaternion_t r = { q->w, -q->x, -q->y, -q->z };
    return r;
}

Quaternion_t Quat_Normalize(const Quaternion_t *q)
{
    float norm = sqrtf(q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z);
    if (norm < 1e-8f) {
        Quaternion_t identity = {1.0f, 0, 0, 0};
        return identity;
    }
    float inv = 1.0f / norm;
    Quaternion_t r = { q->w * inv, q->x * inv, q->y * inv, q->z * inv };
    return r;
}

Vector3f_t Quat_RotateVector(const Quaternion_t *q, const Vector3f_t *v)
{
    // v' = q * [0,v] * q*
    Quaternion_t qv = {0, v->x, v->y, v->z};
    Quaternion_t qc = Quat_Conjugate(q);
    Quaternion_t temp = Quat_Multiply(q, &qv);
    Quaternion_t result = Quat_Multiply(&temp, &qc);
    Vector3f_t r = {result.x, result.y, result.z};
    return r;
}

Quaternion_t Quat_Error(const Quaternion_t *q_target, const Quaternion_t *q_current)
{
    // q_error = q_target * q_current^-1
    Quaternion_t q_inv = Quat_Conjugate(q_current);
    return Quat_Multiply(q_target, &q_inv);
}

Quaternion_t Quat_FromEuler(float roll, float pitch, float yaw)
{
    // ZYX sirasi
    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);

    Quaternion_t q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return q;
}

void Quat_ToEuler(const Quaternion_t *q, float *roll, float *pitch, float *yaw)
{
    float sinr_cosp = 2.0f * (q->w * q->x + q->y * q->z);
    float cosr_cosp = 1.0f - 2.0f * (q->x * q->x + q->y * q->y);
    *roll = atan2f(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q->w * q->y - q->z * q->x);
    if (fabsf(sinp) >= 1.0f)
        *pitch = copysignf(ORBIT_PI / 2.0f, sinp);
    else
        *pitch = asinf(sinp);

    float siny_cosp = 2.0f * (q->w * q->z + q->x * q->y);
    float cosy_cosp = 1.0f - 2.0f * (q->y * q->y + q->z * q->z);
    *yaw = atan2f(siny_cosp, cosy_cosp);
}

// --- Vektor Islemleri ---

float Vec3_Norm(const Vector3f_t *v)
{
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

Vector3f_t Vec3_Cross(const Vector3f_t *a, const Vector3f_t *b)
{
    Vector3f_t r;
    r.x = a->y * b->z - a->z * b->y;
    r.y = a->z * b->x - a->x * b->z;
    r.z = a->x * b->y - a->y * b->x;
    return r;
}

float Vec3_Dot(const Vector3f_t *a, const Vector3f_t *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

// --- ADCS Init ---

void ADCS_Init(void)
{
    mag_prev.x = 0;
    mag_prev.y = 0;
    mag_prev.z = 0;
    target_quaternion.w = 1.0f;
    target_quaternion.x = 0;
    target_quaternion.y = 0;
    target_quaternion.z = 0;
    sim_time = 0;
}

// --- B-dot Detumble ---

void ADCS_BdotDetumble(const Vector3f_t *mag_current,
                       const Vector3f_t *mag_previous,
                       float dt,
                       Vector3f_t *dipole_cmd)
{
    if (dt < 1e-6f) {
        dipole_cmd->x = 0;
        dipole_cmd->y = 0;
        dipole_cmd->z = 0;
        return;
    }

    // m = -k * dB/dt
    float inv_dt = 1.0f / dt;
    float db_x = (mag_current->x - mag_previous->x) * inv_dt;
    float db_y = (mag_current->y - mag_previous->y) * inv_dt;
    float db_z = (mag_current->z - mag_previous->z) * inv_dt;

    dipole_cmd->x = -ADCS_BDOT_GAIN * db_x;
    dipole_cmd->y = -ADCS_BDOT_GAIN * db_y;
    dipole_cmd->z = -ADCS_BDOT_GAIN * db_z;

    // saturasyon limiti
    float mag = Vec3_Norm(dipole_cmd);
    if (mag > ADCS_MTQ_MAX_DIPOLE) {
        float scale = ADCS_MTQ_MAX_DIPOLE / mag;
        dipole_cmd->x *= scale;
        dipole_cmd->y *= scale;
        dipole_cmd->z *= scale;
    }
}

// --- PD Kontrolor ---

void ADCS_PDController(const Quaternion_t *q_error,
                       const Vector3f_t *omega,
                       Vector3f_t *torque_cmd)
{
    // tau = -Kp * sign(w) * q_vec - Kd * w
    float sign = (q_error->w >= 0) ? 1.0f : -1.0f;

    torque_cmd->x = -ADCS_RW_GAIN_P * sign * q_error->x - ADCS_RW_GAIN_D * omega->x;
    torque_cmd->y = -ADCS_RW_GAIN_P * sign * q_error->y - ADCS_RW_GAIN_D * omega->y;
    torque_cmd->z = -ADCS_RW_GAIN_P * sign * q_error->z - ADCS_RW_GAIN_D * omega->z;

    // tork siniri
    float mag = Vec3_Norm(torque_cmd);
    if (mag > ADCS_RW_MAX_TORQUE) {
        float scale = ADCS_RW_MAX_TORQUE / mag;
        torque_cmd->x *= scale;
        torque_cmd->y *= scale;
        torque_cmd->z *= scale;
    }
}

// --- Pointing Modlari ---

void ADCS_NadirTarget(const Quaternion_t *current_q, Quaternion_t *target_q)
{
    (void)current_q;
    target_q->w = 1.0f;
    target_q->x = 0;
    target_q->y = 0;
    target_q->z = 0;
}

void ADCS_TargetPointing(float sat_lat, float sat_lon,
                         float target_lat, float target_lon,
                         Quaternion_t *target_q)
{
    float dlat = (target_lat - sat_lat) * ORBIT_DEG_TO_RAD;
    float dlon = (target_lon - sat_lon) * ORBIT_DEG_TO_RAD;

    float pitch = atanf(dlat * ORBIT_RE_EARTH / GOLGE1_ALTITUDE_KM);
    float yaw   = atanf(dlon * ORBIT_RE_EARTH * cosf(sat_lat * ORBIT_DEG_TO_RAD)
                        / GOLGE1_ALTITUDE_KM);

    *target_q = Quat_FromEuler(0, pitch, yaw);
}

void ADCS_SunPointing(const Vector3f_t *sun_vector, Quaternion_t *target_q)
{
    // panel normali +Y varsayimi
    Vector3f_t panel_normal = {0, 1.0f, 0};
    Vector3f_t sun_norm;
    float sun_mag = Vec3_Norm(sun_vector);

    if (sun_mag < 0.01f) {
        target_q->w = 1.0f;
        target_q->x = 0;
        target_q->y = 0;
        target_q->z = 0;
        return;
    }

    sun_norm.x = sun_vector->x / sun_mag;
    sun_norm.y = sun_vector->y / sun_mag;
    sun_norm.z = sun_vector->z / sun_mag;

    // en kisa rotasyon kuaterniyonu
    Vector3f_t cross = Vec3_Cross(&panel_normal, &sun_norm);
    float dot = Vec3_Dot(&panel_normal, &sun_norm);

    target_q->w = 1.0f + dot;
    target_q->x = cross.x;
    target_q->y = cross.y;
    target_q->z = cross.z;
    *target_q = Quat_Normalize(target_q);
}

// --- Ana Guncelleme Dongusu ---

void ADCS_Update(ADCSData_t *adcs, uint8_t mode, float dt)
{
    sim_time += dt;

    Vector3f_t mag_current;
    ADCS_SimulateMagField(0, sim_time * 4.0f, GOLGE1_ALTITUDE_KM, &mag_current);

    adcs->mag_field[0] = mag_current.x;
    adcs->mag_field[1] = mag_current.y;
    adcs->mag_field[2] = mag_current.z;

    Quaternion_t q_current = {
        adcs->quaternion[0], adcs->quaternion[1],
        adcs->quaternion[2], adcs->quaternion[3]
    };
    Vector3f_t omega = {
        adcs->angular_rate[0], adcs->angular_rate[1], adcs->angular_rate[2]
    };

    adcs->pointing_mode = mode;

    switch (mode) {
        case ADCS_MODE_DETUMBLE:
        {
            Vector3f_t dipole_cmd;
            ADCS_BdotDetumble(&mag_current, &mag_prev, dt, &dipole_cmd);

            // tau = m x B
            Vector3f_t torque = Vec3_Cross(&dipole_cmd, &mag_current);

            // w += tau * dt / I (I ~ 0.01 kg*m2)
            float inv_inertia = 1.0f / 0.01f;
            adcs->angular_rate[0] += torque.x * inv_inertia * dt;
            adcs->angular_rate[1] += torque.y * inv_inertia * dt;
            adcs->angular_rate[2] += torque.z * inv_inertia * dt;

            // dogal sonumleme
            adcs->angular_rate[0] *= (1.0f - 0.02f * dt);
            adcs->angular_rate[1] *= (1.0f - 0.02f * dt);
            adcs->angular_rate[2] *= (1.0f - 0.02f * dt);
            break;
        }

        case ADCS_MODE_NADIR:
        case ADCS_MODE_TARGET:
        case ADCS_MODE_SUN:
        case ADCS_MODE_GROUND_STATION:
        {
            switch (mode) {
                case ADCS_MODE_NADIR:
                    ADCS_NadirTarget(&q_current, &target_quaternion);
                    break;
                case ADCS_MODE_SUN:
                {
                    Vector3f_t sun = {adcs->sun_vector[0], adcs->sun_vector[1],
                                      adcs->sun_vector[2]};
                    ADCS_SunPointing(&sun, &target_quaternion);
                    break;
                }
                default:
                    break;
            }

            Quaternion_t q_error = Quat_Error(&target_quaternion, &q_current);
            Vector3f_t torque_cmd;
            ADCS_PDController(&q_error, &omega, &torque_cmd);

            float inv_inertia = 1.0f / 0.01f;
            adcs->angular_rate[0] += torque_cmd.x * inv_inertia * dt;
            adcs->angular_rate[1] += torque_cmd.y * inv_inertia * dt;
            adcs->angular_rate[2] += torque_cmd.z * inv_inertia * dt;
            break;
        }
    }

    // q_dot = 0.5 * q * [0, w]
    Quaternion_t omega_q = {0, adcs->angular_rate[0], adcs->angular_rate[1],
                            adcs->angular_rate[2]};
    Quaternion_t q_dot = Quat_Multiply(&q_current, &omega_q);
    q_current.w += 0.5f * q_dot.w * dt;
    q_current.x += 0.5f * q_dot.x * dt;
    q_current.y += 0.5f * q_dot.y * dt;
    q_current.z += 0.5f * q_dot.z * dt;
    q_current = Quat_Normalize(&q_current);

    adcs->quaternion[0] = q_current.w;
    adcs->quaternion[1] = q_current.x;
    adcs->quaternion[2] = q_current.y;
    adcs->quaternion[3] = q_current.z;

    adcs->is_stable = ADCS_IsStable(&omega) ? 1 : 0;
    adcs->pointing_error = ADCS_GetPointingError(&q_current, &target_quaternion);

    mag_prev = mag_current;
}

bool ADCS_IsStable(const Vector3f_t *omega)
{
    float rate = Vec3_Norm(omega);
    return (rate < ADCS_STABLE_RATE_THRESH);
}

float ADCS_GetPointingError(const Quaternion_t *q_current,
                            const Quaternion_t *q_target)
{
    Quaternion_t q_err = Quat_Error(q_target, q_current);
    // hata ~ 2 * acos(|w|)
    float w = (q_err.w >= 0) ? q_err.w : -q_err.w;
    if (w > 1.0f) w = 1.0f;
    float error_rad = 2.0f * acosf(w);
    return error_rad * ORBIT_RAD_TO_DEG;
}

// --- Manyetik Alan Simulasyonu (IGRF basit) ---

void ADCS_SimulateMagField(float lat, float lon, float alt, Vector3f_t *mag)
{
    float lat_rad = lat * ORBIT_DEG_TO_RAD;
    float lon_rad = lon * ORBIT_DEG_TO_RAD;

    float r_ratio = ORBIT_RE_EARTH / (ORBIT_RE_EARTH + alt);
    float r3 = r_ratio * r_ratio * r_ratio;
    float B0 = 30.0f; // uT ekvatorial

    mag->x = -B0 * r3 * cosf(lat_rad) * cosf(lon_rad + sim_time * 0.01f);
    mag->y =  B0 * r3 * cosf(lat_rad) * sinf(lon_rad + sim_time * 0.01f);
    mag->z = -2.0f * B0 * r3 * sinf(lat_rad);
}
