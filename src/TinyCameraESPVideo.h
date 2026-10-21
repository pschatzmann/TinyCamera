#pragma once
/**
 * ESP_Video (esp_video / V4L2) compatibility layer for TinyCamera:
 * MIPI-CSI and DVP cameras on ESP32 variants where the ESP32 Arduino core
 * (3.3+) ships Espressif's esp_video stack and its ESP_Video Arduino
 * library.
 *
 * This file bridges ESP_Video to the same camera_config_t / camera_fb_t /
 * esp_camera_* surface that esp_camera.h exposes, so TinyCamera.h,
 * TinyCameraConvert.h and TinyCameraConvertSoftware.h work unmodified on
 * top of it - the same approach TinyCameraRP2040.h takes for PicoCamera.
 *
 * Which chip, which interface (arduino-esp32 3.3.12):
 *  - ESP32-P4: MIPI-CSI (through the ISP) and DVP. The P4 has no
 *    esp32-camera driver, so TinyCamera.h uses this backend automatically.
 *  - ESP32-S3: DVP only (the S3 has no MIPI-CSI peripheral). esp32-camera
 *    remains the default there; #define TINY_CAMERA_USE_ESP_VIDEO before
 *    including TinyCamera.h to use this backend instead.
 *  - Any other chip whose core enables CONFIG_ESP_VIDEO_ENABLE_*_VIDEO_
 *    DEVICE, the same way.
 * The interface is picked from camera_config_t: DVP when the data pins
 * (pin_d0...) are set, else MIPI-CSI (whose lanes are fixed pins, so only
 * the SCCB pins and the optional reset/power-down pins are used).
 *
 * The sensor is auto-detected over SCCB (I2C) by esp_video - whichever
 * sensors the core's sdkconfig enables (MIPI-CSI: OV5647, SC2336, ...;
 * DVP: OV2640, OV3660, OV5640, GC0308, GC2145, ...).
 *
 * Verified on real hardware: MIPI-CSI on a Waveshare ESP32-P4-WIFI6 with
 * its OV5647 module (arduino-esp32 3.3.12, examples/
 * WaveshareESP32P4Capture). DVP (ESP32-S3, ESP32-P4) is only compiled -
 * treat it as unproven until you've tested it yourself.
 *
 * Coverage / limitations (see docs/Tutorial.md):
 *  - Each sensor runs at one fixed resolution chosen by the core's
 *    sdkconfig (e.g. OV5647: 800x800, SC2336: 1280x720, OV2640:
 *    640x480), not per frame_size. Frames are therefore center-cropped to
 *    the requested aspect ratio and scaled (nearest neighbor) in software
 *    to frame_size, or to custom_width/custom_height when both are set -
 *    so the output size always matches what was asked for. Set
 *    native_frame_size to get the sensor's own resolution instead, with
 *    no copy (the frame buffer is then the driver's own V4L2 buffer).
 *  - Pixel formats: RGB565 (native-endian uint16_t per pixel, as the
 *    software converters in TinyCameraConvert.h expect), RGB888,
 *    GRAYSCALE, YUV422 (YUYV) and RAW (8-bit Bayer) - as far as the
 *    sensor/interface provides them. If RGB565 is not available (common
 *    for DVP sensors, which often only output YUYV), YUYV frames are
 *    converted to RGB565 in software. Only RGB565, RGB888 and GRAYSCALE
 *    can be scaled; YUV422/RAW frames always come at the native size.
 *  - PIXFORMAT_JPEG: on chips with a hardware JPEG encoder (ESP32-P4)
 *    frames are captured as RGB565, scaled, and encoded by it. Elsewhere
 *    the sensor's own JPEG output is used, at its native size - if it has
 *    none, capture RGB565 and use TinyCameraConvertSoftware.h instead.
 *  - fb_count is the number of V4L2 capture buffers (at least 2);
 *    fb_location and grab_mode are ignored. Only one frame buffer can be
 *    held at a time.
 *  - sensor_t: set_vflip/set_hmirror are applied by the sensor;
 *    set_framesize/set_pixformat/set_quality reconfigure the output;
 *    set_brightness/set_contrast/set_saturation are accepted and recorded
 *    in sensor_t::status but have no effect on the image.
 */

