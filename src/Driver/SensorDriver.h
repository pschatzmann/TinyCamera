#pragma once
/**
 * Common interface every STM32 sensor driver (currently just
 * Driver/OV7725.h) implements, so TinyCameraSTM32.h's Stm32Camera can
 * auto-detect which sensor is actually connected (try each driver's
 * detect() in turn) and then drive whichever one answers through one
 * interface, without caring which sensor it turned out to be.
 *
 * OV2640Driver/OV7670Driver/OV5640Driver (Driver/OV2640.h, Driver/OV7670.h,
 * Driver/OV5640.h) were removed from this library: none of the three was
 * ever confirmed working end-to-end on real hardware (OV2640/OV7670 were
 * never hardware-tested at all; OV5640 was tested extensively and still
 * had an unresolved alternating-frame bug even after several real
 * register-level fixes - see git history for the full account). Rather
 * than ship drivers with that confidence level as if they were as solid
 * as OV7725 (which *is* fully verified - see TinyCameraSTM32.h's header
 * comment), they were deleted; identifySensorType() below still
 * recognizes their chip IDs on the SCCB bus so begin() can name a
 * detected-but-unsupported sensor in its error message instead of a bare
 * "not found".
 */

#include <Arduino.h>
#include <Wire.h>

#include "TinyCameraLogger.h"

namespace tiny_camera {

namespace sensor_detail {
// Purely informational: 7-bit I2C addresses commonly used by cameras
// other than the ones TinyCamera drives, so a bus scan result can offer a
// plausible guess instead of just a bare address. These are common
// conventions, not certain identification - the address alone doesn't
// prove which chip is present.
struct KnownAddress {
  uint8_t address;
  const char *hint;
};
inline const KnownAddress kOtherKnownAddresses[] = {
    {0x21, "OV7725/OV7670's address - if a SensorDriver for one of those "
           "already tried and didn't match, this is a different/"
           "unsupported sensor also using that address (e.g. some "
           "GC0308-family sensors)"},
    {0x24, "commonly a Himax HM01B0-family sensor"},
};
}  // namespace sensor_detail

/// Scans the full 7-bit I2C address range for anything that ACKs (plain
/// address probe, no register reads - safe against an unidentified
/// device) and logs what it finds. Called once, after every known
/// SensorDriver's detect() has failed, so an unsupported/miswired sensor
/// is diagnosable instead of a bare "not found".
inline void reportI2cBusScan(TwoWire &wire) {
  TinyCameraLogger.info("scanning I2C bus for any device...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    wire.beginTransmission(addr);
    if (wire.endTransmission() != 0) continue;
    found++;

    const char *hint = "not a sensor TinyCamera currently supports";
    for (const auto &known : sensor_detail::kOtherKnownAddresses) {
      if (known.address == addr) {
        hint = known.hint;
        break;
      }
    }
    TinyCameraLogger.error("  found device at 0x%02x (%s)", addr, hint);
  }
  if (found == 0) {
    TinyCameraLogger.error(
        "  no I2C devices found at all - check power/wiring/pull-ups "
        "(SCCB is I2C-compatible and needs pull-ups just the same)");
  }
}

/// Read-only chip-ID probes for the three sensors this library no longer
/// ships a driver for (OV2640, OV7670, OV5640 - see this file's header
/// comment for why). Called once, after the one remaining SensorDriver
/// (OV7725) has failed to detect, so begin()'s error message can name a
/// real, present-but-unsupported sensor instead of a bare "not found" -
/// these are plain register reads with no reset/init, safe to run even
/// though no driver exists to actually bring the sensor up. Returns
/// nullptr if none of the three answer either.
inline const char *identifyUnsupportedSensor(TwoWire &wire) {
  // OV7670 shares OV7725's SCCB address (0x21); the product-ID register
  // (0x0a) is what distinguishes them (0x76 vs OV7725's 0x77).
  {
    wire.beginTransmission(0x21);
    wire.write((uint8_t)0x0a);
    if (wire.endTransmission(true) == 0 &&
        wire.requestFrom((uint8_t)0x21, (uint8_t)1) == 1) {
      if (wire.read() == 0x76) return "OV7670";
    }
  }
  // OV2640: SCCB 0x30, sensor register bank (0xff=0x01), PIDH at 0x0a.
  {
    wire.beginTransmission(0x30);
    wire.write((uint8_t)0xff);
    wire.write((uint8_t)0x01);
    if (wire.endTransmission() == 0) {
      wire.beginTransmission(0x30);
      wire.write((uint8_t)0x0a);
      if (wire.endTransmission(true) == 0 &&
          wire.requestFrom((uint8_t)0x30, (uint8_t)1) == 1) {
        if (wire.read() == 0x26) return "OV2640";
      }
    }
  }
  // OV5640: SCCB 0x3c, 16-bit chip ID at 0x300a/0x300b (expect 0x5640).
  {
    wire.beginTransmission(0x3C);
    wire.write((uint8_t)0x30);
    wire.write((uint8_t)0x0a);
    if (wire.endTransmission(true) == 0 &&
        wire.requestFrom((uint8_t)0x3C, (uint8_t)1) == 1) {
      uint8_t idHigh = wire.read();
      wire.beginTransmission(0x3C);
      wire.write((uint8_t)0x30);
      wire.write((uint8_t)0x0b);
      if (wire.endTransmission(true) == 0 &&
          wire.requestFrom((uint8_t)0x3C, (uint8_t)1) == 1) {
        uint8_t idLow = wire.read();
        if (idHigh == 0x56 && idLow == 0x40) return "OV5640";
      }
    }
  }
  return nullptr;
}

class SensorDriver {
 public:
  virtual ~SensorDriver() = default;

