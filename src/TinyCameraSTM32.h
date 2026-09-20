#pragma once
/**
 * STM32 compatibility layer for TinyCamera. EXPERIMENTAL: works, and
 * OV7725 is verified end-to-end on real hardware (see below), but this
 * backend has only ever been tested on one board/sensor combination
 * (WeAct STM32H750 + OV7725) and its DCMI/SCCB timing needed several
 * rounds of real-hardware debugging to get right even there - treat any
 * other board or sensor as unproven until you've tested it yourself.
 *
 * Reproduces the same camera_config_t / camera_fb_t / esp_camera_*
 * surface that ESP32's esp_camera.h and the RP2040 arduino-pico Camera
 * library expose, so TinyCamera.h and TinyCameraConvert.h work unmodified
 * on STM32 boards with a DCMI (Digital Camera Interface) peripheral - such
 * as the WeAct STM32H750 - wired to a supported camera module.
 *
 * Implementation: STM32Cube HAL DCMI + DMA capture the sensor's parallel
 * output; a SensorDriver (see Driver/SensorDriver.h) brings the sensor
 * up over SCCB (I2C) and configures its resolution/format. HAL calls are
 * used directly alongside the Arduino API, which is the standard
 * STM32duino pattern for peripherals (like DCMI) that have no dedicated
 * Arduino-level wrapper.
 *
 * Sensor support: only OV7725 (Driver/OV7725.h) ships a driver.
 * OV2640/OV7670/OV5640 drivers existed earlier but were removed - none
 * was ever confirmed working end-to-end on real hardware (see
 * Driver/SensorDriver.h's header comment for the full account). begin()
 * still recognizes their chip IDs on the SCCB bus if present, so its
 * error message can name a real-but-unsupported sensor instead of a bare
 * "not found" (see identifyUnsupportedSensor() in Driver/SensorDriver.h).
 *
 * Coverage / limitations (see docs/Tutorial.md for the STM32 section):
 *  - OV7725 has no hardware JPEG encoder - PIXFORMAT_RGB565 only.
 *  - Frame sizes: FRAMESIZE_QVGA and FRAMESIZE_VGA, plus any arbitrary
 *    width/height up to 640x480 via camera_config_t::custom_width/
 *    custom_height (OV7725Driver scales its full field of view down to
 *    that size in hardware - see Driver/OV7725.h).
 *  - fb_count is always 1 (single buffering); grab_mode is ignored.
 *  - As on RP2040, there is no de-facto-standard camera connector across
 *    STM32 dev boards in general, so pins are always set on
 *    camera_config_t explicitly. setPinsWeActStm32H750() in
 *    TinyCameraPins.h fills them in for that board's onboard DCMI
 *    connector specifically (pin table sourced from WeAct's own published
 *    reference firmware - see docs/Tutorial.md). Different boards commonly
 *    use *incompatible* pin orderings on mechanically-identical
 *    connectors - verify against your board's own documented pinout
 *    before wiring a camera module to it; getting this wrong can damage
 *    the camera module, not just fail to work (see docs/Tutorial.md).
 *  - The DCMI capture engine here uses DMA1_Stream0 (request
 *    DMA_REQUEST_DCMI via DMAMUX on H7, DMA_CHANNEL_1 on F7) - matching
 *    WeAct's own STM32H750 DCMI example. If your sketch also uses that
 *    stream for something else, change it.
 *  - XCLK: if pin_xclk is PA8, it is driven via MCO1/HSI48 (matching
 *    WeAct's reference firmware's SystemClock_Config(), confirmed on real
 *    hardware via a standalone MCO1/PA8 toggle-counting diagnostic). Any
 *    other pin falls back to a HardwareTimer PWM, which is unverified.
 *
 * Verified against real hardware: compiled, flashed and run end-to-end
 * on a WeAct STM32H750 board (STMicroelectronics:stm32:GenH7:pnum=
 * WeActMiniH750VBTX) with an OV7725 camera module (SCCB address 0x21,
 * product ID 0x77) plugged into its onboard DCMI connector - sustained
 * runs show zero capture timeouts, zero corrupt/missing pixels, and a
 * clean, correctly-updating live image. Getting there needed several
 * real-hardware-only fixes beyond what porting the register table alone
 * caught: the DCMI HSPolarity setting needed to match WeAct's reference
 * exactly (hardcoded LOW, not per-sensor - an earlier per-sensor version
 * was based on an unreliable live read); XCLK needed to come from
 * MCO1/HSI48, not HSE (an earlier, never-actually-verified assumption);
 * and captured frames showed scattered corrupt pixels traced to a
 * D-Cache coherency bug - STM32duino's core enables the Cortex-M7
 * D-Cache unconditionally before setup() ever runs, and DCMI's DMA
 * writes captured pixels straight to RAM, bypassing the cache entirely,
 * so captureFrame() below explicitly cleans/invalidates the D-Cache
 * around each capture rather than relying on the CPU seeing what DMA
 * actually wrote.
 */

