#pragma once
/**
 * Pixel format conversion helpers for TinyCamera.
 *
 * On ESP32 these wrap the img_converters.h functions provided by the
 * esp32-camera driver.
 *
 * On RP2040 and STM32 there is no img_converters.h equivalent - PicoCamera
 * (arduino-pico's bundled camera library; see TinyCameraRP2040.h) has no
 * hardware/vendor JPEG codec, same as STM32's DCMI backend - so this
 * header provides software implementations instead, with a narrower
 * feature set (see the function comments below for exact coverage). For
 * full JPEG encode/decode on either platform, see
 * TinyCameraConvertSoftware.h instead (backed by the TinyJPEG library).
 */

#include <string.h>

#include "TinyCamera.h"

#if defined(ESP32)
#include "img_converters.h"
#endif

namespace tiny_camera {

/**
 * RAII wrapper around a heap buffer allocated by an img_converters.h
 * function (frame2jpg()/frame2bmp()), freed automatically on destruction.
 */
class TinyCameraBuffer {
 public:
  TinyCameraBuffer() = default;
  TinyCameraBuffer(uint8_t *data, size_t size) : data_(data), size_(size) {}

  TinyCameraBuffer(const TinyCameraBuffer &) = delete;
  TinyCameraBuffer &operator=(const TinyCameraBuffer &) = delete;

  TinyCameraBuffer(TinyCameraBuffer &&other) noexcept
      : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
  }

