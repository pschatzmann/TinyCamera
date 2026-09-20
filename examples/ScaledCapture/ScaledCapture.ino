// Captures RGB565 frames and produces a fixed kOutWidth x kOutHeight
// image on every supported platform (ESP32, RP2040, STM32), using
// hardware scaling where the sensor driver supports it and falling back
// to software scaling otherwise:
//
//  - TinyCamera::setCustomFrameSize() asks the sensor's own DSP to scale
//    in hardware. Support varies (see its doc comment in TinyCamera.h):
//    most ESP32 OV-series sensors and STM32's OV7725 support it, RP2040
//    (arduino-pico's Camera library) never does.
//  - If that isn't supported (returns false), this falls back to
//    scaleRgb565() from TinyCameraConvert.h, a simple nearest-neighbor
//    software resize that works identically on every platform.
//
// Adjust the pin preset in setup() for your board (see TinyCameraPins.h
// for the full list); RP2040 boards have no standard pinout and need
// config.pin_* set manually.

#include "TinyCamera.h"
#include "TinyCameraConvert.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

TinyCamera camera;

constexpr size_t kOutWidth = 160;
constexpr size_t kOutHeight = 120;

uint8_t outBuffer[kOutWidth * kOutHeight * 2];  // RGB565

bool hardwareScaled = false;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  TinyCameraLogger.begin(TinyCameraLogLevel::Info);

  camera_config_t config = TinyCamera::defaultConfig();
  config.pixel_format = PIXFORMAT_RGB565;

#if defined(ESP32)
  setPinsAiThinker(config);
#elif defined(ARDUINO_ARCH_STM32)
  setPinsWeActStm32H750(config);
#else
  // RP2040: fill in your board's pins, e.g.
  // config.pin_xclk = ...; config.pin_d0 = ...; etc.
#endif

  if (!camera.begin(config)) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial.println("Camera ready");

  // Try hardware scaling once, up front. If the sensor/platform doesn't
  // support it, every captureFrame() below returns the sensor's normal
  // frame_size instead, and loop() scales it down in software each time.
  hardwareScaled = camera.setCustomFrameSize(kOutWidth, kOutHeight);
  Serial.println(hardwareScaled
                      ? "Using hardware scaling"
                      : "Hardware scaling unsupported - using software fallback");
}

void loop() {
  TinyCameraFrame frame = camera.captureFrame();
  if (!frame) {
    Serial.println("Capture failed");
    return;
  }

  if (hardwareScaled) {
    // Frame is already kOutWidth x kOutHeight - use frame.data() directly.
    Serial.printf("Frame: %ux%u (hardware-scaled)\n", (unsigned)frame.width(),
                  (unsigned)frame.height());
  } else if (scaleRgb565(frame, outBuffer, kOutWidth, kOutHeight)) {
    Serial.printf("Frame: %ux%u -> %zux%zu (software-scaled)\n",
                  (unsigned)frame.width(), (unsigned)frame.height(),
                  kOutWidth, kOutHeight);
  } else {
    Serial.println("Software scaling failed");
  }
  // frame buffer is automatically returned here, when frame goes out of scope
}
