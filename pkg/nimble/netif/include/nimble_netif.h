/*
 * Copyright (C) 2018-2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    pkg_nimble_netif NimBLE to GNRC netif wrapper
 * @ingroup     pkg_nimble
 * @brief       This module provides GNRC netif support for NimBLE
 * @{
 *
 * @file
 * @brief       GNRC netif integration for NimBLE
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#ifndef PKG_NIMBLE_NETIF_H
#define PKG_NIMBLE_NETIF_H

#include <stdint.h>

#include "clist.h"
#include "net/ble.h"

#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Default L2CAP channel ID to use
 */
#ifndef NIMBLE_NETIF_CID
#define NIMBLE_NETIF_CID            (BLE_L2CAP_CID_IPSP)
#endif

/**
 * @brief   Return codes used by the NimBLE netif module
 */
enum {
    NIMBLE_NETIF_OK         =  0,   /**< everything went fine */
    NIMBLE_NETIF_NOTCONN    = -1,   /**< not connected */
    NIMBLE_NETIF_DEVERR     = -2,   /**< internal BLE stack error */
    NIMBLE_NETIF_BUSY       = -3,   /**< network device is busy */
    NIMBLE_NETIF_NOMEM      = -4,   /**< insufficient memory */
    NIMBLE_NETIF_NOTADV     = -5,   /**< not advertising */
    NIMBLE_NETIF_ALREADY    = -6,   /**< already connected/advertising */
    NIMBLE_NETIF_NOTFOUND   = -7,   /**< no fitting entry found */
};

/**
 * @brief   Event types triggered by the NimBLE netif module
 */
typedef enum {
    NIMBLE_NETIF_CONNECTED_MASTER,  /**< connection established as master */
    NIMBLE_NETIF_CONNECTED_SLAVE,   /**< connection established as slave */
    NIMBLE_NETIF_DISCONNECTED,      /**< connection closed */
    NIMBLE_NETIF_CONNECT_ABORT,     /**< connection establishment aborted */
    NIMBLE_NETIF_CONN_UPDATED,      /**< connection parameter update done */
} nimble_netif_event_t;

/**
 * @brief   Flags describing the state of a single connection context
 */
enum {
    NIMBLE_NETIF_L2CAP_CLIENT       = 0x0001,   /**< l2cap client */
    NIMBLE_NETIF_L2CAP_SERVER       = 0x0002,   /**< l2cap server */
    NIMBLE_NETIF_L2CAP_CONNECTED    = 0x0003,   /**< l2cap is connected */
    NIMBLE_NETIF_GAP_MASTER         = 0x0010,   /**< GAP master */
    NIMBLE_NETIF_GAP_SLAVE          = 0x0020,   /**< GAP slave */
    NIMBLE_NETIF_GAP_CONNECTED      = 0x0030,   /**< GAP is connected */
    NIMBLE_NETIF_ADV                = 0x0100,   /**< currently advertising */
    NIMBLE_NETIF_CONNECTING         = 0x4000,   /**< connection in progress */
    NIMBLE_NETIF_UNUSED             = 0x8000,   /**< context unused */
};

/**
 * @brief
 */
typedef struct {
    struct ble_l2cap_chan *coc;
    uint16_t gaphandle;
    uint16_t state;
    uint8_t addr[BLE_ADDR_LEN];
} nimble_netif_conn_t;


typedef void(*nimble_netif_eventcb_t)(int handle, nimble_netif_event_t event);


/* to be called from: system init (auto_init) */
void nimble_netif_init(void);

/**
 * @brief   Register a global event callback, servicing all NimBLE connections
 *
 * @warning     This function **must** be called before and other action
 *
 * @params[in] cb       event callback to register, may be NULL
 */
// TODO: move this into the _init() function? But what about auto_init?
int nimble_netif_eventcb(nimble_netif_eventcb_t cb);


int nimble_netif_connect(const ble_addr_t *addr,
                         const struct ble_gap_conn_params *conn_params,
                         uint32_t connect_timeout);

int nimble_netif_close(int handle);

/* return: negative for fail */
int nimble_netif_accept(const uint8_t *ad, size_t ad_len,
                        const struct ble_gap_adv_params *adv_params);

/**
 * @brief   Stop accepting incoming connections (and stop advertising)
 * *
 * @return  NIMBLE_NETIF_OK on success
 * @return  NIMBLE_NETIF_NOTADV if no advertising context is set
 */
int nimble_netif_accept_stop(void);

/**
 * @brief   Update the connection parameters for the given connection
 *
 * @param t [description]
 * @param ble_gap_conn_params [description]
 *
 * @return [description]
 */
int nimble_netif_update(int handle, struct ble_gap_conn_params *conn_params);

#ifdef __cplusplus
}
#endif

#endif /* PKG_NIMBLE_NETIF_H */
/** @} */
