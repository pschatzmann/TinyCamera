// Desktop tests for TinyCamera's core API and TinyCameraConvert.h, run
// against the synthetic test pattern (see TinyCameraDesktop.h).

#include <string.h>

#include "TinyCamera.h"
#include "TinyCameraConvert.h"
#include "TinyCameraPins.h"
#include "TestUtils.h"

using namespace tiny_camera;

static void testFormats() {
  struct {
    pixformat_t format;
    size_t bytesPerPixel;
  } cases[] = {{PIXFORMAT_RGB565, 2},
               {PIXFORMAT_RGB888, 3},
               {PIXFORMAT_GRAYSCALE, 1},
               {PIXFORMAT_YUV422, 2}};
  for (auto &c : cases) {
    TinyCamera camera;
    CHECK(camera.begin(testConfig(c.format, FRAMESIZE_QVGA)));
    TinyCameraFrame frame = camera.captureFrame();
    CHECK(frame);
    CHECK(frame.width() == 320);
    CHECK(frame.height() == 240);
    CHECK(frame.format() == c.format);
    CHECK(frame.size() == 320 * 240 * c.bytesPerPixel);
  }
}

static void testFrameSizes() {
  TinyCamera camera;
  CHECK(camera.begin(testConfig(PIXFORMAT_RGB565, FRAMESIZE_VGA)));
  {
    TinyCameraFrame frame = camera.captureFrame();
    CHECK(frame.width() == 640 && frame.height() == 480);
  }
  CHECK(camera.sensor()->set_framesize(camera.sensor(), FRAMESIZE_QQVGA) == 0);
  {
    TinyCameraFrame frame = camera.captureFrame();
    CHECK(frame.width() == 160 && frame.height() == 120);
  }
  CHECK(camera.setCustomFrameSize(100, 50));
  {
    TinyCameraFrame frame = camera.captureFrame();
    CHECK(frame.width() == 100 && frame.height() == 50);
    CHECK(frame.size() == 100 * 50 * 2);
  }
  CHECK(!camera.setCustomFrameSize(0, 50));

  camera_config_t config = testConfig(PIXFORMAT_GRAYSCALE, FRAMESIZE_QVGA);
  config.custom_width = 33;
  config.custom_height = 17;
  CHECK(camera.begin(config));
  TinyCameraFrame frame = camera.captureFrame();
  CHECK(frame.width() == 33 && frame.height() == 17);
}

static void testFrameBufferLifecycle() {
  TinyCamera camera;
  CHECK(!camera.captureFrame());  // not active yet
  CHECK(camera.begin(testConfig(PIXFORMAT_RGB565, FRAMESIZE_QQVGA)));
  CHECK(camera.isActive());
  {
    TinyCameraFrame first = camera.captureFrame();
    CHECK(first);
    // Single buffering: no second frame until the first is returned.
    CHECK(!camera.captureFrame());
    TinyCameraFrame moved = std::move(first);
    CHECK(!first && moved);
  }
  TinyCameraFrame again = camera.captureFrame();
  CHECK(again);
  again.release();
  camera.end();
  CHECK(!camera.isActive());
}

static void testPatternContent() {
  TinyCamera camera;
  CHECK(camera.begin(testConfig(PIXFORMAT_RGB888, FRAMESIZE_QVGA)));
  TinyCameraFrame frame = camera.captureFrame();
  const uint8_t *px = frame.data();
  // Leftmost bar (row 0) is white, rightmost is black.
  CHECK(px[0] == 255 && px[1] == 255 && px[2] == 255);
  const uint8_t *last = px + (320 - 1) * 3;
  CHECK(last[0] == 0 && last[1] == 0 && last[2] == 0);
  // Second bar is yellow.
  const uint8_t *yellow = px + 60 * 3;
  CHECK(yellow[0] == 255 && yellow[1] == 255 && yellow[2] == 0);

  // The moving square makes consecutive frames differ.
  frame.release();
  TinyCameraFrame a = camera.captureFrame();
  uint8_t *copy = (uint8_t *)malloc(a.size());
  memcpy(copy, a.data(), a.size());
  a.release();
  TinyCameraFrame b = camera.captureFrame();
  CHECK(memcmp(copy, b.data(), b.size()) != 0);
  free(copy);
}

static void testFlip() {
  TinyCamera camera;
  CHECK(camera.begin(testConfig(PIXFORMAT_RGB888, FRAMESIZE_QVGA)));
  sensor_t *s = camera.sensor();
  CHECK(s->set_hmirror(s, 1) == 0);
  TinyCameraFrame frame = camera.captureFrame();
  const uint8_t *px = frame.data();
  CHECK(px[0] == 0 && px[1] == 0 && px[2] == 0);  // black bar now on the left
  frame.release();

  CHECK(s->set_hmirror(s, 0) == 0);
  CHECK(s->set_vflip(s, 1) == 0);
  frame = camera.captureFrame();
  // Row 0 is now the gray ramp, which starts at black.
  CHECK(frame.data()[0] == 0);
}

static void testConvert() {
  TinyCamera camera;
  CHECK(camera.begin(testConfig(PIXFORMAT_RGB565, FRAMESIZE_QVGA)));
  TinyCameraFrame frame = camera.captureFrame();

  TinyCameraBuffer bmp = toBmp(frame);
  CHECK(bmp);
  CHECK(bmp.size() == 54 + 320 * 3 * 240);
  CHECK(bmp.data()[0] == 'B' && bmp.data()[1] == 'M');

  static uint8_t rgb888[320 * 240 * 3];
  CHECK(toRgb888(frame, rgb888));
  CHECK(rgb888[0] == 255 && rgb888[1] == 255 && rgb888[2] == 255);

  static uint8_t scaled[160 * 120 * 2];
  CHECK(scaleRgb565(frame, scaled, 160, 120));
  CHECK(((uint16_t *)scaled)[0] == 0xffff);  // white

  CHECK(!toJpg(frame));  // no JPEG encoder in TinyCameraConvert.h here
}

static void testPinPresetsCompile() {
  camera_config_t config = TinyCamera::defaultConfig();
  setPinsAiThinker(config);  // accepted and ignored on the desktop
  config.source = CAMERA_SOURCE_TEST_PATTERN;
  config.test_pattern_fps = 0;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  TinyCamera camera;
  CHECK(camera.begin(config));
}

int main() {
  testFormats();
  testFrameSizes();
  testFrameBufferLifecycle();
  testPatternContent();
  testFlip();
  testConvert();
  testPinPresetsCompile();
  return testResult("test_desktop");
}
