/*
 * Copyright (C) 2017 Frits Kuipers
 *               2018 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_ds18
 * @{
 *
 * @file
 * @brief       Device driver implementation for the Maxim Integrated DS1822 and
 *              DS18B20 temperature sensors.
 *
 * @author      Frits Kuipers <frits.kuipers@gmail.com>
 * @author      Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 * @}
 */

#include "ds18.h"
#include "ds18_internal.h"

#include "log.h"
#include "periph/gpio.h"
#include "xtimer.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

static void ds18_low(ds18_t *dev)
{
    /* Set gpio as output and clear pin */
    gpio_init(dev->pin, GPIO_OUT);
    gpio_clear(dev->pin);
}

static void ds18_release(ds18_t *dev)
{
    /* Init pin as input */
    gpio_init(dev->pin, dev->in_mode);
}

static void ds18_write_bit(ds18_t *dev, uint8_t bit)
{
    /* Initiate write slot */
    ds18_low(dev);

    /* Release pin when bit==1 */
    if (bit) {
        ds18_release(dev);
    }

    /* Wait for slot to end */
    xtimer_usleep(DS18_DELAY_SLOT);
    ds18_release(dev);
  //  xtimer_usleep(1);
}

static int ds18_read_bit(ds18_t *dev, uint8_t *bit)
{
    /* Initiate read slot */
    ds18_low(dev);
    ds18_release(dev);

#if defined(MODULE_DS18_OPTIMIZED)
    xtimer_usleep(DS18_SAMPLE_TIME);
    *bit = gpio_read(dev->pin);
    xtimer_usleep(DS18_DELAY_R_RECOVER);
    return DS18_OK;
#else
    uint32_t start, measurement = 0;

    /* Measure time low of device pin, timeout after slot time*/
    start = xtimer_now_usec();
    DEBUG("4");
    while (!gpio_read(dev->pin) && measurement < DS18_DELAY_SLOT) {
        measurement = xtimer_now_usec() - start;
    }
    DEBUG("5");

    /* If there was a timeout return error */
    if (measurement >= DS18_DELAY_SLOT) {
        return DS18_ERROR;
    }
    DEBUG("6");

    /* When gpio was low for less than the sample time, bit is high*/
    *bit = measurement < DS18_SAMPLE_TIME;
    DEBUG("7");

    /* Wait for slot to end */
    xtimer_usleep(DS18_DELAY_SLOT - measurement);
    DEBUG("8\n");

    return DS18_OK;
#endif
}

static int ds18_read_byte(ds18_t *dev, uint8_t *byte)
{
    uint8_t bit = 0;
    *byte = 0;

    for (int i = 0; i < 8; i++) {
        DEBUG("[DS18] reading bit: %d\n", i);
        if (ds18_read_bit(dev, &bit) == DS18_OK) {
            DEBUG("[DS18] bit read\n");
            *byte |= (bit << i);
        }
        else {
            return DS18_ERROR;
        }
    }

    return DS18_OK;
}

static void ds18_write_byte(ds18_t *dev, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ds18_write_bit(dev, byte & (0x01 << i));
    }
}

static int ds18_reset(ds18_t *dev)
{
    int res;

    /* Line low and sleep the reset delay */
    ds18_low(dev);
    xtimer_usleep(DS18_DELAY_RESET);

    /* Release and wait for the presence response */
    ds18_release(dev);
    xtimer_usleep(DS18_DELAY_PRESENCE);

    /* Check device presence */
    res = gpio_read(dev->pin);

    /* Sleep for reset delay */
    xtimer_usleep(DS18_DELAY_RESET);

    return res;
}

int ds18_trigger(ds18_t *dev)
{
    int res;

    res = ds18_reset(dev);
    if (res) {
        return DS18_ERROR;
    }

    /* Please note that this command triggers a conversion on all devices
     * connected to the bus. */
    ds18_write_byte(dev, DS18_CMD_SKIPROM);
    ds18_write_byte(dev, DS18_CMD_CONVERT);

    return DS18_OK;
}

int ds18_read(ds18_t *dev, int16_t *temperature)
{
    int res;
    uint8_t b1 = 0, b2 = 0;

    DEBUG("[DS18] Reset and read scratchpad\n");
    res = ds18_reset(dev);
    if (res) {
        return DS18_ERROR;
    }

    ds18_write_byte(dev, DS18_CMD_SKIPROM);
    ds18_write_byte(dev, DS18_CMD_RSCRATCHPAD);

    if (ds18_read_byte(dev, &b1) != DS18_OK) {
        DEBUG("[DS18] Error reading temperature byte 1\n");
        return DS18_ERROR;
    }

    DEBUG("[DS18] Received byte: 0x%02x\n", b1);

    if (ds18_read_byte(dev, &b2) != DS18_OK) {
        DEBUG("[DS18] Error reading temperature byte 2\n");
        return DS18_ERROR;
    }

    DEBUG("[DS18] Received byte: 0x%02x\n", b2);

    int32_t measurement = ((int32_t)(b2 << 8 | b1) * 625);
    *temperature = (int16_t)(measurement / 100);

    return DS18_OK;
}

int ds18_get_temperature(ds18_t *dev, int16_t *temperature)
{

    DEBUG("[DS18] Convert T\n");
    if (ds18_trigger(dev)) {
        return DS18_ERROR;
    }

    DEBUG("[DS18] Wait for convert T\n");
    xtimer_usleep(DS18_DELAY_CONVERT);

    return ds18_read(dev, temperature);
}

int ds18_init(ds18_t *dev, const ds18_params_t *params)
{
    int res;

    /* Deduct the input mode from the output mode. If pull-up resistors are
     * used for output then will be used for input as well. */
    dev->in_mode = (params->out_mode == GPIO_OD_PU) ? GPIO_IN_PU : GPIO_IN;

    /* Initialize the device and the pin */
    dev->pin = params->pin;
    dev->out_mode = params->out_mode;
    res = gpio_init(dev->pin, dev->in_mode) == 0 ? DS18_OK : DS18_ERROR;

    return res;
}