#include <Arduino.h>

// H7 and F7 only: both have a Cortex-M7 core with a D-Cache, which
// captureFrame() below depends on (SCB_CleanDCache_by_Addr()/
// SCB_InvalidateDCache_by_Addr() - see its comment). F4's Cortex-M4 core
// has no D-Cache and no such CMSIS functions at all, so it was dropped
// from this list rather than left as a claimed-but-broken target: it
// would fail to compile the moment captureFrame() is instantiated, not
// just misbehave at runtime, since Cortex-M4's core_cm4.h never declares
// those functions in the first place.
#if defined(STM32H7xx) || defined(STM32H7)
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_dcmi.h"
#define TINY_CAMERA_STM32_DMA_USES_DMAMUX 1
#elif defined(STM32F7xx)
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_dcmi.h"
#else
#error \
    "TinyCamera (STM32): no DCMI HAL header known for this STM32 family - add one above. (STM32F4 was intentionally dropped: its Cortex-M4 core has no D-Cache/CMSIS cache functions, which captureFrame() requires.)"
#endif

// esp32-camera's camera_config_t carries LEDC (ESP32 PWM peripheral) fields
// for the XCLK generator. TinyCamera.h sets them unconditionally in
// defaultConfig() for source compatibility across platforms; STM32 doesn't
// use LEDC (XCLK is generated via HardwareTimer, see startXclk() below), so
// these just need to exist as harmless constants.
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_0 0

#include "Driver/OV7725.h"
#include "Driver/SensorDriver.h"
#include "TinyCameraLogger.h"

// ---------------------------------------------------------------------------
// Public compatibility surface (global scope, mirroring esp_camera.h's C API)
// ---------------------------------------------------------------------------

enum pixformat_t {
  PIXFORMAT_RGB565,
  PIXFORMAT_JPEG,
};

enum framesize_t {
  FRAMESIZE_QQVGA,  // 160x120
  FRAMESIZE_QVGA,   // 320x240
  FRAMESIZE_VGA,    // 640x480
};

enum camera_fb_location_t { CAMERA_FB_IN_DRAM, CAMERA_FB_IN_PSRAM };
enum camera_grab_mode_t { CAMERA_GRAB_WHEN_EMPTY, CAMERA_GRAB_LATEST };

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

