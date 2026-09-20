#pragma once
/**
 * STM32duino's per-family stm32yyxx_hal_conf_default.h ships with its
 * module-enable list wrapped in `#if 0` - none of the STM32Cube HAL
 * peripheral drivers (DCMI, DMA, ...) are actually enabled by default.
 * Libraries/sketches opt individual modules back in by dropping a
 * "hal_conf_extra.h" on the include path (STM32duino's own documented
 * extension point for `stm32h7xx_hal_conf.h`'s `__has_include(...)`
 * check), which is what this file does for TinyCamera's STM32 (DCMI)
 * backend.
 *
 * DMA and GPIO are enabled independently of this file (GPIO is unguarded;
 * DMA is pulled in by other core functionality), so only DCMI needs to be
 * added here.
 */

#define HAL_DCMI_MODULE_ENABLED
