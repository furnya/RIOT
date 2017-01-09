/*
 * Copyright (C) 2014-2016 Freie Universität Berlin
 * Copyright (C) 2014 PHYTEC Messtechnik GmbH
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     board_pba-d-01-kw2x
 * @{
 *
 * @file
 * @name        Peripheral MCU configuration for the phyWAVE-KW22 Board
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Johann Fischer <j.fischer@phytec.de>
 * @author      Jonas Remmert <j.remmert@phytec.de>
 */

#ifndef PERIPH_CONF_H_
#define PERIPH_CONF_H_

#include "periph_cpu.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @name Clock system configuration
 * @{
 */
#define KINETIS_CPU_USE_MCG               1

#define KINETIS_MCG_USE_ERC               1
#define KINETIS_MCG_USE_PLL               1
#define KINETIS_MCG_DCO_RANGE             (24000000U)
#define KINETIS_MCG_ERC_OSCILLATOR        0
#define KINETIS_MCG_ERC_FRDIV             2
#define KINETIS_MCG_ERC_RANGE             1
#define KINETIS_MCG_ERC_FREQ              4000000
#define KINETIS_MCG_PLL_PRDIV             1
#define KINETIS_MCG_PLL_VDIV0             0
#define KINETIS_MCG_PLL_FREQ              48000000

#define CLOCK_CORECLOCK                   KINETIS_MCG_PLL_FREQ
#define CLOCK_BUSCLOCK                    CLOCK_CORECLOCK
/** @} */

/**
 * @name Timer configuration
 * @{
 */
#define PIT_NUMOF               (2U)
#define PIT_CONFIG {                 \
        {                            \
            .prescaler_ch = 0,       \
            .count_ch = 1,           \
        },                           \
        {                            \
            .prescaler_ch = 2,       \
            .count_ch = 3,           \
        },                           \
    }
#define LPTMR_NUMOF             (0U)
#define LPTMR_CONFIG {}
#define TIMER_NUMOF             ((PIT_NUMOF) + (LPTMR_NUMOF))

#define PIT_BASECLOCK           (CLOCK_BUSCLOCK)
#define PIT_CLOCKGATE           (BITBAND_REG32(SIM->SCGC6, SIM_SCGC6_PIT_SHIFT))
#define PIT_ISR_0               isr_pit1
#define PIT_ISR_1               isr_pit3
#define LPTMR_ISR_0             isr_lptmr0

/** @} */

/**
 * @name UART configuration
 * @{
 */
static const uart_conf_t uart_config[] = {
    {
        .dev     = UART2,
        .sim_reg = &SIM->SCGC4,
        .rx_pin  = GPIO_PIN(PORT_D, 2),
        .tx_pin  = GPIO_PIN(PORT_D, 3),
        .rx_af   = 3,
        .tx_af   = 3,
        .sim_bit = SIM_SCGC4_UART2_SHIFT,
        .irqn    = UART2_RX_TX_IRQn
    },
    {
        .dev     = UART0,
        .sim_reg = &SIM->SCGC4,
        .rx_pin  = GPIO_PIN(PORT_D, 6),
        .tx_pin  = GPIO_PIN(PORT_D, 7),
        .rx_af   = 3,
        .tx_af   = 3,
        .sim_bit = SIM_SCGC4_UART0_SHIFT,
        .irqn    = UART0_RX_TX_IRQn
    }
};

#define UART_0_ISR          (isr_uart2_rx_tx)
#define UART_1_ISR          (isr_uart0_rx_tx)

#define UART_NUMOF          (sizeof(uart_config) / sizeof(uart_config[0]))
/** @} */

/**
 * @name ADC configuration
 * @{
 */