#include <Arduino.h>
#include <ESP_Video.h>
#include <string.h>
#include <sys/time.h>

#include "driver/ledc.h"  // LEDC_CHANNEL_0/LEDC_TIMER_0 for defaultConfig()
#include "esp_heap_caps.h"
#include "sdkconfig.h"
#if CONFIG_SOC_JPEG_ENCODE_SUPPORTED
#include "driver/jpeg_encode.h"
#define TINY_CAMERA_ESP_VIDEO_HW_JPEG 1
#endif

#if !CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE && \
    !CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
#error "TinyCamera: this chip's ESP32 core has no esp_video MIPI-CSI or DVP support"
#endif

/// Defined whenever the ESP_Video backend is active - test for it in
/// sketches the same way as ESP32/ARDUINO_ARCH_STM32. Note that ESP32 is
/// defined as well.
#define TINY_CAMERA_ESP_VIDEO 1

#include "TinyCameraLogger.h"

// ---------------------------------------------------------------------------
// Public compatibility surface (global scope, mirroring esp_camera.h's C API)
// ---------------------------------------------------------------------------

// Same values as esp32-camera's pixformat_t (minus the formats esp_video
// can't produce).
enum pixformat_t {
  PIXFORMAT_RGB565,
  PIXFORMAT_YUV422,
  PIXFORMAT_GRAYSCALE = 3,
  PIXFORMAT_JPEG,
  PIXFORMAT_RGB888,
  PIXFORMAT_RAW,
};

// Same order and sizes as esp32-camera's framesize_t.
enum framesize_t {
  FRAMESIZE_96X96,    // 96x96
  FRAMESIZE_QQVGA,    // 160x120
  FRAMESIZE_128X128,  // 128x128
  FRAMESIZE_QCIF,     // 176x144
  FRAMESIZE_HQVGA,    // 240x176
  FRAMESIZE_240X240,  // 240x240
  FRAMESIZE_QVGA,     // 320x240
  FRAMESIZE_320X320,  // 320x320
  FRAMESIZE_CIF,      // 400x296
  FRAMESIZE_HVGA,     // 480x320
  FRAMESIZE_VGA,      // 640x480
  FRAMESIZE_SVGA,     // 800x600
  FRAMESIZE_XGA,      // 1024x768
  FRAMESIZE_HD,       // 1280x720
  FRAMESIZE_SXGA,     // 1280x1024
  FRAMESIZE_UXGA,     // 1600x1200
  FRAMESIZE_FHD,      // 1920x1080
  FRAMESIZE_INVALID
};

struct resolution_info_t {
  uint16_t width;
  uint16_t height;
};

/// Width/height of each framesize_t, indexed by it - same name and role as
/// esp32-camera's own resolution[] table.
inline const resolution_info_t resolution[FRAMESIZE_INVALID] = {
    {96, 96},   {160, 120},  {128, 128},  {176, 144},  {240, 176},
    {240, 240}, {320, 240},  {320, 320},  {400, 296},  {480, 320},
    {640, 480}, {800, 600},  {1024, 768}, {1280, 720}, {1280, 1024},
    {1600, 1200}, {1920, 1080},
};

enum camera_fb_location_t { CAMERA_FB_IN_DRAM, CAMERA_FB_IN_PSRAM };
enum camera_grab_mode_t { CAMERA_GRAB_WHEN_EMPTY, CAMERA_GRAB_LATEST };

