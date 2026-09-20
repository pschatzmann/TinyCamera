#pragma once
/**
 * RP2040 compatibility layer for TinyCamera.
 *
 * arduino-pico's bundled camera library is PicoCamera
 * (https://github.com/umeiko/PicoCamera, umeiko), which - as of the
 * rp2040 core versions this was written against - already reproduces
 * esp32-camera's camera_config_t/camera_fb_t/pixformat_t/framesize_t/
 * sensor_t shapes closely, but under a pico_camera_*() function API
 * (pico_camera_init/pico_camera_fb_get/pico_camera_fb_return/...), not
 * esp_camera_*(). Its camera_config_t is also missing a few fields
 * esp32-camera's has (ledc_channel/ledc_timer/grab_mode, and a
 * CAMERA_FB_IN_DRAM enumerator - it has PICO_CAMERA_FB_IN_SRAM/
 * PICO_CAMERA_FB_IN_PSRAM/PICO_CAMERA_FB_AUTO instead) that
 * TinyCamera.h's shared defaultConfig() sets unconditionally on every
 * platform, for source compatibility across ESP32/RP2040/STM32.
 *
 * This file bridges that gap the same way TinyCameraSTM32.h bridges
 * STM32 to the esp32-camera surface: it defines the esp_camera_*()
 * function names and an esp32-camera-shaped camera_config_t that
 * TinyCamera.h/TinyCameraConvert.h actually call, and forwards them to
 * PicoCamera underneath. Unlike STM32 (which has no existing rival
 * implementation to collide with), PicoCamera already defines
 * camera_config_t/camera_fb_t/pixformat_t/framesize_t/sensor_t itself -
 * so its own #include is wrapped in a namespace below, sequestering
 * those names out of the way of the ones this file defines at global
 * scope. pixformat_t/framesize_t/sensor_t/camera_fb_t are then aliased
 * (not redefined) from PicoCamera's own versions, rather than hand-copied,
 * so their values/layout can't drift out of sync with what the driver
 * underneath actually implements - only camera_config_t needs a real,
 * separate superset type, converted field-by-field in esp_camera_init()
 * below.
 *
 * NOT independently verified on real RP2040 + camera hardware (unlike
 * TinyCameraSTM32.h, which has been) - only compiled. PicoCamera's own
 * examples are the closest thing to a hardware-verified reference for
 * this backend; treat this bridge as unproven until you've tested it
 * yourself.
 *
 * Coverage / limitations (see docs/Tutorial.md):
 *  - camera_config_t::custom_width/custom_height (STM32's arbitrary-size
 *    hardware-scaling hook) has no RP2040/PicoCamera equivalent -
 *    TinyCamera::setCustomFrameSize() already returns false unconditionally
 *    on RP2040 (see TinyCamera.h), so this file adds no such fields.
 *  - grab_mode has no PicoCamera equivalent and is ignored, same as STM32.
 *  - TinyCameraConvert.h's toJpg()/toBmp()/toRgb888()/toRgb565() need
 *    img_converters.h, which PicoCamera does not provide (no hardware/
 *    vendor JPEG codec here) - use TinyCameraConvertSoftware.h's
 *    *Software() functions instead, exactly as on STM32.
 */

#include <Arduino.h>

// esp32-camera's camera_config_t carries LEDC (ESP32 PWM peripheral) fields
// for the XCLK generator. TinyCamera.h's shared defaultConfig() sets them
// unconditionally for source compatibility across platforms; PicoCamera
// drives XCLK itself and has no LEDC fields at all, so these just need to
// exist as harmless constants - same reasoning as TinyCameraSTM32.h.
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_0 0

namespace tiny_camera_pico_camera {
#include <PicoCamera.h>
}  // namespace tiny_camera_pico_camera

// ---------------------------------------------------------------------------
// Public compatibility surface (global scope, mirroring esp_camera.h's C API)
// ---------------------------------------------------------------------------

using pixformat_t = tiny_camera_pico_camera::pixformat_t;
using framesize_t = tiny_camera_pico_camera::framesize_t;
using sensor_t = tiny_camera_pico_camera::sensor_t;
using camera_fb_t = tiny_camera_pico_camera::camera_fb_t;

