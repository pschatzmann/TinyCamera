#pragma once
/**
 * TinyCamera - a tiny, header-only C++ wrapper around the ESP32 (esp32-camera),
 * RP2040 and STM32 (DCMI + OV7725, experimental) camera APIs.
 *
 * All three platforms expose (or are given, on STM32 via
 * TinyCameraSTM32.h) a compatible C API (camera_config_t, camera_fb_t,
 * esp_camera_init/deinit/fb_get/fb_return), so this single header works
 * unmodified on any of them.
 */

#if defined(ESP32)
#include "esp_camera.h"
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(TARGET_RP2040)
#include <Camera.h>
#elif defined(ARDUINO_ARCH_STM32)
#include "TinyCameraSTM32.h"
#else
#error \
    "TinyCamera: unsupported platform - only ESP32, RP2040 (arduino-pico Camera library) and STM32 (DCMI + OV7725, experimental) are supported"
#endif

#include "TinyCameraLogger.h"

namespace tiny_camera {

/// Convenience alias for the platform camera pixel format enum.
using PixelFormat = pixformat_t;
/// Convenience alias for the platform camera frame size enum.
using FrameSize = framesize_t;

/**
 * RAII wrapper around a camera_fb_t frame buffer: automatically returns the
 * frame buffer to the driver when the object goes out of scope, so callers
 * can never forget to call esp_camera_fb_return().
 */
class TinyCameraFrame {
 public:
  TinyCameraFrame() = default;
  explicit TinyCameraFrame(camera_fb_t *fb) : fb_(fb) {}

  // Not copyable: a frame buffer must only be returned once.
  TinyCameraFrame(const TinyCameraFrame &) = delete;
  TinyCameraFrame &operator=(const TinyCameraFrame &) = delete;

  TinyCameraFrame(TinyCameraFrame &&other) noexcept : fb_(other.fb_) {
    other.fb_ = nullptr;
  }

  TinyCameraFrame &operator=(TinyCameraFrame &&other) noexcept {
    if (this != &other) {
      release();
      fb_ = other.fb_;
      other.fb_ = nullptr;
    }
    return *this;
  }

  ~TinyCameraFrame() { release(); }

  /// True if this object holds a valid frame buffer.
  bool isValid() const { return fb_ != nullptr; }
  explicit operator bool() const { return isValid(); }

  /// Raw pixel/JPEG data.
  uint8_t *data() const { return fb_ ? fb_->buf : nullptr; }
  /// Length of data() in bytes.
  size_t size() const { return fb_ ? fb_->len : 0; }
  /// Frame width in pixels.
  size_t width() const { return fb_ ? fb_->width : 0; }
  /// Frame height in pixels.
  size_t height() const { return fb_ ? fb_->height : 0; }
  /// Pixel format (e.g. PIXFORMAT_JPEG, PIXFORMAT_RGB565).
  PixelFormat format() const { return fb_ ? fb_->format : PIXFORMAT_JPEG; }
  /// Capture timestamp, as provided by the underlying driver.
  struct timeval timestamp() const {
    return fb_ ? fb_->timestamp : timeval{};
  }

  /// Access to the underlying frame buffer, e.g. to pass to driver APIs.
  camera_fb_t *raw() const { return fb_; }

  /// Explicitly return the frame buffer before this object is destroyed.
  void release() {
    if (fb_ != nullptr) {
      esp_camera_fb_return(fb_);
      fb_ = nullptr;
    }
  }

 private:
  camera_fb_t *fb_ = nullptr;
};

/**
 * Thin, header-only wrapper around the ESP32/RP2040 camera driver.
 *
 * Usage:
 *   camera_config_t config = TinyCamera::defaultConfig();
 *   tiny_camera::setPinsAiThinker(config);  // from TinyCameraPins.h
 *   config.frame_size = FRAMESIZE_QVGA;
 *
 *   TinyCamera camera;
 *   if (!camera.begin(config)) { ... }
 *   if (auto frame = camera.captureFrame()) {
 *     // use frame.data() / frame.size()
 *   }  // frame buffer is returned automatically here
 */
class TinyCamera {
 public:
  TinyCamera() = default;
  ~TinyCamera() { end(); }

  // Not copyable: there is only one camera driver instance.
  TinyCamera(const TinyCamera &) = delete;
  TinyCamera &operator=(const TinyCamera &) = delete;

  /**
   * Returns a camera_config_t pre-filled with commonly used defaults
   * (JPEG output, QVGA, single frame buffer in DRAM). Pins must still be
   * assigned, e.g. via one of the setPinsXxx() helpers in TinyCameraPins.h
   * for ESP32 boards, or manually for RP2040 boards.
   */
  static camera_config_t defaultConfig() {
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    return config;
  }

