// GOLGE-1 Orbit - Yorunge Propagatoru Implementasyonu

#include "golge1_orbit.h"
#include <string.h>

static OrbitalElements_t orbit_elements;
static uint32_t orbit_epoch = 0;
static float gmst_at_epoch = 0.0f;

static uint16_t telem_log_write_idx = 0;

void ORBIT_Init(void)
{
    // a = R_earth + h
    orbit_elements.semi_major_axis = ORBIT_RE_EARTH + GOLGE1_ALTITUDE_KM;
    orbit_elements.eccentricity = GOLGE1_ECCENTRICITY;
    orbit_elements.inclination = GOLGE1_INCLINATION_DEG * ORBIT_DEG_TO_RAD;
    orbit_elements.raan = GOLGE1_RAAN_DEG * ORBIT_DEG_TO_RAD;
    orbit_elements.arg_perigee = 0.0f;
    orbit_elements.mean_anomaly = 0.0f;

    // n = sqrt(mu/a^3)
    float a3 = orbit_elements.semi_major_axis *
               orbit_elements.semi_major_axis *
               orbit_elements.semi_major_axis;
    orbit_elements.mean_motion = sqrtf(ORBIT_MU_EARTH / a3);

    // T = 2*pi/n
    orbit_elements.period = ORBIT_TWO_PI / orbit_elements.mean_motion;

    orbit_epoch = 0;
    gmst_at_epoch = 0.0f;
}

// --- Kepler Denklemi: M = E - e*sin(E) ---

float ORBIT_SolveKepler(float mean_anomaly, float eccentricity)
{
    float E = mean_anomaly;
    float e = eccentricity;

    // Newton-Raphson
    for (int iter = 0; iter < 10; iter++) {
        float f  = E - e * sinf(E) - mean_anomaly;
        float fp = 1.0f - e * cosf(E);

        if (fabsf(fp) < 1e-12f) break;

        float dE = f / fp;
        E -= dE;

        if (fabsf(dE) < 1e-8f) break;
    }

    return E;
}

// --- Yorunge Propagasyonu ---

void ORBIT_Propagate(uint32_t elapsed_sec, SatPosition_t *pos)
{
    float t = (float)(elapsed_sec - orbit_epoch);
    float n = orbit_elements.mean_motion;
    float e = orbit_elements.eccentricity;
    float a = orbit_elements.semi_major_axis;

    // J2 perturbasyonu
    float cos_i = cosf(orbit_elements.inclination);
    float sin_i = sinf(orbit_elements.inclination);
    float p = a * (1.0f - e * e);
    float j2_factor = -1.5f * ORBIT_J2 * (ORBIT_RE_EARTH / p) *
                      (ORBIT_RE_EARTH / p) * n;

    float raan_dot = j2_factor * cos_i;
    float argp_dot = j2_factor * (2.0f - 2.5f * sin_i * sin_i);

    float raan_now = orbit_elements.raan + raan_dot * t;
    float argp_now = orbit_elements.arg_perigee + argp_dot * t;

    // M(t) = M0 + n*t
    float M = orbit_elements.mean_anomaly + n * t;
    M = fmodf(M, ORBIT_TWO_PI);
    if (M < 0) M += ORBIT_TWO_PI;

    // M -> E -> nu
    float E = ORBIT_SolveKepler(M, e);

    float sin_nu = sqrtf(1.0f - e * e) * sinf(E) / (1.0f - e * cosf(E));
    float cos_nu = (cosf(E) - e) / (1.0f - e * cosf(E));
    float nu = atan2f(sin_nu, cos_nu);

    // yorunge duzleminde konum
    float r = a * (1.0f - e * cosf(E));
    float u = nu + argp_now;
    float x_orb = r * cosf(u);
    float y_orb = r * sinf(u);

    // ECI donusumu
    float cos_raan = cosf(raan_now);
    float sin_raan = sinf(raan_now);
    float cos_inc  = cos_i;
    float sin_inc  = sin_i;

    pos->x_eci = x_orb * cos_raan - y_orb * cos_inc * sin_raan;
    pos->y_eci = x_orb * sin_raan + y_orb * cos_inc * cos_raan;
    pos->z_eci = y_orb * sin_inc;

    // hiz (dairesel yaklasim)
    float v = sqrtf(ORBIT_MU_EARTH / r);
    pos->vx_eci = -v * sinf(u) * cos_raan - v * cosf(u) * cos_inc * sin_raan;
    pos->vy_eci = -v * sinf(u) * sin_raan + v * cosf(u) * cos_inc * cos_raan;
    pos->vz_eci =  v * cosf(u) * sin_inc;

    // cografi koordinatlar
    ORBIT_ECItoGeo(pos->x_eci, pos->y_eci, pos->z_eci,
                   elapsed_sec,
                   &pos->latitude, &pos->longitude, &pos->altitude);

    pos->gs_visible = ORBIT_CalculateVisibility(
        pos, GS_LATITUDE, GS_LONGITUDE, GS_ALTITUDE_KM,
        &pos->gs_elevation, &pos->gs_azimuth, &pos->gs_range);
}

// --- ECI -> Cografi ---

void ORBIT_ECItoGeo(float x_eci, float y_eci, float z_eci,
                    uint32_t elapsed_sec,
                    float *lat, float *lon, float *alt)
{
    float gmst = gmst_at_epoch + ORBIT_EARTH_ROT_RATE * (float)elapsed_sec;
    gmst = fmodf(gmst, ORBIT_TWO_PI);

    float r_xy = sqrtf(x_eci * x_eci + y_eci * y_eci);
    float r_total = sqrtf(x_eci * x_eci + y_eci * y_eci + z_eci * z_eci);

    *lat = atan2f(z_eci, r_xy) * ORBIT_RAD_TO_DEG;
    *lon = (atan2f(y_eci, x_eci) - gmst) * ORBIT_RAD_TO_DEG;

    while (*lon > 180.0f)  *lon -= 360.0f;
    while (*lon < -180.0f) *lon += 360.0f;

    *alt = r_total - ORBIT_RE_EARTH;
}