struct camera_config_t {
  int ledc_channel = 0;  // unused; kept for source compatibility
  int ledc_timer = 0;    // unused; kept for source compatibility
  int xclk_freq_hz = 20000000;  // DVP only: MIPI sensors are clocked on-module
  pixformat_t pixel_format = PIXFORMAT_JPEG;
  framesize_t frame_size = FRAMESIZE_QVGA;
  // When both are >0, requests this exact output resolution instead of
  // frame_size's fixed preset (same fields as on STM32/desktop).
  int custom_width = 0;
  int custom_height = 0;
  int jpeg_quality = 12;  // esp32-camera scale: 0-63, lower is better
  int fb_count = 2;       // V4L2 capture buffers, at least 2
  camera_fb_location_t fb_location = CAMERA_FB_IN_PSRAM;  // unused
  camera_grab_mode_t grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // unused

  // ESP_Video-only fields.
  // Deliver frames at the sensor's own resolution (no crop/scale, no
  // copy), ignoring frame_size/custom_width/custom_height.
  bool native_frame_size = false;
  // I2C port for the SCCB bus, and its clock.
  int sccb_i2c_port = 0;
  uint32_t sccb_i2c_freq = 100000;
  // An already-initialized I2C master bus to share (e.g. one also used by
  // an audio codec on the same pins); nullptr: create one on
  // sccb_i2c_port/pin_sccb_sda/pin_sccb_scl.
  i2c_master_bus_handle_t sccb_i2c_bus = nullptr;

  int pin_pwdn = -1;
  int pin_reset = -1;
  int pin_xclk = -1;
  int pin_sccb_sda = -1;
  int pin_sccb_scl = -1;
  // DVP data/sync pins: leave pin_d0 at -1 to use MIPI-CSI.
  int pin_d0 = -1, pin_d1 = -1, pin_d2 = -1, pin_d3 = -1;
  int pin_d4 = -1, pin_d5 = -1, pin_d6 = -1, pin_d7 = -1;
  int pin_vsync = -1;
  int pin_href = -1;
  int pin_pclk = -1;
};

struct camera_fb_t {
  uint8_t *buf = nullptr;
  size_t len = 0;
  size_t width = 0;
  size_t height = 0;
  pixformat_t format = PIXFORMAT_JPEG;
  struct timeval timestamp {};
};

struct camera_status_t {
  framesize_t framesize = FRAMESIZE_QVGA;
  int quality = 12;
  int brightness = 0;
  int contrast = 0;
  int saturation = 0;
  uint8_t vflip = 0;
  uint8_t hmirror = 0;
};

struct sensor_t {
  pixformat_t pixformat = PIXFORMAT_JPEG;
  camera_status_t status;
  int (*set_pixformat)(sensor_t *sensor, pixformat_t pixformat) = nullptr;
  int (*set_framesize)(sensor_t *sensor, framesize_t framesize) = nullptr;
  int (*set_quality)(sensor_t *sensor, int quality) = nullptr;
  int (*set_brightness)(sensor_t *sensor, int level) = nullptr;
  int (*set_contrast)(sensor_t *sensor, int level) = nullptr;
  int (*set_saturation)(sensor_t *sensor, int level) = nullptr;
  int (*set_vflip)(sensor_t *sensor, int enable) = nullptr;
  int (*set_hmirror)(sensor_t *sensor, int enable) = nullptr;
};

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

