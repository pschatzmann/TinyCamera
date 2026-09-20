#pragma once
/**
 * OV7725 SCCB (I2C) sensor driver, used by the STM32 backend
 * (TinyCameraSTM32.h). A simple, older OmniVision VGA sensor with a
 * single flat register space (no DSP/sensor bank split like OV2640) and
 * no hardware JPEG encoder - only raw pixel output (this driver targets
 * RGB565). Listed on WeAct's own STM32H750 DCMI connector as a supported
 * sensor, alongside OV7670, OV2640 and OV5640-AF.
 *
 * Register table and bring-up sequence: ported register-for-register from
 * WeAct's own shipped OV7725 driver for this exact board/connector
 * (github.com/WeActStudio/MiniSTM32H7xx, SDK/HAL/STM32H750/08-DCMI2LCD/
 * Drivers/BSP/Camera/ov7725.c + ov7725_regs.c) - the single most
 * authoritative source available, since it's the literal vendor code for
 * this board, not a generic driver for the sensor in isolation. This
 * superseded an earlier version of this file ported from Espressif's
 * esp32-camera driver instead, which turned out to differ in several
 * concrete, consequential ways (see kCommonInit's comment and
 * TinyCameraSTM32.h's startXclk() for specifics: wrong CLKRC/DSP_CTRL2/
 * COM4 values, a missing DSP_CTRL4 write, and - the most likely actual
 * root cause of prior capture failures - XCLK sourced from HSE at an
 * unverified frequency instead of HSI48/4 = 12MHz, which is what this
 * register table assumes and what WeAct's own firmware actually uses).
 *
 * Sensor identity (SCCB address 0x21, product ID 0x77) confirmed against
 * real hardware: a camera module from a WeAct STM32H750 board's DCMI
 * connector was plugged into an ESP32-S3 board and positively identified
 * as OV7725 by the esp32-camera driver's own sensor detection, which also
 * successfully captured a correctly-sized RGB565 frame from it.
 *
 * Confidence level: both the register values (now ported from WeAct's own
 * vendor driver for this board) and the XCLK generation (now HSI48/4,
 * matching WeAct's SystemClock_Config() exactly - see
 * TinyCameraSTM32.h's startXclk()) are sourced from the actual reference
 * firmware for this board/connector, not a guess or a differently-tuned
 * driver. This combination has NOT yet been re-tested against real
 * hardware after these corrections (they were made from source
 * comparison, not from a further round of live debugging) - if your
 * image still doesn't come out right, that's the next thing to verify.
 */

#include <Arduino.h>
#include <Wire.h>

#include "SensorDriver.h"
#include "TinyCameraLogger.h"

namespace tiny_camera {

class OV7725Driver : public SccbSensorDriver8Bit {
 public:
  const char *name() const override { return "OV7725"; }
  bool supportsJpeg() const override { return false; }

  bool detect() override {
    writeReg(0x12, 0x80);  // COM7: software reset
    delay(10);

    uint8_t pid = 0;
    if (!readReg(kRegPid, pid)) {
      TinyCameraLogger.debug("no SCCB response at 0x%02x (OV7725)",
                              kSccbAddress);
      return false;
    }
    TinyCameraLogger.debug("device at 0x%02x: product ID 0x%02x", kSccbAddress,
                            pid);
    if (pid != 0x77) {
      TinyCameraLogger.debug(
          "device at 0x%02x is not an OV7725 (expected product ID 0x77, "
          "got 0x%02x)",
          kSccbAddress, pid);
      return false;
    }
    return true;
  }

  void applyCommonInit() override {
    writeTable(kCommonInit, sizeof(kCommonInit) / sizeof(RegVal));
    delay(300);  // WeAct's own reset() waits 300ms after writing this
                 // table, before any further configuration - AGC/AEC/PLL
                 // settling time, not arbitrary.
  }

