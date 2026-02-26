/**
 * @file  weather_data.c
 * @brief Minimal JSON field extractor for Open-Meteo current-conditions block.
 *
 * The Open-Meteo response structure is:
 *   {
 *     ...
 *     "current_units": { "temperature_2m": "°C", ... },   <-- string values
 *     "current":       { "temperature_2m": 3.2,  ... }    <-- numeric values
 *   }
 *
 * We MUST search inside the "current":{} block, not from the root, otherwise
 * strstr() finds the key first in "current_units" where values are strings
 * (e.g. "°C") and strtof() returns 0 / fails.
 */

#include "weather_data.h"
#include <string.h>
#include <stdlib.h>   /* atoi */

/* Locate the start of the "current":{} block. */
static const char *find_current_block(const char *json)
{
    /* Match "current":{ but NOT "current_units":{ */
    const char *p = json;
    while ((p = strstr(p, "\"current\":")) != NULL) {
        const char *after = p + strlen("\"current\":");
        while (*after == ' ') after++;
        if (*after == '{') return after;   /* found the object, not an array */
        p++;
    }
    return json;   /* fallback: search whole string */
}

/* Parse a decimal literal like "-3.2" using only integer arithmetic,
 * then do a single integer→float conversion at the end.
 * Avoids strtof() which crashes in the newlib-nano/nosys configuration. */
static int find_float(const char *json, const char *key, float *out)
{
    const char *p = strstr(json, key);
    if (!p) return -1;
    p += strlen(key);
    while (*p == ' ' || *p == ':') p++;

    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') { p++; }

    if (*p < '0' || *p > '9') return -1;

    long ival = 0;
    while (*p >= '0' && *p <= '9') { ival = ival * 10 + (*p - '0'); p++; }

    long frac = 0, fdiv = 1;
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') {
            if (fdiv < 100000L) { frac = frac * 10 + (*p - '0'); fdiv *= 10; }
            p++;
        }
    }

    *out = (float)sign * ((float)ival + (float)frac / (float)fdiv);
    return 0;
}

static int find_int(const char *json, const char *key, int *out)
{
    const char *p = strstr(json, key);
    if (!p) return -1;
    p += strlen(key);
    while (*p == ' ' || *p == ':') p++;
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        *out = atoi(p);
        return 0;
    }
    return -1;
}

/* ── Public API ────────────────────────────────────────── */

int weather_data_parse(const char *json, WeatherData *out)
{
    if (!json || !out) return -1;

    out->temperature  = 0.0f;
    out->wind_speed   = 0.0f;
    out->wind_dir     = 0;
    out->humidity     = 0;
    out->weather_code = 0;

    /* Search within "current":{} to skip "current_units":{} */
    const char *cur = find_current_block(json);

    int err = 0;
    err |= find_float(cur, "\"temperature_2m\"",      &out->temperature);
    err |= find_float(cur, "\"wind_speed_10m\"",      &out->wind_speed);
    err |= find_int  (cur, "\"wind_direction_10m\"",  &out->wind_dir);
    err |= find_int  (cur, "\"relative_humidity_2m\"",&out->humidity);
    err |= find_int  (cur, "\"weather_code\"",        &out->weather_code);

    return err ? -1 : 0;
}

/* ── WMO weather code descriptions (subset) ────────────── */

const char *weather_code_str(int code)
{
    switch (code) {
    case  0: return "Clear sky";
    case  1: return "Mainly clear";
    case  2: return "Partly cloudy";
    case  3: return "Overcast";
    case 45: case 48: return "Foggy";
    case 51: case 53: case 55: return "Drizzle";
    case 61: case 63: case 65: return "Rain";
    case 71: case 73: case 75: return "Snow";
    case 80: case 81: case 82: return "Showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Thunder+hail";
    default: return "Unknown";
    }
}