namespace tiny_camera {
namespace esp_video_detail {

class EspVideoCamera {
 public:
  bool begin(const camera_config_t &config) {
    fbOut_ = false;

    if (config.frame_size < 0 || config.frame_size >= FRAMESIZE_INVALID) {
      TinyCameraLogger.error("unsupported frame_size %d",
                              (int)config.frame_size);
      return false;
    }
    if (!openDevice(config)) {
      end();
      return false;
    }
    nativeWidth_ = capture_.getWidth();
    nativeHeight_ = capture_.getHeight();
    TinyCameraLogger.info("sensor resolution: %ux%u", (unsigned)nativeWidth_,
                           (unsigned)nativeHeight_);

    sensor_ = sensor_t{};
    sensor_.status.framesize = config.frame_size;
    sensor_.status.quality = config.jpeg_quality;
    sensor_.set_pixformat = &setPixformat;
    sensor_.set_framesize = &setFramesize;
    sensor_.set_quality = &setQuality;
    sensor_.set_brightness = &setBrightness;
    sensor_.set_contrast = &setContrast;
    sensor_.set_saturation = &setSaturation;
    sensor_.set_vflip = &setVflip;
    sensor_.set_hmirror = &setHmirror;

    int w = resolution[config.frame_size].width;
    int h = resolution[config.frame_size].height;
    if (config.custom_width > 0 && config.custom_height > 0) {
      w = config.custom_width;
      h = config.custom_height;
    }
    if (config.native_frame_size) {
      w = nativeWidth_;
      h = nativeHeight_;
    }
    if (!validSize(w, h)) {
      end();
      return false;
    }
    width_ = w;
    height_ = h;
    // Allocates the buffers for this size, too.
    if (!applyPixformat(config.pixel_format)) {
      end();
      return false;
    }
    return true;
  }

  void end() {
    held_.end();
    fbOut_ = false;
    capture_.end();
    video_.end();
    freeBuffers();
#if TINY_CAMERA_ESP_VIDEO_HW_JPEG
    if (jpeg_ != nullptr) {
      jpeg_del_encoder_engine(jpeg_);
      jpeg_ = nullptr;
    }
#endif
  }

  camera_fb_t *capture() {
    if (fbOut_) {
      TinyCameraLogger.warn(
          "capture: previous frame buffer not returned yet (only one frame "
          "buffer can be held at a time with ESP_Video)");
      return nullptr;
    }

    ESPVideoBufferClass frame = capture_.captureBuffer();
    if (!frame.valid()) return nullptr;

    const pixformat_t format = sensor_.pixformat;
    if (!yuyvToRgb565_ && !needsScaling() && !hwJpeg()) {
      // Zero-copy: hand out the driver's own buffer until fb_return().
      held_ = std::move(frame);
      fb_.buf = held_.data();
      fb_.len = held_.size();
      fb_.width = nativeWidth_;
      fb_.height = nativeHeight_;
    } else {
      const uint8_t *pixels = frame.data();
      if (yuyvToRgb565_) {
        convertYuyvToRgb565(pixels, conv_, nativeWidth_ * nativeHeight_);
        pixels = conv_;
      }
      if (needsScaling()) {
        scaleInto(pixels, work_);
        pixels = work_;
      }
      if (hwJpeg()) {
        if (!encodeJpeg(pixels)) return nullptr;
      } else {
        fb_.buf = (uint8_t *)pixels;
        fb_.len = width_ * height_ * bytesPerPixel(format);
      }
      fb_.width = width_;
      fb_.height = height_;
      // frame goes out of scope here: its buffer is returned to the driver
      // right away, so the next one can be captured meanwhile.
    }
    fb_.format = format;
    gettimeofday(&fb_.timestamp, nullptr);
    fbOut_ = true;
    return &fb_;
  }

  void release(camera_fb_t *fb) {
    if (fb != &fb_) return;
    held_.end();
    fbOut_ = false;
  }

  sensor_t *sensor() { return &sensor_; }

  bool setCustomFrameSize(int width, int height) {
    if (fbOut_) {
      TinyCameraLogger.error(
          "setCustomFrameSize: return the current frame buffer first");
      return false;
    }
    if (!setOutputSize(width, height)) return false;
    TinyCameraLogger.info("setCustomFrameSize: output is now %dx%d", width,
                           height);
    return true;
  }

