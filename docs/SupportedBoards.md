# Supported Boards

Every `setPinsXxx()` preset in `TinyCameraPins.h`, the platform it's for, and
the exact pins it assigns. Use this as a quick reference or to double-check
against your own board's schematic before wiring anything - see
[Tutorial.md](Tutorial.md#4-assigning-pins) for how to use these, and the
STM32 section's
[pin-compatibility warning](Tutorial.md#10-stm32-dcmi) before assuming two
boards with the same connector are actually compatible.

All pin values below are exactly what each preset sets - nothing is
inferred or rounded. `-1` means the pin isn't broken out / not used.

## ESP32 (classic)

| Board | Function | XCLK | SDA | SCL | PWDN | RESET | D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7 | VSYNC | HREF | PCLK |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| AI-Thinker ESP32-CAM | `setPinsAiThinker` | 0 | 26 | 27 | 32 | -1 | 5 | 18 | 19 | 21 | 36 | 39 | 34 | 35 | 25 | 23 | 22 |
| ESP-EYE | `setPinsEspEye` | 4 | 18 | 23 | -1 | -1 | 34 | 13 | 14 | 35 | 39 | 38 | 37 | 36 | 5 | 27 | 25 |
| M5Stack Camera (original, PSRAM) | `setPinsM5Stack` | 27 | 25 | 23 | -1 | 15 | 32 | 35 | 34 | 5 | 39 | 18 | 36 | 19 | 22 | 26 | 21 |
| ESP32-WROVER-KIT | `setPinsWroverKit` | 21 | 26 | 27 | -1 | -1 | 4 | 5 | 18 | 19 | 36 | 39 | 34 | 35 | 25 | 23 | 22 |
| M5Stack Camera version B | `setPinsM5StackV2` | 27 | 22 | 23 | -1 | 15 | 32 | 35 | 34 | 5 | 39 | 18 | 36 | 19 | 25 | 26 | 21 |
| M5Stack Wide | `setPinsM5StackWide` | 27 | 22 | 23 | -1 | 15 | 32 | 35 | 34 | 5 | 39 | 18 | 36 | 19 | 25 | 26 | 21 |
| M5Stack ESP32CAM (no PSRAM) | `setPinsM5StackEsp32Cam` | 27 | 25 | 23 | -1 | 15 | 17 | 35 | 34 | 5 | 39 | 18 | 36 | 19 | 22 | 26 | 21 |
| M5Stack UnitCam (no PSRAM) | `setPinsM5StackUnitCam` | 27 | 25 | 23 | -1 | 15 | 32 | 35 | 34 | 5 | 39 | 18 | 36 | 19 | 22 | 26 | 21 |
| TTGO T-Journal (no PSRAM) | `setPinsTtgoTJournal` | 27 | 25 | 23 | 0 | 15 | 17 | 35 | 34 | 5 | 39 | 18 | 36 | 19 | 22 | 26 | 21 |
| Generic ESP32-CAM breakout (18-pin header) | `setPinsEsp32CamBoard` | 4 | 18 | 23 | 32 | 33 | 34 | 13 | 14 | 35 | 39 | 21 | 19 | 36 | 5 | 27 | 25 |

**Generic ESP32-CAM breakout note**: `pin_d3`/`pin_d1` (Y5/Y3) are swapped
between header-wired and direct-solder camera boards. If the image looks
wrong (rows/colors), try swapping those two values.

## ESP32-S2 / ESP32-S3

| Board | Function | XCLK | SDA | SCL | PWDN | RESET | D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7 | VSYNC | HREF | PCLK |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| M5Stack CamS3 Unit | `setPinsM5StackCamS3Unit` | 11 | 17 | 41 | -1 | 21 | 6 | 15 | 16 | 7 | 5 | 10 | 4 | 13 | 42 | 18 | 12 |
| Seeed XIAO ESP32S3 Sense | `setPinsXiaoEsp32S3` | 10 | 40 | 39 | -1 | -1 | 15 | 17 | 18 | 16 | 14 | 12 | 11 | 48 | 38 | 47 | 13 |
| Generic ESP32-S3-CAM-LCD | `setPinsEsp32S3CamLcd` | 40 | 17 | 18 | -1 | -1 | 13 | 47 | 14 | 3 | 12 | 42 | 41 | 39 | 21 | 38 | 11 |
| Generic ESP32-S2-CAM breakout (18-pin header) | `setPinsEsp32S2CamBoard` | 42 | 41 | 18 | 1 | 2 | 14 | 12 | 5 | 13 | 15 | 40 | 39 | 16 | 38 | 4 | 3 |
| Espressif ESP32-S3-EYE | `setPinsEsp32S3Eye` | 15 | 4 | 5 | -1 | -1 | 11 | 9 | 8 | 10 | 12 | 18 | 17 | 16 | 6 | 7 | 13 |
| DFRobot FireBeetle 2 ESP32-S3 / Romeo ESP32-S3 | `setPinsDFRobotEsp32S3` | 45 | 1 | 2 | -1 | -1 | 39 | 40 | 41 | 4 | 7 | 8 | 46 | 48 | 6 | 42 | 5 |
| arduino-audio-tools `esp32s3-mic-cam` | `setPinsEsp32S3MicCam` | 10 | 21 | 14 | -1 | -1 | 5 | 3 | 2 | 4 | 6 | 8 | 9 | 11 | 13 | 12 | 7 |

**Generic ESP32-S2-CAM breakout note**: same Y5/Y3 (`pin_d3`/`pin_d1`) swap
caveat as the ESP32-CAM breakout above.

**Unidentified ESP32-S3 camera board?** Try `setPinsEsp32S3Eye` first - many
generic/no-name "ESP32-S3-CAM" boards reuse Espressif's own ESP32-S3-EYE
reference pinout. This is the one preset in this table **confirmed on real
hardware** (captured a correct, correctly-colored JPEG photo). Every other
ESP32/S2/S3 preset is sourced from the ESP32 Arduino core's own bundled
`CameraWebServer` example (Espressif's reference pin list) but has not been
individually re-verified by this library's own testing.

## STM32

| Board | Function | XCLK | SDA | SCL | PWDN | RESET | D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7 | VSYNC | HREF | PCLK |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| WeAct STM32H750 / STM32H743 | `setPinsWeActStm32H750` | PA8 | PB9 | PB8 | -1 | -1 | PC6 | PC7 | PE0 | PE1 | PE4 | PD3 | PE5 | PE6 | PB7 | PA4 | PA6 |

The STM32 backend is **experimental** and currently supports only one
sensor (OV7725) - see [Tutorial.md section 10](Tutorial.md#10-stm32-dcmi)
before using it. `setPinsWeActStm32H750` also sets `xclk_freq_hz = 12000000`
(this board's clock architecture fixes XCLK at 12 MHz via `MCO1`/HSI48,
unlike ESP32-CAM boards where XCLK frequency is a free choice) - not shown
in the table above since it isn't a pin.

This is the only STM32 preset currently shipped. A DevEBox STM32H7xx_M
preset existed at one point but was removed - its camera connector is no
longer purchasable.

## RP2040

No presets are provided - RP2040 camera boards have no de-facto-standard
connector, so pins are always assigned manually. See
[Tutorial.md section 4](Tutorial.md#4-assigning-pins) for the fields to
fill in.
