// Captures a single JPEG frame and prints its size.
// ESP32: tested with an AI-Thinker ESP32-CAM module.
// RP2040: assign config.pin_* for your board before calling camera.begin().

#include "TinyCamera.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

TinyCamera camera;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  camera_config_t config = TinyCamera::defaultConfig();

#if defined(ESP32)
  setPinsAiThinker(config);
#else
  // RP2040: fill in your board's pins, e.g.
  // config.pin_xclk = ...; config.pin_d0 = ...; etc.
#endif

  if (!camera.begin(config)) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial.println("Camera ready");
}

void loop() {
  TinyCameraFrame frame = camera.captureFrame();
  if (frame) {
    Serial.printf("Captured frame: %u bytes (%ux%u)\n",
                  (unsigned)frame.size(), (unsigned)frame.width(),
                  (unsigned)frame.height());
  } else {
    Serial.println("Capture failed");
  }
  delay(2000);
  // frame buffer is automatically returned here, when frame goes out of scope
}