 private:
  /// Starts the MIPI-CSI or DVP video device, picked from config.
  bool openDevice(const camera_config_t &config) {
    ESPVideoCamConfigClass cam;
    if (config.sccb_i2c_bus != nullptr) {
      cam.begin(config.sccb_i2c_bus, config.sccb_i2c_freq, config.pin_reset,
                config.pin_pwdn);
    } else {
      cam.begin((i2c_port_num_t)config.sccb_i2c_port, config.pin_sccb_scl,
                config.pin_sccb_sda, config.sccb_i2c_freq, config.pin_reset,
                config.pin_pwdn);
    }

    const char *device = nullptr;
    if (config.pin_d0 >= 0) {
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
      ESPVideoDVPPinsConfigClass pins;
      pins.begin(config.pin_vsync, config.pin_href, config.pin_pclk,
                 config.pin_xclk, config.pin_d0, config.pin_d1, config.pin_d2,
                 config.pin_d3, config.pin_d4, config.pin_d5, config.pin_d6,
                 config.pin_d7);
      ESPVideoDVPConfigClass dvp;
      dvp.begin(cam, pins, config.xclk_freq_hz);
      if (!video_.begin(dvp)) {
        TinyCameraLogger.error(
            "DVP init failed - is a supported sensor connected and are the "
            "pins right?");
        return false;
      }
      device = ESP_VIDEO_DVP_DEVICE_NAME;
#else
      TinyCameraLogger.error("this chip has no esp_video DVP support");
      return false;
#endif
    } else {
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
      ESPVideoCSIConfigClass csi;
      csi.begin(cam);
      if (!video_.begin(csi)) {
        TinyCameraLogger.error(
            "MIPI-CSI init failed - is a supported sensor connected and are "
            "the SCCB pins right?");
        return false;
      }
      device = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;
#else
      TinyCameraLogger.error(
          "this chip has no MIPI-CSI: set the DVP pins (pin_d0...)");
      return false;
#endif
    }

    int buffers = config.fb_count < 2 ? 2 : config.fb_count;
    if (!capture_.begin(device, buffers)) {
      TinyCameraLogger.error("failed to open %s", device);
      return false;
    }
    return true;
  }

  static size_t bytesPerPixel(pixformat_t format) {
    switch (format) {
      case PIXFORMAT_RGB888:
        return 3;
      case PIXFORMAT_GRAYSCALE:
      case PIXFORMAT_RAW:
        return 1;
      default:  // RGB565, YUV422, and hardware-encoded JPEG's RGB565 input
        return 2;
    }
  }

  static bool toVideoFormat(pixformat_t format, esp_video_format_t &out) {
    switch (format) {
      case PIXFORMAT_RGB565:
        out = ESP_VIDEO_FORMAT_RGB565;
        return true;
      case PIXFORMAT_JPEG:
#if TINY_CAMERA_ESP_VIDEO_HW_JPEG
        out = ESP_VIDEO_FORMAT_RGB565;  // encoded by the hardware encoder
#else
        out = ESP_VIDEO_FORMAT_JPEG;  // the sensor's own encoder
#endif
        return true;
      case PIXFORMAT_RGB888:
        out = ESP_VIDEO_FORMAT_RGB888;
        return true;
      case PIXFORMAT_GRAYSCALE:
        out = ESP_VIDEO_FORMAT_GRAY8;
        return true;
      case PIXFORMAT_YUV422:
        out = ESP_VIDEO_FORMAT_YUV422_YUYV;
        return true;
      case PIXFORMAT_RAW:
        out = ESP_VIDEO_FORMAT_RAW8;
        return true;
      default:
        return false;
    }
  }

  bool hwJpeg() const {
#if TINY_CAMERA_ESP_VIDEO_HW_JPEG
    return sensor_.pixformat == PIXFORMAT_JPEG;
#else
    return false;
#endif
  }

  /// Only packed one-pixel-per-unit formats can be cropped/scaled.
  bool canScale() const {
    const pixformat_t format = sensor_.pixformat;
    return format == PIXFORMAT_RGB565 || format == PIXFORMAT_RGB888 ||
           format == PIXFORMAT_GRAYSCALE || hwJpeg();
  }

  bool needsScaling() const {
    return canScale() && (width_ != nativeWidth_ || height_ != nativeHeight_);
  }

