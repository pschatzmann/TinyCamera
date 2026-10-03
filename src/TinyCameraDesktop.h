#pragma once
/**
 * Desktop (Linux/macOS) compatibility layer for TinyCamera, built via the
 * CMake build in this repository's root CMakeLists.txt. Plain C++/POSIX:
 * it needs no Arduino core, so it works in an ordinary C++ program as
 * well as in Arduino sketches run with the Arduino Emulator
 * (https://github.com/pschatzmann/Arduino-Emulator).
 *
 * Reproduces the same camera_config_t / camera_fb_t / esp_camera_*
 * surface that ESP32's esp_camera.h, TinyCameraRP2040.h and
 * TinyCameraSTM32.h expose, so TinyCamera.h, TinyCameraConvert.h,
 * TinyCameraConvertSoftware.h and the example sketches compile and run
 * unmodified on a desktop machine - useful for developing and testing
 * frame-processing code without flashing a board.
 *
 * Frame sources (camera_config_t::source):
 *  - CAMERA_SOURCE_TEST_PATTERN: a synthetic, animated test image (color
 *    bars, a gray ramp and a moving white square). Works everywhere and
 *    needs no hardware - frame contents are deterministic, which makes it
 *    suitable for automated tests.
 *  - CAMERA_SOURCE_V4L2 (Linux only): a real webcam via Video4Linux2. The
 *    device is camera_config_t::device, else the TINY_CAMERA_DEVICE
 *    environment variable, else /dev/video0. The webcam must support the
 *    YUYV (YUV 4:2:2) capture format, which practically every UVC webcam
 *    does.
 *  - CAMERA_SOURCE_AUTO (default): V4L2 if a webcam can be opened, else
 *    the test pattern.
 *
 * Coverage / limitations:
 *  - Pixel formats: RGB565 (native-endian uint16_t per pixel, as the
 *    software converters in TinyCameraConvert.h expect), RGB888,
 *    GRAYSCALE and YUV422 (YUYV). PIXFORMAT_JPEG is supported only when
 *    the TinyJPEG library (https://github.com/pschatzmann/TinyJPEG) is on
 *    the include path - frames are then software-encoded, emulating a
 *    sensor with a hardware JPEG encoder.
 *  - Frame sizes: every FRAMESIZE_* below, plus any arbitrary size via
 *    camera_config_t::custom_width/custom_height or
 *    TinyCamera::setCustomFrameSize(). If the webcam can't deliver the
 *    requested size itself, its frames are scaled (nearest neighbor) to
 *    it, so the output size always matches what was asked for.
 *  - fb_count is always 1 (single buffering); grab_mode is ignored.
 *  - sensor_t: set_vflip/set_hmirror are applied in software;
 *    set_framesize/set_pixformat reconfigure the output;
 *    set_brightness/set_contrast/set_saturation are accepted and recorded
 *    in sensor_t::status but have no effect on the image.
 *  - All pin_* fields are accepted (so TinyCameraPins.h presets compile)
 *    and ignored.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <chrono>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>
#define TINY_CAMERA_DESKTOP_HAS_V4L2 1
#endif

#if defined(__has_include)
#if __has_include(<TinyJPEGEncoder.h>)
#include <TinyJPEGEncoder.h>
#define TINY_CAMERA_DESKTOP_HAS_JPEG 1
#endif
#endif

/// Defined whenever the desktop backend is active - test for it in
/// sketches the same way as ESP32/ARDUINO_ARCH_STM32.
#define TINY_CAMERA_DESKTOP 1

// esp32-camera's camera_config_t carries LEDC (ESP32 PWM peripheral) fields
// for the XCLK generator. TinyCamera.h's shared defaultConfig() sets them
// unconditionally for source compatibility across platforms; the desktop
// has no XCLK at all, so these just need to exist as harmless constants -
// same reasoning as TinyCameraSTM32.h.
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_0 0

#include "TinyCameraLogger.h"

// ---------------------------------------------------------------------------
// Public compatibility surface (global scope, mirroring esp_camera.h's C API)
// ---------------------------------------------------------------------------

enum pixformat_t {
  PIXFORMAT_RGB565,
  PIXFORMAT_YUV422,
  PIXFORMAT_GRAYSCALE,
  PIXFORMAT_JPEG,
  PIXFORMAT_RGB888,
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

/// Desktop-only: where frames come from - see this file's header comment.
enum camera_source_t {
  CAMERA_SOURCE_AUTO,
  CAMERA_SOURCE_TEST_PATTERN,
  CAMERA_SOURCE_V4L2,
};

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_SUPPORTED 0x106

struct camera_config_t {
  int ledc_channel = 0;  // unused on desktop; kept for source compatibility
  int ledc_timer = 0;    // unused on desktop; kept for source compatibility
  int xclk_freq_hz = 20000000;  // unused on desktop
  pixformat_t pixel_format = PIXFORMAT_JPEG;
  framesize_t frame_size = FRAMESIZE_QVGA;
  // When both are >0, requests this exact output resolution instead of
  // frame_size's fixed preset (same fields as on STM32).
  int custom_width = 0;
  int custom_height = 0;
  int jpeg_quality = 12;  // esp32-camera scale: 0-63, lower is better
  int fb_count = 1;       // always 1 on desktop
  camera_fb_location_t fb_location = CAMERA_FB_IN_DRAM;   // unused on desktop
  camera_grab_mode_t grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // unused on desktop

  // Desktop-only fields.
  camera_source_t source = CAMERA_SOURCE_AUTO;
  // V4L2 device path; nullptr: $TINY_CAMERA_DEVICE, else /dev/video0.
  const char *device = nullptr;
  // Caps the test pattern's frame rate, emulating a real sensor's
  // (0: unthrottled, e.g. for tests). A webcam paces itself.
  int test_pattern_fps = 30;

  int pin_pwdn = -1;
  int pin_reset = -1;
  int pin_xclk = -1;
  int pin_sccb_sda = -1;
  int pin_sccb_scl = -1;
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
  int brightness = 0;
  int contrast = 0;
  int saturation = 0;
  int quality = 12;
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
namespace desktop_detail {

inline uint8_t clamp8(int v) {
  return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

#if defined(TINY_CAMERA_DESKTOP_HAS_V4L2)

/**
 * Minimal V4L2 (Video4Linux2) streaming capture: YUYV frames via mmap()ed
 * driver buffers.
 */
