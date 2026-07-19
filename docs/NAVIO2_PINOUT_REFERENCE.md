# Navio2 Pinout Reference

## Document Status

`current`

## Objective

Provide a text transcription of the official Navio2 pinout image so non-vision tooling can reliably use the same source of truth.

## Source

- Official Emlid page: `https://docs.emlid.com/navio2/dev/pinout/`
- Official image: `https://docs.emlid.com/navio2/img/dev/pinout.png`

## 40-Pin Header Map

| Physical Pin | Raspberry Pi Signal | Navio2 Signal |
|---|---|---|
| 1 | 3.3V | 3.3V |
| 2 | 5V | 5V |
| 3 | GPIO2 | MS5611_SDA |
| 4 | 5V | 5V |
| 5 | GPIO3 | MS5611_SCL |
| 6 | GND | GND |
| 7 | GPIO4 | LED_RED |
| 8 | GPIO14 | UARTTX_DF13 |
| 9 | GND | GND |
| 10 | GPIO15 | UARTRX_DF13 |
| 11 | GPIO17 | GPIO17_DF13 |
| 12 | GPIO18 | GPIO18_DF13 |
| 13 | GPIO27 | LED_GREEN |
| 14 | GND | GND |
| 15 | GPIO22 | LSM9DS1_M_CS |
| 16 | GPIO23 | MPU9250_INT |
| 17 | 3.3V | 3.3V |
| 18 | GPIO24 | RCIO_PC10 |
| 19 | GPIO10 | LSM9DS1/MPU9250/UBLOX_MOSI |
| 20 | GND | GND |
| 21 | GPIO9 | LSM9DS1/MPU9250/UBLOX_MISO |
| 22 | GPIO25 | LSM9DS1_AG_CS |
| 23 | GPIO11 | LSM9DS1/MPU9250/UBLOX_SCLK |
| 24 | GPIO8 | UBLOX_CS |
| 25 | GND | GND |
| 26 | GPIO7 | MPU9250_CS |
| 27 | ID_SD | HAT_EEPROM_SD |
| 28 | ID_SC | HAT_EEPROM_SC |
| 29 | GPIO5 | RCIO_PC11 |
| 30 | GND | GND |
| 31 | GPIO6 | LED_BLUE |
| 32 | GPIO12 | RCIO_SWD_CLK |
| 33 | GPIO13 | RCIO_SWD_IO |
| 34 | GND | GND |
| 35 | GPIO19 | RCIO_SPI_MISO |
| 36 | GPIO16 | RCIO_SPI_CS |
| 37 | GPIO26 | HEADER PIN (free) |
| 38 | GPIO20 | RCIO_SPI_MOSI |
| 39 | GND | GND |
| 40 | GPIO21 | RCIO_SPI_SCLK |

## RCIO-Specific Summary

- RCIO SPI CS: physical pin `36`, `GPIO16`
- RCIO SPI MISO: physical pin `35`, `GPIO19`
- RCIO SPI MOSI: physical pin `38`, `GPIO20`
- RCIO SPI SCLK: physical pin `40`, `GPIO21`
- RCIO SWD CLK: physical pin `32`, `GPIO12`
- RCIO SWD IO: physical pin `33`, `GPIO13`
- RCIO PC10: physical pin `18`, `GPIO24`
- RCIO PC11: physical pin `29`, `GPIO5`

## Important Correction

- `GPIO17/GPIO18` on physical pins `11/12` are **not** RCIO SWD pins.
- They are labeled by Emlid as `GPIO17_DF13` and `GPIO18_DF13`.
- Earlier Pi 5 SWD tests that used `GPIO17/GPIO18` were therefore probing the wrong header pins.

## Power module port (added 2026-07-19)

The Navio2 POWER port is a **Pixhawk-compatible 6-pin power socket** (JST-GH 1.25 mm) — confirmed by Emlid's spec (6-pin, 5.3 V ±0.1 V / 3 A BEC, up to 90 A sense). Standard Pixhawk pin order:

| Pin | Signal | Notes |
|---|---|---|
| 1 | +5 V (VCC) | PM's BEC output (only ~3 A — unused when powering the stack from a separate DC/DC) |
| 2 | +5 V (VCC) | |
| 3 | Current sense | analog, 0–3.3 V into the Navio2 ADC |
| 4 | Voltage sense | analog, 0–3.3 V into the Navio2 ADC |
| 5 | GND | |
| 6 | GND | |

**Compatibility:** Holybro **PM02 V3.2** and **Mauch** sensors use this same Pixhawk pin order → plug in directly (verify the physical connector/pitch). **CUAV** modules use a *different* pin order → need re-pinning/adapter. Always re-calibrate `BATT_VOLT_MULT` + `BATT_AMP_PERVLT` when swapping modules (each has different scaling). ArduPilot analog battery monitor = `BATT_MONITOR 4`.

> Note: Emlid's own pinout diagram is an image; the order above is the Pixhawk standard that "Pixhawk-compatible" implies. Eyeball Emlid's `pinout.png` against this before crimping if you want 100% certainty.