  /// Switches the capture to the format `format` needs (falling back to
  /// YUYV + software conversion for RGB565), restarting the stream.
  bool applyPixformat(pixformat_t format) {
    esp_video_format_t videoFormat;
    if (!toVideoFormat(format, videoFormat)) {
      TinyCameraLogger.error("unsupported pixel format %d", (int)format);
      return false;
    }
    if (capture_.isCaptureStarted()) capture_.stopCapture();
    bool yuyv = false;
    if (!capture_.setFormat(videoFormat)) {
      yuyv = videoFormat == ESP_VIDEO_FORMAT_RGB565 &&
             capture_.setFormat(ESP_VIDEO_FORMAT_YUV422_YUYV);
      if (!yuyv) {
        TinyCameraLogger.error(
            "the camera does not support pixel format %d%s", (int)format,
            format == PIXFORMAT_JPEG
                ? " (no JPEG encoder: capture RGB565 and see "
                  "TinyCameraConvertSoftware.h)"
                : "");
        capture_.startCapture();
        return false;
      }
      TinyCameraLogger.info("no RGB565 output: converting from YUYV");
    }
    if (!capture_.startCapture()) {
      TinyCameraLogger.error("failed to start capturing");
      return false;
    }
    sensor_.pixformat = format;
    yuyvToRgb565_ = yuyv;
    if (!canScale() &&
        (width_ != nativeWidth_ || height_ != nativeHeight_)) {
      TinyCameraLogger.warn(
          "pixel format %d can't be scaled: frames are %ux%u", (int)format,
          (unsigned)nativeWidth_, (unsigned)nativeHeight_);
    }
    return allocateBuffers();
  }

  static bool validSize(int w, int h) {
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) {
      TinyCameraLogger.error("invalid frame size %dx%d", w, h);
      return false;
    }
    return true;
  }

  bool setOutputSize(int w, int h) {
    if (!validSize(w, h)) return false;
    width_ = w;
    height_ = h;
    return allocateBuffers();
  }

  /// (Re)allocates the conversion, scaling and JPEG buffers for the
  /// current output size and pixel format.
  bool allocateBuffers() {
    freeBuffers();
    const size_t bpp = bytesPerPixel(sensor_.pixformat);
    if (yuyvToRgb565_) {
      conv_ = allocFrame(nativeWidth_ * nativeHeight_ * 2, !needsScaling());
      if (conv_ == nullptr) return false;
    }
    if (needsScaling()) {
      work_ = allocFrame(width_ * height_ * bpp, true);
      if (work_ == nullptr) return false;
    }
#if TINY_CAMERA_ESP_VIDEO_HW_JPEG
    if (hwJpeg()) {
      // JPEG output is never larger than the RGB565 input in practice.
      const size_t bytes = width_ * height_ * 2;
      jpeg_encode_memory_alloc_cfg_t mem = {};
      mem.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
      jpegOut_ = (uint8_t *)jpeg_alloc_encoder_mem(bytes, &mem, &jpegOutSize_);
      if (jpegOut_ == nullptr) {
        TinyCameraLogger.error("failed to allocate %u bytes for JPEG",
                                (unsigned)bytes);
        return false;
      }
      if (jpeg_ == nullptr) {
        jpeg_encode_engine_cfg_t engine = {};
        engine.timeout_ms = 1000;
        if (jpeg_new_encoder_engine(&engine, &jpeg_) != ESP_OK) {
          TinyCameraLogger.error("failed to start the JPEG encoder");
          jpeg_ = nullptr;
          return false;
        }
      }
    }
#endif
    return true;
  }

