#pragma once
/**
 * Pin presets for common ESP32/ESP32-S2/ESP32-S3 camera boards, plus the
 * WeAct STM32H750's onboard DCMI camera connector. These fill a
 * camera_config_t (defined by esp_camera.h, the RP2040-compatible
 * Camera.h, or TinyCameraSTM32.h) with the pin assignments of a specific
 * board. RP2040 boards and most STM32 boards have no de-facto-standard
 * camera connector, so no presets are provided for those: fill in
 * camera_config_t::pin_* yourself (see the STM32 section of
 * docs/Tutorial.md for the STM32/DCMI pin fields).
 *
 * The ESP32/S2/S3 pin tables below are sourced from the ESP32 Arduino
 * core's own bundled CameraWebServer example (libraries/ESP32/examples/
 * Camera/CameraWebServer/camera_pins.h), which is Espressif's own
 * reference list of board pinouts. Many generic/no-name "ESP32-S3-CAM"
 * boards reuse the ESP32S3_EYE pinout specifically, since that is
 * Espressif's own official reference design - worth trying first for an
 * unidentified S3 camera board.
 */

#include "TinyCamera.h"

namespace tiny_camera {

/// AI-Thinker ESP32-CAM module
inline void setPinsAiThinker(camera_config_t &config) {
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.pin_xclk = 0;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_d7 = 35;
  config.pin_d6 = 34;
  config.pin_d5 = 39;
  config.pin_d4 = 36;
  config.pin_d3 = 21;
  config.pin_d2 = 19;
  config.pin_d1 = 18;
  config.pin_d0 = 5;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_pclk = 22;
}

/// ESP-EYE module
inline void setPinsEspEye(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 4;
  config.pin_sccb_sda = 18;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 36;
  config.pin_d6 = 37;
  config.pin_d5 = 38;
  config.pin_d4 = 39;
  config.pin_d3 = 35;
  config.pin_d2 = 14;
  config.pin_d1 = 13;
  config.pin_d0 = 34;
  config.pin_vsync = 5;
  config.pin_href = 27;
  config.pin_pclk = 25;
}

/// M5Stack (PSRAM) camera module - the original "M5Camera" module.
/// See setPinsM5StackV2()/setPinsM5StackWide() for other M5Stack variants.
inline void setPinsM5Stack(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = 15;
  config.pin_xclk = 27;
  config.pin_sccb_sda = 25;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 19;
  config.pin_d6 = 36;
  config.pin_d5 = 18;
  config.pin_d4 = 39;
  config.pin_d3 = 5;
  config.pin_d2 = 34;
  config.pin_d1 = 35;
  config.pin_d0 = 32;
  config.pin_vsync = 22;
  config.pin_href = 26;
  config.pin_pclk = 21;
}

/// ESP32-WROVER-KIT
inline void setPinsWroverKit(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 21;
  config.pin_sccb_sda = 26;
  config.pin_sccb_scl = 27;
  config.pin_d7 = 35;
  config.pin_d6 = 34;
  config.pin_d5 = 39;
  config.pin_d4 = 36;
  config.pin_d3 = 19;
  config.pin_d2 = 18;
  config.pin_d1 = 5;
  config.pin_d0 = 4;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_pclk = 22;
}

/// M5Stack camera module, version B ("M5Camera version B" / M5STACK_V2_PSRAM)
inline void setPinsM5StackV2(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = 15;
  config.pin_xclk = 27;
  config.pin_sccb_sda = 22;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 19;
  config.pin_d6 = 36;
  config.pin_d5 = 18;
  config.pin_d4 = 39;
  config.pin_d3 = 5;
  config.pin_d2 = 34;
  config.pin_d1 = 35;
  config.pin_d0 = 32;
  config.pin_vsync = 25;
  config.pin_href = 26;
  config.pin_pclk = 21;
}

/// M5Stack Wide camera module (M5STACK_WIDE). Also has a status LED on
/// GPIO 2, not represented in camera_config_t.
inline void setPinsM5StackWide(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = 15;
  config.pin_xclk = 27;
  config.pin_sccb_sda = 22;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 19;
  config.pin_d6 = 36;
  config.pin_d5 = 18;
  config.pin_d4 = 39;
  config.pin_d3 = 5;
  config.pin_d2 = 34;
  config.pin_d1 = 35;
  config.pin_d0 = 32;
  config.pin_vsync = 25;
  config.pin_href = 26;
  config.pin_pclk = 21;
}

/// M5Stack ESP32CAM module (no PSRAM; M5STACK_ESP32CAM)
inline void setPinsM5StackEsp32Cam(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = 15;
  config.pin_xclk = 27;
  config.pin_sccb_sda = 25;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 19;
  config.pin_d6 = 36;
  config.pin_d5 = 18;
  config.pin_d4 = 39;
  config.pin_d3 = 5;
  config.pin_d2 = 34;
  config.pin_d1 = 35;
  config.pin_d0 = 17;
  config.pin_vsync = 22;
  config.pin_href = 26;
  config.pin_pclk = 21;
}

/// M5Stack UnitCam module (no PSRAM; M5STACK_UNITCAM)
inline void setPinsM5StackUnitCam(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = 15;
  config.pin_xclk = 27;
  config.pin_sccb_sda = 25;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 19;
  config.pin_d6 = 36;
  config.pin_d5 = 18;
  config.pin_d4 = 39;
  config.pin_d3 = 5;
  config.pin_d2 = 34;
  config.pin_d1 = 35;
  config.pin_d0 = 32;
  config.pin_vsync = 22;
  config.pin_href = 26;
  config.pin_pclk = 21;
}

/// TTGO T-Journal (no PSRAM; TTGO_T_JOURNAL)
inline void setPinsTtgoTJournal(camera_config_t &config) {
  config.pin_pwdn = 0;
  config.pin_reset = 15;
  config.pin_xclk = 27;
  config.pin_sccb_sda = 25;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 19;
  config.pin_d6 = 36;
  config.pin_d5 = 18;
  config.pin_d4 = 39;
  config.pin_d3 = 5;
  config.pin_d2 = 34;
  config.pin_d1 = 35;
  config.pin_d0 = 17;
  config.pin_vsync = 22;
  config.pin_href = 26;
  config.pin_pclk = 21;
}

/// Generic ESP32-CAM breakout board wired via its 18-pin header (as
/// opposed to soldered directly to a matching camera board) - Y5/Y3 are
/// swapped on the header vs. direct-solder wiring; this preset is the
/// header variant (ESP32_CAM_BOARD, USE_BOARD_HEADER=0's *other* half -
/// see the source comment in camera_pins.h if your image looks like it
/// needs Y5/Y3 swapped, i.e. pin_d3/pin_d1 swapped here).
inline void setPinsEsp32CamBoard(camera_config_t &config) {
  config.pin_pwdn = 32;
  config.pin_reset = 33;
  config.pin_xclk = 4;
  config.pin_sccb_sda = 18;
  config.pin_sccb_scl = 23;
  config.pin_d7 = 36;
  config.pin_d6 = 19;
  config.pin_d5 = 21;
  config.pin_d4 = 39;
  config.pin_d3 = 35;  // Y5; swap with pin_d1 if colors/rows look wrong
  config.pin_d2 = 14;
  config.pin_d1 = 13;  // Y3; swap with pin_d3 if colors/rows look wrong
  config.pin_d0 = 34;
  config.pin_vsync = 5;
  config.pin_href = 27;
  config.pin_pclk = 25;
}

// --- ESP32-S2 / ESP32-S3 boards ---
// (plain integer GPIO numbers - these compile fine on any ESP32 variant,
// but the pins only physically exist on S2/S3 parts, matching the boards
// below.)

/// M5Stack CamS3 Unit (ESP32-S3, PSRAM). Also has a status LED on GPIO 14,
/// not represented in camera_config_t.
inline void setPinsM5StackCamS3Unit(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = 21;
  config.pin_xclk = 11;
  config.pin_sccb_sda = 17;
  config.pin_sccb_scl = 41;
  config.pin_d7 = 13;
  config.pin_d6 = 4;
  config.pin_d5 = 10;
  config.pin_d4 = 5;
  config.pin_d3 = 7;
  config.pin_d2 = 16;
  config.pin_d1 = 15;
  config.pin_d0 = 6;
  config.pin_vsync = 42;
  config.pin_href = 18;
  config.pin_pclk = 12;
}

/// Seeed Studio XIAO ESP32S3 Sense's official camera module (PSRAM)
inline void setPinsXiaoEsp32S3(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 10;
  config.pin_sccb_sda = 40;
  config.pin_sccb_scl = 39;
  config.pin_d7 = 48;
  config.pin_d6 = 11;
  config.pin_d5 = 12;
  config.pin_d4 = 14;
  config.pin_d3 = 16;
  config.pin_d2 = 18;
  config.pin_d1 = 17;
  config.pin_d0 = 15;
  config.pin_vsync = 38;
  config.pin_href = 47;
  config.pin_pclk = 13;
}

/// Generic ESP32-S3-CAM-LCD boards (ESP32S3_CAM_LCD)
inline void setPinsEsp32S3CamLcd(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 40;
  config.pin_sccb_sda = 17;
  config.pin_sccb_scl = 18;
  config.pin_d7 = 39;
  config.pin_d6 = 41;
  config.pin_d5 = 42;
  config.pin_d4 = 12;
  config.pin_d3 = 3;
  config.pin_d2 = 14;
  config.pin_d1 = 47;
  config.pin_d0 = 13;
  config.pin_vsync = 21;
  config.pin_href = 38;
  config.pin_pclk = 11;
}

/// Generic ESP32-S2-CAM breakout board wired via its 18-pin header (see
/// setPinsEsp32CamBoard()'s comment on the Y5/Y3 header swap - same
/// caveat applies here: swap pin_d3/pin_d1 if colors/rows look wrong).
inline void setPinsEsp32S2CamBoard(camera_config_t &config) {
  config.pin_pwdn = 1;
  config.pin_reset = 2;
  config.pin_xclk = 42;
  config.pin_sccb_sda = 41;
  config.pin_sccb_scl = 18;
  config.pin_d7 = 16;
  config.pin_d6 = 39;
  config.pin_d5 = 40;
  config.pin_d4 = 15;
  config.pin_d3 = 13;  // Y5; swap with pin_d1 if colors/rows look wrong
  config.pin_d2 = 5;
  config.pin_d1 = 12;  // Y3; swap with pin_d3 if colors/rows look wrong
  config.pin_d0 = 14;
  config.pin_vsync = 38;
  config.pin_href = 4;
  config.pin_pclk = 3;
}

/// Espressif ESP32-S3-EYE's official camera module (PSRAM). Many generic/
/// no-name "ESP32-S3-CAM" boards reuse this exact pinout too, since it's
/// Espressif's own reference design - worth trying first for an
/// unidentified ESP32-S3 camera board. Verified end-to-end on real
/// hardware: a generic (unlabeled) ESP32-S3 camera board captured a
/// correct, correctly-colored JPEG photo with this preset.
inline void setPinsEsp32S3Eye(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 15;
  config.pin_sccb_sda = 4;
  config.pin_sccb_scl = 5;
  config.pin_d0 = 11;
  config.pin_d1 = 9;
  config.pin_d2 = 8;
  config.pin_d3 = 10;
  config.pin_d4 = 12;
  config.pin_d5 = 18;
  config.pin_d6 = 17;
  config.pin_d7 = 16;
  config.pin_vsync = 6;
  config.pin_href = 7;
  config.pin_pclk = 13;
}

/// DFRobot FireBeetle 2 ESP32-S3 / DFRobot Romeo ESP32-S3's camera
/// connector (PSRAM) - both boards share this pinout.
inline void setPinsDFRobotEsp32S3(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 45;
  config.pin_sccb_sda = 1;
  config.pin_sccb_scl = 2;
  config.pin_d7 = 48;
  config.pin_d6 = 46;
  config.pin_d5 = 8;
  config.pin_d4 = 7;
  config.pin_d3 = 4;
  config.pin_d2 = 41;
  config.pin_d1 = 40;
  config.pin_d0 = 39;
  config.pin_vsync = 6;
  config.pin_href = 42;
  config.pin_pclk = 5;
}

/// ESP32-S3 mic+cam combo board used in pschatzmann/arduino-audio-tools'
/// esp32s3-mic-cam example. Also has a status LED on GPIO 34, not
/// represented in camera_config_t. PWDN/RESET are not broken out on this
/// board's camera connector.
inline void setPinsEsp32S3MicCam(camera_config_t &config) {
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.pin_xclk = 10;
  config.pin_sccb_sda = 21;
  config.pin_sccb_scl = 14;
  config.pin_d0 = 5;
  config.pin_d1 = 3;
  config.pin_d2 = 2;
  config.pin_d3 = 4;
  config.pin_d4 = 6;
  config.pin_d5 = 8;
  config.pin_d6 = 9;
  config.pin_d7 = 11;
  config.pin_vsync = 13;
  config.pin_href = 12;
  config.pin_pclk = 7;
}

#if defined(ARDUINO_ARCH_STM32)
/**
 * WeAct STM32H750 (and STM32H743) core boards' onboard 8-bit DCMI camera
 * connector (silkscreened for OV7670/OV2640/OV7725/OV5640-AF modules, but
 * TinyCamera currently only ships a driver for OV7725 - see
 * TinyCameraSTM32.h). Pin table and DCMI/clock settings are
 * sourced from WeAct's own published reference firmware:
 * github.com/WeActStudio/MiniSTM32H7xx, SDK/HAL/STM32H750/08-DCMI2LCD.
 *
 * XCLK (pin_xclk = PA8) is generated via MCO1/HSI48 in TinyCameraSTM32.h,
 * matching that reference firmware exactly (its SystemClock_Config()
 * enables HSI48 and sets MCO1 = HSI48/4 = 12MHz) - not HSE and not a timer
 * PWM.
 */
inline void setPinsWeActStm32H750(camera_config_t &config) {
  config.pin_pwdn = -1;   // not broken out on this connector
  config.pin_reset = -1;  // not broken out on this connector
  config.pin_xclk = PA8;
  // This board's clock architecture fixes XCLK at 12MHz (MCO1 = HSI48/4,
  // set up unconditionally by WeAct's own SystemClock_Config(), the same
  // way regardless of which sensor ends up connected) - not a per-sensor
  // choice the way it is on ESP32-CAM boards, so this preset sets it
  // rather than leaving it at TinyCamera::defaultConfig()'s 20MHz.
  config.xclk_freq_hz = 12000000;
  config.pin_sccb_scl = PB8;
  config.pin_sccb_sda = PB9;
  config.pin_pclk = PA6;
  config.pin_vsync = PB7;
  config.pin_href = PA4;
  config.pin_d0 = PC6;
  config.pin_d1 = PC7;
  config.pin_d2 = PE0;
  config.pin_d3 = PE1;
  config.pin_d4 = PE4;
  config.pin_d5 = PD3;
  config.pin_d6 = PE5;
  config.pin_d7 = PE6;
}

#endif  // ARDUINO_ARCH_STM32

}  // namespace tiny_camera
