// Measures sustained capture throughput (fps, KB/frame, MB/s) across a
// few frame_size/pixel_format combinations, so you can see the actual
// tradeoffs on your specific board rather than guessing.
//
// Real numbers measured on a generic ESP32-S3 camera board (OV2640,
// setPinsEsp32S3Eye() pins, PSRAM disabled, default 20MHz xclk_freq_hz):
//   QVGA/VGA/SVGA JPEG (q12): ~11.1 fps, flat across all three sizes -
//     capped by the sensor's own internal frame rate, not transfer time
//     (JPEG compression happens in the sensor before the bytes reach the
//     MCU, so a bigger compressed frame doesn't mean a slower capture).
//   QVGA RGB565 (uncompressed): ~4.0 fps - here the bottleneck is real:
//     150 KB/frame has to move over DMA/I2S with no compression helping.
// Your own numbers will differ with a different sensor, xclk_freq_hz,
// PSRAM availability, or fb_count - that's the point of measuring on your
// actual hardware instead of assuming.
//
// CAMERA_FB_IN_PSRAM configs are skipped unless PSRAM is actually
// available (enable it in Tools > PSRAM on ESP32-S3/S2 boards) - without
// it they fail cleanly with a "frame buffer malloc failed" error, which
// this sketch treats as "not available" rather than a bug.

#include "TinyCamera.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

TinyCamera camera;

struct BenchConfig {
  const char *name;
  framesize_t frameSize;
  pixformat_t pixelFormat;
  int jpegQuality;
  camera_fb_location_t fbLocation;
};

const BenchConfig kConfigs[] = {
    {"QVGA JPEG q12 (DRAM)", FRAMESIZE_QVGA, PIXFORMAT_JPEG, 12, CAMERA_FB_IN_DRAM},
    {"VGA JPEG q12 (DRAM)", FRAMESIZE_VGA, PIXFORMAT_JPEG, 12, CAMERA_FB_IN_DRAM},
    {"SVGA JPEG q12 (DRAM)", FRAMESIZE_SVGA, PIXFORMAT_JPEG, 12, CAMERA_FB_IN_DRAM},
    {"QVGA RGB565 (DRAM)", FRAMESIZE_QVGA, PIXFORMAT_RGB565, 12, CAMERA_FB_IN_DRAM},
    // Larger/PSRAM-backed configs - skipped automatically if PSRAM isn't
    // available on your board/build.
    {"SVGA JPEG q12 (PSRAM, fb=2)", FRAMESIZE_SVGA, PIXFORMAT_JPEG, 12, CAMERA_FB_IN_PSRAM},
};
constexpr int kNumConfigs = sizeof(kConfigs) / sizeof(kConfigs[0]);
constexpr int kFramesPerBench = 60;
constexpr int kWarmupFrames = 5;

void runBenchmark(const BenchConfig &bc) {
  camera_config_t config = TinyCamera::defaultConfig();

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  setPinsEsp32S3Eye(config);  // swap for your board's setPinsXxx() preset
#elif defined(ESP32)
  setPinsAiThinker(config);  // swap for your board's setPinsXxx() preset
#else
  // RP2040/STM32: assign config.pin_* for your board here (see
  // docs/Tutorial.md), and CAMERA_FB_IN_PSRAM won't apply on STM32.
#endif

  config.frame_size = bc.frameSize;
  config.pixel_format = bc.pixelFormat;
  config.jpeg_quality = bc.jpegQuality;
  config.fb_location = bc.fbLocation;
  config.fb_count = (bc.fbLocation == CAMERA_FB_IN_PSRAM) ? 2 : 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  Serial.print(bc.name);
  Serial.print(": ");

  if (!camera.begin(config)) {
    Serial.println("init failed (out of memory, or PSRAM unavailable - skipping)");
    return;
  }

  for (int i = 0; i < kWarmupFrames; i++) camera.captureFrame();  // let AEC/AGC settle

  uint32_t totalBytes = 0;
  uint32_t start = millis();
  int captured = 0;
  for (int i = 0; i < kFramesPerBench; i++) {
    TinyCameraFrame frame = camera.captureFrame();
    if (frame) {
      totalBytes += frame.size();
      captured++;
    }
  }
  uint32_t elapsedMs = millis() - start;
  camera.end();

  float fps = elapsedMs > 0 ? (captured * 1000.0f / elapsedMs) : 0;
  float kbPerFrame = captured > 0 ? (totalBytes / 1024.0f / captured) : 0;
  float mbPerSec = elapsedMs > 0
                       ? (totalBytes / 1024.0f / 1024.0f) / (elapsedMs / 1000.0f)
                       : 0;

  Serial.print(captured);
  Serial.print("/");
  Serial.print(kFramesPerBench);
  Serial.print(" frames in ");
  Serial.print(elapsedMs);
  Serial.print("ms = ");
  Serial.print(fps, 2);
  Serial.print(" fps, avg ");
  Serial.print(kbPerFrame, 1);
  Serial.print(" KB/frame, ");
  Serial.print(mbPerSec, 2);
  Serial.println(" MB/s");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  delay(1000);  // extra settle time for native-USB boards' serial monitor

  Serial.println("Benchmarking capture throughput...");
  for (int i = 0; i < kNumConfigs; i++) {
    runBenchmark(kConfigs[i]);
    delay(200);
  }
  Serial.println("done");
}

void loop() { delay(1000); }
