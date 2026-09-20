#pragma once
/**
 * Small printf-style logger for TinyCamera, shared across all backends
 * (ESP32, RP2040, STM32). Off by default; call TinyCameraLogger.begin()
 * to start printing.
 */

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

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

/**
 * Printf-style logger. Not instantiated directly - use the global
 * TinyCameraLogger instance below.
 *
 * Usage:
 *   TinyCameraLogger.begin(TinyCameraLogLevel::Info);  // default output: Serial
 *   TinyCameraLogger.error("camera init failed: %d", err);
 */
class TinyCameraLoggerClass {
 public:
  /// Starts logging at the given level to the given output (default:
  /// Serial). Pass TinyCameraLogLevel::None to silence logging again.
  void begin(TinyCameraLogLevel level, Print &output = Serial) {
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
    output_->print(prefix);
    output_->println(buf);
  }

  TinyCameraLogLevel level_ = TinyCameraLogLevel::None;
  Print *output_ = nullptr;
};

/// Global logger instance shared by the whole library and available to
/// sketches. Header-only-safe: `inline` gives it a single definition
/// across translation units.
inline TinyCameraLoggerClass TinyCameraLogger;

}  // namespace tiny_camera
