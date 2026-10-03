# TinyCamera

[![Arduino Library](https://img.shields.io/badge/Arduino-Library-blue.svg)](https://www.arduino.cc/reference/en/libraries/)
[![CMake](https://img.shields.io/badge/CMake-Supported-blue.svg)](https://github.com/pschatzmann/TinyCamera/blob/main/CMakeLists.txt)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-green.svg)](License.txt)

A tiny, header-only C++ camera library for Arduino that wraps the ESP32
(`esp32-camera`) camera API, bridges to RP2040's bundled
[PicoCamera](https://github.com/umeiko/PicoCamera) library
(`TinyCameraRP2040.h`), and implements the same interface natively on STM32
(DCMI + DMA, `TinyCameraSTM32.h`), behind one portable interface. All three
platforms expose a compatible C API (`camera_config_t`, `camera_fb_t`,
`esp_camera_init/deinit/fb_get/fb_return`), so the same header works
unmodified on any of them.

## Features

- Header-only: just `#include "TinyCamera.h"`, no `.cpp` file to compile.
- RAII frame buffers (`TinyCameraFrame`): the frame is returned to the
  driver automatically when it goes out of scope.
- Sensible defaults (`TinyCamera::defaultConfig()`) for JPEG/QVGA capture.
- Pin presets for common ESP32-CAM boards in `TinyCameraPins.h`
  (AI-Thinker, ESP-EYE, M5Stack, WROVER-KIT).
- Format conversion (`TinyCameraConvert.h`): JPEG/raw &rarr; JPEG, BMP,
  RGB888, RGB565, wrapping `img_converters.h` on ESP32 (the only platform
  with a hardware/vendor JPEG codec here).
- Software format conversion (`TinyCameraConvertSoftware.h`, needs the
  [TinyJPEG](https://github.com/pschatzmann/TinyJPEG) library): the same
  JPEG/raw &rarr; JPEG/BMP/RGB888/RGB565 conversions as pure software,
  identically on every platform - what makes JPEG usable at all on
  RP2040/STM32, neither of which has a hardware JPEG codec.
- Arbitrary-size scaling: `TinyCamera::setCustomFrameSize()` for hardware
  scaling (sensor-dependent), with a portable software nearest-neighbor
  fallback (`scaleRgb565()`) that works on every platform.

## Supported platforms

- **ESP32** (and variants): requires the `esp32-camera` driver bundled with
  the ESP32 Arduino core.
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

- **Desktop (Linux/macOS)**: runs the library and its examples on your
  computer via the [Arduino Emulator](https://github.com/pschatzmann/Arduino-Emulator),
  built with CMake - see [Desktop / CMake](#desktop--cmake) below.

## Documentation

- [Tutorial](docs/Tutorial.md) for installation, configuration, capturing
frames, format conversion and sensor tuning
- [Performance](docs/Performance.md)
for measured capture throughput and how to benchmark your own hardware
- [Supported Boards](docs/SupportedBoards.md) for every pin preset and the
exact pins it assigns. 
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

## Desktop / CMake

TinyCamera also runs on a desktop computer (Linux/macOS), so you can
develop and test frame-processing code without flashing a board. The
desktop backend (`TinyCameraDesktop.h`, selected automatically by
`TinyCamera.h`) gets its frames from:

- your **webcam** (Linux, via V4L2: `/dev/video0` by default, or set the
  `TINY_CAMERA_DEVICE` environment variable / `camera_config_t::device`),
  or
- a synthetic, animated **test pattern** (color bars), used automatically
  when no webcam can be opened, or always with
  `config.source = CAMERA_SOURCE_TEST_PATTERN`.

It supports RGB565, RGB888, GRAYSCALE and YUV422 frames, plus JPEG when
[TinyJPEG](https://github.com/pschatzmann/TinyJPEG) is available, at every
`FRAMESIZE_*` size and at any custom size (`setCustomFrameSize()`). Pin
settings are accepted and ignored, so the sketches compile unchanged.

Build the examples and the tests with CMake (this fetches the Arduino
Emulator and TinyJPEG from GitHub):

```
cmake -B build
cmake --build build
ctest --test-dir build
./build/examples/DesktopCapture/DesktopCapture   # saves capture.bmp
```

Options: `-DTINYCAMERA_BUILD_EXAMPLES=OFF`, `-DTINYCAMERA_BUILD_TESTS=OFF`,
`-DTINYCAMERA_USE_TINYJPEG=OFF`. To use local checkouts instead of fetching
them, pass `-DFETCHCONTENT_SOURCE_DIR_ARDUINO_EMULATOR=<path>` or
`-DFETCHCONTENT_SOURCE_DIR_TINYJPEG=<path>`.

To use TinyCamera from your own CMake project:

```cmake
add_subdirectory(TinyCamera)
target_link_libraries(my_app PRIVATE TinyCamera)
```