  /// Human-readable sensor name, for logging.
  virtual const char *name() const = 0;

  /// True if this sensor has its own hardware JPEG encoder (so
  /// PIXFORMAT_JPEG can be requested); false if it only ever outputs raw
  /// pixel data (YUV/RGB565/Bayer).
  virtual bool supportsJpeg() const = 0;

  /// Sets up the I2C bus/pins and pulses reset/pwdn if wired, but does
  /// NOT yet confirm a sensor is present - see detect().
  virtual void begin(TwoWire &wire, int pinSda, int pinScl, int pinPwdn,
                      int pinReset) = 0;

  /// Probes this sensor's known SCCB/I2C address and checks its product
  /// ID register(s). Returns true (and leaves the sensor reset) only if
  /// this specific sensor answers - false leaves the I2C bus untouched
  /// enough for the next candidate driver's detect() to try safely.
  virtual bool detect() = 0;

  /// Applies the common bring-up sequence (call once after a successful
  /// detect()).
  virtual void applyCommonInit() = 0;

  /// Configures pixel format + resolution. Returns false for a format/size
  /// this sensor/driver doesn't support.
  virtual bool configure(bool jpeg, int width, int height) = 0;

  /// JPEG quality, 0-63 (lower is higher quality) - a no-op on sensors
  /// without hardware JPEG.
  virtual void setQuality(int quality) = 0;

  virtual int setBrightness(int level) = 0;    // -2..2
  virtual int setContrast(int level) = 0;      // -2..2
  virtual int setSaturation(int level) = 0;    // -2..2
  virtual int setVflip(bool enable) = 0;
  virtual int setHmirror(bool enable) = 0;
};

/// Shared SCCB begin()/reset sequence, previously hand-duplicated
/// verbatim across all four sensor drivers (a duplication that had
/// already drifted out of sync once - OV2640Driver's readReg() used I2C
/// repeated-start while OV7670Driver/OV7725Driver/OV5640Driver had all
/// been fixed to use a plain STOP, which SCCB actually needs). Doesn't
/// implement readReg()/writeReg() itself, since those need two different
/// register-address widths (see SccbSensorDriver8Bit/16Bit below) - only
/// the pin/reset/Wire setup, which is identical either way.
class SccbSensorDriverBase : public SensorDriver {
 public:
  void begin(TwoWire &wire, int pinSda, int pinScl, int pinPwdn,
             int pinReset) override {
    wire_ = &wire;
    pinPwdn_ = pinPwdn;
    pinReset_ = pinReset;

    if (pinPwdn_ >= 0) {
      pinMode(pinPwdn_, OUTPUT);
      digitalWrite(pinPwdn_, LOW);  // power up (active high power-down)
    }
    if (pinReset_ >= 0) {
      pinMode(pinReset_, OUTPUT);
      digitalWrite(pinReset_, LOW);
      delay(5);
      digitalWrite(pinReset_, HIGH);
      delay(resetSettleMs());
    }

#if defined(ARDUINO_ARCH_STM32)
    if (pinSda >= 0 && pinScl >= 0) {
      wire_->setSDA(pinSda);
      wire_->setSCL(pinScl);
    }
#else
    (void)pinSda;
    (void)pinScl;
#endif
    wire_->begin();
    wire_->setClock(100000);
  }

 protected:
  /// Milliseconds to wait after releasing RESET before SCCB traffic. 5ms
  /// (the default here) for OV2640/OV7670/OV7725; OV5640Driver overrides
  /// this to 20ms - it wants a longer post-reset settle than the others.
  virtual uint32_t resetSettleMs() const { return 5; }

  TwoWire *wire_ = nullptr;
  int pinPwdn_ = -1;
  int pinReset_ = -1;
};

/// Adds 8-bit-register-address SCCB read/write on top of
/// SccbSensorDriverBase, for OV7725Driver (currently the only driver
/// extending it - kept as its own class rather than folded directly into
/// OV7725Driver in case a second 8-bit sensor driver is added later).
/// sccbAddress() is the one thing each derived driver still has to
/// supply (its own fixed SCCB address) - everything else is shared.
class SccbSensorDriver8Bit : public SccbSensorDriverBase {
 protected:
  virtual uint8_t sccbAddress() const = 0;

  bool writeReg(uint8_t reg, uint8_t val) {
    wire_->beginTransmission(sccbAddress());
    wire_->write(reg);
    wire_->write(val);
    return wire_->endTransmission() == 0;
  }

  bool readReg(uint8_t reg, uint8_t &val) {
    wire_->beginTransmission(sccbAddress());
    wire_->write(reg);
    if (wire_->endTransmission(true) != 0) return false;  // STOP, not
                                                            // repeated start
                                                            // - SCCB needs
                                                            // this
    if (wire_->requestFrom(sccbAddress(), (uint8_t)1) != 1) return false;
    val = wire_->read();
    return true;
  }
};

}  // namespace tiny_camera
