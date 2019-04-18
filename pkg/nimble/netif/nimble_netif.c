/*
 * Copyright (C) 2018,2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     pkt_nimble_gnrc
 *
 * # TODO
 * - many many things...
 *
 *
 * @{
 *
 * @file
 * @brief       NimBLE integration with GNRC
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <limits.h>

#include "assert.h"
#include "thread.h"
#include "mutex.h"

#include "net/ble.h"
#include "net/bluetil/addr.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/hdr.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/pktbuf.h"
#include "net/gnrc/nettype.h"

#include "nimble_netif.h"
#include "nimble_netif_conn.h"
#include "nimble_riot.h"
#include "host/ble_gap.h"
#include "host/util/util.h"

#define ENABLE_DEBUG            (0)
#include "debug.h"

#ifdef MODULE_GNRC_SIXLOWPAN
#define NTYPE                   GNRC_NETTYPE_SIXLOWPAN
#else
#define NTYPE                   GNRC_NETTYPE_UNDEF
#endif

/* maximum packet size for IPv6 packets */
#ifndef NIMBLE_NETIF_IPV6_MTU
#define NIMBLE_NETIF_IPV6_MTU    (1280U)    /* as specified in RFC7668 */
#endif

/* buffer configuration
 * - we need one RX buffer per connection
 * - and a single shared TX buffer*/
#define MTU_SIZE                (NIMBLE_NETIF_IPV6_MTU)
// #define MBUF_CNT                (MYNEWT_VAL_BLE_MAX_CONNECTIONS + 1)
#define MBUF_CNT                (MYNEWT_VAL_BLE_MAX_CONNECTIONS * 2)
#define MBUF_OVHD               (sizeof(struct os_mbuf) + \
                                 sizeof(struct os_mbuf_pkthdr))
#define MBUF_SIZE               (MBUF_OVHD + MTU_SIZE)

/* allocate a stack for the netif device */
static char _stack[THREAD_STACKSIZE_DEFAULT];

/* keep the actual device state */
static gnrc_netif_t *_nimble_netif = NULL;
static gnrc_nettype_t _nettype = NTYPE;

/* keep a reference to the event callback */
static nimble_netif_eventcb_t _eventcb;

/* allocation of memory for buffering IP packets when handing them to NimBLE */
static os_membuf_t _mem[OS_MEMPOOL_SIZE(MBUF_CNT, MBUF_SIZE)];
static struct os_mempool _mem_pool;
static struct os_mbuf_pool _mbuf_pool;

/* notify the user about state changes for a connection context */
static void _notify(int handle, nimble_netif_event_t event)
{
    if (_eventcb) {
        _eventcb(handle, event);
    }
}

/* copy snip to mbuf */
static struct os_mbuf *_pkt2mbuf(struct os_mbuf_pool *pool, gnrc_pktsnip_t *pkt)
{
    struct os_mbuf *sdu = os_mbuf_get_pkthdr(pool, 0);
    if (sdu == NULL) {
        return NULL;
    }
    while (pkt) {
        int res = os_mbuf_append(sdu, pkt->data, pkt->size);
        if (res != 0) {
            os_mbuf_free_chain(sdu);
            return NULL;
        }
        pkt = pkt->next;
    }
    return sdu;
}

static int _send_pkt(const nimble_netif_conn_t *conn, gnrc_pktsnip_t *pkt)
{
    if (conn->coc == NULL) {
        printf("    [] (%p) err: L2CAP not connected (yet)\n", conn);
        return NIMBLE_NETIF_DEVERR;
    }

    struct os_mbuf *sdu = _pkt2mbuf(&_mbuf_pool, pkt);
    if (sdu == NULL) {
        printf("    [] (%p) err: could not alloc mbuf\n", conn);
        return NIMBLE_NETIF_NOMEM;
    }

    int res;
    do {
        res = ble_l2cap_send(conn->coc, sdu);
    } while (res == BLE_HS_EBUSY);

    if (res != 0) {
        os_mbuf_free_chain(sdu);
        printf("    [] (%p) err: l2cap send failed (%i)\n", conn, res);
        return NIMBLE_NETIF_DEVERR;
    }

    return NIMBLE_NETIF_OK;
}