  // Accepts any 0 < width <= 640, 0 < height <= 480 - not just the 320x240/
  // 640x480 presets WeAct's own reference supports. For those two exact
  // sizes, this writes byte-for-byte the same registers as before (see
  // this file's header comment for how that combination was verified on
  // real hardware). Every other size uses the sensor's full VGA-window
  // readout plus its DSPAUTO automatic scale/zoom DSP block to downscale
  // (or, near 640x480, pass through close to unscaled) the *entire*
  // captured field of view to exactly the requested output size - a real
  // hardware scale, not the software center-crop
  // TinyCameraSTM32.h's example sketches use to fit a smaller display
  // (which keeps full resolution over a narrower FOV instead). This
  // arbitrary-size path was verified on real hardware at 160x80 (see
  // examples/ScaledCapture): the sensor accepted the request,
  // reported no capture errors over a sustained run, and the resulting
  // image showed the sensor's whole field of view scaled down, not a
  // center crop.
  bool configure(bool jpeg, int width, int height) override {
    if (jpeg) return false;
    if (width <= 0 || height <= 0 || width > 640 || height > 480) return false;

    uint16_t w = (uint16_t)width;
    uint16_t h = (uint16_t)height;

    // Output size: HOUTSIZE/VOUTSIZE MSBs + EXHCH (0x2a) LSBs, per the
    // OV7725 App Note register map WeAct's own ov7725.c uses - always set
    // to exactly the requested width/height, regardless of which window/
    // scale path below produces it. An earlier version of this driver
    // wrote the LSBs to HREF (0x32) instead of EXHCH: harmless only by
    // coincidence for 320x240/640x480 specifically (w&3==0 and h&1==0
    // for both, so the intended LSB value is 0 either way, and HREF
    // happened to already hold 0 from kCommonInit) - a real bug for any
    // other size, where it would both miss setting the true output-size
    // LSBs and corrupt HREF's own window-position bits instead.
    writeReg(0x29, (uint8_t)(w >> 2));                        // HOUTSIZE
    writeReg(0x2c, (uint8_t)(h >> 1));                        // VOUTSIZE
    writeReg(0x2a, (uint8_t)((w & 0x3) | ((h & 0x1) << 2)));  // EXHCH

    uint8_t vflipReg;
    readReg(0x0c, vflipReg);  // COM3
    uint8_t vflipAdjust = (vflipReg & 0x80) ? 1 : 0;
    uint8_t com7;
    readReg(0x12, com7);

    bool exactQvga = (width == 320 && height == 240);
    bool exactVga = (width == 640 && height == 480);

    if (exactQvga) {
      // WeAct's own QVGA window/preset, byte-for-byte.
      writeReg(0x12, (uint8_t)(com7 | 0x40));         // COM7 |= RES_QVGA
      writeReg(0x17, 0x3f);                           // HSTART
      writeReg(0x18, 0x50);                           // HSIZE
      writeReg(0x19, (uint8_t)(0x03 - vflipAdjust));  // VSTART
      writeReg(0x1a, 0x78);                           // VSIZE
      writeReg(0xac, 0xff);                           // DSPAUTO: auto scale
    } else {
      // Full VGA-resolution window (largest field of view this sensor
      // has) for every other size, including the 640x480 preset.
      writeReg(0x12, (uint8_t)(com7 & ~0x40));        // COM7 &= ~RES_QVGA
      writeReg(0x17, 0x23);                           // HSTART
      writeReg(0x18, 0xa0);                           // HSIZE
      writeReg(0x19, (uint8_t)(0x07 - vflipAdjust));  // VSTART
      writeReg(0x1a, 0xf0);                           // VSIZE
      if (exactVga) {
        // Output size == window size here, so no actual scaling is
        // needed - matches WeAct's own VGA preset exactly: DSPAUTO
        // disabled, SCAL0-2 explicitly cleared to their reference
        // values, rather than trusting the auto-scale engine to compute
        // a trivial 1:1 ratio itself.
        writeReg(0xac, 0xf3);  // DSPAUTO disabled
        writeReg(0xa0, 0x00);  // SCAL0
        writeReg(0xa1, 0x40);  // SCAL1
        writeReg(0xa2, 0x40);  // SCAL2
      } else {
        writeReg(0xac, 0xff);  // DSPAUTO: auto scale/zoom to the
                                // arbitrary output size set above
      }
    }

    // set_pixformat(PIXFORMAT_RGB565): COM7_SET_FMT(reg, COM7_FMT_RGB) =
    // (reg & 0xFC) | (0x02 & 0x3) - only the "RGB not YUV" bit (0x02),
    // leaving the RGB565/RGB555/RGB444/GBR422 sub-format bits (COM7
    // bits[3:2]) wherever kCommonInit's trailing {COM7, COM7_FMT_RGB565}
    // write already left them (0x04) - plus DSP_CTRL4 = 0x00
    // (DSP_CTRL4_YUV_RGB), which WeAct's set_pixformat() also writes and
    // an earlier version of this driver was missing entirely.
    readReg(0x12, com7);
    writeReg(0x12, (uint8_t)((com7 & 0xFC) | 0x02));
    writeReg(0x67, 0x00);  // DSP_CTRL4: YUV_RGB (RGB565, not RAW8/RAW10)
    delay(30);
    return true;
  }

