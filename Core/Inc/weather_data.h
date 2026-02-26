/**
 * @file  weather_data.h
 * @brief Weather data structure and JSON parser.
 *
 * Extracts the fields returned by the Open-Meteo /v1/forecast API
 * (current conditions block) using strstr + strtof – no library needed.
 */

#ifndef WEATHER_DATA_H
#define WEATHER_DATA_H

typedef struct {
    float temperature;    /* °C  – temperature_2m          */
    float wind_speed;     /* m/s – wind_speed_10m           */
    int   wind_dir;       /* °   – wind_direction_10m       */
    int   humidity;       /* %   – relative_humidity_2m     */
    int   weather_code;   /* WMO – weather_code             */
} WeatherData;

/**
 * @brief  Parse Open-Meteo JSON response into @p out.
 * @param  json  NUL-terminated JSON string.
 * @param  out   Destination struct (zeroed on failure).
 * @retval 0 on success, -1 if any required field is missing.
 */
int weather_data_parse(const char *json, WeatherData *out);

/**
 * @brief  Return a short human-readable WMO weather description.
 *         Returned pointer is a string literal – do not free.
 */
const char *weather_code_str(int code);

#endif /* WEATHER_DATA_H */