static void _netif_init(gnrc_netif_t *netif)
{
    (void)netif;

    DEBUG("[nimg] _netif_init\n");

#ifdef MODULE_GNRC_SIXLOWPAN
    DEBUG("    [] setting max_frag_size to 0\n");
    /* we disable fragmentation for this device, as the L2CAP layer takes care
     * of this */
    _nimble_netif->sixlo.max_frag_size = 0;
#endif
}

static int _netif_send_iter(const nimble_netif_conn_t *conn,
                            int handle, void *arg)
{
    (void)handle;
    _send_pkt(conn, (gnrc_pktsnip_t *)arg);
    return 0;
}

static int _netif_send(gnrc_netif_t *netif, gnrc_pktsnip_t *pkt)
{
    (void)netif;

    // DEBUG("[nimg] _netif_send\n");
    if (pkt->type != GNRC_NETTYPE_NETIF) {
        assert(0);
        return NIMBLE_NETIF_DEVERR;
    }

    gnrc_netif_hdr_t *hdr = (gnrc_netif_hdr_t *)pkt->data;
    /* if packet is bcast or mcast, we send it to every connected node */
    if (hdr->flags &
        (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST)) {
        nimble_netif_conn_foreach(NIMBLE_NETIF_L2CAP_CONNECTED,
                                  _netif_send_iter, pkt->next);
    }
    /* send unicast */
    else {
        int handle = nimble_netif_conn_get_by_addr(
            gnrc_netif_hdr_get_dst_addr(hdr));
        nimble_netif_conn_t *conn = nimble_netif_conn_get(handle);
        assert(conn);
        int res = _send_pkt(conn, pkt->next);
        assert(res == NIMBLE_NETIF_OK);
        (void)res;
    }

    /* release the packet in GNRC's packet buffer */
    gnrc_pktbuf_release(pkt);
    return 0;
}

/* not used, we pass incoming data to GNRC directly from the NimBLE thread */
static gnrc_pktsnip_t *_netif_recv(gnrc_netif_t *netif)
{
    (void)netif;
    return NULL;
}

static const gnrc_netif_ops_t _nimble_netif_ops = {
    .init = _netif_init,
    .send = _netif_send,
    .recv = _netif_recv,
    .get = gnrc_netif_get_from_netdev,
    .set = gnrc_netif_set_from_netdev,
    .msg_handler = NULL,
};

static inline int _netdev_init(netdev_t *dev)
{
    _nimble_netif = dev->context;

    /* get our own address from the controller */
    int res = ble_hs_id_copy_addr(nimble_riot_own_addr_type,
                                  _nimble_netif->l2addr, NULL);
    assert(res == 0);
    (void)res;
    return 0;
}

static inline int _netdev_get(netdev_t *dev, netopt_t opt,
                              void *value, size_t max_len)
{
    (void)dev;
    int res = -ENOTSUP;

    switch (opt) {
        case NETOPT_ADDRESS:
            assert(max_len >= BLE_ADDR_LEN);
            memcpy(value, _nimble_netif->l2addr, BLE_ADDR_LEN);
            res = BLE_ADDR_LEN;
            break;
        case NETOPT_ADDR_LEN:
        case NETOPT_SRC_LEN:
            assert(max_len == sizeof(uint16_t));
            *((uint16_t *)value) = BLE_ADDR_LEN;
            res = sizeof(uint16_t);
            break;
        case NETOPT_MAX_PACKET_SIZE:
            assert(max_len >= sizeof(uint16_t));
            *((uint16_t *)value) = MTU_SIZE;
            res = sizeof(uint16_t);
            break;
        case NETOPT_PROTO:
            assert(max_len == sizeof(gnrc_nettype_t));
            *((gnrc_nettype_t *)value) = _nettype;
            res = sizeof(gnrc_nettype_t);
            break;
        case NETOPT_DEVICE_TYPE:
            assert(max_len == sizeof(uint16_t));
            *((uint16_t *)value) = NETDEV_TYPE_BLE;
            res = sizeof(uint16_t);
            break;
        default:
            break;
    }

    return res;
}

