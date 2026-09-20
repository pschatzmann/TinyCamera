#pragma once
/**
 * Software JPEG conversion helpers for TinyCamera, backed by the TinyJPEG
 * library (https://github.com/pschatzmann/TinyJPEG - TinyJPEGEncoder /
 * TinyJPEGDecoder). Unlike TinyCameraConvert.h, every function here works
 * identically on every platform this library supports (ESP32, RP2040,
 * STM32) since none of it depends on img_converters.h or any
 * hardware/vendor JPEG codec - it's a pure software fallback, useful e.g.
 * on STM32 where TinyCameraConvert.h's toJpg()/toRgb565()/toRgb888() can't
 * handle JPEG at all.
 *
 * Costs more RAM and CPU than a hardware/vendor path where one is
 * available - prefer TinyCameraConvert.h's functions there and reach for
 * the *Software() functions here only where no such path exists, or where
 * a platform-independent fallback is otherwise needed.
 *
 * Requires the TinyJPEG library to be installed; including this header
 * without it is a build error (see the #error below) rather than a
 * silently missing feature.
 */

#include <string.h>

#include "TinyCamera.h"
#include "TinyCameraConvert.h"

// Included unconditionally, not behind __has_include: the Arduino build
// system (arduino-cli/arduino-builder) decides which installed libraries
// to put on the include path by textually scanning for #include lines
// *before* any macro/#if is resolved. An #include gated behind
// __has_include() is invisible to that scan, so TinyJPEG's own path never
// gets added and __has_include then (self-fulfillingly) always evaluates
// false, even when the library is installed. An unconditional #include
// here is what lets arduino-cli find and add it; if it's genuinely
// missing, the compiler's own "No such file or directory" on the line
// below is the error - see the library's project page if you hit it.
// https://github.com/pschatzmann/TinyJPEG
#include <TinyJPEGDecoder.h>
#include <TinyJPEGEncoder.h>

