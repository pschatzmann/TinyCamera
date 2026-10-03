#pragma once
// Minimal assertion helpers shared by the desktop tests.

#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, \
              #cond);                                                 \
      g_failures++;                                                   \
    }                                                                 \
  } while (0)

static int testResult(const char *name) {
  if (g_failures == 0) {
    printf("%s: all tests passed\n", name);
    return 0;
  }
  printf("%s: %d check(s) failed\n", name, g_failures);
  return 1;
}

// Test-pattern config: no webcam, no frame-rate throttling.
static camera_config_t testConfig(pixformat_t format, framesize_t size) {
  camera_config_t config = tiny_camera::TinyCamera::defaultConfig();
  config.source = CAMERA_SOURCE_TEST_PATTERN;
  config.test_pattern_fps = 0;
  config.pixel_format = format;
  config.frame_size = size;
  return config;
}