static inline int _netdev_set(netdev_t *dev, netopt_t opt,
                              const void *value, size_t val_len)
{
    (void)dev;
    int res = -ENOTSUP;

    switch (opt) {
        case NETOPT_PROTO:
            assert(val_len == sizeof(gnrc_nettype_t));
            memcpy(&_nettype, value, sizeof(gnrc_nettype_t));
            res = sizeof(gnrc_nettype_t);
            break;
        default:
            break;
    }

    return res;
}

static const netdev_driver_t _nimble_netdev_driver = {
    .send = NULL,
    .recv = NULL,
    .init = _netdev_init,
    .isr  =  NULL,
    .get  = _netdev_get,
    .set  = _netdev_set,
};

static netdev_t _nimble_netdev_dummy = {
    .driver = &_nimble_netdev_driver,
};


static int _on_data(nimble_netif_conn_t *conn, struct ble_l2cap_event *event)
{
    int ret = 0;
    struct os_mbuf *rxb = event->receive.sdu_rx;
    size_t rx_len = (size_t)OS_MBUF_PKTLEN(rxb);

    // DEBUG("    [] (%p) ON_DATA: received %u bytes\n", conn, rx_len);

    /* allocate netif header */
    gnrc_pktsnip_t *if_snip = gnrc_netif_hdr_build(conn->addr, BLE_ADDR_LEN,
                                                   _nimble_netif->l2addr,
                                                   BLE_ADDR_LEN);
    if (if_snip == NULL) {
        DEBUG("    [] (%p) err: unable to allocate netif hdr\n", conn);
        ret = NIMBLE_NETIF_NOMEM;
        goto end;
    }

    /* we need to add the device PID to the netif header */
    gnrc_netif_hdr_t *netif_hdr = (gnrc_netif_hdr_t *)if_snip->data;
    netif_hdr->if_pid = _nimble_netif->pid;

    /* allocate space in the pktbuf to store the packet */
    gnrc_pktsnip_t *payload = gnrc_pktbuf_add(if_snip, NULL, rx_len, _nettype);
    if (payload == NULL) {
        DEBUG("    [] (%p) err: unable to allocate payload in pktbuf\n", conn);
        gnrc_pktbuf_release(if_snip);
        ret = NIMBLE_NETIF_NOMEM;
        goto end;
    }

    /* copy payload from mbuf into pktbuffer */
    int res = os_mbuf_copydata(rxb, 0, rx_len, payload->data);
    if (res != 0) {
        DEBUG("    [] (%p) err: could not copy data from mbuf chain\n", conn);
        gnrc_pktbuf_release(payload);
        ret = NIMBLE_NETIF_DEVERR;
        goto end;
    }

    /* finally dispatch the receive packet to GNRC */
    // DEBUG("    [] (%p) handing snip of type %i to GNRC\n", conn, (int)payload->type);
    if (!gnrc_netapi_dispatch_receive(payload->type, GNRC_NETREG_DEMUX_CTX_ALL,
                                      payload)) {
        DEBUG("    [] (%p) err: no on interested in the new pkt\n", conn);
        gnrc_pktbuf_release(payload);
    }
    // DEBUG("    [] (%p) GNRC did accept the packet\n", conn);

end:
    /* copy the receive data and free the mbuf */
    os_mbuf_free_chain(rxb);
    /* free the mbuf and allocate a new one for receiving new data */
    rxb = os_mbuf_get_pkthdr(&_mbuf_pool, 0);
    /* due to buffer provisioning, there should always be enough space */
    assert(rxb != NULL);
    ble_l2cap_recv_ready(event->receive.chan, rxb);

    return ret;
}

