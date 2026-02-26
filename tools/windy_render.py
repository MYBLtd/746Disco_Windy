#!/usr/bin/env python3
"""
windy_render.py
================
Renders a Windy.com-style weather map for the STM32F746G-DISCO (480×272 RGB565).

Data sources (no API key needed):
  Map tiles   – OpenStreetMap tile CDN
  Weather     – Open-Meteo public forecast API

Target coordinates taken from:
  https://www.windy.com/56.460/13.592?56.111,13.592,8
  Forecast pin : 56.460 N, 13.592 E  (Scania, southern Sweden)
  Map centre   : 56.111 N, 13.592 E  (zoomed to show regional context)

Outputs
  windy_480x272.png        – preview (open in any viewer)
  ../Core/Inc/windy_img.h  – C header with uint16_t array for firmware
  windy_480x272.bin        – raw little-endian RGB565 (for SD-card loading)

Run:
  cd tools/
  pip3 install -r requirements.txt
  python3 windy_render.py
"""

import argparse
import math
import struct
import time
import os
import datetime
import requests
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from io import BytesIO

# ── Target & display configuration ──────────────────────────────────────────

PIN_LAT   = 56.460   # forecast pin latitude
PIN_LON   = 13.592   # forecast pin longitude
MAP_LAT   = 56.460   # map-centre latitude  (adjusted to keep pin centred)
MAP_LON   = 13.592   # map-centre longitude
ZOOM      = 9        # tile zoom  (9 gives ~80 km wide at this latitude)

WIDTH     = 480
HEIGHT    = 272
TILE_SIZE = 256

PANEL_W   = 150      # left data-panel width in pixels
TILE_ATTR = "© OpenStreetMap contributors"

# ── Tile maths ───────────────────────────────────────────────────────────────

def _latlon_to_tile_float(lat, lon, zoom):
    """Return fractional tile coordinates (xtile, ytile)."""
    n = 2.0 ** zoom
    xt = (lon + 180.0) / 360.0 * n
    lr = math.radians(lat)
    yt = (1.0 - math.log(math.tan(lr) + 1.0 / math.cos(lr)) / math.pi) / 2.0 * n
    return xt, yt

def _tile_float_to_pixel(tx_f, ty_f, origin_tx_f, origin_ty_f):
    """Convert a fractional tile position to canvas pixel (origin = canvas top-left)."""
    px = (tx_f - origin_tx_f) * TILE_SIZE
    py = (ty_f - origin_ty_f) * TILE_SIZE
    return px, py

# ── OSM tile download ────────────────────────────────────────────────────────

_SESSION = requests.Session()
_SESSION.headers.update({"User-Agent": "STM32F746-windy-display/1.0 "
                                        "(github: embedded-weather-demo)"})

def _fetch_tile(x, y, zoom, retries=3):
    url = f"https://tile.openstreetmap.org/{zoom}/{int(x)}/{int(y)}.png"
    for attempt in range(retries):
        try:
            r = _SESSION.get(url, timeout=12)
            r.raise_for_status()
            return Image.open(BytesIO(r.content)).convert("RGB")
        except Exception as e:
            if attempt == retries - 1:
                print(f"  [warn] tile {x},{y} failed: {e} – using placeholder")
                img = Image.new("RGB", (TILE_SIZE, TILE_SIZE), (22, 27, 38))
                return img
            time.sleep(0.5)