  /// Allocates a frame-sized buffer; `last` marks the buffer that feeds
  /// the hardware JPEG encoder, which needs its own aligned memory.
  uint8_t *allocFrame(size_t bytes, bool last) const {
    uint8_t *buf = nullptr;
#if TINY_CAMERA_ESP_VIDEO_HW_JPEG
    if (last && hwJpeg()) {
      jpeg_encode_memory_alloc_cfg_t mem = {};
      mem.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER;
      size_t allocated = 0;
      buf = (uint8_t *)jpeg_alloc_encoder_mem(bytes, &mem, &allocated);
    }
#endif
    (void)last;
    if (buf == nullptr) {
      buf = (uint8_t *)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM);
    }
    if (buf == nullptr) {
      buf = (uint8_t *)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_8BIT);
    }
    if (buf == nullptr) {
      TinyCameraLogger.error("failed to allocate %u bytes", (unsigned)bytes);
    }
    return buf;
  }

  void freeBuffers() {
    free(conv_);
    conv_ = nullptr;
    free(work_);
    work_ = nullptr;
    free(jpegOut_);
    jpegOut_ = nullptr;
    jpegOutSize_ = 0;
  }

  static uint8_t clamp8(int v) {
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
  }

  /// YUYV (YUV 4:2:2) to native-endian RGB565, BT.601.
  static void convertYuyvToRgb565(const uint8_t *src, uint8_t *dst,
                                  size_t pixels) {
    uint16_t *out = (uint16_t *)dst;
    for (size_t i = 0; i + 1 < pixels; i += 2, src += 4) {
      const int u = src[1] - 128, v = src[3] - 128;
      const int dr = (359 * v) >> 8, dg = (88 * u + 183 * v) >> 8,
                db = (454 * u) >> 8;
      for (int k = 0; k < 2; k++) {
        const int y = src[k * 2];
        const uint8_t r = clamp8(y + dr), g = clamp8(y - dg),
                      b = clamp8(y + db);
        *out++ = (uint16_t)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
      }
    }
  }

  /// Center-crops the native frame to the output's aspect ratio and
  /// scales it to the output size (nearest neighbor).
  void scaleInto(const uint8_t *src, uint8_t *dst) const {
    const size_t sw = nativeWidth_, sh = nativeHeight_;
    const size_t dw = width_, dh = height_;
    size_t cropW = sw, cropH = sh;
    if (sw * dh > sh * dw) {
      cropW = sh * dw / dh;
    } else {
      cropH = sw * dh / dw;
    }
    const size_t x0 = (sw - cropW) / 2, y0 = (sh - cropH) / 2;
    const size_t bpp = bytesPerPixel(sensor_.pixformat);
    for (size_t y = 0; y < dh; y++) {
      const uint8_t *row = src + ((y0 + y * cropH / dh) * sw + x0) * bpp;
      switch (bpp) {
        case 2: {
          const uint16_t *s = (const uint16_t *)row;
          uint16_t *d = (uint16_t *)dst + y * dw;
          for (size_t x = 0; x < dw; x++) d[x] = s[x * cropW / dw];
          break;
        }
        case 1: {
          uint8_t *d = dst + y * dw;
          for (size_t x = 0; x < dw; x++) d[x] = row[x * cropW / dw];
          break;
        }
        default: {
          uint8_t *d = dst + y * dw * 3;
          for (size_t x = 0; x < dw; x++, d += 3) {
            memcpy(d, row + (x * cropW / dw) * 3, 3);
          }
          break;
        }
      }
    }
  }

  bool encodeJpeg(const uint8_t *rgb565) {
#if TINY_CAMERA_ESP_VIDEO_HW_JPEG
    // esp32-camera's jpeg_quality runs 0 (best) to 63 (worst); the
    // hardware encoder's runs 1 (worst) to 100 (best).
    int q = sensor_.status.quality;
    q = 100 - (q < 0 ? 0 : (q > 63 ? 63 : q)) * 99 / 63;
    jpeg_encode_cfg_t cfg = {};
    cfg.width = width_;
    cfg.height = height_;
    cfg.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
    cfg.sub_sample = JPEG_DOWN_SAMPLING_YUV420;
    cfg.image_quality = (uint32_t)q;
    uint32_t len = 0;
    esp_err_t err =
        jpeg_encoder_process(jpeg_, &cfg, rgb565, width_ * height_ * 2,
                             jpegOut_, jpegOutSize_, &len);
    if (err != ESP_OK) {
      TinyCameraLogger.error("capture: JPEG encoding failed (%d)", err);
      return false;
    }
    fb_.buf = jpegOut_;
    fb_.len = len;
    return true;
#else
    (void)rgb565;
    return false;
#endif
  }

  static EspVideoCamera *self(sensor_t *s);

  static int setPixformat(sensor_t *s, pixformat_t format) {
    EspVideoCamera *cam = self(s);
    if (cam->fbOut_) return -1;
    return cam->applyPixformat(format) ? 0 : -1;
  }
  static int setFramesize(sensor_t *s, framesize_t size) {
    if (size < 0 || size >= FRAMESIZE_INVALID) return -1;
    EspVideoCamera *cam = self(s);
    if (cam->fbOut_) return -1;
    if (!cam->setOutputSize(resolution[size].width, resolution[size].height))
      return -1;
    s->status.framesize = size;
    return 0;
  }
  static int setQuality(sensor_t *s, int quality) {
    s->status.quality = quality;
    return 0;
  }
  static int setBrightness(sensor_t *s, int level) {
    s->status.brightness = level;
    return 0;
  }
  static int setContrast(sensor_t *s, int level) {
    s->status.contrast = level;
    return 0;
  }
  static int setSaturation(sensor_t *s, int level) {
    s->status.saturation = level;
    return 0;
  }
  static int setVflip(sensor_t *s, int enable) {
    if (!self(s)->capture_.setSensorVFlip(enable != 0)) return -1;
    s->status.vflip = enable ? 1 : 0;
    return 0;
  }
  static int setHmirror(sensor_t *s, int enable) {
    if (!self(s)->capture_.setSensorHFlip(enable != 0)) return -1;
    s->status.hmirror = enable ? 1 : 0;
    return 0;
  }

  sensor_t sensor_;
  camera_fb_t fb_;
  ESPVideoClass video_;
  ESPVideoCaptureDevClass capture_;
  ESPVideoBufferClass held_;  // the frame handed out zero-copy, if any
