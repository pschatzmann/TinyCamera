// Desktop tests for PIXFORMAT_JPEG capture and TinyCameraConvertSoftware.h
// (only built when TinyJPEG is available).

#include "TinyCamera.h"
#include "TinyCameraConvertSoftware.h"
#include "TestUtils.h"

using namespace tiny_camera;

static void testJpegCapture() {
  TinyCamera camera;
  CHECK(camera.begin(testConfig(PIXFORMAT_JPEG, FRAMESIZE_QVGA)));
  TinyCameraFrame frame = camera.captureFrame();
  CHECK(frame);
  CHECK(frame.format() == PIXFORMAT_JPEG);
  CHECK(frame.size() > 0 && frame.size() < 320 * 240 * 3);
  CHECK(frame.data()[0] == 0xff && frame.data()[1] == 0xd8);  // SOI marker

  // Decode it back and check the top-left pixel is (close to) white.
  static uint8_t rgb565[320 * 240 * 2];
  CHECK(toRgb565Software(frame, rgb565));
  uint16_t px = ((uint16_t *)rgb565)[0];
  CHECK((px >> 11) >= 28 && ((px >> 5) & 0x3f) >= 56 && (px & 0x1f) >= 28);

  TinyCameraBuffer bmp = toBmpSoftware(frame);
  CHECK(bmp && bmp.size() == 54 + 320 * 3 * 240);
}

static void testSoftwareEncode() {
  TinyCamera camera;
  CHECK(camera.begin(testConfig(PIXFORMAT_RGB565, FRAMESIZE_QVGA)));
  TinyCameraFrame frame = camera.captureFrame();
  TinyCameraBuffer jpg = toJpgSoftware(frame, 80);
  CHECK(jpg);
  CHECK(jpg.size() > 0 && jpg.data()[0] == 0xff && jpg.data()[1] == 0xd8);
}

int main() {
  testJpegCapture();
  testSoftwareEncode();
  return testResult("test_desktop_jpeg");
}
