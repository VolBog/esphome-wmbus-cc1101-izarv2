# ESPHome wM-Bus: CC1101 + izarv2/PRIOS (Diehl IZAR)

Adds the `izarv2` driver (SAP / Diehl PRIOS) to a CC1101 radio, on top of the
IoTLabs xmq-based wM-Bus stack. This makes a **Diehl IZAR RC 868 i W R4** water
meter decode on an ESP32 + CC1101, while keeping full CC1101 compatibility.

## What changed (5 files, +395/−2)

| File | Change |
|------|--------|
| `components/wmbus_radio/transceiver_cc1101.h` | **New** — CC1101 class implementing the IoTLabs `Transceiver` interface (register map, SPI strobes, `set_frequency_hz`). |
| `components/wmbus_radio/transceiver_cc1101.cpp` | **New** — software reset (SRES), part# check, GDO0 interrupt-driven `read(span, timeout)`, FIFOTHR=1, 26 MHz crystal, 868.95 MHz, 100 kbps 2-FSK. Register values match the proven upstream SzczepanLeon CC1101. |
| `components/wmbus_radio/transceiver_cc1101.py` | **New** — ESPHome schema: `radio_type: CC1101`, `cs_pin`, `dio1_pin`, optional `reset_pin` (CC1101 has none), optional `frequency` (default 868.95 MHz). |
| `components/wmbus_radio/__init__.py` | Register CC1101 in the `TRANSCEIVERS` dict. |
| `components/wmbus_common/telegram.cpp` | Fix ELL `0x8C`/`0x8E` address parsing — `0x8C` carries only a 4-byte ID (no MFCT/version/CRC); `0x8E` carries 8-byte ID + CRC. (Without this, Diehl ELL frames were mis-parsed.) |

The izarv2 driver + PRIOS decoding already exist in the IoTLabs base (`library.xmq`);
no copying needed — only the radio + the ELL fix were required.

## Why this base

- **SzczepanLeon (upstream)** has CC1101 but no izarv2.
- **IoTLabs** has izarv2 + PRIOS but only SX1262/SX1276 radios (rejects CC1101).
- **xpoh697** has CC1101 but only the old `driver_izar.cpp` (no confirmed izarv2).

This repo = IoTLabs base + a CC1101 ported to the IoTLabs `Transceiver` interface +
the ELL fix. CC1101 is the *only* radio path; no SX1262/SX1276 substitution.

## Verification (what was and wasn't tested)

| Check | Result |
|-------|--------|
| ESPHome 2026.10.2 schema validation (CC1101 + izarv2 + meter) | ✅ PASS |
| ESP32 + ESP-IDF 5.5.6 compile (CC1101 + izarv2 + PRIOS) | ✅ PASS (300 KB, 16% flash, no CC1101 warnings) |
| 12/12 izarv2 host telegram tests | ✅ PASS |
| Real telegrams from your environment decoded | ✅ PASS (7 meters, izarv2 auto-selected) |
| Old `izar` driver | ✅ preserved (base unchanged) |
| Static-registration crash (Guru Meditation) | ✅ not reintroduced — izarv2 only, `drivers: [izarv2]` |
| **CC1101 radio reception** (live SPI/RF) | ⚠️ **NOT hardware-verified** — compile-only; register values match proven upstream CC1101 |

## ESPHome config (your hardware)

```yaml
esphome:
  name: licznikwody

esp32:
  variant: esp32
  framework:
    type: esp-idf

external_components:
  - source: https://github.com/VolBog/esphome-wmbus-cc1101-izarv2.git
    refresh_interval: 1d

spi:
  clk_pin: GPIO18     # your existing pins
  mosi_pin: GPIO23
  miso_pin: GPIO19

wmbus_common:
  drivers:
    - izarv2

wmbus_radio:
  radio_type: CC1101
  cs_pin: GPIO5        # your existing pins
  dio1_pin: GPIO6
  frequency: 868.95MHz

wmbus_meter:
  - id: water_meter
    meter_id: "217e4d4f"   # your meter (prefix C25FA, serial 054543)
    type: izarv2

sensor:
  - platform: wmbus_meter
    id: total_water
    parent_id: water_meter
    field: total_m3
    name: "Water Total"
  - platform: wmbus_meter
    id: battery
    parent_id: water_meter
    field: battery_y
    name: "Battery"

logger:
  level: DEBUG
```

> **Your meter is `217e4d4f`** — prefix `C25FA`, serial `054543` (exact match to
> your meter). The telegram you originally captured (`217e4d8c`) was a **neighbor's
> meter** (serial 054604) — confirmed from your live log.

## Build & flash

```bash
esphome config licznikwody.yaml      # validate
esphome compile licznikwody.yaml     # build
esphome flash licznikwody.yaml       # flash (or use ESPHome Device Builder on HA)
```

In the HA Device Builder add-on, point `external_components` at the repo URL above.

## Test procedure

1. Flash, watch serial log: `CC1101 setup done`, `CC1101 part: 00, version: 00`.
2. Wait ~30 s for a frame: `Frame created (… bytes) [RSSI: …, mode:…]`.
3. Confirm `217e4d4f` appears and `total_m3` matches the meter's LCD display.
4. If no frames: check `dio1_pin` wiring (GDO0), `cs_pin`, and SPI pins; try
   `FIFOTHR`/`dio1_pin` polarity.

## Limitations

- **Encryption**: Diehl ELL frames here use `CI 0x8C` (no explicit key in frame).
  Your meter's telegrams decoded without a key — it is unencrypted. If a meter
  required a key, add `key:` to `wmbus_meter` (16-byte hex).
- **Value scaling**: `total_m3` etc. are validated against the meter display in the
  test procedure. 41.866 m³ is the decoded value for your meter.
- **CC1101 reception** is compile-verified, not RF-verified — the register values
  match the working upstream CC1101, but live SPI/RF behavior must be confirmed on
  the physical device.
