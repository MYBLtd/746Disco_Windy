/**
 * @file  weather_config.h
 * @brief Compile-time configuration for WiFi and weather API.
 *
 * Edit WIFI_SSID and WIFI_PASS before flashing.
 */

#ifndef WEATHER_CONFIG_H
#define WEATHER_CONFIG_H

/* ── WiFi credentials ────────────────────────────────────────────── */
#define WIFI_SSID   "IOT2G-DoNotConnect!Private!"
#define WIFI_PASS   "W@tEenPrutW@chtw00rd!"

/* ── Open-Meteo API (plain HTTP, no TLS required) ────────────────── */
#define API_HOST    "api.open-meteo.com"
#define API_PORT    80
#define API_PATH    "/v1/forecast"                              \
                    "?latitude=56.460&longitude=13.592"        \
                    "&current=temperature_2m"                   \
                    ",relative_humidity_2m"                     \
                    ",wind_speed_10m"                           \
                    ",wind_direction_10m"                       \
                    ",weather_code"                             \
                    "&wind_speed_unit=ms"

/* ── Image server (Pad A) ─────────────────────────────────────────── */
/* Edit IMAGE_HOST to the LAN IP of your Debian 12 server.           */
/* The server serves windy_temp.bin and windy_hum.bin via HTTP.      */
#define IMAGE_HOST      "172.20.2.5"   /* ← change to your server IP */
#define IMAGE_PORT      8080
#define IMAGE_PATH_TEMP "/windy_temp.bin"
#define IMAGE_PATH_HUM  "/windy_hum.bin"

/* ── Refresh / flip intervals ────────────────────────────────────── */
#define WEATHER_REFRESH_MS   (10UL * 60UL * 1000UL)   /* 10 minutes  */
#define SENSOR_FLIP_MS       5000UL                    /* T/RH flip   */

#endif /* WEATHER_CONFIG_H */
