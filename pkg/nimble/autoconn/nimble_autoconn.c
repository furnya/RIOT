/*
 * Copyright (C) 2018-2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     pkg_nimble_autoconn
 * @{
 *
 * @file
 * @brief       Autoconn connection manager implementation
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include "assert.h"
#include "random.h"
#include "net/bluetil/ad.h"
#include "net/bluetil/addr.h"
#include "nimble_netif.h"
#include "nimble_netif_conn.h"
#include "nimble_scanner.h"
#include "nimble_autoconn.h"
#include "nimble_autoconn_params.h"

#include "host/ble_hs.h"
#include "nimble/nimble_port.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

#if defined(MODULE_NIMBLE_AUTOCONN_IPSP)
#define SVC_FILTER      BLE_GATT_SVC_IPSS
#elif defined(MODULE_NIMBLE_AUTOCONN_NDNSP)
#define SVC_FILTER      BLE_GATT_SVC_NDNSS
#else
#error "NimBLE autoconn: please select a fitting submodule"
#endif

#define CONN_TIMEOUT_MUL            (5U)

enum {
    STATE_SCAN,
    STATE_ADV,
    STATE_IDLE,
    STATE_CONN,
};

static volatile uint8_t _state = STATE_IDLE;
static volatile uint8_t _active = 0;

#ifndef AUTOCONN_SCAN_ONLY
static bluetil_ad_t _ad;
static uint8_t _ad_buf[BLE_HS_ADV_MAX_SZ];
#endif

static struct ble_gap_adv_params _adv_params;
static struct ble_gap_conn_params _conn_params;
static uint32_t _conn_timeout;

static struct ble_npl_callout _state_evt;
static ble_npl_time_t _timeout_adv_period;
static ble_npl_time_t _timeout_scan_period;
static ble_npl_time_t _period_jitter;

/* this is run inside the NimBLE host thread */
static void _on_state_change(struct ble_npl_event *ev)
{
    (void)ev;
    ble_npl_time_t offset;
    offset = (ble_npl_time_t)random_uint32_range(0, (uint32_t)_period_jitter);

    if (_state == STATE_SCAN) {
        /* stop scanning */
        nimble_scanner_stop();
#ifndef AUTOCONN_SCAN_ONLY
        /* start advertising/accepting */
        int res = nimble_netif_accept(_ad.buf, _ad.pos, &_adv_params);
        assert((res == NIMBLE_NETIF_OK) || (res == NIMBLE_NETIF_NOMEM));
#endif

        /* schedule next state change */
        _state = STATE_ADV;
        ble_npl_callout_reset(&_state_evt, (_timeout_adv_period + offset));
    }
    else if (_state == STATE_ADV) {
#ifndef AUTOCONN_SCAN_ONLY
        /* stop advertising/accepting */
        nimble_netif_accept_stop();
#endif
        /* start scanning */
        nimble_scanner_start();
        _state = STATE_SCAN;
        ble_npl_callout_reset(&_state_evt, (_timeout_scan_period + offset));
    }
}