// pixformat_t/framesize_t are unscoped enums declared inside the
// tiny_camera_pico_camera namespace above, so their enumerators
// (PIXFORMAT_JPEG, FRAMESIZE_QVGA, ...) live in that namespace too, not
// globally - esp32-camera's own esp_camera.h has them bare at global
// scope, which is what TinyCamera.h/TinyCameraPins.h/every example
// (PIXFORMAT_JPEG, FRAMESIZE_QVGA, ...) expects to find unqualified.
// Pulling each one in individually (rather than `using namespace
// tiny_camera_pico_camera`) keeps PicoCamera's own internal names
// (pico_camera_init(), resolution[], PICO_CAMERA_FB_AUTO, ...) out of
// global scope.
using tiny_camera_pico_camera::PIXFORMAT_RGB565;
using tiny_camera_pico_camera::PIXFORMAT_YUV422;
using tiny_camera_pico_camera::PIXFORMAT_GRAYSCALE;
using tiny_camera_pico_camera::PIXFORMAT_JPEG;
using tiny_camera_pico_camera::PIXFORMAT_INVALID;
using tiny_camera_pico_camera::FRAMESIZE_96X96;
using tiny_camera_pico_camera::FRAMESIZE_QQVGA;
using tiny_camera_pico_camera::FRAMESIZE_QCIF;
using tiny_camera_pico_camera::FRAMESIZE_HQVGA;
using tiny_camera_pico_camera::FRAMESIZE_240X240;
using tiny_camera_pico_camera::FRAMESIZE_QVGA;
using tiny_camera_pico_camera::FRAMESIZE_CIF;
using tiny_camera_pico_camera::FRAMESIZE_HVGA;
using tiny_camera_pico_camera::FRAMESIZE_VGA;
using tiny_camera_pico_camera::FRAMESIZE_SVGA;
using tiny_camera_pico_camera::FRAMESIZE_XGA;
using tiny_camera_pico_camera::FRAMESIZE_HD;
using tiny_camera_pico_camera::FRAMESIZE_SXGA;
using tiny_camera_pico_camera::FRAMESIZE_UXGA;
using tiny_camera_pico_camera::FRAMESIZE_INVALID;

enum camera_fb_location_t { CAMERA_FB_IN_DRAM, CAMERA_FB_IN_PSRAM };
enum camera_grab_mode_t { CAMERA_GRAB_WHEN_EMPTY, CAMERA_GRAB_LATEST };

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1

struct camera_config_t {
  int ledc_channel = 0;  // unused on RP2040; kept for source compatibility
  int ledc_timer = 0;    // unused on RP2040; kept for source compatibility
  int xclk_freq_hz = 10000000;
  pixformat_t pixel_format = tiny_camera_pico_camera::PIXFORMAT_JPEG;
  framesize_t frame_size = tiny_camera_pico_camera::FRAMESIZE_QVGA;
  int jpeg_quality = 12;
  int fb_count = 1;
  camera_fb_location_t fb_location = CAMERA_FB_IN_DRAM;
  camera_grab_mode_t grab_mode = CAMERA_GRAB_WHEN_EMPTY;  // unused - PicoCamera has no grab-mode equivalent

  // PicoCamera-specific: which RP2040 I2C peripheral (0 or 1) the SCCB bus
  // uses - see PicoCamera.h's camera_config_t::pin_sccb_sda doc comment for
  // the exact pin<->port mapping and the "reuse an already-initialized bus"
  // (-1) mode. Defaults to port 0, PicoCamera's own default.
  int sccb_i2c_port = 0;

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

inline esp_err_t esp_camera_init(const camera_config_t *config) {
  tiny_camera_pico_camera::camera_config_t pc = {};
  pc.pin_pwdn = config->pin_pwdn;
  pc.pin_reset = config->pin_reset;
  pc.pin_xclk = config->pin_xclk;
  pc.pin_sccb_sda = config->pin_sccb_sda;
  pc.pin_sccb_scl = config->pin_sccb_scl;
  pc.pin_d0 = config->pin_d0;
  pc.pin_d1 = config->pin_d1;
  pc.pin_d2 = config->pin_d2;
  pc.pin_d3 = config->pin_d3;
  pc.pin_d4 = config->pin_d4;
  pc.pin_d5 = config->pin_d5;
  pc.pin_d6 = config->pin_d6;
  pc.pin_d7 = config->pin_d7;
  pc.pin_vsync = config->pin_vsync;
  pc.pin_href = config->pin_href;
  pc.pin_pclk = config->pin_pclk;
  pc.xclk_freq_hz = config->xclk_freq_hz;
  pc.sccb_i2c_port = config->sccb_i2c_port;
  pc.pixel_format = config->pixel_format;
  pc.frame_size = config->frame_size;
  pc.jpeg_quality = config->jpeg_quality;
  pc.fb_count = config->fb_count;
  pc.fb_location = (config->fb_location == CAMERA_FB_IN_PSRAM)
                        ? tiny_camera_pico_camera::PICO_CAMERA_FB_IN_PSRAM
                        : tiny_camera_pico_camera::PICO_CAMERA_FB_IN_SRAM;

  auto err = tiny_camera_pico_camera::pico_camera_init(&pc);
  // PICO_CAMERA_OK is a #define (see pico_camera.h), not a namespaced
  // enumerator - macros aren't scoped by C++ namespaces, so it's used
  // unqualified here even though pico_camera_init() itself is qualified.
  return (err == PICO_CAMERA_OK) ? ESP_OK : (esp_err_t)err;
}

inline void esp_camera_deinit() {
  tiny_camera_pico_camera::pico_camera_deinit();
}

inline camera_fb_t *esp_camera_fb_get() {
  return tiny_camera_pico_camera::pico_camera_fb_get();
}

inline void esp_camera_fb_return(camera_fb_t *fb) {
  tiny_camera_pico_camera::pico_camera_fb_return(fb);
}

inline sensor_t *esp_camera_sensor_get() {
  return tiny_camera_pico_camera::pico_camera_sensor_get();
}