class V4l2Source {
 public:
  ~V4l2Source() { close(); }

  /// Opens `device` and starts streaming YUYV at (or near - the driver
  /// picks the closest size it supports) width x height.
  bool open(const char *device, int width, int height) {
    fd_ = ::open(device, O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
      TinyCameraLogger.info("cannot open %s: %s", device, strerror(errno));
      return false;
    }

    v4l2_capability cap = {};
    if (xioctl(VIDIOC_QUERYCAP, &cap) < 0 ||
        !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
      TinyCameraLogger.error("%s is not a streaming video capture device",
                              device);
      close();
      return false;
    }

    v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (xioctl(VIDIOC_S_FMT, &fmt) < 0 ||
        fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
      TinyCameraLogger.error("%s does not support YUYV capture", device);
      close();
      return false;
    }
    width_ = fmt.fmt.pix.width;
    height_ = fmt.fmt.pix.height;
    stride_ = fmt.fmt.pix.bytesperline ? fmt.fmt.pix.bytesperline
                                       : (size_t)width_ * 2;

    v4l2_requestbuffers req = {};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(VIDIOC_REQBUFS, &req) < 0 || req.count < 1) {
      TinyCameraLogger.error("%s: VIDIOC_REQBUFS failed", device);
      close();
      return false;
    }
    for (uint32_t i = 0; i < req.count; i++) {
      v4l2_buffer buf = {};
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = i;
      if (xioctl(VIDIOC_QUERYBUF, &buf) < 0) {
        close();
        return false;
      }
      void *p = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd_, buf.m.offset);
      if (p == MAP_FAILED) {
        TinyCameraLogger.error("%s: mmap failed", device);
        close();
        return false;
      }
      buffers_.push_back({p, buf.length});
      if (xioctl(VIDIOC_QBUF, &buf) < 0) {
        close();
        return false;
      }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(VIDIOC_STREAMON, &type) < 0) {
      TinyCameraLogger.error("%s: VIDIOC_STREAMON failed", device);
      close();
      return false;
    }
    streaming_ = true;
    TinyCameraLogger.info("V4L2: streaming %s at %dx%d YUYV", device, width_,
                           height_);
    return true;
  }

  void close() {
    if (streaming_) {
      v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      xioctl(VIDIOC_STREAMOFF, &type);
      streaming_ = false;
    }
    for (auto &b : buffers_) munmap(b.start, b.length);
    buffers_.clear();
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  int width() const { return width_; }
  int height() const { return height_; }

  /// Waits for the next frame and converts it to RGB888, scaling (nearest
  /// neighbor) to outW x outH.
  bool read(uint8_t *rgb, int outW, int outH) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    timeval tv = {2, 0};
    int r = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (r <= 0) {
      TinyCameraLogger.warn("V4L2: timeout waiting for a frame");
      return false;
    }

    v4l2_buffer buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(VIDIOC_DQBUF, &buf) < 0) {
      TinyCameraLogger.warn("V4L2: VIDIOC_DQBUF failed: %s", strerror(errno));
      return false;
    }
    const uint8_t *src = (const uint8_t *)buffers_[buf.index].start;
    for (int y = 0; y < outH; y++) {
      const uint8_t *row = src + (size_t)(y * height_ / outH) * stride_;
      uint8_t *dst = rgb + (size_t)y * outW * 3;
      for (int x = 0; x < outW; x++) {
        int sx = x * width_ / outW;
        // YUYV: each 4-byte group holds Y0 U Y1 V for two pixels.
        const uint8_t *px = row + (sx & ~1) * 2;
        int yy = px[(sx & 1) * 2];
        int u = px[1] - 128;
        int v = px[3] - 128;
        dst[x * 3 + 0] = clamp8(yy + ((359 * v) >> 8));
        dst[x * 3 + 1] = clamp8(yy - ((88 * u + 183 * v) >> 8));
        dst[x * 3 + 2] = clamp8(yy + ((454 * u) >> 8));
      }
    }
    xioctl(VIDIOC_QBUF, &buf);
    return true;
  }

 private:
  struct MappedBuffer {
    void *start;
    size_t length;
  };

  int xioctl(unsigned long request, void *arg) {
    int r;
    do {
      r = ioctl(fd_, request, arg);
    } while (r < 0 && errno == EINTR);
    return r;
  }

  int fd_ = -1;
  int width_ = 0;
  int height_ = 0;
  size_t stride_ = 0;
  bool streaming_ = false;
  std::vector<MappedBuffer> buffers_;
};