namespace tiny_camera {

namespace tiny_camera_software_detail {

/// Maps this library's PixelFormat to TinyJPEGEncoder's JEPixelFormat.
/// Returns false for formats TinyJPEG's encoder can't take directly
/// (PIXFORMAT_JPEG, YUV, RAW, ...) - convert to RGB565/RGB888/GRAYSCALE
/// first in that case.
inline bool toJEFormat(PixelFormat in, JEPixelFormat &out) {
  switch (in) {
    case PIXFORMAT_RGB565:
      out = JE_FMT_RGB565;
      return true;
// PIXFORMAT_RGB888 only exists in esp32-camera's pixformat_t - neither
// STM32's own (TinyCameraSTM32.h) nor RP2040/PicoCamera's (see
// TinyCameraRP2040.h) defines it, since neither backend ever produces
// RGB888 frames.
#if defined(ESP32)
    case PIXFORMAT_RGB888:
      out = JE_FMT_RGB888;
      return true;
#endif
// PIXFORMAT_GRAYSCALE exists on ESP32 and RP2040/PicoCamera, but not on
// STM32's narrower pixformat_t (RGB565/JPEG only).
#if !defined(ARDUINO_ARCH_STM32)
    case PIXFORMAT_GRAYSCALE:
      out = JE_FMT_GRAY8;
      return true;
#endif
    default:
      return false;
  }
}

/// Per-block decode context for toRgb565Software(): destWidth is the
/// destination image width (frame.width()), used to compute each block's
/// row stride in the caller's output buffer.
struct RawCopyCtx {
  uint8_t *out;
  uint16_t destWidth;
};

inline bool copyRgb565Block(tinyjpeg::TinyJPEGDecoder &decoder, int16_t x,
                             int16_t y, uint16_t w, uint16_t h,
                             uint16_t *data) {
  auto *ctx = static_cast<RawCopyCtx *>(decoder.getUserData());
  uint16_t *out = reinterpret_cast<uint16_t *>(ctx->out);
  for (uint16_t row = 0; row < h; row++) {
    memcpy(out + (size_t)(y + row) * ctx->destWidth + x, data + (size_t)row * w,
           (size_t)w * sizeof(uint16_t));
  }
  return true;
}

/// Per-block decode context for toRgb888Software() - same idea as
/// RawCopyCtx, but the callback also unpacks RGB565 -> RGB888 per pixel.
struct Rgb888CopyCtx {
  uint8_t *out;
  uint16_t destWidth;
};

inline bool copyRgb888Block(tinyjpeg::TinyJPEGDecoder &decoder, int16_t x,
                             int16_t y, uint16_t w, uint16_t h,
                             uint16_t *data) {
  auto *ctx = static_cast<Rgb888CopyCtx *>(decoder.getUserData());
  for (uint16_t row = 0; row < h; row++) {
    uint8_t *dstRow = ctx->out + ((size_t)(y + row) * ctx->destWidth + x) * 3;
    const uint16_t *srcRow = data + (size_t)row * w;
    for (uint16_t col = 0; col < w; col++) {
      uint16_t px = srcRow[col];
      uint8_t r = (uint8_t)((px >> 11) & 0x1f);
      uint8_t g = (uint8_t)((px >> 5) & 0x3f);
      uint8_t b = (uint8_t)(px & 0x1f);
      dstRow[col * 3 + 0] = (uint8_t)((r << 3) | (r >> 2));
      dstRow[col * 3 + 1] = (uint8_t)((g << 2) | (g >> 4));
      dstRow[col * 3 + 2] = (uint8_t)((b << 3) | (b >> 2));
    }
  }
  return true;
}

}  // namespace tiny_camera_software_detail

/// Decodes a JPEG frame to RGB565 into a caller-provided buffer of at
/// least width() * height() * 2 bytes, via TinyJPEGDecoder. Platform-
/// independent software fallback for TinyCameraConvert.h's toRgb565(),
/// which on STM32 can't handle JPEG input at all.
inline bool toRgb565Software(const TinyCameraFrame &frame, uint8_t *outBuf) {
  if (!frame || outBuf == nullptr || frame.format() != PIXFORMAT_JPEG) {
    TinyCameraLogger.error(
        "toRgb565Software(): frame is invalid, outBuf is null, or frame is "
        "not JPEG");
    return false;
  }
  tiny_camera_software_detail::RawCopyCtx ctx{outBuf, (uint16_t)frame.width()};
  tinyjpeg::TinyJPEGDecoder decoder;
  decoder.setUserData(&ctx);
  decoder.setCallback(tiny_camera_software_detail::copyRgb565Block);
  auto r = decoder.drawJpg(0, 0, frame.data(), frame.size());
  if (r != JDR_OK) {
    TinyCameraLogger.error("toRgb565Software(): drawJpg() failed (%d)",
                            (int)r);
    return false;
  }
  return true;
}

/// Decodes a JPEG frame to interleaved RGB888 into a caller-provided
/// buffer of at least width() * height() * 3 bytes, via TinyJPEGDecoder.
/// Platform-independent software fallback for TinyCameraConvert.h's
/// toRgb888(), which on STM32 can't handle JPEG input at all.
inline bool toRgb888Software(const TinyCameraFrame &frame, uint8_t *outBuf) {
  if (!frame || outBuf == nullptr || frame.format() != PIXFORMAT_JPEG) {
    TinyCameraLogger.error(
        "toRgb888Software(): frame is invalid, outBuf is null, or frame is "
        "not JPEG");
    return false;
  }
  tiny_camera_software_detail::Rgb888CopyCtx ctx{outBuf,
                                                   (uint16_t)frame.width()};
  tinyjpeg::TinyJPEGDecoder decoder;
  decoder.setUserData(&ctx);
  decoder.setCallback(tiny_camera_software_detail::copyRgb888Block);
  auto r = decoder.drawJpg(0, 0, frame.data(), frame.size());
  if (r != JDR_OK) {
    TinyCameraLogger.error("toRgb888Software(): drawJpg() failed (%d)",
                            (int)r);
    return false;
  }
  return true;
}

/// Encodes an RGB565/RGB888/GRAYSCALE frame to a JPEG buffer via
/// TinyJPEGEncoder. `quality` is 1 (smallest/lowest quality) to 100
/// (largest/highest quality). Platform-independent software fallback for
/// TinyCameraConvert.h's toJpg(), which on STM32 only passes through
/// frames already captured as JPEG.
inline TinyCameraBuffer toJpgSoftware(const TinyCameraFrame &frame,
                                        uint8_t quality = 80) {
  if (!frame || frame.format() == PIXFORMAT_JPEG) {
    TinyCameraLogger.error(
        "toJpgSoftware(): frame is invalid or already JPEG");
    return TinyCameraBuffer();
  }
  JEPixelFormat jeFmt;
  if (!tiny_camera_software_detail::toJEFormat(frame.format(), jeFmt)) {
    TinyCameraLogger.error(
        "toJpgSoftware(): unsupported pixel format for software encoding");
    return TinyCameraBuffer();
  }

  // Upper bound for the encoded size: a baseline JPEG is essentially
  // always smaller than its uncompressed source.
  size_t cap = frame.size();
  uint8_t *out = (uint8_t *)malloc(cap);
  if (out == nullptr) {
    TinyCameraLogger.error("toJpgSoftware(): failed to allocate %u bytes",
                            (unsigned)cap);
    return TinyCameraBuffer();
  }

  tinyjpeg::TinyJPEGEncoder encoder;
  encoder.setQuality(quality);
  size_t jpgSize = 0;
  auto r = encoder.encodeJpg(frame.data(), (uint16_t)frame.width(),
                              (uint16_t)frame.height(), jeFmt, out, cap,
                              jpgSize);
  if (r != JER_OK) {
    TinyCameraLogger.error("toJpgSoftware(): encodeJpg() failed (%d)",
                            (int)r);
    free(out);
    return TinyCameraBuffer();
  }

  uint8_t *shrunk = (uint8_t *)realloc(out, jpgSize);
  return TinyCameraBuffer(shrunk ? shrunk : out, jpgSize);
}

/// Decodes a JPEG frame and re-encodes it as a BMP buffer (24-bit RGB888
/// pixel data, bottom-up rows), via TinyJPEGDecoder. Platform-independent
/// software fallback for TinyCameraConvert.h's toBmp(), which on STM32
/// only supports RGB565 input (no software JPEG decoder there).
inline TinyCameraBuffer toBmpSoftware(const TinyCameraFrame &frame) {
  if (!frame || frame.format() != PIXFORMAT_JPEG) {
    TinyCameraLogger.error("toBmpSoftware(): frame is invalid or not JPEG");
    return TinyCameraBuffer();
  }

  const size_t w = frame.width();
  const size_t h = frame.height();
  uint16_t *rgb565 = (uint16_t *)malloc(w * h * sizeof(uint16_t));
  if (rgb565 == nullptr) {
    TinyCameraLogger.error("toBmpSoftware(): failed to allocate %u bytes",
                            (unsigned)(w * h * sizeof(uint16_t)));
    return TinyCameraBuffer();
  }
  if (!toRgb565Software(frame, (uint8_t *)rgb565)) {
    free(rgb565);
    return TinyCameraBuffer();
  }

  const size_t rowBytes = (w * 3 + 3) & ~3u;  // rows padded to 4 bytes
  const size_t pixelDataSize = rowBytes * h;
  const size_t fileSize = 54 + pixelDataSize;

  uint8_t *out = (uint8_t *)malloc(fileSize);
  if (out == nullptr) {
    TinyCameraLogger.error("toBmpSoftware(): failed to allocate %u bytes",
                            (unsigned)fileSize);
    free(rgb565);
    return TinyCameraBuffer();
  }
  memset(out, 0, 54);

  // BITMAPFILEHEADER
  out[0] = 'B';
  out[1] = 'M';
  *(uint32_t *)(out + 2) = (uint32_t)fileSize;
  *(uint32_t *)(out + 10) = 54;  // pixel data offset
  // BITMAPINFOHEADER
  *(uint32_t *)(out + 14) = 40;
  *(int32_t *)(out + 18) = (int32_t)w;
  *(int32_t *)(out + 22) = (int32_t)h;
  *(uint16_t *)(out + 26) = 1;   // planes
  *(uint16_t *)(out + 28) = 24;  // bits per pixel
  *(uint32_t *)(out + 34) = (uint32_t)pixelDataSize;

  uint8_t *dst = out + 54;
  for (size_t y = 0; y < h; y++) {
    // BMP rows are stored bottom-up.
    const uint16_t *srcRow = rgb565 + (h - 1 - y) * w;
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
  free(rgb565);
  return TinyCameraBuffer(out, fileSize);
}

}  // namespace tiny_camera