static int _on_l2cap_client_evt(struct ble_l2cap_event *event, void *arg)
{
    int handle = (int)arg;
    nimble_netif_conn_t *conn = nimble_netif_conn_get(handle);
    // printf("l2cap client: handle: %i, conn: %p\n", handle, conn);
    // printf("l2cap slave client, state is 0x%04x\n", (int)conn->state);
    assert(conn && (conn->state & NIMBLE_NETIF_GAP_CONNECTED));

    // TODO: remove
    // if (event->type != BLE_L2CAP_EVENT_COC_DATA_RECEIVED) {
    //     printf("[nimg] (%p) _on_l2cap_slave_evt: event %i\n",
    //           conn, (int)event->type);
    // }

    switch (event->type) {
        case BLE_L2CAP_EVENT_COC_CONNECTED:
            conn->coc = event->connect.chan;
            conn->state |= NIMBLE_NETIF_L2CAP_CLIENT;
            conn->state &= ~NIMBLE_NETIF_CONNECTING;
            _notify(handle, NIMBLE_NETIF_CONNECTED_MASTER);
            break;
        case BLE_L2CAP_EVENT_COC_DISCONNECTED:
            assert(conn->coc);
            conn->coc = NULL;
            conn->state &= ~NIMBLE_NETIF_L2CAP_CONNECTED;
            break;
        case BLE_L2CAP_EVENT_COC_ACCEPT:
            /* this event should never be triggered for the L2CAP client */
            assert(0);
            break;
        case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
            _on_data(conn, event);
            break;
        default:
            assert(0);
            break;
    }

    return 0;
}

static int _on_l2cap_server_evt(struct ble_l2cap_event *event, void *arg)
{
    (void)arg;
    int handle;
    nimble_netif_conn_t *conn;

    // TODO: remove
    // if (event->type != BLE_L2CAP_EVENT_COC_DATA_RECEIVED) {
    //     printf("[nimg] _on_l2cap_server_evt: event %i\n", (int)event->type);
    // }

    switch (event->type) {
        case BLE_L2CAP_EVENT_COC_CONNECTED:
            handle = nimble_netif_conn_get_adv();
            conn = nimble_netif_conn_get(handle);
            assert(conn);
            conn->coc = event->connect.chan;
            conn->state |= NIMBLE_NETIF_L2CAP_SERVER;
            conn->state &= ~(NIMBLE_NETIF_ADV | NIMBLE_NETIF_CONNECTING);
            _notify(handle, NIMBLE_NETIF_CONNECTED_SLAVE);
            break;
        case BLE_L2CAP_EVENT_COC_DISCONNECTED:
            conn = nimble_netif_conn_get(nimble_netif_conn_get_by_gaphandle(
                event->disconnect.conn_handle));
            assert(conn && conn->coc);
            conn->coc = NULL;
            conn->state &= ~NIMBLE_NETIF_L2CAP_CONNECTED;
            break;
        case BLE_L2CAP_EVENT_COC_ACCEPT: {
            struct os_mbuf *sdu_rx = os_mbuf_get_pkthdr(&_mbuf_pool, 0);
            /* there should always be enough buffer space */
            assert(sdu_rx != NULL);
            ble_l2cap_recv_ready(event->accept.chan, sdu_rx);
            break;
        }
        case BLE_L2CAP_EVENT_COC_DATA_RECEIVED:
            conn = nimble_netif_conn_get(nimble_netif_conn_get_by_gaphandle(
                event->receive.conn_handle));
            assert(conn);
            _on_data(conn, event);
            break;
        default:
            assert(0);
            break;
    }

    return 0;
}