#endif  // TINY_CAMERA_DESKTOP_HAS_V4L2

class DesktopCamera {
 public:
  bool begin(const camera_config_t &config) {
    config_ = config;
    frameCount_ = 0;
    fbOut_ = false;

    if (config.frame_size < 0 || config.frame_size >= FRAMESIZE_INVALID) {
      TinyCameraLogger.error("unsupported frame_size %d",
                              (int)config.frame_size);
      return false;
    }
#if !defined(TINY_CAMERA_DESKTOP_HAS_JPEG)
    if (config.pixel_format == PIXFORMAT_JPEG) {
      TinyCameraLogger.error(
          "PIXFORMAT_JPEG needs the TinyJPEG library on the include path "
          "(https://github.com/pschatzmann/TinyJPEG)");
      return false;
    }
#endif

    int w = resolution[config.frame_size].width;
    int h = resolution[config.frame_size].height;
    if (config.custom_width > 0 && config.custom_height > 0) {
      w = config.custom_width;
      h = config.custom_height;
    }
    setOutputSize(w, h);

    useV4l2_ = false;
    if (config.source != CAMERA_SOURCE_TEST_PATTERN) {
#if defined(TINY_CAMERA_DESKTOP_HAS_V4L2)
      const char *device = config.device;
      if (device == nullptr) device = getenv("TINY_CAMERA_DEVICE");
      if (device == nullptr) device = "/dev/video0";
      useV4l2_ = v4l2_.open(device, w, h);
#endif
      if (!useV4l2_ && config.source == CAMERA_SOURCE_V4L2) {
        TinyCameraLogger.error("no V4L2 webcam available");
        return false;
      }
    }
    if (!useV4l2_) {
      TinyCameraLogger.info("using the synthetic test pattern (%dx%d)", w, h);
    }

    sensor_ = sensor_t{};
    sensor_.pixformat = config.pixel_format;
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
    return true;
  }