def build_map_canvas(map_lat, map_lon, zoom):
    """Download enough tiles to cover WIDTH×HEIGHT centred on (map_lat, map_lon).
    Returns (canvas_image, origin_tile_float_x, origin_tile_float_y).
    """
    cx_f, cy_f = _latlon_to_tile_float(map_lat, map_lon, zoom)

    # top-left tile index of the canvas (add 1-tile margin on each side)
    x0 = int(cx_f) - 1
    y0 = int(cy_f) - 1

    # how many tiles we need in each direction
    cols = math.ceil(WIDTH  / TILE_SIZE) + 3
    rows = math.ceil(HEIGHT / TILE_SIZE) + 3

    canvas_w = cols * TILE_SIZE
    canvas_h = rows * TILE_SIZE
    canvas = Image.new("RGB", (canvas_w, canvas_h), (22, 27, 38))

    print(f"Downloading {cols}×{rows} = {cols*rows} OSM tiles (zoom {zoom})…")
    n_tiles = 2 ** zoom
    for ty in range(rows):
        for tx in range(cols):
            tile = _fetch_tile((x0 + tx) % n_tiles, (y0 + ty) % n_tiles, zoom)
            canvas.paste(tile, (tx * TILE_SIZE, ty * TILE_SIZE))
            print(f"  tile ({x0+tx}, {y0+ty})", end="\r")
    print()

    # crop to WIDTH×HEIGHT centered on the target
    origin_x_f = x0          # fractional tile x of canvas left edge
    origin_y_f = y0          # fractional tile y of canvas top edge

    cx_px, cy_px = _tile_float_to_pixel(cx_f, cy_f, origin_x_f, origin_y_f)
    left  = int(cx_px) - WIDTH  // 2
    top   = int(cy_px) - HEIGHT // 2
    crop  = canvas.crop((left, top, left + WIDTH, top + HEIGHT))

    # shift origins to match the crop
    new_ox_f = origin_x_f + left  / TILE_SIZE
    new_oy_f = origin_y_f + top   / TILE_SIZE

    return crop, new_ox_f, new_oy_f

# ── Weather data ─────────────────────────────────────────────────────────────

def fetch_weather(lat, lon):
    """Return Open-Meteo JSON for current + 24-h hourly forecast."""
    url = (
        "https://api.open-meteo.com/v1/forecast"
        f"?latitude={lat}&longitude={lon}"
        "&current=temperature_2m,apparent_temperature,"
        "wind_speed_10m,wind_direction_10m,"
        "weather_code,relative_humidity_2m,precipitation"
        "&hourly=wind_speed_10m,wind_direction_10m,wind_gusts_10m,"
        "temperature_2m,precipitation_probability"
        "&forecast_days=1"
        "&wind_speed_unit=kmh"
    )
    print(f"Fetching weather for {lat}°N {lon}°E …")
    r = _SESSION.get(url, timeout=10)
    r.raise_for_status()
    data = r.json()
    c = data["current"]
    print(f"  T={c['temperature_2m']}°C  "
          f"wind={c['wind_speed_10m']} km/h @ {c['wind_direction_10m']}°  "
          f"RH={c['relative_humidity_2m']}%")
    return data

# ── Colour helpers ───────────────────────────────────────────────────────────

# Wind-speed colour ramp (matches Windy.com palette closely)
_WIND_RAMP = [
    (  0, ( 48, 18, 59)),   # very calm – dark purple
    (  3, ( 68, 90,230)),   # calm      – blue
    ( 10, (  0,200,200)),   # light     – cyan
    ( 20, (  0,210, 90)),   # moderate  – green
    ( 35, (230,230,  0)),   # fresh     – yellow
    ( 50, (255,140,  0)),   # strong    – orange
    ( 75, (220,  0,  0)),   # gale      – red
    (100, (180,  0,180)),   # storm     – magenta
]

def _lerp_color(c0, c1, t):
    return tuple(int(c0[i] + t * (c1[i] - c0[i])) for i in range(3))

def wind_color(speed_kmh):
    ramp = _WIND_RAMP
    s = max(0.0, float(speed_kmh))
    for i in range(len(ramp) - 1):
        s0, c0 = ramp[i]
        s1, c1 = ramp[i + 1]
        if s <= s1:
            t = (s - s0) / (s1 - s0) if s1 > s0 else 0.0
            return _lerp_color(c0, c1, t)
    return ramp[-1][1]

def _beaufort(kmh):
    thresholds = [1, 6, 12, 20, 29, 39, 50, 62, 75, 89, 103, 118]
    for b, t in enumerate(thresholds):
        if kmh < t:
            return b
    return 12