struct camera_config_t {
  int ledc_channel = 0;  // unused on STM32; kept for source compatibility
  int ledc_timer = 0;    // unused on STM32; kept for source compatibility
  int xclk_freq_hz = 20000000;
  pixformat_t pixel_format = PIXFORMAT_JPEG;
  framesize_t frame_size = FRAMESIZE_QVGA;
  // STM32-only: when both are >0, requests this exact output resolution
  // instead of frame_size's fixed preset - the active sensor driver
  // decides whether it can produce it (see SensorDriver::configure()'s
  // width/height parameters). OV7725Driver supports any 0<w<=640,
  // 0<h<=480 by scaling its full captured field of view down (via its
  // DSP's automatic scale/zoom block) to exactly this size in hardware,
  // rather than the software center-crop TinyCameraSTM32.h's example
  // sketches use to fit a smaller display - see Driver/OV7725.h's
  // configure() for specifics. The other three drivers still only
  // support frame_size's presets and ignore these fields.
  int custom_width = 0;
  int custom_height = 0;
  int jpeg_quality = 12;
  int fb_count = 1;                                    // always 1 on STM32
  camera_fb_location_t fb_location = CAMERA_FB_IN_DRAM;
  camera_grab_mode_t grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // unused on STM32

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

struct sensor_t {
  int (*set_brightness)(sensor_t *sensor, int level) = nullptr;
  int (*set_contrast)(sensor_t *sensor, int level) = nullptr;
  int (*set_saturation)(sensor_t *sensor, int level) = nullptr;
  int (*set_vflip)(sensor_t *sensor, int enable) = nullptr;
  int (*set_hmirror)(sensor_t *sensor, int enable) = nullptr;
};

esp_err_t esp_camera_init(const camera_config_t *config);
void esp_camera_deinit();
camera_fb_t *esp_camera_fb_get();
void esp_camera_fb_return(camera_fb_t *fb);
sensor_t *esp_camera_sensor_get();

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

namespace tiny_camera {
namespace stm32_detail {

// Largest JPEG frame accepted at VGA; grown/shrunk here if you need more
// headroom for a given quality/scene. Must be a multiple of 4 bytes.
constexpr size_t kMaxJpegSize = 40 * 1024;

// Fixed DMA resource used for DCMI transfers. Change if this collides with
// something else in your sketch.
constexpr uint32_t kDmaStreamIrqPriority = 1;

// Cortex-M7's D-Cache line size, on every STM32 part that has one - used
// to align/size the frame buffer for SCB_CleanDCache_by_Addr()/
// SCB_InvalidateDCache_by_Addr() in captureFrame() below.
constexpr size_t kDCacheLineSize = 32;

class Stm32Camera {
 public:
  bool begin(const camera_config_t &config) {
    config_ = config;
    if (config.pin_xclk < 0 || config.pin_pclk < 0 || config.pin_vsync < 0 ||
        config.pin_href < 0 || config.pin_sccb_sda < 0 ||
        config.pin_sccb_scl < 0 || config.pin_d0 < 0 || config.pin_d1 < 0 ||
        config.pin_d2 < 0 || config.pin_d3 < 0 || config.pin_d4 < 0 ||
        config.pin_d5 < 0 || config.pin_d6 < 0 || config.pin_d7 < 0) {
      // All 8 data pins are required, not just pin_d0: initDcmi() below
      // always configures DCMI_EXTEND_DATA_8B regardless of how many were
      // actually set, and initDcmiPins() silently skips any that are
      // still -1 - so a caller who forgot to wire/set one of pin_d1..
      // pin_d7 previously got a "successful" begin() and a camera that
      // captures garbage from floating data lines, with no error pointing
      // at the real cause.
      TinyCameraLogger.error(
          "camera_config_t is missing required pin(s) (xclk/pclk/vsync/href/"
          "sccb_sda/sccb_scl/d0-d7 - this backend only supports an 8-bit "
          "data bus)");
      return false;
    }

    TinyCameraLogger.debug("begin(): starting XCLK on pin %d",
                            config.pin_xclk);
    if (!startXclk()) {
      TinyCameraLogger.error("failed to start XCLK on pin %d",
                              config.pin_xclk);
      return false;
    }
    TinyCameraLogger.debug("begin(): XCLK started");
    delay(10);  // let the sensor's clock/PLL settle before SCCB traffic

    activeSensor_ = nullptr;
    // Only OV7725 ships a driver - see Driver/SensorDriver.h's header
    // comment for why OV2640/OV7670/OV5640 were removed. Kept as a
    // candidate-array loop (rather than calling ov7725_ directly) so
    // adding a second verified driver later is a one-line change, not a
    // restructuring.
    SensorDriver *candidates[] = {&ov7725_};
    for (SensorDriver *candidate : candidates) {
      TinyCameraLogger.debug("begin(): trying %s", candidate->name());
      candidate->begin(Wire, config.pin_sccb_sda, config.pin_sccb_scl,
                        config.pin_pwdn, config.pin_reset);
      TinyCameraLogger.debug("begin(): %s driver begin() done, calling detect()",
                              candidate->name());
      if (candidate->detect()) {
        TinyCameraLogger.debug("begin(): %s detect() -> true", candidate->name());
        activeSensor_ = candidate;
        break;
      }
      TinyCameraLogger.debug("begin(): %s detect() -> false", candidate->name());
    }
    if (activeSensor_ == nullptr) {
      // identifyUnsupportedSensor() recognizes OV2640/OV7670/OV5640's
      // chip IDs even though no driver exists to actually run them (see
      // Driver/SensorDriver.h), so a real sensor of one of those types
      // gets named here instead of a bare "not found".
      const char *unsupported = identifyUnsupportedSensor(Wire);
      if (unsupported != nullptr) {
        TinyCameraLogger.error(
            "detected a %s sensor on the SCCB bus (sda=%d, scl=%d), but "
            "this library no longer ships a driver for it - only OV7725 "
            "is currently supported (see Driver/SensorDriver.h)",
            unsupported, config.pin_sccb_sda, config.pin_sccb_scl);
      } else {
        TinyCameraLogger.error(
            "no supported sensor found on SCCB bus (sda=%d, scl=%d) - "
            "only OV7725 is currently supported",
            config.pin_sccb_sda, config.pin_sccb_scl);
      }
      reportI2cBusScan(Wire);
      stopXclk();
      return false;
    }
    TinyCameraLogger.info("%s detected, applying common init",
                           activeSensor_->name());
    activeSensor_->applyCommonInit();

    bool jpeg = (config.pixel_format == PIXFORMAT_JPEG);
    if (config.custom_width > 0 && config.custom_height > 0) {
      frameWidth_ = (size_t)config.custom_width;
      frameHeight_ = (size_t)config.custom_height;
    } else {
      frameWidth_ = width(config.frame_size);
      frameHeight_ = height(config.frame_size);
    }
    if (jpeg && !activeSensor_->supportsJpeg()) {
      TinyCameraLogger.error(
          "%s has no hardware JPEG encoder - use PIXFORMAT_RGB565 instead",
          activeSensor_->name());
      stopXclk();
      return false;
    }
    if (!activeSensor_->configure(jpeg, frameWidth_, frameHeight_)) {
      TinyCameraLogger.error(
          "%s: unsupported pixel_format/frame_size combination (%ux%u, %s)",
          activeSensor_->name(), (unsigned)frameWidth_, (unsigned)frameHeight_,
          jpeg ? "JPEG" : "RGB565");
      stopXclk();
      return false;
    }
    activeSensor_->setQuality(config.jpeg_quality);

    // Rounded up to a D-cache line (32 bytes on Cortex-M7) and allocated
    // on that same alignment: SCB_CleanDCache_by_Addr()/
    // SCB_InvalidateDCache_by_Addr() below operate on whole cache lines,
    // so an unaligned or odd-sized buffer would touch (and for Clean,
    // silently corrupt-by-omission) bytes belonging to a neighboring heap
    // allocation that happens to share its first/last cache line.
    size_t bufSize = jpeg ? kMaxJpegSize : frameWidth_ * frameHeight_ * 2;
    bufSize = (bufSize + kDCacheLineSize - 1) & ~(kDCacheLineSize - 1);
    frameBuffer_ = (uint8_t *)aligned_alloc(kDCacheLineSize, bufSize);
    if (frameBuffer_ == nullptr) {
      TinyCameraLogger.error("failed to allocate %u-byte frame buffer",
                              (unsigned)bufSize);
      stopXclk();
      return false;
    }
    frameBufferCapacity_ = bufSize;

    if (!initDcmiPins(config) || !initDcmi(jpeg)) {
      TinyCameraLogger.error("DCMI/DMA initialization failed");
      free(frameBuffer_);
      frameBuffer_ = nullptr;
      stopXclk();
      return false;
    }

    active_ = true;
    TinyCameraLogger.info("STM32 camera ready (%ux%u, %s)",
                           (unsigned)frameWidth_, (unsigned)frameHeight_,
                           jpeg ? "JPEG" : "RGB565");
    return true;
  }

