// Captures RGB565 frames from a camera module wired to a WeAct STM32H750
// board's onboard DCMI camera connector, and live-previews them on that
// same board's bundled 160x80 ST7735 LCD via the TinyGPU library
// (https://github.com/pschatzmann/TinyGPU - install it alongside
// TinyCamera; this example will not compile without it).
//
// setPinsWeActStm32H750() fills in that board's verified DCMI pin mapping,
// and TinyGPU's MiniSTM32H750 board class drives its onboard LCD - both
// sourced from real hardware/reference firmware, see docs/Tutorial.md.
// The sensor itself is auto-detected (see TinyCameraSTM32.h), though only
// OV7725 currently ships a working driver - see Driver/SensorDriver.h's
// header comment for why OV2640/OV7670/OV5640 were removed.
//
// Frame size is FRAMESIZE_QVGA (320x240). Since the LCD is only 160x80,
// cropCenterToLcd() below copies a centered 160x80 window out of the
// 320x240 frame each time - not a zero-copy view like a same-sized frame
// would allow, but a small, simple copy either way.

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
uint16_t lcdBuffer[kLcdWidth * kLcdHeight];

// Copies a centered kLcdWidth x kLcdHeight window out of a wider/taller
// source frame, row by row (the two widths differ, so this can't just be
// a flat memcpy/external-buffer view the way a same-sized frame could).
void cropCenterToLcd(const TinyCameraFrame &frame) {
  size_t xOffset = (frame.width() - kLcdWidth) / 2;
  size_t yOffset = (frame.height() - kLcdHeight) / 2;
  const uint16_t *src = (const uint16_t *)frame.data();
  for (size_t y = 0; y < kLcdHeight; y++) {
    const uint16_t *srcRow = src + (y + yOffset) * frame.width() + xOffset;
    memcpy(lcdBuffer + y * kLcdWidth, srcRow, kLcdWidth * sizeof(uint16_t));
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  TinyCameraLogger.begin(TinyCameraLogLevel::Info);  // use ::Debug for more detail

  camera_config_t config = TinyCamera::defaultConfig();
  setPinsWeActStm32H750(config);  // also sets xclk_freq_hz = 12MHz, matching
                                   // this board's fixed clock architecture
  config.pixel_format = PIXFORMAT_RGB565;  // TinyGPU needs raw pixels, not JPEG
  config.frame_size = FRAMESIZE_QVGA;      // widest size every supported
                                            // sensor's RGB565 mode covers

  if (!camera.begin(config)) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial.println("Camera ready");

  if (!lcd.begin()) {
    Serial.println("LCD init failed");
    while (true) delay(1000);
  }
  Serial.println("LCD ready");

  // lcdBuffer's pointer/size never change across frames, so this only
  // needs doing once here, not per captureFrame(). setExternalBuffer()
  // alone is not enough: it sets the surface's buffer pointer/capacity
  // but not its buffer_size (only resizeBuffer() does that) - and
  // writeData() sends surface.size() (== buffer_size) bytes of pixel
  // data, so skipping this call leaves buffer_size at its default of 0
  // and writeData() sends the CASET/RASET/RAMWR address-window commands
  // but zero pixel bytes, silently leaving the panel's existing GRAM
  // contents on screen forever.
  lcdView.setExternalBuffer(lcdBuffer, sizeof(lcdBuffer));
  lcdView.resizeBuffer(kLcdWidth, kLcdHeight);
}

void loop() {
  TinyCameraFrame frame = camera.captureFrame();
  if (frame) {
    cropCenterToLcd(frame);
    lcd.display().writeData(lcdView);
  } else {
    Serial.println("Capture failed");
  }
  // frame buffer is automatically returned here, when frame goes out of scope
}