def _wmo_description(code):
    """Map WMO weather code to short description."""
    table = {
        0: "Clear sky", 1: "Mainly clear", 2: "Partly cloudy", 3: "Overcast",
        45: "Fog", 48: "Icing fog",
        51: "Light drizzle", 53: "Drizzle", 55: "Heavy drizzle",
        61: "Light rain", 63: "Rain", 65: "Heavy rain",
        71: "Light snow", 73: "Snow", 75: "Heavy snow",
        77: "Snow grains",
        80: "Light showers", 81: "Showers", 82: "Heavy showers",
        85: "Snow showers", 86: "Heavy snow showers",
        95: "Thunderstorm", 96: "Hail storm", 99: "Heavy hail storm",
    }
    return table.get(code, f"WMO {code}")

def _dir_arrow(deg):
    """Unicode arrow pointing in the wind-FROM direction."""
    arrows = "↑↗→↘↓↙←↖"
    return arrows[round(deg / 45) % 8]

# ── Wind-field overlay ───────────────────────────────────────────────────────

def draw_wind_field(draw, speed_base, dir_base, panel_w):
    """Draw a 15×9 grid of coloured wind arrows over the map area."""
    cols, rows = 15, 9
    cell_w = (WIDTH - panel_w) / cols
    cell_h = HEIGHT / rows
    rng = np.random.default_rng(42)          # fixed seed → deterministic look

    for row in range(rows):
        for col in range(cols):
            # small spatial perturbation for a flow-field feel
            noise_spd = speed_base * (0.75 + 0.5 * math.sin(col * 0.9 + row * 1.3 + 0.5))
            noise_dir = dir_base  + 18 * math.sin(col * 1.4 + row * 0.8)

            colour = wind_color(noise_spd)
            cx = int(panel_w + (col + 0.5) * cell_w)
            cy = int((row + 0.5) * cell_h)

            # arrow length proportional to speed
            length = max(6, int(noise_spd / 5))
            rad = math.radians(noise_dir - 180)   # direction the wind blows TO
            ex = cx + int(math.sin(rad) * length)
            ey = cy - int(math.cos(rad) * length)

            draw.line([(cx, cy), (ex, ey)], fill=(*colour, 200), width=2)
            # arrowhead dot
            draw.ellipse([(ex - 2, ey - 2), (ex + 2, ey + 2)],
                         fill=(*colour, 240))

# ── Left data panel ──────────────────────────────────────────────────────────