  TinyCameraBuffer &operator=(TinyCameraBuffer &&other) noexcept {
    if (this != &other) {
      release();
      data_ = other.data_;
      size_ = other.size_;
      other.data_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  ~TinyCameraBuffer() { release(); }

  bool isValid() const { return data_ != nullptr; }
  explicit operator bool() const { return isValid(); }

  uint8_t *data() const { return data_; }
  size_t size() const { return size_; }

  void release() {
    if (data_ != nullptr) {
      free(data_);
      data_ = nullptr;
      size_ = 0;
    }
  }

 private:
  uint8_t *data_ = nullptr;
  size_t size_ = 0;
};

/// Software fallback for arbitrary-size scaling, for when
/// TinyCamera::setCustomFrameSize() isn't supported (returns false) -
/// e.g. RP2040 (no hardware support at all), an ESP32 sensor whose
/// esp32-camera driver lacks set_res_raw(), or an ESP32/STM32 sensor
/// that simply rejects the requested size. Capture normally at whatever
/// size the sensor actually supports, then scale the result down (or up)
/// into a caller-provided buffer of exactly outWidth * outHeight * 2
/// bytes. Nearest-neighbor only (no interpolation) - lower quality than
/// hardware scaling, but simple, fast, needs no FPU, and works
/// identically on every platform this library supports, since it's pure
/// pixel math with no dependency on img_converters.h or any sensor
/// driver. RGB565 only - scaling a JPEG frame would need a full
/// decode/re-encode round-trip, well outside this function's scope (and
/// this library's "tiny" scope in general); convert to RGB565 first if
/// you're starting from JPEG (see toRgb565() below).
inline bool scaleRgb565(const TinyCameraFrame &frame, uint8_t *outBuf,
                          size_t outWidth, size_t outHeight) {
  if (!frame || outBuf == nullptr || outWidth == 0 || outHeight == 0) {
    TinyCameraLogger.error(
        "scaleRgb565(): frame is invalid, outBuf is null, or output size "
        "is zero");
    return false;
  }
  if (frame.format() != PIXFORMAT_RGB565) {
    TinyCameraLogger.error("scaleRgb565(): frame is not RGB565");
    return false;
  }
  const size_t srcWidth = frame.width();
  const size_t srcHeight = frame.height();
  const uint16_t *src = (const uint16_t *)frame.data();
  uint16_t *dst = (uint16_t *)outBuf;
  for (size_t y = 0; y < outHeight; y++) {
    size_t srcY = (y * srcHeight) / outHeight;
    const uint16_t *srcRow = src + srcY * srcWidth;
    uint16_t *dstRow = dst + y * outWidth;
    for (size_t x = 0; x < outWidth; x++) {
      size_t srcX = (x * srcWidth) / outWidth;
      dstRow[x] = srcRow[srcX];
    }
  }
  return true;
}

#if defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_RP2040) || \
    defined(PICO_RP2040) || defined(TARGET_RP2040)

/// Converts a captured frame to a JPEG buffer. Neither STM32 nor RP2040
/// (PicoCamera) has a hardware/vendor JPEG encoder here, so this only
/// supports frames that are already JPEG (i.e. captured with
/// pixel_format = PIXFORMAT_JPEG, encoded by the sensor's own hardware
/// encoder), in which case it just copies the existing data; `quality` is
/// ignored. Returns an invalid buffer for any other format - prefer
/// capturing directly as PIXFORMAT_JPEG over converting after the fact,
/// or see TinyCameraConvertSoftware.h's toJpgSoftware() for a real
/// (software) encoder.
inline TinyCameraBuffer toJpg(const TinyCameraFrame &frame, uint8_t quality = 12) {
  (void)quality;
  if (!frame || frame.format() != PIXFORMAT_JPEG) {
    TinyCameraLogger.error(
        "toJpg(): frame is invalid or not already JPEG (no software JPEG "
        "encoder here - see TinyCameraConvertSoftware.h's toJpgSoftware())");
    return TinyCameraBuffer();
  }
  uint8_t *out = (uint8_t *)malloc(frame.size());
  if (out == nullptr) {
    TinyCameraLogger.error("toJpg(): failed to allocate %u bytes",
                            (unsigned)frame.size());
    return TinyCameraBuffer();
  }
  memcpy(out, frame.data(), frame.size());
  return TinyCameraBuffer(out, frame.size());
}

/// Converts an RGB565 frame to a BMP buffer (24-bit RGB888 pixel data,
/// bottom-up rows, as required by the BMP format). JPEG frames are not
/// supported (no software JPEG decoder is included) - capture as
/// PIXFORMAT_RGB565 to use this, or see TinyCameraConvertSoftware.h's
/// toBmpSoftware() for JPEG input.
inline TinyCameraBuffer toBmp(const TinyCameraFrame &frame) {
  if (!frame || frame.format() != PIXFORMAT_RGB565) {
    TinyCameraLogger.error(
        "toBmp(): frame is invalid or not RGB565 (no software JPEG decoder "
        "here - see TinyCameraConvertSoftware.h's toBmpSoftware())");
    return TinyCameraBuffer();
  }

  const size_t w = frame.width();
  const size_t h = frame.height();
  const size_t rowBytes = (w * 3 + 3) & ~3u;  // rows padded to 4 bytes
  const size_t pixelDataSize = rowBytes * h;
  const size_t fileSize = 54 + pixelDataSize;

  uint8_t *out = (uint8_t *)malloc(fileSize);
  if (out == nullptr) {
    TinyCameraLogger.error("toBmp(): failed to allocate %u bytes",
                            (unsigned)fileSize);
    return TinyCameraBuffer();
  }
  memset(out, 0, 54);

  // BITMAPFILEHEADER
  out[0] = 'B'; out[1] = 'M';
  *(uint32_t *)(out + 2) = (uint32_t)fileSize;
  *(uint32_t *)(out + 10) = 54;  // pixel data offset
  // BITMAPINFOHEADER
  *(uint32_t *)(out + 14) = 40;
  *(int32_t *)(out + 18) = (int32_t)w;
  *(int32_t *)(out + 22) = (int32_t)h;
  *(uint16_t *)(out + 26) = 1;   // planes
  *(uint16_t *)(out + 28) = 24;  // bits per pixel
  *(uint32_t *)(out + 34) = (uint32_t)pixelDataSize;

  const uint16_t *src = (const uint16_t *)frame.data();
  uint8_t *dst = out + 54;
  for (size_t y = 0; y < h; y++) {
    // BMP rows are stored bottom-up.
    const uint16_t *srcRow = src + (h - 1 - y) * w;
    uint8_t *dstRow = dst + y * rowBytes;
    for (size_t x = 0; x < w; x++) {
      uint16_t px = srcRow[x];
      uint8_t r = (uint8_t)((px >> 11) & 0x1f);
      uint8_t g = (uint8_t)((px >> 5) & 0x3f);
      uint8_t b = (uint8_t)(px & 0x1f);
      dstRow[x * 3 + 0] = (uint8_t)((b << 3) | (b >> 2));
      dstRow[x * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
      dstRow[x * 3 + 2] = (uint8_t)((r << 3) | (r >> 2));
    }
  }
  return TinyCameraBuffer(out, fileSize);
}

/// Converts an RGB565 frame to interleaved RGB888 into a caller-provided
/// buffer of at least width() * height() * 3 bytes. JPEG frames are not
/// supported.
inline bool toRgb888(const TinyCameraFrame &frame, uint8_t *outBuf) {
  if (!frame || outBuf == nullptr || frame.format() != PIXFORMAT_RGB565) {
    TinyCameraLogger.error(
        "toRgb888(): frame is invalid, outBuf is null, or frame is not "
        "RGB565");
    return false;
  }
  const uint16_t *src = (const uint16_t *)frame.data();
  size_t count = frame.width() * frame.height();
  for (size_t i = 0; i < count; i++) {
    uint16_t px = src[i];
    uint8_t r = (uint8_t)((px >> 11) & 0x1f);
    uint8_t g = (uint8_t)((px >> 5) & 0x3f);
    uint8_t b = (uint8_t)(px & 0x1f);
    outBuf[i * 3 + 0] = (uint8_t)((r << 3) | (r >> 2));
    outBuf[i * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
    outBuf[i * 3 + 2] = (uint8_t)((b << 3) | (b >> 2));
  }
  return true;
}

/// Copies an RGB565 frame into a caller-provided buffer of at least
/// width() * height() * 2 bytes. Unlike ESP32, this does not decode
/// JPEG - it only supports frames already captured as PIXFORMAT_RGB565
/// (see TinyCameraConvertSoftware.h's toRgb565Software() for JPEG input).
inline bool toRgb565(const TinyCameraFrame &frame, uint8_t *outBuf) {
  if (!frame || outBuf == nullptr || frame.format() != PIXFORMAT_RGB565) {
    TinyCameraLogger.error(
        "toRgb565(): frame is invalid, outBuf is null, or frame is not "
        "already RGB565 (no software JPEG decoder here - see "
        "TinyCameraConvertSoftware.h's toRgb565Software())");
    return false;
  }
  memcpy(outBuf, frame.data(), frame.size());
  return true;
}

#else  // ESP32

/// Converts any captured frame to a JPEG buffer at the given quality
/// (0-63, lower is higher quality). If the frame is already JPEG, prefer
/// using its data directly instead of paying the conversion cost again.
inline TinyCameraBuffer toJpg(const TinyCameraFrame &frame, uint8_t quality = 12) {
  if (!frame) {
    TinyCameraLogger.error("toJpg(): frame is invalid");
    return TinyCameraBuffer();
  }
  uint8_t *out = nullptr;
  size_t outLen = 0;
  if (!frame2jpg(frame.raw(), quality, &out, &outLen)) {
    TinyCameraLogger.error("toJpg(): frame2jpg() failed");
    return TinyCameraBuffer();
  }
  return TinyCameraBuffer(out, outLen);
}

/// Converts any captured frame to a BMP buffer.
inline TinyCameraBuffer toBmp(const TinyCameraFrame &frame) {
  if (!frame) {
    TinyCameraLogger.error("toBmp(): frame is invalid");
    return TinyCameraBuffer();
  }
  uint8_t *out = nullptr;
  size_t outLen = 0;
  if (!frame2bmp(frame.raw(), &out, &outLen)) {
    TinyCameraLogger.error("toBmp(): frame2bmp() failed");
    return TinyCameraBuffer();
  }
  return TinyCameraBuffer(out, outLen);
}

/// Converts a raw (non-JPEG) frame to interleaved RGB888 into a
/// caller-provided buffer of at least width() * height() * 3 bytes.
inline bool toRgb888(const TinyCameraFrame &frame, uint8_t *outBuf) {
  if (!frame || outBuf == nullptr) {
    TinyCameraLogger.error("toRgb888(): frame is invalid or outBuf is null");
    return false;
  }
  bool ok = fmt2rgb888(frame.data(), frame.size(), frame.format(), outBuf);
  if (!ok) TinyCameraLogger.error("toRgb888(): fmt2rgb888() failed");
  return ok;
}

/// Converts a JPEG frame to RGB565 into a caller-provided buffer of at
/// least width() * height() * 2 bytes.
inline bool toRgb565(const TinyCameraFrame &frame, uint8_t *outBuf) {
  if (!frame || outBuf == nullptr) {
    TinyCameraLogger.error("toRgb565(): frame is invalid or outBuf is null");
    return false;
  }
  if (frame.format() != PIXFORMAT_JPEG) {
    TinyCameraLogger.error("toRgb565(): frame is not JPEG");
    return false;
  }
  bool ok = jpg2rgb565(frame.data(), frame.size(), outBuf, JPG_SCALE_NONE);
  if (!ok) TinyCameraLogger.error("toRgb565(): jpg2rgb565() failed");
  return ok;
}

#endif  // ARDUINO_ARCH_STM32 || RP2040

}  // namespace tiny_camera
