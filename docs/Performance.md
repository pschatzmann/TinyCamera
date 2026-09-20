# Performance

Capture throughput depends heavily on your specific sensor, `xclk_freq_hz`,
frame size/format, and memory (DRAM vs. PSRAM) - there is no single "TinyCamera
speed" number. Use the `CaptureBenchmark` example to measure it on your own
hardware rather than relying on numbers measured on different hardware.

## Measuring it yourself

The `CaptureBenchmark` example captures a batch of frames at several
`frame_size`/`pixel_format`/`fb_location` combinations and reports fps,
average bytes/frame, and MB/s for each. Flash it, open the serial monitor,
and read the results - each line is one configuration:

```
QVGA JPEG q12 (DRAM): 60/60 frames in 5405ms = 11.10 fps, avg 6.0 KB/frame, 0.07 MB/s
```

A config that needs PSRAM you don't have fails cleanly with a line like
`init failed (out of memory, or PSRAM unavailable - skipping)` - not a bug,
just that combination isn't available on your board/build.

To adapt it to your own board, edit its `kConfigs` table (frame sizes,
JPEG quality, DRAM vs. PSRAM) and swap the `setPinsXxx()` call in
`runBenchmark()` for your board's preset - see
[Assigning pins](Tutorial.md#4-assigning-pins).

## Measured results: ESP32-S3, generic camera board

**Platform: ESP32-S3.** Measured with the `CaptureBenchmark` example,
unmodified, on a generic (no-name, unlabeled) ESP32-S3 camera board -
sensor model not verified (the esp32-camera driver auto-detects it
internally; this was not checked) - using `setPinsEsp32S3Eye()` pins
(Espressif's ESP32-S3-EYE reference pinout, which this board's sensor
happened to answer to), PSRAM **disabled** in the build, and the default
`xclk_freq_hz` (20 MHz, see `TinyCamera::defaultConfig()`). 60 frames per
configuration, timed back-to-back with no delay between captures
(`grab_mode = CAMERA_GRAB_WHEN_EMPTY`).

These numbers are specific to this one ESP32-S3 board/sensor/build and
will not transfer directly to ESP32 (classic), RP2040 or STM32, or even to
a different ESP32-S3 camera board - re-run `CaptureBenchmark` on your own
hardware for numbers that apply to it.

| Configuration              | FPS   | Avg. size/frame | Throughput |
| --------------------------- | ----- | ---------------- | ---------- |
| QVGA (320x240) JPEG, q12    | 11.10 | 6.0 KB            | 0.07 MB/s  |
| VGA (640x480) JPEG, q12     | 11.10 | 23.1 KB           | 0.25 MB/s  |
| SVGA (800x600) JPEG, q12    | 11.10 | 41.6 KB           | 0.45 MB/s  |
| QVGA (320x240) RGB565 (raw) | 3.91  | 150.0 KB          | 0.57 MB/s  |
| SVGA JPEG, PSRAM, fb_count=2 | -    | -                 | failed: no PSRAM in this build |

### What this shows

- **JPEG capture rate is flat across frame sizes** (11.10 fps at QVGA, VGA
  *and* SVGA, identically). JPEG compression happens inside the sensor
  before any bytes reach the MCU, so a larger compressed frame doesn't
  mean a slower capture here - throughput is capped by the **sensor's own
  internal frame rate** (set by its clock configuration, i.e.
  `xclk_freq_hz` and the sensor's own PLL/timing setup), not by how much
  data has to move. If you need a higher frame rate than this, the first
  thing to try is raising `xclk_freq_hz` (sensor/board dependent - check
  what your specific sensor tolerates) rather than lowering resolution.
- **Raw RGB565 is a different story**: with no compression, a QVGA raw
  frame is 150 KB vs. JPEG's 6 KB at the same resolution, and here the
  bottleneck genuinely is transfer time - fps drops to well under a third
  of the JPEG rate. Prefer JPEG capture unless you specifically need raw
  pixels (e.g. for `TinyCameraConvert.h`'s `toRgb888()`/`toBmp()`, or
  further pixel-level processing).
- **PSRAM-backed configs need PSRAM actually enabled** (`Tools > PSRAM` in
  the Arduino IDE/`arduino-cli`'s board options on ESP32-S2/S3) and
  present on the board. Without it, `camera_fb_location = CAMERA_FB_IN_PSRAM`
  fails `begin()` cleanly rather than crashing - `CaptureBenchmark` reports
  this as "skipping", and TinyCamera's own logger (see
  [Logging](Tutorial.md#9-logging)) reports it as an error if enabled.

### What determines your numbers

None of the above is a universal TinyCamera limit - all of these are
degrees of freedom that change the result:

- **Sensor and its clock config** (`xclk_freq_hz`, sensor's own PLL
  settings) - the single biggest factor for JPEG fps, per the flat-rate
  result above.
- **PSRAM availability and `fb_count`** - double-buffering (`fb_count = 2`)
  needs PSRAM on most ESP32 variants and lets capture overlap with your
  processing, but needs the memory to back it.
- **JPEG quality** (`jpeg_quality`) - lower numbers (less compression, most
  detail) mean larger frames; whether that affects fps depends on whether
  you're sensor-clock-limited (as above) or transfer-limited.
- **Platform/bus**: ESP32's I2S-based DMA capture, RP2040's PIO-based
  capture, and STM32's DCMI+DMA path (see the
  [STM32 section](Tutorial.md#10-stm32-dcmi)) all have different
  achievable throughput ceilings.

If you get numbers that seem off for your hardware, run
`CaptureBenchmark` again after changing one variable at a time (quality,
frame size, `xclk_freq_hz`, PSRAM) to see which one actually moves the
number - guessing from a table measured on different hardware won't tell
you much.