  void end() {
    if (!active_) return;
    HAL_DCMI_Stop(&hdcmi_);
    HAL_DMA_DeInit(&hdma_);
    HAL_DCMI_DeInit(&hdcmi_);
    stopXclk();
    if (frameBuffer_ != nullptr) {
      free(frameBuffer_);
      frameBuffer_ = nullptr;
    }
    active_ = false;
  }

  bool isActive() const { return active_; }

  // Backs TinyCamera::setCustomFrameSize() on STM32 - see its doc comment
  // in TinyCamera.h. Unlike ESP32's set_res_raw()-based implementation
  // (which only scales down from whatever frame_size is currently
  // configured), OV7725Driver::configure() always reconfigures from the
  // sensor's full field of view regardless of prior state, so this
  // accepts any 0<width<=640, 0<height<=480 at any time, not just
  // shrinking. The frame buffer is reallocated if the new size needs
  // more room than the one begin() originally allocated.
  bool setCustomFrameSize(int width, int height) {
    if (!active_) {
      TinyCameraLogger.error(
          "setCustomFrameSize() called while camera is not active");
      return false;
    }
    if (width <= 0 || height <= 0) {
      TinyCameraLogger.error("setCustomFrameSize: invalid size %dx%d", width,
                              height);
      return false;
    }
    bool jpeg = (config_.pixel_format == PIXFORMAT_JPEG);
    if (!activeSensor_->configure(jpeg, width, height)) {
      TinyCameraLogger.error(
          "setCustomFrameSize: %s rejected %dx%d (%s)", activeSensor_->name(),
          width, height, jpeg ? "JPEG" : "RGB565");
      return false;
    }
    activeSensor_->setQuality(config_.jpeg_quality);

    size_t newBufSize = jpeg ? kMaxJpegSize : (size_t)width * (size_t)height * 2;
    newBufSize = (newBufSize + kDCacheLineSize - 1) & ~(kDCacheLineSize - 1);
    if (newBufSize > frameBufferCapacity_) {
      uint8_t *newBuf = (uint8_t *)aligned_alloc(kDCacheLineSize, newBufSize);
      if (newBuf == nullptr) {
        TinyCameraLogger.error(
            "setCustomFrameSize: failed to allocate %u-byte frame buffer",
            (unsigned)newBufSize);
        return false;
      }
      free(frameBuffer_);
      frameBuffer_ = newBuf;
      frameBufferCapacity_ = newBufSize;
    }

    frameWidth_ = (size_t)width;
    frameHeight_ = (size_t)height;
    TinyCameraLogger.info("setCustomFrameSize: %s now %ux%u",
                           activeSensor_->name(), (unsigned)frameWidth_,
                           (unsigned)frameHeight_);
    return true;
  }