  void end() {
#if defined(TINY_CAMERA_DESKTOP_HAS_V4L2)
    v4l2_.close();
#endif
    useV4l2_ = false;
    fbOut_ = false;
    rgb_.clear();
    rgb_.shrink_to_fit();
    out_.clear();
    out_.shrink_to_fit();
  }

  camera_fb_t *capture() {
    if (fbOut_) {
      TinyCameraLogger.warn(
          "capture: previous frame buffer not returned yet (fb_count is "
          "always 1 on desktop)");
      return nullptr;
    }

    if (useV4l2_) {
#if defined(TINY_CAMERA_DESKTOP_HAS_V4L2)
      if (!v4l2_.read(rgb_.data(), width_, height_)) return nullptr;
#endif
    } else {
      throttle();
      drawTestPattern();
    }
    applyFlip();
    if (!encode()) return nullptr;

    fb_.buf = out_.data();
    fb_.width = width_;
    fb_.height = height_;
    fb_.format = sensor_.pixformat;
    gettimeofday(&fb_.timestamp, nullptr);
    frameCount_++;
    fbOut_ = true;
    return &fb_;
  }

  void release(camera_fb_t *fb) {
    if (fb == &fb_) fbOut_ = false;
  }

  sensor_t *sensor() { return &sensor_; }