  /// No hardware JPEG on this sensor - a no-op.
  void setQuality(int quality) override { (void)quality; }

  int setBrightness(int level) override {  // -2..2
    writeReg(0x9b, (uint8_t)(constrain(level, -2, 2) * 0x20));
    return 0;
  }

  int setContrast(int level) override {  // -2..2
    writeReg(0x9c, (uint8_t)(0x20 + constrain(level, -2, 2) * 0x10));
    return 0;
  }

  // esp32-camera's own OV7725 driver doesn't implement saturation control
  // (set_saturation is an explicit no-op stub returning failure there) -
  // matched here rather than guessing a register for it.
  int setSaturation(int level) override {
    (void)level;
    return -1;
  }

  int setVflip(bool enable) override {
    uint8_t v;
    if (!readReg(0x0c, v)) return -1;  // COM3
    v = enable ? (v | 0x80) : (v & ~0x80);
    writeReg(0x0c, v);
    return 0;
  }

  int setHmirror(bool enable) override {
    uint8_t v;
    if (!readReg(0x0c, v)) return -1;  // COM3
    v = enable ? (v | 0x40) : (v & ~0x40);
    writeReg(0x0c, v);
    return 0;
  }

 private:
  struct RegVal {
    uint8_t reg;
    uint8_t val;
  };

  static constexpr uint8_t kSccbAddress = 0x21;
  static constexpr uint8_t kRegPid = 0x0a;

