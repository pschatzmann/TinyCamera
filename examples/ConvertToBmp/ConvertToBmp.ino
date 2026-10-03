// Captures a frame (JPEG on ESP32, RGB565 elsewhere) and converts it to BMP
// using TinyCameraConvert.h.
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
  // Off ESP32, toBmp() has no JPEG decoder - capture raw RGB565 instead
  // (or see TinyCameraConvertSoftware.h's toBmpSoftware()).
  config.pixel_format = PIXFORMAT_RGB565;
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
      Serial.print("Converted to BMP: ");
      Serial.print((unsigned)bmp.size());
      Serial.println(" bytes");
      // e.g. write bmp.data()/bmp.size() to a file or stream it
    } else {
      Serial.println("BMP conversion failed");
    }
  } else {
    Serial.println("Capture failed");
  }
  delay(2000);
}