  camera_fb_t *captureFrame() {
    if (!active_) {
      TinyCameraLogger.error("captureFrame() called while camera is not active");
      return nullptr;
    }

    captureDone_ = false;
    bool jpeg = (config_.pixel_format == PIXFORMAT_JPEG);
    // Fill with a sentinel before every capture: makes "DCMI never wrote
    // this byte" distinguishable from "DCMI wrote a genuine 0", and stops
    // findJpegLength() below from matching a stale EOI marker left over
    // in the tail of the buffer by a previous, larger JPEG frame.
    memset(frameBuffer_, 0xAA, frameBufferCapacity_);
    // Cortex-M7's D-Cache (enabled unconditionally by STM32duino's own
    // core startup, before setup() ever runs - see main.cpp's
    // SCB_EnableDCache()) is write-back: the memset() above may still be
    // sitting in cache lines, not yet in physical RAM. DCMI's DMA writes
    // captured pixels straight to RAM, bypassing the cache entirely, so
    // without this Clean call the sentinel fill might never actually
    // reach the RAM DMA is about to write into - only the CPU's cached
    // copy. (Confirmed live: with a matching InvalidateDCache below but
    // no Clean here, the exact "untouched" pattern this comment describes
    // is what live testing on real hardware showed - scattered runs of
    // stale bytes covering ~10% of every frame, all consistent with a
    // cache-coherency problem rather than a real capture gap.)
    SCB_CleanDCache_by_Addr((uint32_t *)frameBuffer_, (int32_t)frameBufferCapacity_);
    size_t words = frameBufferCapacity_ / 4;
    if (HAL_DCMI_Start_DMA(&hdcmi_, DCMI_MODE_SNAPSHOT,
                            (uint32_t)frameBuffer_, words) != HAL_OK) {
      TinyCameraLogger.error("HAL_DCMI_Start_DMA failed");
      return nullptr;
    }

    uint32_t start = millis();
    while (!captureDone_) {
      if (millis() - start > 1000) {
        TinyCameraLogger.warn(
            "capture timed out after 1000ms (check VSYNC/HSYNC/PCLK wiring)");
        HAL_DCMI_Stop(&hdcmi_);  // leave DCMI in a clean state so the next
                                  // captureFrame() can start, not stuck
        return nullptr;  // capture timeout
      }
    }
    HAL_DCMI_Stop(&hdcmi_);  // snapshot done; clean state for next capture

    // Discards any D-Cache lines for this buffer that are still holding
    // the pre-capture sentinel (or an earlier frame's data) instead of
    // what DMA just wrote directly to RAM, so the reads below - and the
    // caller's - see the real captured bytes. This is Invalidate, not
    // Clean+Invalidate: nothing the CPU wrote to this buffer since the
    // Clean call above needs preserving, so a plain discard is correct
    // and marginally cheaper.
    SCB_InvalidateDCache_by_Addr((uint32_t *)frameBuffer_,
                                  (int32_t)frameBufferCapacity_);

    fb_.buf = frameBuffer_;
    fb_.width = frameWidth_;
    fb_.height = frameHeight_;
    fb_.format = config_.pixel_format;
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    fb_.timestamp = tv;

    if (jpeg) {
      size_t len = findJpegLength(frameBuffer_, frameBufferCapacity_);
      if (len == 0) {
        TinyCameraLogger.warn(
            "no JPEG EOI marker found in capture buffer (frame likely "
            "corrupt or larger than kMaxJpegSize)");
        return nullptr;
      }
      fb_.len = len;
    } else {
      fb_.len = frameWidth_ * frameHeight_ * 2;
    }
    // (success is logged generically by TinyCamera::captureFrame())
    return &fb_;
  }

  // Called from the DCMI frame-complete callback (see below).
  void onFrameComplete() { captureDone_ = true; }

  sensor_t *sensorApi() { return &sensorApi_; }
  SensorDriver *activeSensor() { return activeSensor_; }

  DCMI_HandleTypeDef hdcmi_{};
  DMA_HandleTypeDef hdma_{};

 private:
  static size_t width(framesize_t fs) {
    switch (fs) {
      case FRAMESIZE_QQVGA: return 160;
      case FRAMESIZE_QVGA: return 320;
      case FRAMESIZE_VGA: return 640;
    }
    return 320;
  }
  static size_t height(framesize_t fs) {
    switch (fs) {
      case FRAMESIZE_QQVGA: return 120;
      case FRAMESIZE_QVGA: return 240;
      case FRAMESIZE_VGA: return 480;
    }
    return 240;
  }

