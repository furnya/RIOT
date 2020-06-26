/*
 * Copyright (C) 2019-2020 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_ble_nimble_netif_rpble RPL-over-BLE for NimBLE
 * @ingroup     net_ble_nimble
 * @brief       RPL-over-BLE for Nimble implementation
 *
 * # About
 * This module provides a BLE connection manager for establishing IP-over-BLE
 * connections between BLE nodes based on information provided by the RPL
 * routing protocol.
 *
 * # Concept
 *
 * ##
 *
 * ## Strategy
 * - parents are advertising their presence (if slots are open)
 * - children are scanning for potential parents
 * - children will initiate connection procedure 'best' parent
 * - metric for best parent:
 *   - lowest rank
 *   - rssi?
 *    -most free connection slots
 *    --> all compiled into a single score value
 *
 * ## State Machine
 * initial: is master?
 * -> yes: start accepting (advertising)
 * -> no: start discovery loop
 *
 * new connection to parent established?
 * -> stop scanning
 * -> start accepting (advertising)
 *
 * # Parameters
 * - advertising itvl T_A (+ advertising timeout period T_ATO?)
 * - scan interval T_S + scan window T_W
 *
 * - initial parent selection window T_IPSW -> time to wait until selecting a
 *   parent from discovered options
 *
 * - conn timeout -> deduct from T_A
 * - connection itvl, supervision timeout, slave latency -> 0
 *
 * # TODOs
 * @todo        never remove active parents from ppt
 * @todo        use DODAG ID in evaluation phase: but how?
 *
 * @{
 *
 * @file
 * @brief       Interface for the generic BLE advertising data processing module
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#ifndef NIMBLE_RPBLE_H
#define NIMBLE_RPBLE_H

#include "nimble_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    NIMBLE_RPBLE_OK         =  0,
    NIMBLE_RPBLE_NO_CHANGE  = -1,
};

typedef struct {
    uint32_t scan_itvl;
    uint32_t scan_win;

    uint32_t adv_itvl;

    uint32_t conn_scanitvl;
    uint32_t conn_itvl;
    uint16_t conn_latency;
    uint32_t conn_super_to;
    uint32_t conn_timeout;

    /* time the node searches and ranks potential parents
     * -> itvl := rand([eval_itvl:2*eval_itvl)) */
    uint32_t eval_itvl;

    const char *name;
} nimble_rpble_cfg_t;

typedef struct {
    uint8_t inst_id;
    uint8_t dodag_id[16];
    uint8_t version;
    uint8_t role;
    uint16_t rank;
    uint8_t free_slots;
} nimble_rpble_ctx_t;

int nimble_rpble_init(const nimble_rpble_cfg_t *cfg);

int nimble_rpble_eventcb(nimble_netif_eventcb_t cb);

int nimble_rpble_update(const nimble_rpble_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMBLE_RPBLE_H */
/** @} */