static void _on_gap_connected(nimble_netif_conn_t *conn, uint16_t conn_handle)
{
    struct ble_gap_conn_desc desc;
    int res = ble_gap_conn_find(conn_handle, &desc);
    assert(res == 0);

    conn->gaphandle = conn_handle;
    memcpy(conn->addr, desc.peer_id_addr.val, BLE_ADDR_LEN);
}

static int _on_gap_master_evt(struct ble_gap_event *event, void *arg)
{
    int res = 0;
    int handle = (int)arg;
    nimble_netif_conn_t *conn = nimble_netif_conn_get(handle);
    // printf("gap master: handle: %i, conn: %p\n", handle, conn);
    assert(conn);

    // printf("[nimg] (%p) _on_gap_master_evt: event %i\n", conn, (int)event->type);

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT: {
            if (event->connect.status != 0) {
                nimble_netif_conn_free(handle);
                _notify(handle, NIMBLE_NETIF_CONNECT_ABORT);
                return 0;
            }
            _on_gap_connected(conn, event->connect.conn_handle);
            conn->state |= NIMBLE_NETIF_GAP_MASTER;

            struct os_mbuf *sdu_rx = os_mbuf_get_pkthdr(&_mbuf_pool, 0);
            /* we should never run out of buffer space... */
            assert(sdu_rx != NULL);
            res = ble_l2cap_connect(event->connect.conn_handle,
                                    NIMBLE_NETIF_CID, MTU_SIZE, sdu_rx,
                                    _on_l2cap_client_evt, (void *)handle);
            if (res != 0) {
                os_mbuf_free_chain(sdu_rx);
                printf("    [] (%p) l2cap connect: FAIL (%i)\n", conn, res);
            }
            break;
        }
        case BLE_GAP_EVENT_DISCONNECT:
            nimble_netif_conn_free(handle);
            _notify(handle, NIMBLE_NETIF_DISCONNECTED);
            break;
        case BLE_GAP_EVENT_MTU:
            printf("[nimg] GAP MTU event, new MTU is %u\n",
                   (unsigned)event->mtu.value);
            break;
        default:
            DEBUG("    [] (%p) ERROR: unknown GAP event!\n", conn);
            assert(0);
            break;
    }

    return res;
}

static int _on_gap_slave_evt(struct ble_gap_event *event, void *arg)
{
    int handle = (int)arg;
    nimble_netif_conn_t *conn = nimble_netif_conn_get(handle);
    // printf("gap slave: handle: %i, conn: %p\n", handle, conn);
    assert(conn);

    // printf("[nimg] (%p) _on_gap_slave_evt: event %i\n", conn, (int)event->type);

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT: {
            if (event->connect.status != 0) {
                nimble_netif_conn_free(handle);
                _notify(handle, NIMBLE_NETIF_CONNECT_ABORT);
                break;
            }
            _on_gap_connected(conn, event->connect.conn_handle);
            conn->state |= NIMBLE_NETIF_GAP_SLAVE;
            printf("[nimg] GAP connected, state 0x%04x\n", (int)conn->state);
            break;
        }
        case BLE_GAP_EVENT_DISCONNECT:
            printf("    [] (%p) GAP slave disconnect (%i)\n", conn,
                   event->disconnect.reason);
            nimble_netif_conn_free(handle);
            _notify(handle, NIMBLE_NETIF_DISCONNECTED);
            break;
        default:
            DEBUG("    [] (%p) ERROR: unknown GAP event!\n", conn);
            assert(0);
            break;
    }

    return 0;
}

