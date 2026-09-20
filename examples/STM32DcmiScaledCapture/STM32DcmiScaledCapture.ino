// Like STM32DcmiCapture, but instead of capturing the sensor's full QVGA
// (320x240) frame and cropping a 160x80 center window in software
// (losing the rest of the field of view), this requests 160x80 directly
// via camera_config_t's custom_width/custom_height - the sensor's own
// DSP scale/zoom engine downscales its full captured scene to exactly
// that size in hardware, so the whole field of view ends up on the LCD,
// not just a cropped center slice of it.
//
// Requires OV7725 (see Driver/OV7725.h's configure()) - currently the
// only sensor driver this library ships for the STM32 backend (see
// Driver/SensorDriver.h's header comment).
//
// custom_width/custom_height (set before begin(), as below) and
// TinyCamera::setCustomFrameSize() (callable any time after begin()) are
// two equivalent ways to reach the same underlying resizing - this
// example uses the config-field form since that's what's actually been
// tested on real hardware; see docs/Tutorial.md's "Arbitrary frame
// sizes" section for the method form.

#include <TinyGPU.h>
#include <TinyGPU/Boards/LCDBoards.h>

#include "TinyCamera.h"
#include "TinyCameraLogger.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;
using namespace tinygpu;

TinyCamera camera;
MiniSTM32H750 lcd;

constexpr size_t kLcdWidth = 160;
constexpr size_t kLcdHeight = 80;

SurfaceWithExternalBuffer<RGB565> lcdView(kLcdWidth, kLcdHeight);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  TinyCameraLogger.begin(TinyCameraLogLevel::Info);  // use ::Debug for more detail

  camera_config_t config = TinyCamera::defaultConfig();
  setPinsWeActStm32H750(config);
  config.pixel_format = PIXFORMAT_RGB565;  // TinyGPU needs raw pixels, not JPEG
  config.custom_width = kLcdWidth;         // scaled in hardware to exactly
  config.custom_height = kLcdHeight;       // the LCD's size - see this
                                            // file's header comment

  if (!camera.begin(config)) {
    Serial.println("Camera init failed (only OV7725 supports custom_width/"
                    "custom_height - see this file's header comment)");
    while (true) delay(1000);
  }
  Serial.println("Camera ready");

  if (!lcd.begin()) {
    Serial.println("LCD init failed");
    while (true) delay(1000);
  }
  Serial.println("LCD ready");
}

void loop() {
  TinyCameraFrame frame = camera.captureFrame();
  if (frame) {
    // frame is already exactly kLcdWidth x kLcdHeight - no crop needed.
    lcdView.setExternalBuffer((void *)frame.data(), frame.size());
    lcdView.resizeBuffer(kLcdWidth, kLcdHeight);
    lcd.display().writeData(lcdView);
  } else {
    Serial.println("Capture failed");
  }
  // frame buffer is automatically returned here, when frame goes out of scope
}