def draw_panel(draw, weather, panel_w):
    cur    = weather["current"]
    temp   = cur["temperature_2m"]
    feel   = cur["apparent_temperature"]
    spd    = cur["wind_speed_10m"]
    wdir   = cur["wind_direction_10m"]
    rh     = cur["relative_humidity_2m"]
    wcode  = cur["weather_code"]
    precip = cur.get("precipitation", 0.0)

    hourly = weather["hourly"]
    h_spd  = hourly["wind_speed_10m"]
    h_gust = hourly["wind_gusts_10m"]
    gust_now = max(h_gust[:3]) if h_gust else spd * 1.6

    # Semi-transparent background
    draw.rectangle([(0, 0), (panel_w - 1, HEIGHT - 1)],
                   fill=(8, 12, 28, 215))

    # Separator line
    draw.line([(panel_w - 1, 0), (panel_w - 1, HEIGHT)],
              fill=(60, 80, 120, 255), width=1)

    fnt_big   = ImageFont.load_default(size=30)
    fnt_med   = ImageFont.load_default(size=14)
    fnt_small = ImageFont.load_default(size=11)
    fnt_tiny  = ImageFont.load_default(size=9)

    WHITE  = (240, 240, 255)
    GREY   = (150, 155, 180)
    DIM    = (100, 105, 130)
    CYAN   = ( 80, 200, 255)
    ORANGE = (255, 165,  60)

    y = 8
    # Temperature
    draw.text((10, y), f"{temp:.1f}°C", fill=WHITE, font=fnt_big)
    y += 34
    draw.text((10, y), f"feels {feel:.0f}°C", fill=GREY, font=fnt_small)
    y += 16

    # Weather description
    draw.text((10, y), _wmo_description(wcode), fill=CYAN, font=fnt_small)
    y += 18

    # Divider
    draw.line([(8, y), (panel_w - 10, y)], fill=(50, 60, 100, 200))
    y += 6

    # Wind
    wcolour = wind_color(spd)
    draw.text((10, y), "Wind", fill=GREY, font=fnt_small)
    y += 13
    draw.text((10, y), f"{spd:.1f} km/h", fill=wcolour, font=fnt_med)
    y += 17
    draw.text((10, y), f"{_dir_arrow(wdir)} {wdir:.0f}°  BFT {_beaufort(spd)}",
              fill=(200, 205, 225), font=fnt_small)
    y += 16

    # Gusts
    draw.text((10, y), "Gusts", fill=GREY, font=fnt_small)
    y += 13
    draw.text((10, y), f"{gust_now:.1f} km/h", fill=ORANGE, font=fnt_med)
    y += 18

    draw.line([(8, y), (panel_w - 10, y)], fill=(50, 60, 100, 200))
    y += 6

    # Humidity
    draw.text((10, y), "Humidity", fill=GREY, font=fnt_small)
    y += 13
    draw.text((10, y), f"{rh}%", fill=CYAN, font=fnt_med)
    y += 17

    # Precipitation
    if precip > 0:
        draw.text((10, y), f"Precip {precip:.1f} mm", fill=(120, 200, 255), font=fnt_small)
        y += 14

    draw.line([(8, y), (panel_w - 10, y)], fill=(50, 60, 100, 200))
    y += 6

    # Location
    draw.text((10, y), f"{PIN_LAT:.3f}°N", fill=DIM, font=fnt_tiny)
    y += 11
    draw.text((10, y), f"{PIN_LON:.3f}°E", fill=DIM, font=fnt_tiny)
    y += 11
    draw.text((10, y), "Scania, Sweden", fill=DIM, font=fnt_tiny)
    y += 11

    # Timestamp
    ts = datetime.datetime.utcnow().strftime("%d %b %H:%MZ")
    draw.text((10, HEIGHT - 13), ts, fill=DIM, font=fnt_tiny)

# ── 24-hour wind chart ───────────────────────────────────────────────────────