  // Scans backward from the end of a possibly-oversized JPEG capture
  // buffer for the JPEG end-of-image marker (0xFFD9), since DCMI's JPEG
  // snapshot mode fills (part of) the buffer without reporting length.
  static size_t findJpegLength(const uint8_t *buf, size_t capacity) {
    for (size_t i = capacity - 2; i > 0; i--) {
      if (buf[i] == 0xFF && buf[i + 1] == 0xD9) return i + 2;
    }
    return 0;
  }

  // Generates XCLK for the sensor. On PA8 (the MCO1 pin on every STM32
  // F4/F7/H7 part), this outputs a clock derived from HSI48 (the internal
  // 48MHz RC oscillator) via the RCC clock-output peripheral - simpler and
  // more accurate than a timer PWM, since it doesn't depend on a separate
  // timer's clock tree. HSI48, not HSE, matches WeAct's own STM32H750 DCMI
  // reference firmware exactly: its SystemClock_Config() enables HSI48
  // (RCC_OSCILLATORTYPE_HSI48) and configures MCO1 = HSI48/4 = 12MHz as
  // standard boot setup, independent of which sensor is detected - an
  // earlier version of this function used HSE instead, which was an
  // unverified guess, not sourced from the reference firmware (which this
  // comment previously, incorrectly, claimed it was). HSI48 also avoids
  // depending on the board's actual HSE crystal frequency/precision, which
  // varies by board and isn't otherwise used by this library. For any
  // other pin, falls back to a HardwareTimer PWM (less validated - check
  // your sensor tolerates the resulting XCLK before relying on it).
  bool startXclk() {
    if (config_.pin_xclk < 0) return true;  // module has its own oscillator
    PinName pn = digitalPinToPinName(config_.pin_xclk);

    if (pn == PA_8) {
      // Enable HSI48 if it isn't already (harmless/no-op if your sketch's
      // own clock config already turned it on, e.g. for USB CDC support).
      RCC_OscInitTypeDef oscInit{};
      oscInit.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
      oscInit.HSI48State = RCC_HSI48_ON;
      HAL_RCC_OscConfig(&oscInit);

      constexpr uint32_t kHsi48Hz = 48000000UL;
      uint32_t divider =
          (kHsi48Hz + config_.xclk_freq_hz / 2) / config_.xclk_freq_hz;
      if (divider < 1) divider = 1;
      if (divider > 15) divider = 15;
      configureAfPin(config_.pin_xclk, GPIO_AF0_MCO);
      HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI48, mcoDivider(divider));
      return true;
    }

    TIM_TypeDef *inst = (TIM_TypeDef *)pinmap_peripheral(pn, PinMap_TIM);
    if (inst == nullptr) return false;
    xclkTimer_ = new HardwareTimer(inst);
    uint32_t channel = STM_PIN_CHANNEL(pinmap_function(pn, PinMap_TIM));
    xclkTimer_->setMode(channel, TIMER_OUTPUT_COMPARE_PWM1, config_.pin_xclk);
    xclkTimer_->setOverflow(config_.xclk_freq_hz, HERTZ_FORMAT);
    xclkTimer_->setCaptureCompare(channel, 50, PERCENT_COMPARE_FORMAT);
    xclkTimer_->resume();
    return true;
  }

  static uint32_t mcoDivider(uint32_t divider) {
    switch (divider) {
      case 1: return RCC_MCODIV_1;
      case 2: return RCC_MCODIV_2;
      case 3: return RCC_MCODIV_3;
      case 4: return RCC_MCODIV_4;
      case 5: return RCC_MCODIV_5;
      case 6: return RCC_MCODIV_6;
      case 7: return RCC_MCODIV_7;
      case 8: return RCC_MCODIV_8;
      case 9: return RCC_MCODIV_9;
      case 10: return RCC_MCODIV_10;
      case 11: return RCC_MCODIV_11;
      case 12: return RCC_MCODIV_12;
      case 13: return RCC_MCODIV_13;
      case 14: return RCC_MCODIV_14;
      default: return RCC_MCODIV_15;
    }
  }

  void stopXclk() {
    if (xclkTimer_ != nullptr) {
      xclkTimer_->pause();
      delete xclkTimer_;
      xclkTimer_ = nullptr;
    }
  }