// --- Yer Istasyonu Gorunurluk ---

bool ORBIT_CalculateVisibility(const SatPosition_t *sat_pos,
                                float gs_lat, float gs_lon, float gs_alt,
                                float *elevation, float *azimuth, float *range)
{
    float gs_lat_rad = gs_lat * ORBIT_DEG_TO_RAD;
    float gs_lon_rad = gs_lon * ORBIT_DEG_TO_RAD;
    float sat_lat_rad = sat_pos->latitude * ORBIT_DEG_TO_RAD;
    float sat_lon_rad = sat_pos->longitude * ORBIT_DEG_TO_RAD;

    float dlat = sat_lat_rad - gs_lat_rad;
    float dlon = sat_lon_rad - gs_lon_rad;

    float ground_dist = ORBIT_GroundDistance(gs_lat, gs_lon,
                                             sat_pos->latitude,
                                             sat_pos->longitude);

    float h = sat_pos->altitude - gs_alt;
    *range = sqrtf(ground_dist * ground_dist + h * h);

    if (ground_dist > 0.01f) {
        *elevation = atan2f(h - ground_dist * ground_dist / (2.0f * ORBIT_RE_EARTH),
                           ground_dist) * ORBIT_RAD_TO_DEG;
    } else {
        *elevation = 90.0f;
    }

    float east  = dlon * cosf(gs_lat_rad) * ORBIT_RE_EARTH;
    float north = dlat * ORBIT_RE_EARTH;
    *azimuth = atan2f(east, north) * ORBIT_RAD_TO_DEG;
    if (*azimuth < 0) *azimuth += 360.0f;

    return (*elevation >= GS_MIN_ELEVATION_DEG);
}

// --- Gecis Penceresi Tahmini ---

void ORBIT_PredictNextPass(uint32_t current_time, PassWindow_t *window)
{
    window->is_valid = false;
    window->max_elevation = 0;

    bool was_visible = false;
    uint32_t search_duration = (uint32_t)(orbit_elements.period * 2.0f);
    uint32_t step = 30;

    SatPosition_t pos;

    for (uint32_t t = 0; t < search_duration; t += step) {
        uint32_t check_time = current_time + t;
        ORBIT_Propagate(check_time, &pos);

        if (pos.gs_visible && !was_visible) {
            window->aos_time = check_time;
            window->is_valid = true;
            was_visible = true;
        }

        if (pos.gs_visible && pos.gs_elevation > window->max_elevation) {
            window->max_elevation = pos.gs_elevation;
            window->max_el_time = check_time;
        }

        if (!pos.gs_visible && was_visible) {
            window->los_time = check_time;
            window->duration = window->los_time - window->aos_time;
            return;
        }
    }

    if (was_visible) {
        window->los_time = current_time + search_duration;
        window->duration = window->los_time - window->aos_time;
    }
}

// --- Tutulma Hesabi (silindirik golge) ---

void ORBIT_CalculateEclipse(const SatPosition_t *sat_pos,
                             uint32_t elapsed_sec,
                             EclipseInfo_t *eclipse)
{
    // gunes yon vektoru (yillik hareket)
    float sun_angle = ORBIT_TWO_PI * (float)elapsed_sec / (365.25f * 86400.0f);
    float sun_x = cosf(sun_angle) * ORBIT_AU;
    float sun_y = sinf(sun_angle) * ORBIT_AU;
    float sun_z = sinf(sun_angle) * ORBIT_AU * sinf(23.44f * ORBIT_DEG_TO_RAD);

    float sun_r = sqrtf(sun_x * sun_x + sun_y * sun_y + sun_z * sun_z);
    float sx = sun_x / sun_r;
    float sy = sun_y / sun_r;
    float sz = sun_z / sun_r;

    // uydu konumunun gunes yonune projeksiyonu
    float dot = sat_pos->x_eci * sx + sat_pos->y_eci * sy + sat_pos->z_eci * sz;

    if (dot < 0) {
        float proj_x = sat_pos->x_eci - dot * sx;
        float proj_y = sat_pos->y_eci - dot * sy;
        float proj_z = sat_pos->z_eci - dot * sz;
        float shadow_dist = sqrtf(proj_x * proj_x + proj_y * proj_y + proj_z * proj_z);

        if (shadow_dist < ORBIT_RE_EARTH) {
            eclipse->in_eclipse = true;
            eclipse->sunlit_fraction = 0.0f;
            return;
        }
    }

    eclipse->in_eclipse = false;
    eclipse->sunlit_fraction = 1.0f;
}

// --- Haversine mesafe ---

float ORBIT_GroundDistance(float lat1, float lon1, float lat2, float lon2)
{
    // a = sin2(dlat/2) + cos*cos*sin2(dlon/2)
    float dlat = (lat2 - lat1) * ORBIT_DEG_TO_RAD;
    float dlon = (lon2 - lon1) * ORBIT_DEG_TO_RAD;
    float lat1r = lat1 * ORBIT_DEG_TO_RAD;
    float lat2r = lat2 * ORBIT_DEG_TO_RAD;

    float a = sinf(dlat * 0.5f) * sinf(dlat * 0.5f) +
              cosf(lat1r) * cosf(lat2r) *
              sinf(dlon * 0.5f) * sinf(dlon * 0.5f);

    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

    return ORBIT_RE_EARTH * c;
}