#if TINY_CAMERA_ESP_VIDEO_HW_JPEG
  jpeg_encoder_handle_t jpeg_ = nullptr;
#endif
  uint8_t *conv_ = nullptr;     // YUYV frame converted to RGB565
  uint8_t *work_ = nullptr;     // scaled frame
  uint8_t *jpegOut_ = nullptr;  // encoded JPEG
  size_t jpegOutSize_ = 0;
  size_t nativeWidth_ = 0;
  size_t nativeHeight_ = 0;
  size_t width_ = 0;
  size_t height_ = 0;
  bool yuyvToRgb565_ = false;
  bool fbOut_ = false;
};

/// The single camera driver instance, as on every other platform.
inline EspVideoCamera espVideoCamera;

inline EspVideoCamera *EspVideoCamera::self(sensor_t *) {
  return &espVideoCamera;
}

}  // namespace esp_video_detail

/// ESP_Video backend for TinyCamera::setCustomFrameSize().
inline bool tinyCameraEspVideoSetCustomFrameSize(int width, int height) {
  return esp_video_detail::espVideoCamera.setCustomFrameSize(width, height);
}

}  // namespace tiny_camera

inline esp_err_t esp_camera_init(const camera_config_t *config) {
  if (config == nullptr) return ESP_FAIL;
  return tiny_camera::esp_video_detail::espVideoCamera.begin(*config)
             ? ESP_OK
             : ESP_FAIL;
}

inline void esp_camera_deinit() {
  tiny_camera::esp_video_detail::espVideoCamera.end();
}

inline camera_fb_t *esp_camera_fb_get() {
  return tiny_camera::esp_video_detail::espVideoCamera.capture();
}

inline void esp_camera_fb_return(camera_fb_t *fb) {
  tiny_camera::esp_video_detail::espVideoCamera.release(fb);
}

inline sensor_t *esp_camera_sensor_get() {
  return tiny_camera::esp_video_detail::espVideoCamera.sensor();
}
