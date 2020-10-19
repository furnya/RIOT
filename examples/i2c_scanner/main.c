/*
 * Copyright (C) 2020 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       I2C device scanner
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "periph/i2c.h"

#ifndef I2C_SCANNER_DEV
#define I2C_SCANNER_DEV         I2C_DEV(0)
#endif

#define I2C_ADDR_MAX            (0x7f)


int main(void)
{
    unsigned cnt = 0;

    puts("I2C Scanner");
    printf("Scanning I2C_DEV(%u)\n", (unsigned)I2C_SCANNER_DEV);

    for (uint16_t addr = 0; addr <= I2C_ADDR_MAX; addr++) {
        uint8_t tmp;
        int res = i2c_read_byte(I2C_SCANNER_DEV, addr, &tmp, 0);
        (void)tmp;
        if (res == 0) {
            cnt++;
            printf("0x%02x - found device\n", (unsigned)addr);
        }
    }

    printf("Scan complete: %u devices found", cnt);
    return 0;
}