void nimble_netif_init(void)
{
    int res;
    (void)res;

    /* setup the connection context table */
    nimble_netif_conn_init();

    /* initialize of BLE related buffers */
    res = os_mempool_init(&_mem_pool, MBUF_CNT, MBUF_SIZE, _mem, "nim_gnrc");
    assert(res == 0);
    res = os_mbuf_pool_init(&_mbuf_pool, &_mem_pool, MBUF_SIZE, MBUF_CNT);
    assert(res == 0);

    res = ble_l2cap_create_server(NIMBLE_NETIF_CID, MTU_SIZE,
                                  _on_l2cap_server_evt, NULL);
    assert(res == 0);
    (void)res;

    gnrc_netif_create(_stack, sizeof(_stack), GNRC_NETIF_PRIO,
                      "nimble_netif", &_nimble_netdev_dummy, &_nimble_netif_ops);
}

int nimble_netif_eventcb(nimble_netif_eventcb_t cb)
{
    _eventcb = cb;
    return NIMBLE_NETIF_OK;
}

int nimble_netif_connect(const ble_addr_t *addr,
                         const struct ble_gap_conn_params *conn_params,
                         uint32_t connect_timeout)
{
    assert(addr);
    assert(_eventcb);

    /* check that there is no open connection with the given addr */
    if (nimble_netif_conn_connected(addr->val) != 0) {
        printf("    [] ERROR: already connected to that address\n");
        return NIMBLE_NETIF_ALREADY;
    }
    if (nimble_netif_conn_connecting()) {
        return NIMBLE_NETIF_BUSY;
    }

    /* get empty connection context */
    int handle = nimble_netif_conn_start_connection(addr->val);
    if (handle == NIMBLE_NETIF_CONN_INVALID) {
        printf("    [] ERROR: no free connection context\n");
        return NIMBLE_NETIF_NOMEM;
    }

    int res = ble_gap_connect(nimble_riot_own_addr_type, addr, connect_timeout,
                              conn_params, _on_gap_master_evt, (void *)handle);
    assert(res == 0);
    (void)res;

    return NIMBLE_NETIF_OK;
}

int nimble_netif_close(int handle)
{
    if ((handle < 0) || (handle > MYNEWT_VAL_BLE_MAX_CONNECTIONS)) {
        return NIMBLE_NETIF_NOTFOUND;
    }

    nimble_netif_conn_t *conn = nimble_netif_conn_get(handle);
    if (!(conn->state & NIMBLE_NETIF_L2CAP_CONNECTED)) {
        return NIMBLE_NETIF_NOTCONN;
    }

    int res = ble_gap_terminate(ble_l2cap_get_conn_handle(conn->coc),
                                BLE_ERR_REM_USER_CONN_TERM);
    // TODO; use assert only
    if (res != 0) {
        DEBUG("    [] ERROR: triggering termination (%i)\n", res);
        assert(0);
        return NIMBLE_NETIF_DEVERR;
    }

    return NIMBLE_NETIF_OK;
}

int nimble_netif_accept(const uint8_t *ad, size_t ad_len,
                        const struct ble_gap_adv_params *adv_params)
{
    assert(ad);
    assert(adv_params);

    int res;
    (void)res;

    /* are we already advertising? */
    res = nimble_netif_conn_start_adv();
    if (res != NIMBLE_NETIF_OK) {
        return res;
    }

    /* set advertisement data */
    res = ble_gap_adv_set_data(ad, (int)ad_len);
    assert(res == 0);
    /* remember context and start advertising */
    res = ble_gap_adv_start(nimble_riot_own_addr_type, NULL, BLE_HS_FOREVER,
                            adv_params, _on_gap_slave_evt, NULL);
    assert(res == 0);

    return NIMBLE_NETIF_OK;
}

int nimble_netif_accept_stop(void)
{

    int handle = nimble_netif_conn_get_adv();
    if (handle == NIMBLE_NETIF_CONN_INVALID) {
        return NIMBLE_NETIF_NOTADV;
    }

    int res = ble_gap_adv_stop();
    assert((res == 0) || (res == BLE_HS_EALREADY));
    (void)res;
    nimble_netif_conn_free(handle);

    return NIMBLE_NETIF_OK;
}
