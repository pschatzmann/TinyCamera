# Desktop / CMake

TinyCamera also runs on a desktop computer (Linux, macOS and Windows), so
you can develop and test frame-processing code without flashing a board.
The desktop backend (`TinyCameraDesktop.h`, selected automatically by
`TinyCamera.h`) gets its frames from one of these sources
(`camera_config_t::source`):

| Source | Platforms | Needs | Description |
| --- | --- | --- | --- |
| `CAMERA_SOURCE_SDL` | Linux, macOS, Windows | SDL3 | Your webcam, through [SDL3](https://www.libsdl.org/)'s camera API |
| `CAMERA_SOURCE_V4L2` | Linux | nothing | Your webcam, through the Linux kernel's V4L2 interface directly |
| `CAMERA_SOURCE_TEST_PATTERN` | all | nothing | A synthetic, animated test image (color bars) |
| `CAMERA_SOURCE_AUTO` (default) | all | | The first of SDL, V4L2 that can open a webcam, else the test pattern |

`camera_config_t::device` (or the `TINY_CAMERA_DEVICE` environment
variable) selects the webcam: for SDL a camera index (`0`, `1`, ...) or
part of its name (e.g. `FaceTime`), for V4L2 a device path (default
`/dev/video0`). Without it, SDL uses the first camera it finds.

It supports RGB565, RGB888, GRAYSCALE and YUV422 frames, plus JPEG when
[TinyJPEG](https://github.com/pschatzmann/TinyJPEG) is available, at every
`FRAMESIZE_*` size and at any custom size (`setCustomFrameSize()`). Pin
settings are accepted and ignored, so the sketches compile unchanged.

## Plain C++ or Arduino sketches

The library itself is plain C++17 and needs no Arduino core, so
you can use it in an ordinary C++ program with its own `main()`:

```cpp
#include "TinyCamera.h"
using namespace tiny_camera;

int main() {
  TinyCameraLogger.begin(TinyCameraLogLevel::Info);  // logs to stderr
  camera_config_t config = TinyCamera::defaultConfig();
  config.pixel_format = PIXFORMAT_RGB565;
  TinyCamera camera;
  if (!camera.begin(config)) return 1;
  TinyCameraFrame frame = camera.captureFrame();
  // use frame.data() / frame.size() / frame.width() / frame.height()
}
```

The example sketches are written for Arduino (`setup()`/`loop()`,
`Serial`), so on the desktop they run with the
[Arduino Emulator](https://github.com/pschatzmann/Arduino-Emulator). It
provides the `main()` that calls `setup()` and then `loop()` forever, and a
`Serial` that writes to the terminal. The CMake build fetches it only when
the examples are built.

`TinyCameraLogger` writes to an Arduino `Print` (default: `Serial`)
whenever `Arduino.h` is available, i.e. on every board and with the
emulator. Without it, `begin()` takes a `FILE &` instead, and the default
is `stderr`.

## Webcam capture (SDL3)

[SDL](https://www.libsdl.org/) is a widely used cross-platform library
for multimedia. Its camera API (SDL 3.2 and later) uses each operating
system's native camera system: V4L2 or PipeWire on Linux, AVFoundation on
macOS and Media Foundation on Windows. TinyCamera asks SDL for RGB frames
at the size you requested; SDL converts (and if necessary scales) the
camera's own format, so any webcam the OS supports works.

The CMake build uses SDL3 by default: an installed SDL3 if CMake finds
one (e.g. from your package manager, Homebrew or vcpkg), else it
downloads and builds SDL3 as a static library, which takes about a minute
the first time. Turn it off with `-DTINYCAMERA_USE_SDL3=OFF`. Outside the
CMake build, define `TINY_CAMERA_USE_SDL3` and link SDL3 yourself.

Notes:

- macOS asks for permission the first time a program uses the camera.
  For a program run from a terminal, the permission belongs to the
  terminal app (System Settings > Privacy & Security > Camera). TinyCamera
  waits up to 60 seconds for an answer, and fails if access is denied.
- The macOS and Windows support comes from SDL; TinyCamera's SDL source
  has only been tested on Linux so far.

## Webcam capture (V4L2)

V4L2 (Video4Linux2) is the standard Linux kernel interface for video
capture devices such as webcams, USB capture cards and camera interfaces.
Each camera appears as a device file (`/dev/video0`, `/dev/video1`, ...).
A program opens that file, uses `ioctl()` calls to ask what the device
supports and to choose a format and resolution, then streams frames
through buffers it shares with the driver (`mmap`). Most USB webcams
follow the UVC standard, so they work with the generic `uvcvideo` driver
without anything extra to install.

TinyCamera's desktop backend uses V4L2 like this:

1. It opens the device: `camera_config_t::device` if set, else the
   `TINY_CAMERA_DEVICE` environment variable, else `/dev/video0`.
2. It requests frames in YUYV (YUV 4:2:2), a raw format practically every
   webcam supports. If the webcam can't deliver the exact size you asked
   for, it uses the nearest size it supports.
3. It converts each frame to RGB and scales it to the size you requested,
   so the frame size always matches your configuration.
4. It converts the result to the configured `pixel_format`, so a webcam
   behaves like a camera module on an ESP32.

Notes:

- Your user needs read/write access to the device. On most distributions
  the logged-in user has it already; otherwise add yourself to the
  `video` group.
- To list the formats and resolutions your camera supports, install
  `v4l-utils` and run `v4l2-ctl --list-formats-ext`.
- Webcams need a few frames for auto exposure to settle, so the first
  frames may be dark. The `DesktopCapture` example skips 30 frames before
  saving one.
- V4L2 is Linux-only. On macOS and Windows, use the SDL3 source.
- Use `CAMERA_SOURCE_V4L2` when you want a Linux build without any
  dependencies (`-DTINYCAMERA_USE_SDL3=OFF`).

## Building with CMake

Build the examples and the tests with CMake (this fetches TinyJPEG, SDL3
if it isn't installed, and for the examples the Arduino Emulator, from
GitHub):

```
cmake -B build
cmake --build build
ctest --test-dir build
./build/examples/DesktopCapture/DesktopCapture   # saves capture.bmp
```

Options (pass as `-D<option>=<value>` to the first `cmake` call):

| Option | Default | Description |
| --- | --- | --- |
| `TINYCAMERA_BUILD_EXAMPLES` | `ON` when TinyCamera is the top-level project, else `OFF` | Build the example sketches for the desktop |
| `TINYCAMERA_BUILD_TESTS` | `ON` when TinyCamera is the top-level project, else `OFF` | Build the desktop test suite (`test/desktop/`) |
| `TINYCAMERA_USE_SDL3` | `ON` | Use SDL3 for webcam capture on every platform (installed, else fetched and built) |
| `TINYCAMERA_USE_TINYJPEG` | `ON` | Fetch TinyJPEG, enabling `PIXFORMAT_JPEG` and `TinyCameraConvertSoftware.h` |
| `FETCHCONTENT_SOURCE_DIR_ARDUINO_EMULATOR` | not set | Path to a local Arduino-Emulator checkout to use instead of fetching it (only used for the examples) |
| `FETCHCONTENT_SOURCE_DIR_TINYJPEG` | not set | Path to a local TinyJPEG checkout to use instead of fetching it |
| `FETCHCONTENT_SOURCE_DIR_SDL3` | not set | Path to a local SDL source checkout to build instead of fetching it |

To use TinyCamera from your own CMake project (this doesn't need the
Arduino Emulator):

```cmake
add_subdirectory(TinyCamera)
target_link_libraries(my_app PRIVATE TinyCamera)
```
