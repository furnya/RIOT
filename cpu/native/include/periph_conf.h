/**
 * Native CPU peripheral configuration
 *
 * Copyright (C) 2014 Ludwig Ortmann <ludwig.ortmann@fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * @ingroup native_cpu
 * @{
 * @file
 * @author  Ludwig Ortmann <ludwig.ortmann@fu-berlin.de>
 */

#ifndef PERIPH_CONF_H
#define PERIPH_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/**
 * @name hardware timer clock skew avoidance
 * @{
 */
#define NATIVE_TIMER_MIN_RES 200
/** @} */

/**
 * @name Random Number Generator configuration
 * @{
 */
#define RANDOM_NUMOF        (1U)
/** @} */

/**
 * @name RealTime Clock configuration
 * @{
 */
#define RTC_NUMOF (1)
/** @} */

/**
 * @name Timer peripheral configuration
 * @{
 */
#define TIMER_NUMOF        (1U)
#define TIMER_0_EN         1

/** @def set TIMER0 for hwtimer wrapper */
#define HW_TIMER TIMER_0
/** @} */

#define XTIMER_OVERHEAD 14
#define XTIMER_USLEEP_UNTIL_OVERHEAD 1

#ifdef __cplusplus
}
#endif

#endif /* PERIPH_CONF_H */
/** @} */
