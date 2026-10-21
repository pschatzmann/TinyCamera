// Captures RGB565 frames through the ESP_Video backend (TinyCameraESPVideo.h)
// and prints their size - the same sketch on both interfaces it supports:
//  - ESP32-P4: MIPI-CSI camera, e.g. on a Waveshare ESP32-P4-WIFI6. The P4
//    has no esp32-camera driver, so ESP_Video is used automatically.
//  - ESP32-S3: DVP camera, e.g. an ESP32-S3-EYE. esp32-camera is the
//    default there: TINY_CAMERA_USE_ESP_VIDEO below opts in to ESP_Video.
// The interface is picked from the pins: DVP when pin_d0... are set, else
// MIPI-CSI.

#define TINY_CAMERA_USE_ESP_VIDEO

#include "TinyCamera.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

// On the Waveshare ESP32-P4-WIFI6 the output shows up on UART0 (Serial0),
// not on the USB CDC Serial.
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define Console Serial0
#else
#define Console Serial
#endif

TinyCamera camera;

void setup() {
  Console.begin(115200);
  while (!Console) delay(10);

  TinyCameraLogger.begin(TinyCameraLogLevel::Info, Console);

  camera_config_t config = TinyCamera::defaultConfig();
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  setPinsWaveshareEsp32P4Wifi6(config);  // MIPI-CSI: SCCB pins only
#else
  setPinsEsp32S3Eye(config);  // DVP
#endif
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;  // cropped/scaled from the sensor's size
  config.fb_count = 2;

  if (!camera.begin(config)) {
    Console.println("Camera init failed");
    while (true) delay(1000);
  }
  Console.println("Camera ready");
}

void loop() {
  uint32_t start = millis();
  TinyCameraFrame frame = camera.captureFrame();
  if (frame) {
    Console.printf("Captured frame: %u bytes (%ux%u) in %u ms\n",
                  (unsigned)frame.size(), (unsigned)frame.width(),
                  (unsigned)frame.height(), (unsigned)(millis() - start));
  } else {
    Console.println("Capture failed");
  }
  delay(1000);
  // frame buffer is automatically returned here, when frame goes out of scope
}