  // Configures every DCMI signal pin for alternate-function push-pull mode.
  // Uses direct HAL_GPIO_Init (rather than a board PinMap_DCMI table, which
  // not all STM32duino variants define) so this works on any board once you
  // pass the correct pins.
  bool initDcmiPins(const camera_config_t &config) {
    const int pins[] = {config.pin_pclk, config.pin_vsync, config.pin_href,
                         config.pin_d0,   config.pin_d1,    config.pin_d2,
                         config.pin_d3,   config.pin_d4,    config.pin_d5,
                         config.pin_d6,   config.pin_d7};
    for (int pin : pins) {
      if (pin < 0) continue;  // fewer than 8 data lines is not supported here
      if (!configureAfPin(pin, kDcmiAlternateFunction)) return false;
    }
    return true;
  }

  bool configureAfPin(int pin, uint32_t af) {
    PinName pn = digitalPinToPinName(pin);
    if (pn == NC) return false;
    GPIO_TypeDef *port = get_GPIO_Port(STM_PORT(pn));
    if (port == nullptr) return false;
    set_GPIO_Port_Clock(STM_PORT(pn));

    GPIO_InitTypeDef init{};
    init.Pin = (uint32_t)(1U << STM_PIN(pn));
    init.Mode = GPIO_MODE_AF_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    init.Alternate = af;
    HAL_GPIO_Init(port, &init);
    return true;
  }

  // DCMI/DMA settings below (polarities, DMA_REQUEST_DCMI/DMAMUX, stream,
  // FIFO/priority) match WeAct's own published STM32H750 DCMI reference
  // firmware for this board's camera connector (see docs/Tutorial.md), not
  // a generic guess - only DMA_MODE_NORMAL differs, since that example
  // streams continuously to an LCD (DMA_CIRCULAR) while TinyCamera captures
  // one snapshot frame per captureFrame() call.
  bool initDcmi(bool jpeg) {
    __HAL_RCC_DCMI_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdcmi_.Instance = DCMI;
    hdcmi_.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
    hdcmi_.Init.PCKPolarity = DCMI_PCKPOLARITY_RISING;
    hdcmi_.Init.VSPolarity = DCMI_VSPOLARITY_LOW;
    // HSPolarity is DCMI_HSPOLARITY_LOW unconditionally, matching WeAct's
    // own reference firmware exactly: its MX_DCMI_Init() sets this once,
    // before Camera_Init_Device() even runs its SCCB auto-detection, and
    // never varies it by which of the four sensors was found. An earlier
    // version of this code switched to DCMI_HSPOLARITY_HIGH for OV7670/
    // OV7725 based on a live-hardware read that, in hindsight (given every
    // other capture attempt at the time was also timing out for unrelated
    // reasons - see Driver/OV7725.h and startXclk() below), was not a
    // reliable confirmation. Trust the reference firmware instead.
    hdcmi_.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
    hdcmi_.Init.CaptureRate = DCMI_CR_ALL_FRAME;
    hdcmi_.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
    hdcmi_.Init.JPEGMode = jpeg ? DCMI_JPEG_ENABLE : DCMI_JPEG_DISABLE;
    if (HAL_DCMI_Init(&hdcmi_) != HAL_OK) return false;

    hdma_.Instance = DMA1_Stream0;
#if defined(TINY_CAMERA_STM32_DMA_USES_DMAMUX)
    hdma_.Init.Request = DMA_REQUEST_DCMI;
#else
    hdma_.Init.Channel = DMA_CHANNEL_1;
#endif
    hdma_.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_.Init.MemInc = DMA_MINC_ENABLE;
    hdma_.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma_.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma_.Init.Mode = DMA_NORMAL;
    // DMA_PRIORITY_VERY_HIGH (not WeAct's DMA_PRIORITY_LOW): live testing
    // showed scattered runs of dropped/corrupted pixels throughout each
    // captured frame (not a single clean missing region, which a register/
    // window bug would produce) - present even with zero Serial output
    // during capture, ruling out USB-CDC print activity as the cause.
    // WeAct's reference never runs Arduino's SysTick (1kHz) or a USB CDC
    // stack concurrently with its DCMI capture, so its identical low-
    // priority/direct-mode DMA config never has to compete with those for
    // AHB bus arbitration the way this sketch's does. Raising this
    // stream's priority reduces how often that contention wins.
    hdma_.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    hdma_.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_) != HAL_OK) return false;

    __HAL_LINKDMA(&hdcmi_, DMA_Handle, hdma_);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, kDmaStreamIrqPriority, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DCMI_IRQn, kDmaStreamIrqPriority, 0);
    HAL_NVIC_EnableIRQ(DCMI_IRQn);

