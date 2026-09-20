// Captures a JPEG frame and converts it to BMP using TinyCameraConvert.h.
// ESP32: tested with an AI-Thinker ESP32-CAM module.
// RP2040: assign config.pin_* for your board before calling camera.begin().

#include "TinyCamera.h"
#include "TinyCameraConvert.h"
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
    TinyCameraBuffer bmp = toBmp(frame);
    if (bmp) {
      Serial.printf("Converted to BMP: %u bytes\n", (unsigned)bmp.size());
      // e.g. write bmp.data()/bmp.size() to a file or stream it
    } else {
      Serial.println("BMP conversion failed");
    }
  } else {
    Serial.println("Capture failed");
  }
  delay(2000);
}