static const adc_conf_t adc_config[] = {
    /* dev, pin, channel */
    { ADC0, GPIO_PIN(PORT_E, 2), 1 },
    { ADC0, GPIO_PIN(PORT_E, 3), 1 },
    { ADC0, GPIO_PIN(PORT_D, 7), 22 },
    { ADC0, GPIO_PIN(PORT_D, 5), 6 },
    { ADC0, GPIO_PIN(PORT_E, 0), 10 },
    { ADC0, GPIO_PIN(PORT_E, 1), 11 },
};

#define ADC_NUMOF           (sizeof(adc_config) / sizeof(adc_config[0]))
/** @} */

/**
 * @name DAC configuration
 * @{
 */
#define DAC_CONFIG {}
#define DAC_NUMOF  0
/** @} */

/**
 * @name PWM configuration
 * @{
 */
#define PWM_NUMOF           (1U)
#define PWM_0_EN            1
#define PWM_MAX_CHANNELS    4
#define PWM_MAX_VALUE       0xffff

/* PWM 0 device configuration */
#define PWM_0_DEV           FTM0
#define PWM_0_CHANNELS      3
#define PWM_0_CLK           (48e6)
#define PWM_0_CLKEN()       (SIM->SCGC6 |= (SIM_SCGC6_FTM0_MASK))
#define PWM_0_CLKDIS()      (SIM->SCGC6 &= ~(SIM_SCGC6_FTM0_MASK))
/* PWM 0 pin configuration */

#define PWM_0_CH0_GPIO      GPIO_PIN(PORT_A, 4)
#define PWM_0_CH0_FTMCHAN   1
#define PWM_0_CH0_AF        3

#define PWM_0_CH1_GPIO      GPIO_PIN(PORT_D, 4)
#define PWM_0_CH1_FTMCHAN   4
#define PWM_0_CH1_AF        4

#define PWM_0_CH2_GPIO      GPIO_PIN(PORT_D, 6)
#define PWM_0_CH2_FTMCHAN   6
#define PWM_0_CH2_AF        4

#define PWM_0_CH3_GPIO      GPIO_PIN(PORT_A, 1)
#define PWM_0_CH3_FTMCHAN   1
#define PWM_0_CH3_AF        3
/** @} */


/**
 * @name SPI configuration
 * @{
 */
#define SPI_NUMOF               (2U)
#define SPI_0_EN                1
#define SPI_1_EN                1
#define SPI_IRQ_PRIO            1
#define KINETIS_SPI_USE_HW_CS   1

/* SPI 0 device config */
#define SPI_0_DEV               SPI0
#define SPI_0_INDEX             0
#define SPI_0_CTAS              0
#define SPI_0_CLKEN()           (SIM->SCGC6 |= (SIM_SCGC6_SPI0_MASK))
#define SPI_0_CLKDIS()          (SIM->SCGC6 &= ~(SIM_SCGC6_SPI0_MASK))
#define SPI_0_IRQ               SPI0_IRQn
#define SPI_0_IRQ_HANDLER       isr_spi0
#define SPI_0_FREQ              (48e6)

/* SPI 0 pin configuration */
#define SPI_0_PORT              PORTC
#define SPI_0_PORT_CLKEN()      (SIM->SCGC5 |= (SIM_SCGC5_PORTC_MASK))
#define SPI_0_AF                2

#define SPI_0_PCS0_PIN          4
#define SPI_0_SCK_PIN           5
#define SPI_0_SOUT_PIN          6
#define SPI_0_SIN_PIN           7

#define SPI_0_PCS0_ACTIVE_LOW   1

/* SPI 1 device config */
#define SPI_1_DEV               SPI1
#define SPI_1_INDEX             1
#define SPI_1_CTAS              0
#define SPI_1_CLKEN()           (SIM->SCGC6 |= (SIM_SCGC6_SPI1_MASK))
#define SPI_1_CLKDIS()          (SIM->SCGC6 &= ~(SIM_SCGC6_SPI1_MASK))
#define SPI_1_IRQ               SPI1_IRQn
#define SPI_1_IRQ_HANDLER       isr_spi1
#define SPI_1_FREQ              (48e6)

