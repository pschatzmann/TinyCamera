// Captures an RGB565 frame and encodes it to JPEG entirely in software,
// via TinyCameraConvertSoftware.h (backed by the TinyJPEG library:
// https://github.com/pschatzmann/TinyJPEG - install it alongside
// TinyCamera; this example will not compile without it).
//
// Unlike TinyCameraConvert.h's toJpg(), this works identically on every
// platform, including STM32 (DCMI), which has no hardware JPEG encoder
// and so can't produce JPEG at all via TinyCameraConvert.h.
//
// The same header also provides toRgb565Software()/toRgb888Software()/
// toBmpSoftware() for the opposite direction (JPEG -> raw/BMP) - useful on
// STM32 too, since TinyCameraConvert.h's toRgb565()/toRgb888()/toBmp()
// there only accept RGB565 input, not JPEG. See docs/Tutorial.md section 6.

#include "TinyCamera.h"
#include "TinyCameraConvertSoftware.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

TinyCamera camera;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  camera_config_t config = TinyCamera::defaultConfig();

#if defined(ESP32)
  setPinsAiThinker(config);
#elif defined(ARDUINO_ARCH_STM32)
  setPinsWeActStm32H750(config);
#else
  // RP2040: assign config.pin_* for your board before calling camera.begin().
#endif

  // toJpgSoftware() needs raw pixels, not JPEG - see TinyCameraConvertSoftware.h.
  config.pixel_format = PIXFORMAT_RGB565;

  if (!camera.begin(config)) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial.println("Camera ready");
}

void loop() {
  TinyCameraFrame frame = camera.captureFrame();
  if (frame) {
    TinyCameraBuffer jpg = toJpgSoftware(frame, 80);  // quality 1-100
    if (jpg) {
      Serial.printf("Software-encoded to JPEG: %u bytes (from %u bytes RGB565)\n",
                    (unsigned)jpg.size(), (unsigned)frame.size());
      // e.g. write jpg.data()/jpg.size() to a file or stream it
    } else {
      Serial.println("Software JPEG encoding failed");
    }
  } else {
    Serial.println("Capture failed");
  }
  delay(2000);
  // frame buffer is automatically returned here, when frame goes out of scope
}
