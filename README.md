# TinyCamera

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](License.txt)

A tiny, header-only C++ camera library for Arduino that wraps the ESP32
(`esp32-camera`) and RP2040 (arduino-pico `Camera`) camera APIs, and
implements the same interface natively on STM32 (DCMI + DMA), behind one
portable interface. All three platforms expose a compatible C API
(`camera_config_t`, `camera_fb_t`, `esp_camera_init/deinit/fb_get/fb_return`),
so the same header works unmodified on any of them.

## Features

- Header-only: just `#include "TinyCamera.h"`, no `.cpp` file to compile.
- RAII frame buffers (`TinyCameraFrame`): the frame is returned to the
  driver automatically when it goes out of scope.
- Sensible defaults (`TinyCamera::defaultConfig()`) for JPEG/QVGA capture.
- Pin presets for common ESP32-CAM boards in `TinyCameraPins.h`
  (AI-Thinker, ESP-EYE, M5Stack, WROVER-KIT).
- Format conversion (`TinyCameraConvert.h`): JPEG/raw &rarr; JPEG, BMP,
  RGB888, RGB565, wrapping `img_converters.h`.
- Arbitrary-size scaling: `TinyCamera::setCustomFrameSize()` for hardware
  scaling (sensor-dependent), with a portable software nearest-neighbor
  fallback (`scaleRgb565()`) that works on every platform.

## Documentation

- [Tutorial](docs/Tutorial.md) for installation, configuration, capturing
frames, format conversion and sensor tuning
- [Performance](docs/Performance.md)
for measured capture throughput and how to benchmark your own hardware
- [Supported Boards](docs/SupportedBoards.md) for every pin preset and the
exact pins it assigns. 
- Runnable sketches are in [examples/](examples/).

## Supported platforms

- **ESP32** (and variants): requires the `esp32-camera` driver bundled with
  the ESP32 Arduino core.
- **RP2040**: requires the `Camera` library bundled with the
  [arduino-pico](https://github.com/earlephilhower/arduino-pico) core, which
  mirrors the ESP32 camera API. RP2040 camera boards have no standard
  pinout, so pins must be assigned manually in `camera_config_t`.
- **STM32** (DCMI-capable parts, e.g. STM32H7) - **experimental**: no
  external driver needed, TinyCamera implements the same interface
  directly on top of the STM32Cube HAL DCMI/DMA peripherals, but only one
  sensor (OV7725, auto-detected over SCCB/I2C) is currently supported and
  this backend has only been verified on one board/sensor combination.
  See the [STM32 section](docs/Tutorial.md#10-stm32-dcmi) of the tutorial
  before using it, especially its notes on camera-connector pin
  compatibility between boards.
