# TinyCamera

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![CMake](https://img.shields.io/badge/CMake-Supported-blue.svg)](https://github.com/pschatzmann/TinyCamera/blob/main/CMakeLists.txt)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](License.txt)

A tiny, header-only C++ camera library for Arduino that wraps the ESP32
(`esp32-camera`) camera API, bridges to RP2040's bundled
[PicoCamera](https://github.com/umeiko/PicoCamera) library
(`TinyCameraRP2040.h`), and implements the same interface natively on STM32
(DCMI + DMA, `TinyCameraSTM32.h`), and bridges to the ESP32 core's
`ESP_Video` library for MIPI-CSI/DVP cameras (ESP32-P4, ESP32-S3,
`TinyCameraESPVideo.h`), behind one portable interface. All platforms
expose a compatible C API (`camera_config_t`, `camera_fb_t`,
`esp_camera_init/deinit/fb_get/fb_return`), so the same header works
unmodified on any of them.

## Features

- Header-only: just `#include "TinyCamera.h"`, no `.cpp` file to compile.
- RAII frame buffers (`TinyCameraFrame`): the frame is returned to the
  driver automatically when it goes out of scope.
- Sensible defaults (`TinyCamera::defaultConfig()`) for JPEG/QVGA capture.
- Pin presets for common ESP32-CAM boards in `TinyCameraPins.h`
  (AI-Thinker, ESP-EYE, M5Stack, WROVER-KIT, ESP32-S3 boards, Waveshare
  ESP32-P4-WIFI6).
- Format conversion (`TinyCameraConvert.h`): JPEG/raw &rarr; JPEG, BMP,
  RGB888, RGB565, wrapping `img_converters.h` on ESP32 with
  `esp32-camera`. On the ESP32-P4, `PIXFORMAT_JPEG` frames are encoded by
  the chip's hardware JPEG encoder.
- Software format conversion (`TinyCameraConvertSoftware.h`, needs the
  [TinyJPEG](https://github.com/pschatzmann/TinyJPEG) library): the same
  JPEG/raw &rarr; JPEG/BMP/RGB888/RGB565 conversions as pure software,
  identically on every platform - what makes JPEG usable at all on
  RP2040/STM32, neither of which has a hardware JPEG codec.
- Arbitrary-size scaling: `TinyCamera::setCustomFrameSize()` for hardware
  scaling (sensor-dependent), with a portable software nearest-neighbor
  fallback (`scaleRgb565()`) that works on every platform.

## Supported platforms

- **ESP32** (and variants except the ESP32-P4): requires the `esp32-camera`
  driver bundled with the ESP32 Arduino core.
- **RP2040**: requires [PicoCamera](https://github.com/umeiko/PicoCamera),
  bundled with the [arduino-pico](https://github.com/earlephilhower/arduino-pico)
  core. `TinyCameraRP2040.h` (included automatically by `TinyCamera.h`)
  bridges PicoCamera's own `pico_camera_*()` API to the `esp_camera_*()`
  surface the rest of this library expects - not independently verified on
  real RP2040 hardware (only compiled), unlike the STM32 backend. PicoCamera
  has no hardware JPEG codec, so `TinyCameraConvert.h`'s JPEG-related
  functions only pass through frames already captured as JPEG; use
  `TinyCameraConvertSoftware.h` for real JPEG encode/decode. RP2040 camera
  boards have no standard pinout, so pins must be assigned manually in
  `camera_config_t`.
- **STM32** (DCMI-capable parts, e.g. STM32H7) - **experimental**: no
  external driver needed, TinyCamera implements the same interface
  directly on top of the STM32Cube HAL DCMI/DMA peripherals, but only one
  sensor (OV7725, auto-detected over SCCB/I2C) is currently supported and
  this backend has only been verified on one board/sensor combination.
  See the [STM32 section](docs/Tutorial.md#10-stm32-dcmi) of the tutorial
  before using it, especially its notes on camera-connector pin
  compatibility between boards.

- **ESP32-P4 / ESP_Video** - **experimental**: MIPI-CSI (ESP32-P4) and DVP
  (ESP32-P4, opt-in on ESP32-S3) cameras via the `ESP_Video` library bundled
  with the ESP32 Arduino core 3.3+, including the P4's hardware JPEG
  encoder. The P4 uses this backend automatically; on the S3 define
  `TINY_CAMERA_USE_ESP_VIDEO` before including `TinyCamera.h`. MIPI-CSI is
  verified on a Waveshare ESP32-P4-WIFI6; DVP is only compiled so far - see
  the
  [ESP_Video section](docs/Tutorial.md#11-esp_video-esp32-p4-mipi-csi-esp32-s3-dvp)
  of the tutorial. Examples:
  - [WaveshareESP32P4Capture](examples/WaveshareESP32P4Capture): captures
    640x480 JPEGs on a Waveshare ESP32-P4-WIFI6 and saves them to the TF
    card as `/captures/capture_N.jpg`. Build it with
    `esp32:esp32:esp32p4:ChipVariant=postv3`; its output goes to `Serial0`
    (UART0), which is what the board's USB-C port shows.
  - [ESPVideoCapture](examples/ESPVideoCapture): the same sketch for
    MIPI-CSI on the ESP32-P4 and DVP on the ESP32-S3.

- **Desktop (Linux/macOS/Windows)**: runs the library in a plain C++ program, and
  the example sketches via the [Arduino Emulator](https://github.com/pschatzmann/Arduino-Emulator),
  built with CMake - see [Desktop / CMake](docs/Desktop.md).

## Documentation

- [Tutorial](docs/Tutorial.md) for installation, configuration, capturing
frames, format conversion and sensor tuning
- [Performance](docs/Performance.md)
for measured capture throughput and how to benchmark your own hardware
- [Supported Boards](docs/SupportedBoards.md) for every pin preset and the
exact pins it assigns. 
- [Desktop / CMake](docs/Desktop.md) for running TinyCamera on Linux/macOS/Windows
(webcam or test pattern) and building it with CMake
- Runnable sketches are in [examples/](examples/).

## Installation in Arduino

You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/TinyCamera.git
```

I recommend to use git because you can easily update to the latest version just by executing the ```git pull``` command in the project folder.

### Optional dependency

`TinyCameraConvertSoftware.h` (pure-software JPEG conversion, see
[Features](#features)) needs the
[TinyJPEG](https://github.com/pschatzmann/TinyJPEG) library installed
alongside TinyCamera - install it the same way (Library Manager or
`git clone` into your `libraries/` folder). It's only required if your
sketch includes `TinyCameraConvertSoftware.h`; everything else in
TinyCamera has no dependencies beyond the platform's own camera driver.
