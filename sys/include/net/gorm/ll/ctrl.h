/*
 * Copyright (C) 2018 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_gorm
 * @brief
 * @{
 *
 * @file
 * @brief       Gorm's interface for handling link layer control messages
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#ifndef GORM_LL_CTRL_H
#define GORM_LL_CTRL_H

#include "net/gorm.h"

#ifdef __cplusplus
extern "C" {
#endif

void gorm_ll_ctrl_on_data(gorm_ctx_t *con, gorm_buf_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* GORM_LL_CTRL_H */
/** @} */
