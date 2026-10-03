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
