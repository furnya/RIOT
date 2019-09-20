/*
 * Copyright (C) 2014 Freie Universität Berlin
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
 * @brief       Hello World application
 *
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Ludwig Knüpfer <ludwig.knuepfer@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>

#include <periph/gpio.h>
#include <xtimer.h>

#include "assert.h"
#include "u8g2.h"
#include "periph/i2c.h"

#define TEST_ADDR 0x3c

#define TEST_PIN_CS         GPIO_PIN(0,0)
#define TEST_PIN_DC         GPIO_PIN(0,0)
#define TEST_PIN_RESET      GPIO_PIN(0,0)

/**
 * @brief   RIOT-OS logo, 64x32 pixels at 8 pixels per byte.
 */
static const uint8_t logo[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x3C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x1E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x70, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x0E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xF0, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x1E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3C, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xF0, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0xF8,
    0x30, 0x3C, 0x3F, 0xC0, 0x00, 0x0C, 0x77, 0xF0, 0x38, 0x7E, 0x3F, 0xC0,
    0x00, 0x7E, 0x73, 0xC0, 0x38, 0xE7, 0x06, 0x00, 0x00, 0xFC, 0x71, 0x00,
    0x38, 0xE3, 0x06, 0x00, 0x01, 0xF0, 0x70, 0x00, 0x38, 0xE3, 0x06, 0x00,
    0x01, 0xC0, 0x70, 0x00, 0x38, 0xE3, 0x06, 0x00, 0x03, 0x80, 0x70, 0xC0,
    0x38, 0xE3, 0x06, 0x00, 0x03, 0x80, 0x71, 0xE0, 0x38, 0xE3, 0x06, 0x00,
    0x03, 0x80, 0x70, 0xE0, 0x38, 0xE3, 0x06, 0x00, 0x03, 0x80, 0x70, 0xF0,
    0x38, 0xE3, 0x06, 0x00, 0x03, 0x80, 0x70, 0x70, 0x38, 0xE3, 0x06, 0x00,
    0x03, 0x80, 0xF0, 0x78, 0x38, 0xE3, 0x06, 0x00, 0x03, 0xC1, 0xE0, 0x3C,
    0x38, 0xE7, 0x06, 0x00, 0x01, 0xE3, 0xE0, 0x3C, 0x38, 0x7E, 0x06, 0x00,
    0x01, 0xFF, 0xC0, 0x1C, 0x30, 0x3C, 0x06, 0x00, 0x00, 0x7F, 0x80, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static gpio_t pins[] = {
    [U8X8_PIN_CS] = GPIO_UNDEF,
    [U8X8_PIN_DC] = GPIO_UNDEF,
    [U8X8_PIN_RESET] = GPIO_UNDEF,
};

/**
 * @brief   Bit mapping to indicate which pins are set.
 */
static uint32_t pins_enabled = (1 << U8X8_PIN_RESET);

#define OLED_I2C        (I2C_DEV(0))
#define OLED_ADDR       (0x3c)

uint8_t pinsel[] = { 4, 6,  7,  8,  9, 10, 15, 16, 17, 18, 19,
                  20, 21, 22, 23, 24, 27, 28, 29, 30, 31 };

int main(void)
{
    uint32_t screen = 0;

    puts("Hello Display!");
    puts("Initializing to I2C.");

    for (unsigned i = 0; i < sizeof(pinsel); i++) {
        printf("trying with GPIO_PIN(0, %i)\n", (int)pinsel[i]);

        for (unsigned foo = 0; foo < sizeof(pinsel); foo++) {
            gpio_init(GPIO_PIN(0, pinsel[foo]), GPIO_OUT);
            gpio_clear(GPIO_PIN(0, pinsel[foo]));
        }

        pins[U8X8_PIN_RESET] = GPIO_PIN(0, pinsel[i]);
        assert(pins[U8X8_PIN_RESET] != GPIO_UNDEF);


        u8g2_t u8g2;
        memset(&u8g2, 0, sizeof(u8g2));
        u8g2_Setup_ssd1306_i2c_128x64_noname_1(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_vcomh0_1(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_alt0_1(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_noname_2(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_vcomh0_2(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_alt0_2(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_vcomh0_f(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);
        // u8g2_Setup_ssd1306_i2c_128x64_alt0_f(&u8g2, U8G2_R0, u8x8_byte_riotos_hw_i2c, u8x8_gpio_and_delay_riotos);

        u8g2_SetPins(&u8g2, pins, pins_enabled);
        u8g2_SetDevice(&u8g2, OLED_I2C);
        u8g2_SetI2CAddress(&u8g2, OLED_ADDR);

        /* initialize the display */
        puts("Initializing display.");
        u8g2_InitDisplay(&u8g2);
        u8g2_SetPowerSave(&u8g2, 0);

        /* start drawing in a loop */
        puts("Drawing on screen.");
        for (int s = 0; s < 3; s++) {
            u8g2_FirstPage(&u8g2);

            do {
                u8g2_SetDrawColor(&u8g2, 1);
                u8g2_SetFont(&u8g2, u8g2_font_helvB12_tf);

                switch (screen) {
                    case 0:
                        // printf("draw THIS\n");
                        u8g2_DrawStr(&u8g2, 12, 22, "THIS");
                        break;
                    case 1:
                        // printf("draw IS\n");
                        u8g2_DrawStr(&u8g2, 24, 22, "IS");
                        break;
                    case 2:
                        // printf("draw RIOT\n");
                        u8g2_DrawBitmap(&u8g2, 0, 0, 8, 32, logo);
                        break;
                }
            } while (u8g2_NextPage(&u8g2));

            /* show screen in next iteration */
            screen = (screen + 1) % 3;

            /* sleep a little */
            xtimer_usleep(250 * 1000);
        }

        u8g2_SetPowerSave(&u8g2, 1);
        xtimer_usleep(100 * 1000);
        puts("---");
    }

    puts("DONE");

    return 0;
}