  /// Initializes the camera driver with the given configuration.
  bool begin(const camera_config_t &config) {
    if (active_) end();
    esp_err_t err = esp_camera_init(&config);
    active_ = (err == ESP_OK);
    if (active_) {
      TinyCameraLogger.info("camera initialized");
    } else {
      TinyCameraLogger.error("camera init failed (esp_camera_init: %d)", err);
    }
    return active_;
  }

  /// Releases the camera driver and all resources it holds.
  void end() {
    if (active_) {
      esp_camera_deinit();
      active_ = false;
      TinyCameraLogger.info("camera released");
    }
  }

  /// True if begin() succeeded and end() has not been called since.
  bool isActive() const { return active_; }

  /**
   * Captures a single frame. The returned TinyCameraFrame owns the frame
   * buffer and returns it to the driver automatically when it goes out of
   * scope; check isValid()/operator bool() before use.
   */
  TinyCameraFrame captureFrame() {
    if (!active_) {
      TinyCameraLogger.error("captureFrame() called while camera is not active");
      return TinyCameraFrame();
    }
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == nullptr) {
      TinyCameraLogger.warn("capture failed (no frame buffer returned)");
    } else {
      TinyCameraLogger.debug("captured frame: %u bytes (%ux%u)",
                              (unsigned)fb->len, (unsigned)fb->width,
                              (unsigned)fb->height);
    }
    return TinyCameraFrame(fb);
  }

  /// Access to the underlying camera sensor for advanced tuning
  /// (brightness, contrast, flip, ...); nullptr if the camera is not active.
  sensor_t *sensor() const { return active_ ? esp_camera_sensor_get() : nullptr; }

  /// Requests the sensor's own DSP to scale its current field of view
  /// down to an arbitrary custom output size, in hardware - not a
  /// software crop. Call after begin(); the next captureFrame() reflects
  /// the new size. Support varies by platform:
  ///  - ESP32: works if the currently-detected sensor's esp32-camera
  ///    driver implements set_res_raw() (most OV-series sensors do;
  ///    simpler ones like GC0308 don't). Uses the *currently configured*
  ///    frame_size as the source window (via sensor_t::status.framesize
  ///    and esp32-camera's own resolution[] table) rather than each
  ///    sensor's native pixel array size, so it needs no per-sensor-model
  ///    geometry table - only ever scales down from whatever begin() was
  ///    last configured with, never up past it.
  ///  - RP2040: not supported - arduino-pico's Camera library exposes no
  ///    equivalent raw-window/scale hook, only the fixed FRAMESIZE_*
  ///    list via set_framesize(). Always returns false.
  ///  - STM32: works for OV7725 (the only sensor this backend currently
  ///    supports - see TinyCameraSTM32.h), which always reconfigures from
  ///    the sensor's full field of view rather than scaling down
  ///    incrementally from the current size the way ESP32's
  ///    set_res_raw()-based path does - so, unlike ESP32, this accepts
  ///    any 0<width<=640, 0<height<=480 at any time, not just shrinking.
  ///    camera_config_t::custom_width/custom_height (set before the
  ///    initial begin()) is still supported too and behaves the same way
  ///    - this method is simply the equivalent call usable afterward, for
  ///    the same cross-platform API surface as ESP32/RP2040.
  /// Returns false (and logs why) if unsupported on the current
  /// platform/sensor, or if the call itself fails.
  bool setCustomFrameSize(int width, int height) {
    if (!active_) {
      TinyCameraLogger.error(
          "setCustomFrameSize() called while camera is not active");
      return false;
    }
#if defined(ESP32)
    sensor_t *s = esp_camera_sensor_get();
    if (s == nullptr || s->set_res_raw == nullptr) {
      TinyCameraLogger.error(
          "setCustomFrameSize: this sensor's driver has no set_res_raw() "
          "support");
      return false;
    }
    resolution_info_t cur = resolution[s->status.framesize];
    int err = s->set_res_raw(s, 0, 0, cur.width, cur.height, 0, 0,
                              cur.width, cur.height, width, height,
                              /*scale=*/true, /*binning=*/false);
    if (err != 0) {
      TinyCameraLogger.error("setCustomFrameSize: set_res_raw failed (%d)",
                              err);
      return false;
    }
    TinyCameraLogger.info("setCustomFrameSize: scaled %ux%u -> %dx%d",
                            cur.width, cur.height, width, height);
    return true;
#elif defined(ARDUINO_ARCH_STM32)
    return tinyCameraStm32SetCustomFrameSize(width, height);
#else
    (void)width;
    (void)height;
    TinyCameraLogger.error(
        "setCustomFrameSize: not supported on this platform");
    return false;
#endif
  }

 private:
  bool active_ = false;
};

}  // namespace tiny_camera