/* SPI 1 pin1configuration */
#define SPI_1_PORT              KW2XDRF_PORT_DEV
#define SPI_1_PORT_CLKEN()      KW2XDRF_PORT_CLKEN();
#define SPI_1_AF                KW2XDRF_PIN_AF

#define SPI_1_PCS0_PIN          KW2XDRF_PCS0_PIN
#define SPI_1_SCK_PIN           KW2XDRF_SCK_PIN
#define SPI_1_SOUT_PIN          KW2XDRF_SOUT_PIN
#define SPI_1_SIN_PIN           KW2XDRF_SIN_PIN

#define SPI_1_PCS0_ACTIVE_LOW   1

/** @} */


/**
 * @name I2C configuration
 * @{
 */
#define I2C_NUMOF               (1U)
#define I2C_CLK                 (48e6)
#define I2C_0_EN                1
#define I2C_IRQ_PRIO            1
/* Low (10 kHz): MUL = 4, SCL divider = 2560, total: 10240 */
#define KINETIS_I2C_F_ICR_LOW        (0x3D)
#define KINETIS_I2C_F_MULT_LOW       (2)
/* Normal (100 kHz): MUL = 2, SCL divider = 240, total: 480 */
#define KINETIS_I2C_F_ICR_NORMAL     (0x1F)
#define KINETIS_I2C_F_MULT_NORMAL    (1)
/* Fast (400 kHz): MUL = 1, SCL divider = 128, total: 128 */
#define KINETIS_I2C_F_ICR_FAST       (0x17)
#define KINETIS_I2C_F_MULT_FAST      (0)
/* Fast plus (1000 kHz): MUL = 1, SCL divider = 48, total: 48 */
#define KINETIS_I2C_F_ICR_FAST_PLUS  (0x10)
#define KINETIS_I2C_F_MULT_FAST_PLUS (0)

/* I2C 0 device configuration */
#define I2C_0_DEV               I2C1
#define I2C_0_CLKEN()           (SIM->SCGC4 |= (SIM_SCGC4_I2C1_MASK))
#define I2C_0_CLKDIS()          (SIM->SCGC4 &= ~(SIM_SCGC4_I2C1_MASK))
#define I2C_0_IRQ               I2C1_IRQn
#define I2C_0_IRQ_HANDLER       isr_i2c1
/* I2C 0 pin configuration */
#define I2C_0_PORT              PORTE
#define I2C_0_PORT_CLKEN()      (SIM->SCGC5 |= (SIM_SCGC5_PORTE_MASK))
#define I2C_0_PIN_AF            6
#define I2C_0_SDA_PIN           0
#define I2C_0_SCL_PIN           1
#define I2C_0_PORT_CFG          (PORT_PCR_MUX(I2C_0_PIN_AF) | PORT_PCR_ODE_MASK)

/** @} */


/**
 * @name GPIO configuration
 * @{
 */
#define GPIO_IRQ_PRIO       CPU_DEFAULT_IRQ_PRIO
/** @} */

/**
* @name RTT and RTC configuration
* @{
*/
#define RTT_NUMOF            (1U)
#define RTC_NUMOF            (1U)
#define RTT_DEV              RTC
#define RTT_IRQ              RTC_IRQn
#define RTT_IRQ_PRIO         10
#define RTT_UNLOCK()         (SIM->SCGC6 |= (SIM_SCGC6_RTC_MASK))
#define RTT_ISR              isr_rtc
#define RTT_FREQUENCY        (1)
#define RTT_MAX_VALUE        (0xffffffff)
/** @} */

/**
 * @name Random Number Generator configuration
 * @{
 */
#define KINETIS_RNGA        RNG
#define HWRNG_CLKEN()       (SIM->SCGC6 |= (1 << 9))
#define HWRNG_CLKDIS()      (SIM->SCGC6 &= ~(1 << 9))

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __PERIPH_CONF_H */
/** @} */