    activeInstance_ = this;
    return true;
  }

  // DCMI-generation-independent constant: DCMI's alternate function is
  // AF13 on the F4/F7/H7 STM32 families that expose the peripheral.
  static constexpr uint32_t kDcmiAlternateFunction = GPIO_AF13_DCMI;

  camera_config_t config_{};
  OV7725Driver ov7725_;
  SensorDriver *activeSensor_ = nullptr;
  sensor_t sensorApi_{};
  camera_fb_t fb_{};
  uint8_t *frameBuffer_ = nullptr;
  size_t frameBufferCapacity_ = 0;
  size_t frameWidth_ = 0;
  size_t frameHeight_ = 0;
  HardwareTimer *xclkTimer_ = nullptr;
  volatile bool captureDone_ = false;
  bool active_ = false;

 public:
  static Stm32Camera *activeInstance_;
};

inline Stm32Camera *Stm32Camera::activeInstance_ = nullptr;

inline Stm32Camera &instance() {
  static Stm32Camera camera;
  return camera;
}

// ---- sensor_t glue: forwards to the active (auto-detected) sensor driver
// of the active camera. Only reachable while a camera is active (these are
// only wired up in esp_camera_init() after a successful begin()), so
// activeSensor() is never null here.
inline int sensorSetBrightness(sensor_t *, int level) {
  return instance().activeSensor()->setBrightness(level);
}
inline int sensorSetContrast(sensor_t *, int level) {
  return instance().activeSensor()->setContrast(level);
}
inline int sensorSetSaturation(sensor_t *, int level) {
  return instance().activeSensor()->setSaturation(level);
}
inline int sensorSetVflip(sensor_t *, int enable) {
  return instance().activeSensor()->setVflip(enable != 0);
}
inline int sensorSetHmirror(sensor_t *, int enable) {
  return instance().activeSensor()->setHmirror(enable != 0);
}

}  // namespace stm32_detail
}  // namespace tiny_camera

// ---------------------------------------------------------------------------
// HAL interrupt plumbing - forwards DCMI/DMA IRQs and the frame-complete
// event to the active Stm32Camera instance.
// ---------------------------------------------------------------------------

extern "C" void DCMI_IRQHandler(void) {
  if (tiny_camera::stm32_detail::Stm32Camera::activeInstance_ != nullptr) {
    HAL_DCMI_IRQHandler(
        &tiny_camera::stm32_detail::Stm32Camera::activeInstance_->hdcmi_);
  }
}

extern "C" void DMA1_Stream0_IRQHandler(void) {
  if (tiny_camera::stm32_detail::Stm32Camera::activeInstance_ != nullptr) {
    HAL_DMA_IRQHandler(
        &tiny_camera::stm32_detail::Stm32Camera::activeInstance_->hdma_);
  }
}

extern "C" void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi) {
  (void)hdcmi;
  if (tiny_camera::stm32_detail::Stm32Camera::activeInstance_ != nullptr) {
    tiny_camera::stm32_detail::Stm32Camera::activeInstance_->onFrameComplete();
  }
}

// ---------------------------------------------------------------------------
// Public esp_camera_* API implementation
// ---------------------------------------------------------------------------

inline esp_err_t esp_camera_init(const camera_config_t *config) {
  auto &cam = tiny_camera::stm32_detail::instance();
  if (!cam.begin(*config)) return ESP_FAIL;

  sensor_t *api = cam.sensorApi();
  api->set_brightness = tiny_camera::stm32_detail::sensorSetBrightness;
  api->set_contrast = tiny_camera::stm32_detail::sensorSetContrast;
  api->set_saturation = tiny_camera::stm32_detail::sensorSetSaturation;
  api->set_vflip = tiny_camera::stm32_detail::sensorSetVflip;
  api->set_hmirror = tiny_camera::stm32_detail::sensorSetHmirror;
  return ESP_OK;
}

inline void esp_camera_deinit() { tiny_camera::stm32_detail::instance().end(); }

inline camera_fb_t *esp_camera_fb_get() {
  return tiny_camera::stm32_detail::instance().captureFrame();
}

inline void esp_camera_fb_return(camera_fb_t *fb) {
  (void)fb;  // single, statically-owned frame buffer - nothing to release
}

inline sensor_t *esp_camera_sensor_get() {
  auto &cam = tiny_camera::stm32_detail::instance();
  return cam.isActive() ? cam.sensorApi() : nullptr;
}

// Not part of esp32-camera's own API (esp_camera_* is reserved for
// functions that mirror it) - backs TinyCamera::setCustomFrameSize() on
// STM32, called from TinyCamera.h's ARDUINO_ARCH_STM32 branch. See
// Stm32Camera::setCustomFrameSize()'s doc comment above for behavior.
inline bool tinyCameraStm32SetCustomFrameSize(int width, int height) {
  return tiny_camera::stm32_detail::instance().setCustomFrameSize(width,
                                                                    height);
}
