// Captures JPEG frames from the MIPI-CSI camera (OV5647) of a Waveshare
// ESP32-P4-WIFI6 board and saves them to the TF card as
// /captures/capture_N.jpg.
// Without a TF card the frames are only captured and their size printed.
//
// The ESP32-P4 has no esp32-camera driver: TinyCamera uses the ESP_Video
// library bundled with the ESP32 Arduino core (3.3 or newer) instead - see
// TinyCameraESPVideo.h. The sensor runs at a fixed resolution (800x800 for
// the OV5647); frames are center-cropped and scaled to frame_size, then
// encoded by the P4's hardware JPEG encoder.
//
// Build with FQBN esp32:esp32:esp32p4:ChipVariant=postv3 (Arduino IDE:
// Tools > Chip Variant > "v3.00 or newer"): this board ships with a v3.x
// ESP32-P4, and the default "Before v3.00" build boot-loops.
//
// Output goes to Serial0 (UART0): that is what the board's USB-C port
// shows, not the USB CDC Serial.

#include <SD_MMC.h>
#include "TinyCamera.h"
#include "TinyCameraPins.h"

using namespace tiny_camera;

// Directory on the TF card the pictures are written to.
const char *kCaptureDir = "/captures";
TinyCamera camera;
bool sdReady = false;
int frameNo = 0;

void setup() {
  Serial0.begin(115200);
  while (!Serial0) delay(10);

  TinyCameraLogger.begin(TinyCameraLogLevel::Info, Serial0);

  camera_config_t config = TinyCamera::defaultConfig();
  setPinsWaveshareEsp32P4Wifi6(config);
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;  // 640x480, cropped from 800x800
  config.fb_count = 2;

  if (!camera.begin(config)) {
    Serial0.println("Camera init failed");
    while (true) delay(1000);
  }
  Serial0.println("Camera ready");

  // The TF card slot uses the core's default ESP32-P4 SDMMC pins; 1-bit
  // mode, since 4-bit writes fail with CRC errors on this board.
  sdReady = SD_MMC.begin("/sdcard", true);
  if (sdReady && !SD_MMC.exists(kCaptureDir) && !SD_MMC.mkdir(kCaptureDir)) {
    Serial0.printf("Failed to create %s: not saving frames\n", kCaptureDir);
    sdReady = false;
  }
  Serial0.println(sdReady ? "TF card ready" : "No TF card: not saving frames");
}

void loop() {
  TinyCameraFrame frame = camera.captureFrame();
  if (!frame) {
    Serial0.println("Capture failed");
    delay(1000);
    return;
  }

  char name[48];
  snprintf(name, sizeof(name), "%s/capture_%d.jpg", kCaptureDir, frameNo++);
  Serial0.printf("Captured frame: %u bytes (%ux%u)", (unsigned)frame.size(),
                (unsigned)frame.width(), (unsigned)frame.height());

  if (sdReady) {
    File file = SD_MMC.open(name, FILE_WRITE);
    if (file && file.write(frame.data(), frame.size()) == frame.size()) {
      Serial0.printf(" -> %s", name);
    } else {
      Serial0.print(" -> failed to write ");
      Serial0.print(name);
    }
    file.close();
  }
  Serial0.println();
  delay(2000);
  // frame buffer is automatically returned here, when frame goes out of scope
}