  // Common bring-up, ported register-for-register from WeAct's own
  // shipped OV7725 driver for this exact board/connector
  // (github.com/WeActStudio/MiniSTM32H7xx, SDK/HAL/STM32H750/08-DCMI2LCD/
  // Drivers/BSP/Camera/ov7725_regs.c's ov7725_default_regs[], as applied
  // by ov7725.c's ov7725_reset()) - the single most authoritative source
  // available for this driver, since it's the literal vendor code for
  // this board. This table assumes a 12MHz sensor input clock (see
  // CLKRC's comment below) - TinyCameraSTM32.h drives XCLK at exactly
  // that (MCO1 = HSI48/4 = 12MHz) when pin_xclk is PA8, matching WeAct's
  // own SystemClock_Config(). An earlier version of this table was
  // ported from esp32-camera's ov7725.c instead, which differs in
  // several places now corrected here (CLKRC, DSP_CTRL2, COM4, and a
  // DSP_CTRL4 write in configure() esp32-camera's driver doesn't need
  // because ESP32 assumes a different XCLK/CLKRC combination).
  static constexpr RegVal kCommonInit[] = {
      {0x3d, 0x03},  // COM12
      // Placeholder VGA window - overwritten by configure() immediately
      // after this table, for whatever frame_size was actually requested.
      {0x17, 0x22}, {0x18, 0xa4}, {0x19, 0x07}, {0x1a, 0xf0}, {0x32, 0x00},
      {0x29, 0xa0}, {0x2c, 0xf0}, {0x2a, 0x00},
      {0x11, 0x81},  // CLKRC: 12MHz input / 2 = 6MHz (bypass=0x80 | 0x01)
      {0x42, 0x7f},  // TGT_B
      {0x4d, 0x09},  // FIXGAIN
      {0x63, 0xe0},  // AWB_CTRL0
      {0x64, 0xff},  // DSP_CTRL1
      {0x65, 0x2f},  // DSP_CTRL2: 0x20 | VDCW|HDCW|VZOOM|HZOOM enable
      {0x66, 0x00},  // DSP_CTRL3
      {0x67, 0x48},  // DSP_CTRL4 (overridden to 0x00/YUV_RGB by
                      // configure() when RGB565 is selected)
      {0x15, 0x02},  // COM10: VSYNC negative
      {0x13, 0xf0},  // COM8
      {0x0d, 0x40},  // COM4: PLL 4x (6MHz x 4 = 24MHz internal)
      {0x0f, 0xc5},  // COM6
      {0x14, 0x11},  // COM9
      {0x22, 0x7f},  // BDBASE
      {0x23, 0x03},  // BDSTEP
      {0x24, 0x40},  // AEW
      {0x25, 0x30},  // AEB
      {0x26, 0xa1},  // VPT
      {0x2b, 0x00},  // EXHCL
      {0x6b, 0xaa},  // AWB_CTRL3
      {0x13, 0xff},  // COM8
      {0x90, 0x05},  // EDGE1
      {0x91, 0x01},  // DNSOFF
      {0x92, 0x03}, {0x93, 0x00},  // EDGE2/EDGE3
      {0x94, 0xb0}, {0x95, 0x9d}, {0x96, 0x13}, {0x97, 0x16}, {0x98, 0x7b},
      {0x99, 0x91},  // MTX1-6
      {0x9a, 0x1e},  // MTX_CTRL
      {0x9b, 0x08},  // BRIGHTNESS
      {0x9c, 0x20},  // CONTRAST
      {0x9e, 0x81},  // UVADJ0
      {0xa6, 0x06},  // SDE: contrast/brightness + saturation enable
      // Gamma
      {0x7e, 0x0c}, {0x7f, 0x16}, {0x80, 0x2a}, {0x81, 0x4e}, {0x82, 0x61},
      {0x83, 0x6f}, {0x84, 0x7b}, {0x85, 0x86}, {0x86, 0x8e}, {0x87, 0x97},
      {0x88, 0xa4}, {0x89, 0xaf}, {0x8a, 0xc5}, {0x8b, 0xd7}, {0x8c, 0xe8},
      {0x8d, 0x20},  // SLOP
      {0x33, 0x00},  // DM_LNL
      {0x22, 0x7f}, {0x23, 0x03},  // BDBASE/BDSTEP (written twice, matching
                                    // the reference table exactly)
      {0x4a, 0x10}, {0x49, 0x10}, {0x4b, 0x14}, {0x4c, 0x17}, {0x46, 0x01},
      // LC_RADI/LC_COEF/LC_COEFB/LC_COEFR/LC_CTR
      {0x0e, 0xf5},  // COM5
      {0x12, 0x04},  // COM7: RGB565 sub-format select bits (bits[3:2]) -
                      // configure() later ORs in bits[1:0] for RGB-vs-YUV
                      // without touching these bits.
  };

  void writeTable(const RegVal *table, size_t count) {
    for (size_t i = 0; i < count; i++) writeReg(table[i].reg, table[i].val);
  }

  uint8_t sccbAddress() const override { return kSccbAddress; }
};

}  // namespace tiny_camera