def draw_hourly_chart(draw, weather, panel_w):
    h_spd  = weather["hourly"]["wind_speed_10m"][:24]
    h_gust = weather["hourly"]["wind_gusts_10m"][:24]
    if not h_spd:
        return

    CHART_X = panel_w + 6
    CHART_Y = HEIGHT - 50
    CHART_W = WIDTH - panel_w - 12
    CHART_H = 38

    fnt = ImageFont.load_default(size=9)
    GREY = (130, 135, 160)

    # Background
    draw.rectangle([(CHART_X - 2, CHART_Y - 12),
                    (CHART_X + CHART_W + 2, CHART_Y + CHART_H + 1)],
                   fill=(6, 8, 22, 210))
    draw.text((CHART_X, CHART_Y - 12), "24h wind km/h", fill=GREY, font=fnt)

    max_spd = max(max(h_gust), 10.0)
    bw = max(1, CHART_W // 24)

    for h in range(len(h_spd)):
        bx = CHART_X + int(h * CHART_W / 24)
        # gust bar (lighter, full height)
        gh = int(h_gust[h] / max_spd * CHART_H)
        gc = wind_color(h_gust[h])
        draw.rectangle([(bx, CHART_Y + CHART_H - gh),
                        (bx + bw - 1, CHART_Y + CHART_H)],
                       fill=(*gc, 90))
        # speed bar (solid)
        sh = int(h_spd[h] / max_spd * CHART_H)
        sc = wind_color(h_spd[h])
        draw.rectangle([(bx, CHART_Y + CHART_H - sh),
                        (bx + bw - 1, CHART_Y + CHART_H)],
                       fill=(*sc, 200))

    # Y-axis label (max)
    draw.text((CHART_X + CHART_W + 3, CHART_Y),
              f"{int(max_spd)}", fill=GREY, font=fnt)

# ── Wind-speed colour bar ────────────────────────────────────────────────────

def draw_color_bar(draw):
    BAR_X = WIDTH - 115
    BAR_Y = HEIGHT - 50
    BAR_W = 100
    BAR_H = 8
    fnt   = ImageFont.load_default(size=9)
    GREY  = (130, 135, 160)

    draw.text((BAR_X, BAR_Y - 11), "km/h", fill=GREY, font=fnt)
    for px in range(BAR_W):
        c = wind_color(px * 100 / BAR_W)
        draw.line([(BAR_X + px, BAR_Y), (BAR_X + px, BAR_Y + BAR_H)], fill=c)
    draw.text((BAR_X - 4,       BAR_Y + BAR_H + 1), "0",   fill=GREY, font=fnt)
    draw.text((BAR_X + BAR_W - 10, BAR_Y + BAR_H + 1), "100", fill=GREY, font=fnt)

# ── Pin marker ───────────────────────────────────────────────────────────────

def draw_pin(draw, canvas_ox_f, canvas_oy_f):
    """Draw a marker circle at the forecast pin location."""
    px_f, py_f = _latlon_to_tile_float(PIN_LAT, PIN_LON, ZOOM)
    px, py = _tile_float_to_pixel(px_f, py_f, canvas_ox_f, canvas_oy_f)
    cx, cy = int(px), int(py)

    if PANEL_W < cx < WIDTH and 0 < cy < HEIGHT:
        r = 6
        draw.ellipse([(cx - r, cy - r), (cx + r, cy + r)],
                     fill=(255, 60, 60, 220), outline=(255, 255, 255, 240), width=2)
        draw.ellipse([(cx - 2, cy - 2), (cx + 2, cy + 2)],
                     fill=(255, 255, 255, 255))
        draw.text((cx + 9, cy - 6), f"{PIN_LAT:.2f}°N",
                  fill=(255, 240, 240), font=ImageFont.load_default(size=9))

# ── Attribution ──────────────────────────────────────────────────────────────

def draw_attribution(draw):
    fnt = ImageFont.load_default(size=8)
    draw.text((PANEL_W + 3, HEIGHT - 9), TILE_ATTR,
              fill=(80, 85, 110, 200), font=fnt)

# ── RGB565 conversion ────────────────────────────────────────────────────────

def to_rgb565(img_rgb):
    """Convert PIL RGB image to flat numpy uint16 array (RGB565, LE)."""
    arr = np.array(img_rgb, dtype=np.uint16)
    r = (arr[:, :, 0] >> 3).astype(np.uint16)
    g = (arr[:, :, 1] >> 2).astype(np.uint16)
    b = (arr[:, :, 2] >> 3).astype(np.uint16)
    return ((r << 11) | (g << 5) | b).flatten()

# ── Main render ──────────────────────────────────────────────────────────────

def render(weather, out_png, out_bin, out_h):
    # 1. Map background
    map_img, ox_f, oy_f = build_map_canvas(MAP_LAT, MAP_LON, ZOOM)

    # 2. Dark overlay (Windy night-mode feel)
    dark = Image.new("RGBA", (WIDTH, HEIGHT), (0, 4, 18, 145))
    img  = map_img.convert("RGBA")
    img  = Image.alpha_composite(img, dark)

    draw = ImageDraw.Draw(img, "RGBA")

    # 3. Wind field arrows
    cur = weather["current"]
    draw_wind_field(draw, cur["wind_speed_10m"], cur["wind_direction_10m"], PANEL_W)

    # 4. Forecast pin
    draw_pin(draw, ox_f, oy_f)

    # 5. Data panel (drawn last so it's on top)
    draw_panel(draw, weather, PANEL_W)

    # 6. 24-hour chart (bottom of map area)
    draw_hourly_chart(draw, weather, PANEL_W)

    # 7. Wind colour bar
    draw_color_bar(draw)

    # 8. Attribution
    draw_attribution(draw)

    # Flatten to RGB
    img = img.convert("RGB")

    # 9. PNG preview
    img.save(out_png)
    print(f"Preview  → {out_png}")

    # 10. RGB565 binary (little-endian, direct memcpy into STM32 framebuffer)
    pixels = to_rgb565(img)
    with open(out_bin, "wb") as f:
        for px in pixels:
            f.write(struct.pack("<H", int(px)))
    print(f"Binary   → {out_bin}  ({len(pixels) * 2:,} bytes)")

    # 11. C header (optional – skip when serving from a remote machine)
    if out_h is None:
        return
    ts = datetime.datetime.utcnow().strftime("%Y-%m-%d %H:%MZ")
    total = WIDTH * HEIGHT
    with open(out_h, "w") as f:
        f.write("/* ------------------------------------------------------------------ *\n")
        f.write(f" * windy_img.h  –  auto-generated by tools/windy_render.py\n")
        f.write(f" * {ts}  |  {PIN_LAT}°N {PIN_LON}°E  |  zoom {ZOOM}\n")
        f.write(f" * {cur['temperature_2m']}°C  "
                f"wind {cur['wind_speed_10m']} km/h @ {cur['wind_direction_10m']}°\n")
        f.write(" * DO NOT EDIT – re-run windy_render.py to refresh\n")
        f.write(" * ------------------------------------------------------------------ */\n\n")
        f.write("#ifndef WINDY_IMG_H\n#define WINDY_IMG_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"#define WINDY_IMG_WIDTH  {WIDTH}U\n")
        f.write(f"#define WINDY_IMG_HEIGHT {HEIGHT}U\n")
        f.write(f"/* {total:,} px × 2 bytes = {total*2:,} bytes in Flash .rodata */\n\n")
        f.write("/* Placed in Flash by the linker (const → .rodata).\\n"
                "   LTDC can read directly from Flash via AHB – no memcpy needed. */\n")
        f.write(f"static const uint16_t windy_img[{total}U] = {{\n")
        PER_LINE = 16
        for i, px in enumerate(pixels):
            if i % PER_LINE == 0:
                f.write("    ")
            f.write(f"0x{int(px):04X}")
            if i < total - 1:
                f.write(",")
            if i % PER_LINE == PER_LINE - 1:
                f.write("\n")
            else:
                f.write(" ")
        if total % PER_LINE != 0:
            f.write("\n")
        f.write("};\n\n#endif /* WINDY_IMG_H */\n")
    print(f"C header → {out_h}  ({os.path.getsize(out_h):,} bytes)")

# ── Entry point ──────────────────────────────────────────────────────────────

if __name__ == "__main__":
    HERE = os.path.dirname(os.path.abspath(__file__))

    ap = argparse.ArgumentParser(description="Render Windy weather display image")
    ap.add_argument("--out-bin", default=os.path.join(HERE, "windy_480x272.bin"),
                    help="Output path for raw RGB565 binary (default: tools/windy_480x272.bin)")
    ap.add_argument("--out-png", default=os.path.join(HERE, "windy_480x272.png"),
                    help="Output path for PNG preview (default: tools/windy_480x272.png)")
    ap.add_argument("--no-header", action="store_true",
                    help="Skip C header generation (use on server, not dev machine)")
    args = ap.parse_args()

    weather = fetch_weather(PIN_LAT, PIN_LON)

    render(
        weather,
        out_png=args.out_png,
        out_bin=args.out_bin,
        out_h  =None if args.no_header
                else os.path.join(HERE, "../Core/Inc/windy_img.h"),
    )
    print("Done.")