static int _filter_uuid(const bluetil_ad_t *ad)
{
    bluetil_ad_data_t incomp;
    if (bluetil_ad_find(ad, BLE_GAP_AD_UUID16_INCOMP, &incomp) == BLUETIL_AD_OK) {
        uint16_t filter_uuid = SVC_FILTER;
        for (unsigned i = 0; i < incomp.len; i += 2) {
            if (memcmp(&filter_uuid, &incomp.data[i], 2) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

static void _on_scan_evt(uint8_t type, const ble_addr_t *addr, int8_t rssi,
                         const uint8_t *ad_buf, size_t ad_len)
{
    (void)rssi;

    /* we are only interested in ADV_IND packets, the rest can be dropped right
     * away */
    if (type != BLE_HCI_ADV_TYPE_ADV_IND) {
        return;
    }

    bluetil_ad_t ad = {
        .buf  = (uint8_t *)ad_buf,
        .pos  = ad_len,
        .size = ad_len
    };

    /* for connection checking we need the address in network byte order */
    uint8_t addrn[BLE_ADDR_LEN];
    bluetil_addr_swapped_cp(addr->val, addrn);

    if (_filter_uuid(&ad) && !nimble_netif_conn_connected(addrn)) {
        nimble_autoconn_disable();
        _state = STATE_CONN;
        int res = nimble_netif_connect(addr, &_conn_params, _conn_timeout);
        assert(res >= 0);
        DEBUG("[autoconn] SCAN success, initiating connection\n");
    }
}

static void _on_netif_evt(int handle, nimble_netif_event_t event)
{
    switch (event) {
        case NIMBLE_NETIF_CONNECTED_MASTER:
            DEBUG("[autoconn] CONNECTED as master %i\n", handle);
            assert(_state == STATE_CONN);
            _state = STATE_IDLE;
            nimble_autoconn_enable();
            break;
        case NIMBLE_NETIF_CONNECTED_SLAVE:
            DEBUG("[autoconn] CONNECTED as slave %i\n", handle);
            nimble_autoconn_enable();
            break;
        case NIMBLE_NETIF_CLOSED_MASTER:
            DEBUG("[autoconn] CLOSED master connection\n");
            nimble_autoconn_enable();
            break;
        case NIMBLE_NETIF_CLOSED_SLAVE:
            DEBUG("[autoconn] CLOSED slave connection\n");
            nimble_autoconn_enable();
            break;
        case NIMBLE_NETIF_CONNECT_ABORT:
            DEBUG("[autoconn] CONNECT ABORT\n");
            assert(_state == STATE_CONN);
            _state = STATE_IDLE;
            nimble_autoconn_enable();
            break;
        case NIMBLE_NETIF_CONN_UPDATED:
            DEBUG("[autoconn] CONNECTION UPDATED %i\n", handle);
            /* nothing to do here */
            break;
        case NIMBLE_NETIF_GAP_SLAVE_CONN:
            _state = STATE_SCAN;
            nimble_autoconn_enable();
            break;
        default:
            /* this should never happen */
            assert(0);
    }
}

int nimble_autoconn_init(const nimble_autoconn_params_t *params,
                         uint8_t *adbuf, size_t adlen)
{
    int res;
    (void)res;

    /* register our event callback */
    nimble_netif_eventcb(_on_netif_evt);
    /* setup state machine timer (we use NimBLEs callouts for this) */
    ble_npl_callout_init(&_state_evt, nimble_port_get_dflt_eventq(),
                         _on_state_change, NULL);

    /* update parameters, this also triggers advertising and scanning */
    res = nimble_autoconn_update(params, adbuf, adlen);
    assert(res == NIMBLE_AUTOCONN_OK);

    /* TODO: remove and make explicit */
#ifndef AUTOCONN_SCAN_ONLY
    nimble_autoconn_enable();
#endif

    return NIMBLE_AUTOCONN_OK;
}

// TODO: return error on invalid config
int nimble_autoconn_update(const nimble_autoconn_params_t *params,
                           uint8_t *adbuf, size_t adlen)
{
    int res;
    (void)res;

#ifndef AUTOCONN_SCAN_ONLY
    assert(adlen < sizeof(_ad_buf));
#endif

    /* scan and advertising period configuration */
    ble_npl_time_ms_to_ticks(params->period_adv, &_timeout_adv_period);
    ble_npl_time_ms_to_ticks(params->period_scan, &_timeout_scan_period);
    ble_npl_time_ms_to_ticks(params->period_jitter, &_period_jitter);

    /* set preferred connection parameters */
    _conn_params.scan_itvl = ((params->scan_itvl * 1000) / BLE_HCI_SCAN_ITVL);
    _conn_params.scan_window = ((params->scan_win * 1000) / BLE_HCI_SCAN_ITVL);
    _conn_params.itvl_min = ((params->conn_itvl * 1000) / BLE_HCI_CONN_ITVL);
    _conn_params.itvl_max = ((params->conn_itvl * 1000) / BLE_HCI_CONN_ITVL);
    _conn_params.latency = 0;
    _conn_params.supervision_timeout = (params->conn_super_to / 10);
    _conn_params.min_ce_len = 0;
    _conn_params.max_ce_len = 0;
    _conn_timeout = params->adv_itvl * CONN_TIMEOUT_MUL;

    /* initialize scanner with default parameters */
    struct ble_gap_disc_params scan_params = {
        .itvl = ((params->scan_itvl * 1000) / BLE_HCI_SCAN_ITVL),
        .window = ((params->scan_win * 1000) / BLE_HCI_SCAN_ITVL),
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 1,
    };
    nimble_scanner_init(&scan_params, _on_scan_evt);

#ifndef AUTOCONN_SCAN_ONLY
    /* generate advertising data */
    if (adlen > 0) {
        assert(adbuf != NULL);
        memcpy(_ad_buf, adbuf, adlen);
        bluetil_ad_init(&_ad, _ad_buf, adlen, sizeof(_ad_buf));
    }
    else {
        uint16_t svc = SVC_FILTER;
        bluetil_ad_init_with_flags(&_ad, _ad_buf, sizeof(_ad_buf),
                                   BLUETIL_AD_FLAGS_DEFAULT);
        bluetil_ad_add(&_ad, BLE_GAP_AD_UUID16_INCOMP, &svc, sizeof(svc));
        if (params->node_id) {
            bluetil_ad_add(&_ad, BLE_GAP_AD_NAME,
                           params->node_id, strlen(params->node_id));
        }
    }
#else
    (void)adbuf;
    (void)adlen;
#endif

    _adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    _adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    _adv_params.itvl_min = ((params->adv_itvl * 1000) / BLE_HCI_ADV_ITVL);
    _adv_params.itvl_max = ((params->adv_itvl * 1000) / BLE_HCI_ADV_ITVL);
    _adv_params.channel_map = 0;
    _adv_params.filter_policy = 0;
    _adv_params.high_duty_cycle = 0;

    /* for advertising and scanning, the new parameters will be applied
     * automatically. */

    // TODO: apply _conn_params for all open connections where we are MASTER

    return NIMBLE_AUTOCONN_OK;
}

void nimble_autoconn_enable(void)
{
    DEBUG("[autoconn] ENBALED\n");
    if (nimble_netif_conn_count(NIMBLE_NETIF_UNUSED) > 0) {
        _active = 1;
        _state = STATE_ADV;
        _on_state_change(NULL);
    }
}

void nimble_autoconn_disable(void)
{
    DEBUG("[autoconn] DISABLED\n");
    if ((_state == STATE_ADV) || (_state == STATE_SCAN)) {
        _state = STATE_IDLE;
        ble_npl_callout_stop(&_state_evt);
        nimble_scanner_stop();
#ifndef AUTOCONN_SCAN_ONLY
        nimble_netif_accept_stop();
#endif
    }
    _active = 0;
}
