/*
 * Copyright (C) 2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_ck12
 * @{
 *
 * @file
 * @brief       Board specific configuration for the CK12 smart watch
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @name    Button pin configuration
 * @{
 */
// #define BTN0_PIN            GPIO_PIN(0, 11)
// #define BTN0_MODE           GPIO_IN_PU
/** @} */

#define VIB_PIN         GPIO_PIN(0, 2)

/**
 * @brief   Initialize board specific hardware
 */
void board_init(void);

void board_vibrate(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
/** @} */