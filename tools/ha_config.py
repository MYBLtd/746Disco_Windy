"""
ha_config.py
============
Home Assistant sensor configuration for windy_render.py.

Edit the entity IDs in ROOMS to match your actual HA configuration.
Entity IDs follow the convention:  sensor.<room>_temperature / sensor.<room>_humidity

Also set HA_URL to the IP of your Home Assistant instance.
Token is loaded from ~/.ha_token – paste a Long-Lived Access Token there.
"""

import os

HA_URL   = "http://10.120.30.3:8123"
HA_TOKEN = open(os.path.expanduser("~/.ha_token")).read().strip()

# 4×4 grid (16 cells): 14 rooms + 2 None for empty slots.
# Order: row 0 (top) left→right, then row 1, etc.
# Set None for empty / unused grid cells.
ROOMS = [
    # ── row 0 ──────────────────────────────────────────────────────────────
    {
        "name":        "Kantoor",
        "temp_entity": "sensor.kantoor_temperature",
        "hum_entity":  "sensor.kantoor_humidity",
    },
    {
        "name":        "Badkamer",
        "temp_entity": "sensor.badkamer_temperature",
        "hum_entity":  "sensor.badkamer_humidity",
    },
    {
        "name":        "Slaapkamer",
        "temp_entity": "sensor.slaapkamer_temperature",
        "hum_entity":  "sensor.slaapkamer_humidity",
    },
    {
        "name":        "Kleding",
        "temp_entity": "sensor.kleding_temperature",
        "hum_entity":  "sensor.kleding_humidity",
    },
    # ── row 1 ──────────────────────────────────────────────────────────────
    {
        "name":        "Keuken",
        "temp_entity": "sensor.keuken_temperature",
        "hum_entity":  "sensor.keuken_humidity",
    },
    {
        "name":        "Woonkamer",
        "temp_entity": "sensor.woonkamer_temperature",
        "hum_entity":  "sensor.woonkamer_humidity",
    },
    {
        "name":        "Biblioth.",
        "temp_entity": "sensor.bibliotheek_temperature",
        "hum_entity":  "sensor.bibliotheek_humidity",
    },
    None,   # empty cell (col 3, row 1)
    # ── row 2 ──────────────────────────────────────────────────────────────
    {
        "name":        "Servers",
        "temp_entity": "sensor.servers_temperature",
        "hum_entity":  "sensor.servers_humidity",
    },
    {
        "name":        "Wasruimte",
        "temp_entity": "sensor.wasruimte_temperature",
        "hum_entity":  "sensor.wasruimte_humidity",
    },
    {
        "name":        "Dungeon",
        "temp_entity": "sensor.dungeon_temperature",
        "hum_entity":  "sensor.dungeon_humidity",
    },
    {
        "name":        "Mancave",
        "temp_entity": "sensor.mancave_temperature",
        "hum_entity":  "sensor.mancave_humidity",
    },
    # ── row 3 ──────────────────────────────────────────────────────────────
    {
        "name":        "Outside",
        "temp_entity": "sensor.outside_temperature",
        "hum_entity":  "sensor.outside_humidity",
    },
    {
        "name":        "Tuinkamer",
        "temp_entity": "sensor.tuinkamer_temperature",
        "hum_entity":  "sensor.tuinkamer_humidity",
    },
    {
        "name":        "Garage",
        "temp_entity": "sensor.garage_temperature",
        "hum_entity":  "sensor.garage_humidity",
    },
    None,   # empty cell (col 3, row 3)
]
