# Desktop / CMake

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
- V4L2 is Linux-only. macOS (AVFoundation) and Windows (Media Foundation)
  are not supported, so there the backend always uses the test pattern.

## Building with CMake

Build the examples and the tests with CMake (this fetches the Arduino
Emulator and TinyJPEG from GitHub):

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
| `TINYCAMERA_USE_TINYJPEG` | `ON` | Fetch TinyJPEG, enabling `PIXFORMAT_JPEG` and `TinyCameraConvertSoftware.h` |
| `FETCHCONTENT_SOURCE_DIR_ARDUINO_EMULATOR` | not set | Path to a local Arduino-Emulator checkout to use instead of fetching it |
| `FETCHCONTENT_SOURCE_DIR_TINYJPEG` | not set | Path to a local TinyJPEG checkout to use instead of fetching it |

To use TinyCamera from your own CMake project:

```cmake
add_subdirectory(TinyCamera)
target_link_libraries(my_app PRIVATE TinyCamera)
```
