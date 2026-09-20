# TinyCamera Tutorial

This tutorial walks through using TinyCamera, from installing the library to
capturing frames, converting them to other formats and tuning the sensor.

- [1. Installation](#1-installation)
- [2. Your first capture](#2-your-first-capture)
- [3. Configuring the camera](#3-configuring-the-camera)
- [4. Assigning pins](#4-assigning-pins)
- [5. Working with frames](#5-working-with-frames)
- [6. Format conversion](#6-format-conversion)
  - [Software conversion (TinyJPEG)](#software-conversion-tinyjpeg)
- [7. Tuning the sensor](#7-tuning-the-sensor)
- [8. Managing the camera lifecycle](#8-managing-the-camera-lifecycle)
- [9. Logging](#9-logging)
- [10. STM32 (DCMI)](#10-stm32-dcmi)
- [11. Troubleshooting](#11-troubleshooting)

## 1. Installation

TinyCamera is header-only: there is no `.cpp` file to compile and no build
configuration to adjust. Copy the library into your Arduino `libraries/`
folder (or add it to your PlatformIO `lib_deps`) and include the headers you
need:

```cpp
#include "TinyCamera.h"         // TinyCamera, TinyCameraFrame
#include "TinyCameraPins.h"     // setPinsAiThinker() and friends (ESP32 boards)
#include "TinyCameraConvert.h"  // toJpg(), toBmp(), toRgb888(), toRgb565()
#include "TinyCameraLogger.h"   // optional: TinyCameraLogger (see section 9)
// optional: TinyCameraConvertSoftware.h - toJpgSoftware() and friends,
// needs the TinyJPEG library installed too (see section 6)
```

Everything lives in the `tiny_camera` namespace. The examples use
`using namespace tiny_camera;` for brevity.

The underlying driver comes from your board's core:

- **ESP32** (and variants): the `esp32-camera` driver bundled with the ESP32
  Arduino core. Nothing extra to install.
- **RP2040**: [PicoCamera](https://github.com/umeiko/PicoCamera), bundled
  with the [arduino-pico](https://github.com/earlephilhower/arduino-pico)
  core - nothing extra to install, but its API (`pico_camera_*()`) isn't
  quite esp32-camera's own. `TinyCameraRP2040.h` (included automatically
  by `TinyCamera.h`) bridges the two, the same way `TinyCameraSTM32.h`
  does for STM32; unlike that backend, this bridge has only been
  compiled, not run on real RP2040 hardware. PicoCamera has no hardware
  JPEG codec, so `TinyCameraConvert.h`'s JPEG-related functions only
  pass through frames already captured as JPEG on this platform (same
  limitation as STM32) - see
  [Software conversion (TinyJPEG)](#software-conversion-tinyjpeg) for
  real JPEG encode/decode.
- **STM32** (DCMI-capable parts): implemented natively in this library on
  top of the STM32Cube HAL - nothing extra to install, but there's more to
  know than a one-line summary here; see [section 10](#10-stm32-dcmi).

Any other target fails at compile time with a clear `#error`.

## 2. Your first capture

The shortest useful sketch: initialize the camera, grab a JPEG frame every two
seconds and print its size.

```cpp
#include "TinyCamera.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

TinyCamera camera;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  camera_config_t config = TinyCamera::defaultConfig();
  setPinsAiThinker(config);  // ESP32-CAM (AI-Thinker)

  if (!camera.begin(config)) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial.println("Camera ready");
}

void loop() {
  TinyCameraFrame frame = camera.captureFrame();
  if (frame) {
    Serial.printf("Captured %u bytes (%ux%u)\n", (unsigned)frame.size(),
                  (unsigned)frame.width(), (unsigned)frame.height());
  } else {
    Serial.println("Capture failed");
  }
  delay(2000);
  // frame goes out of scope here and is returned to the driver automatically
}
```

Three things to note:

1. `begin()` returns `false` if the driver could not be initialized — always
   check it, since a wrong pin assignment is the most common mistake.
2. `captureFrame()` returns a `TinyCameraFrame`, which converts to `bool`, so
   `if (frame)` tests whether a frame was actually captured.
3. You never call `esp_camera_fb_return()`. The frame returns itself when it
   goes out of scope.

This sketch is available as the `SimpleCapture` example.

## 3. Configuring the camera

`TinyCamera::defaultConfig()` returns a `camera_config_t` pre-filled with
sensible values:

| Field          | Default                  | Meaning                            |
| -------------- | ------------------------ | ---------------------------------- |
| `pixel_format` | `PIXFORMAT_JPEG`         | Compressed JPEG output             |
| `frame_size`   | `FRAMESIZE_QVGA`         | 320x240                            |
| `jpeg_quality` | `12`                     | 0-63, **lower means better**       |
| `fb_count`     | `1`                      | One frame buffer                   |
| `fb_location`  | `CAMERA_FB_IN_DRAM`      | Buffer in DRAM (no PSRAM required) |
| `grab_mode`    | `CAMERA_GRAB_WHEN_EMPTY` | Capture on demand                  |
| `xclk_freq_hz` | `20000000`               | 20 MHz sensor clock                |

It is a plain struct, so override whatever you need before calling `begin()`:

```cpp
camera_config_t config = TinyCamera::defaultConfig();
setPinsAiThinker(config);

config.frame_size = FRAMESIZE_VGA;   // 640x480
config.jpeg_quality = 10;            // better quality, larger frames

// With PSRAM you can afford larger frames and double buffering:
config.fb_location = CAMERA_FB_IN_PSRAM;
config.fb_count = 2;

camera.begin(config);
```

Two rules of thumb:

- Large frame sizes (`FRAMESIZE_SVGA` and above) generally need PSRAM. Without
  it, keep to QVGA/VGA or `begin()` may fail for lack of memory.
- For raw formats set both `pixel_format` and a small frame size, e.g.
  `PIXFORMAT_RGB565` at QVGA is 150 KB per frame — uncompressed frames get big
  fast.

The aliases `PixelFormat` and `FrameSize` are provided for
`pixformat_t` / `framesize_t` if you prefer the namespaced names.

### Arbitrary frame sizes

`frame_size` only accepts the named `FRAMESIZE_*` presets. For an exact
custom resolution instead - e.g. to match a small display exactly, the
way `ScaledCapture` does (with a software-scaling fallback for platforms/
sensors that don't support it) - call
`camera.setCustomFrameSize(width, height)` **after** `begin()`. It asks
the sensor's own DSP to scale its current field of view down to that
exact size in hardware (not a software crop), and the next
`captureFrame()` reflects the new size:

```cpp
camera.begin(config);
if (!camera.setCustomFrameSize(160, 80)) {
  Serial.println("arbitrary sizes not supported here");
}
```

Support depends heavily on platform and sensor:

- **ESP32**: works if the currently-detected sensor's `esp32-camera`
  driver implements the low-level `set_res_raw()` hook - true for most
  OV-series sensors, false for simpler ones (e.g. GC0308). Only scales
  *down* from whatever `frame_size` was last configured with, never up
  past it.
- **RP2040**: not supported at all - arduino-pico's `Camera` library has
  no equivalent hook, only the fixed `FRAMESIZE_*` list. Always returns
  `false`.
- **STM32**: works for OV7725 (the only sensor this backend currently
  supports - see [section 10](#10-stm32-dcmi)). Unlike ESP32, OV7725
  always reconfigures from the sensor's full field of view rather than
  scaling down incrementally from the current size, so this accepts any
  0<width<=640, 0<height<=480 at any time, not just shrinking - the
  frame buffer is reallocated automatically if the new size needs more
  room. `camera_config_t::custom_width`/`custom_height`, set before the
  initial `begin()`, is still supported too and behaves the same way -
  this method is just the equivalent call usable afterward.

As with everything platform-specific in this library, this hasn't been
verified against every sensor it might technically support - test on
your own hardware before relying on it.

#### Software fallback

When `setCustomFrameSize()` returns `false` - RP2040, an ESP32 sensor
without `set_res_raw()`, or any rejected size - `TinyCameraConvert.h`'s
`scaleRgb565()` is a pure-software fallback that works identically on
every platform, since it's just pixel math with no sensor/hardware
dependency at all. Capture normally, then scale the result yourself:

```cpp
#include "TinyCameraConvert.h"

uint16_t scaled[160 * 80];  // outWidth * outHeight, RGB565 = 2 bytes/pixel

if (!camera.setCustomFrameSize(160, 80)) {
  if (auto frame = camera.captureFrame()) {
    if (scaleRgb565(frame, (uint8_t *)scaled, 160, 80)) {
      // scaled now holds a 160x80 RGB565 image
    }
  }
}
```

It's nearest-neighbor only (no interpolation) - lower image quality than
hardware scaling, but simple, needs no FPU, and works both down and up.
RGB565 source frames only; convert from JPEG with `toRgb565()` first if
needed.

## 4. Assigning pins

`TinyCameraPins.h` provides presets for common ESP32/ESP32-S2/ESP32-S3
camera boards, sourced from the ESP32 Arduino core's own bundled
CameraWebServer example. Call one on your config before `begin()`:

```cpp
// ESP32
setPinsAiThinker(config);        // AI-Thinker ESP32-CAM
setPinsEspEye(config);           // ESP-EYE
setPinsM5Stack(config);          // M5Stack (PSRAM) camera module
setPinsM5StackV2(config);        // M5Stack camera module, version B
setPinsM5StackWide(config);      // M5Stack Wide
setPinsM5StackEsp32Cam(config);  // M5Stack ESP32CAM (no PSRAM)
setPinsM5StackUnitCam(config);   // M5Stack UnitCam (no PSRAM)
setPinsWroverKit(config);        // ESP32-WROVER-KIT
setPinsTtgoTJournal(config);     // TTGO T-Journal (no PSRAM)
setPinsEsp32CamBoard(config);    // generic ESP32-CAM breakout (18-pin header)

// ESP32-S2 / ESP32-S3
setPinsM5StackCamS3Unit(config); // M5Stack CamS3 Unit
setPinsXiaoEsp32S3(config);      // Seeed XIAO ESP32S3 Sense's camera module
setPinsEsp32S3CamLcd(config);    // generic ESP32-S3-CAM-LCD boards
setPinsEsp32S2CamBoard(config);  // generic ESP32-S2-CAM breakout (18-pin header)
setPinsEsp32S3Eye(config);       // Espressif ESP32-S3-EYE - also what most
                                  // generic/no-name "ESP32-S3-CAM" boards use
setPinsDFRobotEsp32S3(config);   // DFRobot FireBeetle 2 / Romeo ESP32-S3
setPinsEsp32S3MicCam(config);    // arduino-audio-tools' esp32s3-mic-cam board
```

If you have an unlabeled/generic ESP32-S3 camera board and don't know which
preset it needs, they're safe to try in sequence: a wrong preset just makes
`begin()` fail (or the sensor won't be detected), not any lasting harm.
`setPinsEsp32S3Eye()` is worth trying first, since it's Espressif's own
reference design and the one most generic boards copy - confirmed end to
end on real hardware, including visually inspecting a captured JPEG photo
for correct content/color (see [Verifying captured images](#verifying-captured-images)).

RP2040 camera boards have no de-facto-standard pinout, so no presets are
provided. Fill in the pins yourself:

```cpp
camera_config_t config = TinyCamera::defaultConfig();
config.pin_xclk = 21;
config.pin_pclk = 22;
config.pin_vsync = 25;
config.pin_href = 23;
config.pin_sccb_sda = 26;
config.pin_sccb_scl = 27;
config.pin_d0 = 5;   // ... through pin_d7
config.pin_pwdn = -1;   // -1 means "not connected"
config.pin_reset = -1;
```

To keep one sketch portable across both platforms, guard the preset call:

```cpp
#if defined(ESP32)
  setPinsAiThinker(config);
#else
  // RP2040: assign config.pin_* for your board here
#endif
```

## 5. Working with frames

`TinyCameraFrame` owns the driver's frame buffer and exposes it read-only:

| Method        | Returns                                        |
| ------------- | ---------------------------------------------- |
| `data()`      | Pointer to the JPEG or pixel data              |
| `size()`      | Length of `data()` in bytes                    |
| `width()`     | Frame width in pixels                          |
| `height()`    | Frame height in pixels                         |
| `format()`    | Pixel format, e.g. `PIXFORMAT_JPEG`            |
| `timestamp()` | Capture time as a `struct timeval`             |
| `raw()`       | The underlying `camera_fb_t*` for driver calls |
| `isValid()`   | Whether a frame is held (same as `operator bool`) |

Because the frame owns the buffer, it is **move-only**: it cannot be copied,
only moved. Two consequences worth knowing:

```cpp
TinyCameraFrame a = camera.captureFrame();
// TinyCameraFrame b = a;            // does not compile: copying is deleted
TinyCameraFrame b = std::move(a);    // OK: a is now empty, b owns the buffer
```

Keep the frame's scope short. With `fb_count = 1` the driver has only one
buffer, so holding a frame blocks the next capture:

```cpp
void loop() {
  {
    TinyCameraFrame frame = camera.captureFrame();
    if (frame) sendToServer(frame.data(), frame.size());
  }  // released here, before anything slow happens
  doSomethingSlow();
}
```

If you need to release earlier without leaving the scope, call
`frame.release()` explicitly. It is safe to call more than once.

Streaming a frame over the network or to a file is just a matter of writing
`data()` for `size()` bytes:

```cpp
if (auto frame = camera.captureFrame()) {
  file.write(frame.data(), frame.size());
}
```

## 6. Format conversion

`TinyCameraConvert.h` wraps the driver's `img_converters.h` functions. The
buffer-allocating conversions return a `TinyCameraBuffer`, an RAII wrapper that
`free()`s its heap buffer on destruction — exactly like `TinyCameraFrame`, and
likewise move-only.

```cpp
#include "TinyCameraConvert.h"

if (auto frame = camera.captureFrame()) {
  TinyCameraBuffer bmp = toBmp(frame);       // any format -> BMP
  if (bmp) {
    file.write(bmp.data(), bmp.size());
  }

  TinyCameraBuffer jpg = toJpg(frame, 12);   // any format -> JPEG (quality 0-63)
}
```

The raw-pixel conversions write into a buffer **you** allocate, so size it
correctly:

```cpp
// Raw (non-JPEG) frame -> interleaved RGB888, 3 bytes per pixel
std::vector<uint8_t> rgb888(frame.width() * frame.height() * 3);
if (toRgb888(frame, rgb888.data())) { /* ... */ }

// JPEG frame -> RGB565, 2 bytes per pixel
std::vector<uint8_t> rgb565(frame.width() * frame.height() * 2);
if (toRgb565(frame, rgb565.data())) { /* ... */ }
```

Notes and limits:

- `toRgb565()` accepts **JPEG frames only** and returns `false` for anything
  else.
- `toRgb888()` is for raw frames; feeding it a JPEG frame is not what you want.
- If the frame is already JPEG, use `frame.data()` / `frame.size()` directly
  instead of calling `toJpg()` — re-encoding costs time and quality.
- Every conversion returns `false` (or an invalid buffer) on failure, usually
  because the heap allocation did not fit. Always check before using the result.

The `ConvertToBmp` example shows the full flow.

### Software conversion (TinyJPEG)

`TinyCameraConvert.h`'s conversions wrap `img_converters.h`, a hardware/vendor
JPEG codec only ESP32 (`esp32-camera`) actually has - neither RP2040
(PicoCamera) nor STM32 provides one, so on those two platforms
`TinyCameraConvert.h`'s JPEG-related functions only pass through frames
already captured as JPEG (see [section 10](#10-stm32-dcmi) for STM32's
specifics; RP2040's are the same). `TinyCameraConvertSoftware.h` is a
separate, opt-in header providing the same four conversions as pure software,
backed by the [TinyJPEG](https://github.com/pschatzmann/TinyJPEG) library
(`TinyJPEGEncoder`/`TinyJPEGDecoder` - install it alongside TinyCamera; this
header does not compile without it, on purpose - see below). Unlike
`TinyCameraConvert.h`, these work identically on every platform:

```cpp
#include "TinyCameraConvertSoftware.h"

TinyCameraBuffer jpg = toJpgSoftware(frame, 80);  // RGB565/RGB888/GRAYSCALE -> JPEG, quality 1-100
TinyCameraBuffer bmp = toBmpSoftware(frame);      // JPEG -> BMP

std::vector<uint8_t> rgb565(frame.width() * frame.height() * 2);
toRgb565Software(frame, rgb565.data());           // JPEG -> RGB565

std::vector<uint8_t> rgb888(frame.width() * frame.height() * 3);
toRgb888Software(frame, rgb888.data());           // JPEG -> RGB888
```

This is what makes JPEG usable on RP2040/STM32 at all: `toJpgSoftware()`
encodes an `RGB565` capture to JPEG, and `toRgb565Software()`/
`toRgb888Software()`/`toBmpSoftware()` decode a JPEG frame back to raw
pixels or BMP - none of which `TinyCameraConvert.h` can do on either
platform (see
[Coverage and limits vs. ESP32/RP2040](#coverage-and-limits-vs-esp32rp2040)
for STM32's specifics; RP2040's are the same, minus custom-size scaling).
On ESP32 these functions still work, but prefer `TinyCameraConvert.h`'s
hardware-backed versions there: TinyJPEG has no dynamic allocation and a
small fixed workspace, which costs more CPU time than `esp32-camera`'s
own codec.

Two things worth knowing before reaching for these:

- **The TinyJPEG library is a hard dependency of this header, not an
  optional one.** Including `TinyCameraConvertSoftware.h` without it
  installed fails to compile with a `TinyJPEGEncoder.h`/`TinyJPEGDecoder.h`
  "No such file or directory" error - by design, so a missing dependency is a
  clear build error rather than a silently missing feature. Only include
  this header from sketches that actually use it.
- The decode-side functions (`toRgb565Software()`/`toRgb888Software()`/
  `toBmpSoftware()`) require a `TinyCameraFrame` whose `format()` is already
  `PIXFORMAT_JPEG` - capture with `config.pixel_format = PIXFORMAT_JPEG`
  first (not supported on STM32; see [section 10](#10-stm32-dcmi)).
  `toJpgSoftware()` is the reverse: it rejects a frame that's already JPEG.

The `ConvertSoftware` example captures RGB565 and encodes it to JPEG in
software - the RP2040/STM32-relevant case, though the example itself is
platform-generic.

## 7. Tuning the sensor

`camera.sensor()` gives access to the underlying `sensor_t` for brightness,
contrast, orientation and similar settings. It returns `nullptr` when the
camera is not active, so check it:

```cpp
if (sensor_t *s = camera.sensor()) {
  s->set_brightness(s, 1);    // -2 to 2
  s->set_contrast(s, 1);      // -2 to 2
  s->set_saturation(s, 0);    // -2 to 2
  s->set_vflip(s, 1);         // 1 = flip vertically
  s->set_hmirror(s, 1);       // 1 = mirror horizontally
}
```

These are the driver's own APIs and the exact set of supported calls depends on
your sensor model (OV2640, OV3660, OV5640, ...); unsupported settings are
simply ignored by the driver.

## 8. Managing the camera lifecycle

```cpp
camera.begin(config);   // initialize (implicitly ends a previous session)
camera.isActive();      // true between a successful begin() and end()
camera.end();           // release the driver and its buffers
```

`TinyCamera` is move-free and non-copyable — there is only one camera driver
instance on the device — and its destructor calls `end()`, so a global or
stack-scoped `TinyCamera` cleans up by itself.

Calling `begin()` again on an active camera deinitializes first, which is the
supported way to change resolution or pixel format at runtime:

```cpp
config.frame_size = FRAMESIZE_VGA;
camera.begin(config);  // re-initializes with the new settings
```

Make sure no `TinyCameraFrame` is still alive when you re-init or call `end()`,
otherwise it will return a buffer to a driver that no longer owns it.

## 9. Logging

TinyCamera has an optional printf-style logger, `TinyCameraLogger`, shared by
`TinyCamera`, `TinyCameraConvert.h` and the STM32 backend. It's silent by
default; turn it on with `begin()`:

```cpp
#include "TinyCameraLogger.h"

using namespace tiny_camera;

TinyCameraLogger.begin(TinyCameraLogLevel::Info);  // default output: Serial
```

Levels, from least to most verbose - a message prints when its own level is
`<=` the level passed to `begin()`:

| Level                       | Use                                              |
| ---------------------------- | ------------------------------------------------ |
| `TinyCameraLogLevel::None`  | Silent (the default)                             |
| `TinyCameraLogLevel::Error` | Failures: `begin()`/capture/conversion failed     |
| `TinyCameraLogLevel::Warn`  | Recoverable issues: a capture timed out, one frame dropped |
| `TinyCameraLogLevel::Info`  | Lifecycle: camera ready/released, resolution      |
| `TinyCameraLogLevel::Debug` | Per-frame and low-level detail (e.g. SCCB/sensor probing on STM32) |

`begin()` takes an optional second argument for where to send output (any
Arduino `Print`, not just `Serial`):

```cpp
TinyCameraLogger.begin(TinyCameraLogLevel::Debug, Serial1);
```

You can also log your own sketch's messages through it:

```cpp
TinyCameraLogger.error("upload failed: %d", httpCode);
TinyCameraLogger.info("frame %u sent", frameCount);
```

`TinyCameraLogger.begin(TinyCameraLogLevel::None)` turns logging back off.

## 10. STM32 (DCMI)

> **This backend is experimental.** It works, and OV7725 is verified
> end-to-end on real hardware, but it has only ever been exercised on one
> board/sensor combination (WeAct STM32H750 + OV7725), and its DCMI/SCCB
> timing needed several rounds of real-hardware debugging to get right
> even there. Treat any other board or sensor as unproven until you've
> tested it yourself - and read the pin-compatibility warning below
> before wiring a camera module to a board it wasn't sold with.

Boards with an STM32 that exposes a DCMI (Digital Camera Interface)
peripheral - such as the WeAct STM32H750 - are supported via a compatibility
layer (`TinyCameraSTM32.h`, included automatically by `TinyCamera.h` when
building for `ARDUINO_ARCH_STM32`) that implements the same
`camera_config_t` / `camera_fb_t` / `esp_camera_*` surface on top of the
STM32Cube HAL DCMI + DMA peripherals. Everything from sections 1-8 above
(capturing, frames, lifecycle) works the same way; this section covers
what's different.

### Hardware

- **Sensor:** only **OV7725** currently has a driver (`src/Driver/OV7725.h`).
  Earlier versions of this library also shipped OV2640/OV7670/OV5640
  drivers, but none of the three was ever confirmed working end-to-end on
  real hardware (OV2640/OV7670 were never hardware-tested at all; OV5640
  was tested extensively and still had an unresolved capture bug even
  after several real fixes), so they were removed rather than kept as
  false confidence. If a different, recognized sensor (OV2640/OV7670/
  OV5640) is on the SCCB bus, `begin()`'s error message names it
  specifically instead of a bare "not found" - so you know your wiring is
  fine and the gap is a missing driver, not a bug to chase. Turn on
  `TinyCameraLogger` (see [Logging](#9-logging)) to see this and other
  detection detail.
- **WeAct STM32H750 boards** have an onboard 8-bit DCMI camera connector,
  silkscreened for four sensors (OV7670/OV2640/OV7725/OV5640-AF) - only
  OV7725 of those is actually supported by this library. Use
  `setPinsWeActStm32H750()` from `TinyCameraPins.h` and skip straight to
  the [Software](#software) example below. Its pin table, DCMI/DMA
  settings and clock configuration come from WeAct's own published
  reference firmware
  ([WeActStudio/MiniSTM32H7xx](https://github.com/WeActStudio/MiniSTM32H7xx),
  `SDK/HAL/STM32H750/08-DCMI2LCD`), not a guess - confirmed end to end on
  real hardware with an OV7725 module (see the verification note below).

> **Camera connectors are not interchangeable between boards, even when
> they look identical.** Different STM32 dev boards commonly expose a
> mechanically-identical 24-pin FPC "camera connector," but each vendor
> invents its own pin ordering - there is no industry standard here.
> Plugging a camera module sold for one board straight into a different
> board's same-size connector can scramble entirely different signals
> together, including the camera module's own internal power rails - this
> has been observed to make a camera module physically hot and likely
> destroy it, not just fail to capture. **Before wiring any camera module
> to a board it wasn't sold with, verify both connectors' actual pin
> assignments against real documentation (schematic or vendor pinout) for
> that exact board - never assume "same pin count" means "compatible".**

- **Any other STM32/DCMI board:** there is no standard camera connector
  across STM32 boards in general (like RP2040), so wire the sensor
  module's 8 data lines, PCLK, VSYNC, HREF, XCLK and SCCB SDA/SCL (and
  PWDN/RESET if broken out) to any free pins and fill in
  `camera_config_t::pin_*` yourself. The data/sync pins must support your
  MCU's DCMI alternate function (AF13 on STM32F4/F7/H7) - check your
  board's datasheet/pinout for which physical pins that includes.
- **Clock:** if `pin_xclk` is `PA8`, TinyCamera drives it via `MCO1`/HSI48
  (STM32's internal 48 MHz oscillator, routed through the clock-output
  peripheral) rather than a timer - this is what `setPinsWeActStm32H750()`
  uses, matching WeAct's reference firmware's own `SystemClock_Config()`
  exactly. Any other `pin_xclk` falls back to a `HardwareTimer` PWM instead
  (the pin must be timer-capable; this fallback path is unverified). If
  your module has its own crystal/oscillator, set `config.pin_xclk = -1`
  instead.
- **D-Cache:** if your STM32 has a data cache (e.g. STM32H7's Cortex-M7)
  and your core enables it - STM32duino's does, unconditionally, before
  `setup()` even runs - `TinyCameraSTM32.h` already handles the
  cache-coherency implications for its own frame buffer (DMA writes bypass
  the cache, so the buffer is cache-line-aligned and explicitly
  cleaned/invalidated around each capture). Nothing you need to do, but if
  you see a `TinyCameraFrame`'s bytes looking correct-but-stale or
  scattered garbage in an otherwise-working capture on a D-Cache part, that
  class of bug is what to suspect first.

### Software

```cpp
#include "TinyCamera.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

TinyCamera camera;
camera_config_t config = TinyCamera::defaultConfig();
setPinsWeActStm32H750(config);  // WeAct STM32H750; fill in pin_* yourself otherwise

camera.begin(config);
```

A full example sketch is provided for a WeAct STM32H750, live-previewing
the captured frames on that board's bundled 160x80 LCD via
[TinyGPU](https://github.com/pschatzmann/TinyGPU) (install it alongside
TinyCamera; it needs it to compile):

- **`STM32DcmiCapture`**: captures a fixed `FRAMESIZE_QVGA` (320x240) RGB565
  frame and crops a centered 160x80 window out of it in software to fit
  the LCD - only shows the cropped center of the field of view, not the
  whole scene.

For requesting an exact output size directly instead of cropping, see the
platform-generic **`ScaledCapture`** example (no display/TinyGPU
dependency): it requests `custom_width`/`custom_height` (see below) via
`camera.setCustomFrameSize()`, so the sensor's own DSP scales its whole
captured field of view down to exactly that size in hardware where
supported (e.g. STM32's OV7725) - no software crop, and the full scene
ends up in the frame - falling back to software scaling
(`scaleRgb565()` in `TinyCameraConvert.h`) on platforms/sensors where
hardware scaling isn't available (e.g. RP2040).

```cpp
#include "TinyCamera.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

TinyCamera camera;
camera_config_t config = TinyCamera::defaultConfig();
setPinsWeActStm32H750(config);  // WeAct STM32H750; fill in pin_* yourself otherwise
config.pixel_format = PIXFORMAT_RGB565;

// Either a named preset (you crop to fit a smaller display in software -
// see STM32DcmiCapture):
config.frame_size = FRAMESIZE_QVGA;

// ...or an arbitrary size the sensor scales its full field of view down
// to in hardware, with a software fallback where unsupported (see
// ScaledCapture):
// config.custom_width = 160;
// config.custom_height = 80;

camera.begin(config);
```

`camera.setCustomFrameSize(width, height)` (see
[Arbitrary frame sizes](#arbitrary-frame-sizes) in section 3) is the same
underlying resizing as `custom_width`/`custom_height` above, just callable
any time after `begin()` instead of only before the first one - use
whichever fits your sketch's structure better.

For any other STM32/DCMI board without a bundled display, skip TinyGPU and
just use `camera.begin(config)` / `camera.captureFrame()` as in the earlier
sections.

### Coverage and limits vs. ESP32/RP2040

- Supported pixel formats: `PIXFORMAT_RGB565` only - OV7725 has no
  hardware JPEG encoder, so `PIXFORMAT_JPEG` isn't available on this
  backend at all right now.
- Supported frame sizes: the named presets `FRAMESIZE_QVGA` and
  `FRAMESIZE_VGA`, plus any arbitrary size up to 640x480 via
  `custom_width`/`custom_height` or `setCustomFrameSize()` (see above),
  which OV7725's DSP scales its full field of view down to in hardware.
- `fb_count` is always 1 (single buffering) and `grab_mode` is ignored.
- In `TinyCameraConvert.h`, `toJpg()` only accepts frames already captured
  as JPEG (it copies the data; `quality` is ignored), and `toBmp()` /
  `toRgb888()` / `toRgb565()` only accept `PIXFORMAT_RGB565` frames - this
  backend has no hardware JPEG codec, so none of these can produce or
  consume JPEG. For that, capture as `PIXFORMAT_RGB565` and use
  `TinyCameraConvertSoftware.h`'s `toJpgSoftware()` instead (see
  [Software conversion (TinyJPEG)](#software-conversion-tinyjpeg)) - a
  software JPEG encoder, so slower than a hardware path, but the only way
  to get JPEG off this backend at all.
- The DCMI capture engine uses `DMA1_Stream0` (matching WeAct's reference
  firmware); if your sketch needs that stream for something else, adjust it
  in `TinyCameraSTM32.h`.
- `TinyCameraSTM32.h` needs `HAL_DCMI_MODULE_ENABLED`, which STM32duino
  3.x's default HAL config leaves disabled. `src/hal_conf_extra.h` in this
  library defines it via STM32duino's own extension point for
  `stm32*_hal_conf.h` (a `hal_conf_extra.h` anywhere on the include path) -
  you don't need to do anything for this, but if you also ship your own
  `hal_conf_extra.h` or `hal_conf_custom.h`, make sure it also defines
  `HAL_DCMI_MODULE_ENABLED`.

> **What's been verified on real hardware:** this backend has been
> compiled, flashed and run end-to-end on an actual WeAct STM32H750 board
> with an OV7725 camera module (SCCB address 0x21, product ID 0x77)
> plugged into its onboard DCMI connector, capturing and displaying live
> RGB565 frames on that board's bundled LCD via `STM32DcmiCapture`, and
> the same `setCustomFrameSize()`/`custom_width`/`custom_height` hardware
> scaling path now shipped as the platform-generic `ScaledCapture`
> example - sustained runs showed zero capture
> timeouts, zero corrupt/missing pixels, and a clean, correctly-updating
> live image. Getting there took several rounds of real-hardware
> debugging beyond what a source-level port could catch: the DCMI/DMA/
> GPIO configuration, XCLK clock source (`MCO1`/HSI48, not HSE) and
> OV7725's register table all needed correcting against WeAct's actual
> reference firmware, `TinyGPU`'s `SurfaceWithExternalBuffer` needed an
> explicit `resizeBuffer()` call to actually transmit pixel data, and -
> the least obvious of the bunch - the frame buffer needed explicit
> D-Cache clean/invalidate calls around each capture, since STM32duino's
> core enables the Cortex-M7 D-Cache unconditionally and DCMI's DMA
> writes bypass it (see the D-Cache note above). This is still only one
> board/sensor combination, though - a different STM32 board, or a
> different camera module even on the same board, is unproven until you
> test it yourself, and (see the pin-compatibility warning above) should
> never be wired up without first verifying its connector's actual pin
> assignments against real documentation.

## 11. Troubleshooting

**`begin()` returns false.** Almost always pins or memory. Verify you called
the right `setPinsXxx()` for your board, then try a smaller `frame_size` or
`CAMERA_FB_IN_DRAM` with `fb_count = 1`. Some boards also need a lower
`xclk_freq_hz` (try `10000000`).

**`captureFrame()` returns an invalid frame.** With `fb_count = 1` this usually
means a previous frame is still alive. Shorten its scope or call `release()`.

**Compile error "unsupported platform".** TinyCamera supports ESP32, RP2040
and STM32 (DCMI) only; check that the right board is selected in the IDE.

**Conversion functions fail.** Check the input format (`toRgb565()` needs JPEG),
the output buffer size, and available heap — BMP output of a VGA frame is
roughly 900 KB.

**Frames are dark, upside down or mirrored.** Use `camera.sensor()` as shown in
[Tuning the sensor](#7-tuning-the-sensor); many modules are physically mounted
rotated.

### Verifying captured images

A frame's byte count and JPEG start/end markers (`0xFFD8`...`0xFFD9`) only
tell you capture *ran* - not that the picture is actually correct (right
colors, not corrupted/garbled, camera actually pointed where you think). To
check the real content, get the JPEG off the device and look at it:

```cpp
#include "base64.h"  // bundled with the ESP32 Arduino core

TinyCameraFrame frame = camera.captureFrame();
if (frame) {
  Serial.println("BEGIN_JPEG");
  Serial.println(base64::encode(frame.data(), frame.size()));
  Serial.println("END_JPEG");
}
```

Then, on the host, read the serial port, pull out everything between
`BEGIN_JPEG`/`END_JPEG`, base64-decode it, and save it as a `.jpg` file -
any text-mode serial capture avoids the binary-safety issues of dumping raw
JPEG bytes over a text-oriented Serial connection. On boards without a
convenient host-side pipeline, writing the frame to an SD card (raw bytes,
no encoding needed there) works just as well - the point is getting the
actual decoded image in front of your eyes, not just checking that
`captureFrame()` returned something.
