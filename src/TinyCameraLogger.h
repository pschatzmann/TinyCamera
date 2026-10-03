#pragma once
/**
 * Small printf-style logger for TinyCamera, shared across all backends
 * (ESP32, RP2040, STM32, desktop). Off by default; call
 * TinyCameraLogger.begin() to start printing.
 *
 * Output goes to an Arduino Print (default: Serial) when Arduino.h is
 * available - on every board, and on the desktop with the Arduino
 * Emulator. A plain desktop build without the emulator (see
 * docs/Desktop.md) has no Print/Serial, so it writes to a FILE* (default:
 * stderr) instead.
 */

#include <stdarg.h>
#include <stdio.h>

#if defined(ARDUINO) || defined(HOST) || !defined(__has_include)
#include <Arduino.h>
#define TINY_CAMERA_LOGGER_USES_PRINT 1
#elif __has_include(<Arduino.h>)
#include <Arduino.h>
#define TINY_CAMERA_LOGGER_USES_PRINT 1
#endif

namespace tiny_camera {

/// Log verbosity, from least to most verbose. A message is printed when
/// its own level is <= the level passed to begin().
enum class TinyCameraLogLevel {
  None = 0,
  Error = 1,
  Warn = 2,
  Info = 3,
  Debug = 4,
};

#if defined(TINY_CAMERA_LOGGER_USES_PRINT)
/// Where log output goes: an Arduino Print.
using TinyCameraLogOutput = Print;
#define TINY_CAMERA_LOG_DEFAULT_OUTPUT Serial
#else
/// Where log output goes: a stdio stream.
using TinyCameraLogOutput = FILE;
#define TINY_CAMERA_LOG_DEFAULT_OUTPUT (*stderr)
#endif

/**
 * Printf-style logger. Not instantiated directly - use the global
 * TinyCameraLogger instance below.
 *
 * Usage:
 *   TinyCameraLogger.begin(TinyCameraLogLevel::Info);  // default: Serial/stderr
 *   TinyCameraLogger.error("camera init failed: %d", err);
 */
class TinyCameraLoggerClass {
 public:
  /// Starts logging at the given level to the given output (default:
  /// Serial, or stderr without Arduino.h). Pass TinyCameraLogLevel::None
  /// to silence logging again.
  void begin(TinyCameraLogLevel level,
             TinyCameraLogOutput &output = TINY_CAMERA_LOG_DEFAULT_OUTPUT) {
    level_ = level;
    output_ = &output;
  }

  TinyCameraLogLevel level() const { return level_; }

  void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(TinyCameraLogLevel::Error, "[TinyCamera][ERROR] ", fmt, args);
    va_end(args);
  }

  void warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(TinyCameraLogLevel::Warn, "[TinyCamera][WARN] ", fmt, args);
    va_end(args);
  }

  void info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(TinyCameraLogLevel::Info, "[TinyCamera][INFO] ", fmt, args);
    va_end(args);
  }

  void debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log(TinyCameraLogLevel::Debug, "[TinyCamera][DEBUG] ", fmt, args);
    va_end(args);
  }

 private:
  void log(TinyCameraLogLevel msgLevel, const char *prefix, const char *fmt,
            va_list args) {
    if (output_ == nullptr || msgLevel > level_ ||
        level_ == TinyCameraLogLevel::None) {
      return;
    }
    char buf[160];
    vsnprintf(buf, sizeof(buf), fmt, args);
#if defined(TINY_CAMERA_LOGGER_USES_PRINT)
    output_->print(prefix);
    output_->println(buf);
#else
    fprintf(output_, "%s%s\n", prefix, buf);
#endif
  }

  TinyCameraLogLevel level_ = TinyCameraLogLevel::None;
  TinyCameraLogOutput *output_ = nullptr;
};

/// Global logger instance shared by the whole library and available to
/// sketches. Header-only-safe: `inline` gives it a single definition
/// across translation units.
inline TinyCameraLoggerClass TinyCameraLogger;

}  // namespace tiny_camera
