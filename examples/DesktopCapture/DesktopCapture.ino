// Desktop only (Linux/macOS, via the Arduino Emulator - see the
// "Desktop" section of README.md): captures a few RGB565 frames and saves
// the last one as capture.bmp in the current directory.
//
// Frames come from your webcam (Linux/V4L2: /dev/video0, or set the
// TINY_CAMERA_DEVICE environment variable) if one can be opened, else
// from a synthetic test pattern - see TinyCameraDesktop.h. Set
// config.source to force one or the other.
//
// Build and run:
//   cmake -B build && cmake --build build
//   ./build/examples/DesktopCapture/DesktopCapture

#include <stdio.h>
#include <stdlib.h>

#include "TinyCamera.h"
#include "TinyCameraConvert.h"

#if !defined(TINY_CAMERA_DESKTOP)
#error "DesktopCapture only runs on the desktop (Arduino Emulator)"
#endif

using namespace tiny_camera;

TinyCamera camera;

void setup() {
  Serial.begin(115200);
  TinyCameraLogger.begin(TinyCameraLogLevel::Info);

  camera_config_t config = TinyCamera::defaultConfig();
  config.pixel_format = PIXFORMAT_RGB565;  // toBmp() needs raw pixels here
  config.frame_size = FRAMESIZE_VGA;
  // config.source = CAMERA_SOURCE_TEST_PATTERN;  // or CAMERA_SOURCE_V4L2

  if (!camera.begin(config)) {
    Serial.println("Camera init failed");
    exit(1);
  }

  // Webcams usually need a few frames for auto exposure to settle.
  for (int i = 0; i < 30; i++) camera.captureFrame();

  TinyCameraFrame frame = camera.captureFrame();
  if (!frame) {
    Serial.println("Capture failed");
    exit(1);
  }
  TinyCameraBuffer bmp = toBmp(frame);
  FILE *file = fopen("capture.bmp", "wb");
  if (!bmp || file == nullptr ||
      fwrite(bmp.data(), 1, bmp.size(), file) != bmp.size()) {
    Serial.println("Writing capture.bmp failed");
    exit(1);
  }
  fclose(file);

  Serial.print("Saved capture.bmp (");
  Serial.print(frame.width());
  Serial.print("x");
  Serial.print(frame.height());
  Serial.println(")");
  exit(0);
}

void loop() {}