  bool setCustomFrameSize(int width, int height) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
      TinyCameraLogger.error("setCustomFrameSize: invalid size %dx%d", width,
                              height);
      return false;
    }
    if (fbOut_) {
      TinyCameraLogger.error(
          "setCustomFrameSize: return the current frame buffer first");
      return false;
    }
    setOutputSize(width, height);
    TinyCameraLogger.info("setCustomFrameSize: output is now %dx%d", width,
                           height);
    return true;
  }

 private:
  void setOutputSize(int w, int h) {
    width_ = w;
    height_ = h;
    rgb_.assign((size_t)w * h * 3, 0);
  }

  void throttle() {
    if (config_.test_pattern_fps <= 0) return;
    const auto interval =
        std::chrono::microseconds(1000000 / config_.test_pattern_fps);
    if (frameCount_ > 0) {
      std::this_thread::sleep_until(lastFrameTime_ + interval);
    }
    lastFrameTime_ = std::chrono::steady_clock::now();
  }

  /// Color bars over the top 2/3, a gray ramp below, and a white square
  /// that moves one step per frame - so consecutive frames differ.
  void drawTestPattern() {
    static const uint8_t kBars[8][3] = {
        {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
        {255, 0, 255},   {255, 0, 0},   {0, 0, 255},   {0, 0, 0},
    };
    const int barsH = height_ * 2 / 3;
    for (int y = 0; y < height_; y++) {
      uint8_t *row = rgb_.data() + (size_t)y * width_ * 3;
      for (int x = 0; x < width_; x++) {
        if (y < barsH) {
          const uint8_t *c = kBars[x * 8 / width_];
          row[x * 3 + 0] = c[0];
          row[x * 3 + 1] = c[1];
          row[x * 3 + 2] = c[2];
        } else {
          uint8_t g = (uint8_t)(x * 255 / (width_ > 1 ? width_ - 1 : 1));
          row[x * 3 + 0] = row[x * 3 + 1] = row[x * 3 + 2] = g;
        }
      }
    }

    int size = (height_ < width_ ? height_ : width_) / 6;
    if (size < 1) size = 1;
    int range = width_ - size;
    int x0 = range > 0 ? (int)((frameCount_ * 4) % range) : 0;
    int y0 = (barsH - size) / 2;
    if (y0 < 0) y0 = 0;
    for (int y = y0; y < y0 + size && y < height_; y++) {
      memset(rgb_.data() + ((size_t)y * width_ + x0) * 3, 255,
             (size_t)size * 3);
    }
  }

  void applyFlip() {
    const size_t rowBytes = (size_t)width_ * 3;
    if (sensor_.status.hmirror) {
      for (int y = 0; y < height_; y++) {
        uint8_t *row = rgb_.data() + y * rowBytes;
        for (int l = 0, r = width_ - 1; l < r; l++, r--) {
          for (int c = 0; c < 3; c++) {
            uint8_t t = row[l * 3 + c];
            row[l * 3 + c] = row[r * 3 + c];
            row[r * 3 + c] = t;
          }
        }
      }
    }
    if (sensor_.status.vflip) {
      std::vector<uint8_t> tmp(rowBytes);
      for (int t = 0, b = height_ - 1; t < b; t++, b--) {
        uint8_t *top = rgb_.data() + t * rowBytes;
        uint8_t *bot = rgb_.data() + b * rowBytes;
        memcpy(tmp.data(), top, rowBytes);
        memcpy(top, bot, rowBytes);
        memcpy(bot, tmp.data(), rowBytes);
      }
    }
  }

  /// Converts the RGB888 working image rgb_ into the requested pixel
  /// format in out_.
  bool encode() {
    const size_t count = (size_t)width_ * height_;
    const uint8_t *src = rgb_.data();
    switch (sensor_.pixformat) {
      case PIXFORMAT_RGB888:
        out_.assign(src, src + count * 3);
        break;
      case PIXFORMAT_RGB565: {
        out_.resize(count * 2);
        uint16_t *dst = (uint16_t *)out_.data();
        for (size_t i = 0; i < count; i++, src += 3) {
          dst[i] = (uint16_t)(((src[0] & 0xf8) << 8) | ((src[1] & 0xfc) << 3) |
                              (src[2] >> 3));
        }
        break;
      }
      case PIXFORMAT_GRAYSCALE:
        out_.resize(count);
        for (size_t i = 0; i < count; i++, src += 3) {
          out_[i] = luma(src);
        }
        break;
      case PIXFORMAT_YUV422: {
        out_.resize(count * 2);
        uint8_t *dst = out_.data();
        for (size_t i = 0; i + 1 < count; i += 2, src += 6, dst += 4) {
          int r = (src[0] + src[3]) / 2, g = (src[1] + src[4]) / 2,
              b = (src[2] + src[5]) / 2;
          dst[0] = luma(src);
          dst[1] = clamp8(((-43 * r - 85 * g + 128 * b) >> 8) + 128);
          dst[2] = luma(src + 3);
          dst[3] = clamp8(((128 * r - 107 * g - 21 * b) >> 8) + 128);
        }
        break;
      }
      case PIXFORMAT_JPEG:
#if defined(TINY_CAMERA_DESKTOP_HAS_JPEG)
      {
        // esp32-camera's jpeg_quality runs 0 (best) to 63 (worst);
        // TinyJPEG's runs 1 (worst) to 100 (best).
        if (width_ > JE_MAX_WIDTH) {
          TinyCameraLogger.error(
              "capture: JPEG width %d exceeds TinyJPEG's JE_MAX_WIDTH (%d) - "
              "define JE_MAX_WIDTH larger for the whole build",
              width_, (int)JE_MAX_WIDTH);
          return false;
        }
        int q = sensor_.status.quality;
        q = 100 - (q < 0 ? 0 : (q > 63 ? 63 : q)) * 99 / 63;
        out_.resize(count * 3 + 1024);
        size_t len = 0;
        tinyjpeg::TinyJPEGEncoder encoder;
        encoder.setQuality((uint8_t)q);
        if (encoder.encodeJpg(rgb_.data(), (uint16_t)width_, (uint16_t)height_,
                              JE_FMT_RGB888, out_.data(), out_.size(),
                              len) != JER_OK) {
          TinyCameraLogger.error("capture: JPEG encoding failed");
          return false;
        }
        out_.resize(len);
        break;
      }
#else
        TinyCameraLogger.error("capture: JPEG needs the TinyJPEG library");
        return false;
#endif
      default:
        TinyCameraLogger.error("capture: unsupported pixel format %d",
                                (int)sensor_.pixformat);
        return false;
    }
    fb_.len = out_.size();
    return true;
  }

  static uint8_t luma(const uint8_t *rgb) {
    return (uint8_t)((77 * rgb[0] + 150 * rgb[1] + 29 * rgb[2]) >> 8);
  }

  static DesktopCamera *self(sensor_t *s);

  static int setPixformat(sensor_t *s, pixformat_t format) {
#if !defined(TINY_CAMERA_DESKTOP_HAS_JPEG)
    if (format == PIXFORMAT_JPEG) return -1;
#endif
    s->pixformat = format;
    return 0;
  }
  static int setFramesize(sensor_t *s, framesize_t size) {
    if (size < 0 || size >= FRAMESIZE_INVALID) return -1;
    DesktopCamera *cam = self(s);
    if (cam->fbOut_) return -1;
    cam->setOutputSize(resolution[size].width, resolution[size].height);
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
    s->status.vflip = enable ? 1 : 0;
    return 0;
  }
  static int setHmirror(sensor_t *s, int enable) {
    s->status.hmirror = enable ? 1 : 0;
    return 0;
  }

  camera_config_t config_;
  sensor_t sensor_;
  camera_fb_t fb_;
  std::vector<uint8_t> rgb_;  // working image, RGB888
  std::vector<uint8_t> out_;  // frame buffer in the requested format
  int width_ = 0;
  int height_ = 0;
  uint32_t frameCount_ = 0;
  std::chrono::steady_clock::time_point lastFrameTime_;
  bool fbOut_ = false;
  bool useV4l2_ = false;
#if defined(TINY_CAMERA_DESKTOP_HAS_V4L2)
  V4l2Source v4l2_;
#endif
};

/// The single camera driver instance, as on every other platform.
inline DesktopCamera desktopCamera;

inline DesktopCamera *DesktopCamera::self(sensor_t *) { return &desktopCamera; }

}  // namespace desktop_detail

/// Desktop backend for TinyCamera::setCustomFrameSize().
inline bool tinyCameraDesktopSetCustomFrameSize(int width, int height) {
  return desktop_detail::desktopCamera.setCustomFrameSize(width, height);
}

}  // namespace tiny_camera

inline esp_err_t esp_camera_init(const camera_config_t *config) {
  if (config == nullptr) return ESP_FAIL;
  return tiny_camera::desktop_detail::desktopCamera.begin(*config) ? ESP_OK
                                                                   : ESP_FAIL;
}

inline void esp_camera_deinit() {
  tiny_camera::desktop_detail::desktopCamera.end();
}

inline camera_fb_t *esp_camera_fb_get() {
  return tiny_camera::desktop_detail::desktopCamera.capture();
}

inline void esp_camera_fb_return(camera_fb_t *fb) {
  tiny_camera::desktop_detail::desktopCamera.release(fb);
}

inline sensor_t *esp_camera_sensor_get() {
  return tiny_camera::desktop_detail::desktopCamera.sensor();
}
