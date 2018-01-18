/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers_nrf5x_nrfmin_gnrc
 * @{
 *
 * @file
 * @brief       GNRC adapter for the nrfmin radio driver
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include "net/gnrc.h"
#include "thread.h"
#include "net/gnrc/netif.h"

#include "nrfmin_gnrc.h"

#define ENABLE_DEBUG            (1)
#include "debug.h"

/**
 * @brief   Definition of default thread priority and stacksize
 * @{
 */
#ifndef NRFMIN_GNRC_THREAD_PRIO
#define NRFMIN_GNRC_THREAD_PRIO     GNRC_NETIF_PRIO
#endif

#ifndef NRFMIN_GNRC_STACKSIZE
#define NRFMIN_GNRC_STACKSIZE       THREAD_STACKSIZE_DEFAULT
#endif
/** @} */

/**
 * @brief
 */
#define BCAST   (GNRC_NETIF_HDR_FLAGS_BROADCAST | GNRC_NETIF_HDR_FLAGS_MULTICAST)

/**
 * @brief   Allocate the stack for the GNRC netdev thread to run in
 */
static char stack[NRFMIN_GNRC_STACKSIZE];

static void dump(void *buf, int len)
{
    // uint8_t *tmp = (uint8_t *)buf;
    // for (int i = 0; i < (int)len; ) {
    //     printf("[%02i] ", i);
    //     for (int l = 0; (l < 16) && (i < (int)len); l++) {
    //         printf("%02x ", (int)tmp[i++]);
    //     }
    //     puts("");
    // }

    (void)buf;
    (void)len;
}

static int hdr_netif_to_nrfmin(nrfmin_hdr_t *nrfmin, gnrc_pktsnip_t *pkt)
{
    gnrc_netif_hdr_t *netif = (gnrc_netif_hdr_t *)pkt->data;

    if (!(netif->flags & BCAST) && (netif->dst_l2addr_len != 2)) {
        return -EINVAL;
    }

    nrfmin->len = gnrc_pkt_len(pkt->next) + NRFMIN_HDR_LEN;
    if (netif->flags & BCAST) {
        nrfmin->dst_addr = NRFMIN_ADDR_BCAST;
    }
    else {
        memcpy(&nrfmin->dst_addr, gnrc_netif_hdr_get_dst_addr(netif), 2);
    }
    nrfmin->src_addr = nrfmin_get_addr();
    if (pkt->next) {
        nrfmin->proto = (uint8_t)pkt->next->type;
    }
    else {
        nrfmin->proto = 0;
    }

    return 0;
}

static int gnrc_nrfmin_send(gnrc_netif_t *dev, gnrc_pktsnip_t *pkt)
{
    int res;
    struct iovec *vec;
    size_t vec_len;
    gnrc_pktsnip_t *vec_snip;
    nrfmin_hdr_t nrfmin_hdr;

    assert(pkt);

    if (pkt->type != GNRC_NETTYPE_NETIF) {
        DEBUG("[nrfmin_gnrc] send: first header is not generic netif header\n");
        return -EBADMSG;
    }

    /* build the nrfmin header from the generic netif header */
    res = hdr_netif_to_nrfmin(&nrfmin_hdr, pkt);
    if (res < 0) {
        DEBUG("[nrfmin_gnrc] send: failed to build nrfmin header\n");
        gnrc_pktbuf_release(pkt);
        return res;
    }

    /* create iovec of data */
    vec_snip = gnrc_pktbuf_get_iovec(pkt, &vec_len);
    if (vec_snip == NULL) {
        DEBUG("[nrfmin_gnrc] send: failed to create IO vector\n");
        gnrc_pktbuf_release(pkt);
        return -ENOBUFS;
    }

    /* link first entry of the vector to the nrfmin header */
    vec = (struct iovec *)vec_snip->data;
    vec[0].iov_base = &nrfmin_hdr;
    vec[0].iov_len = NRFMIN_HDR_LEN;

    size_t tmplen = 0;
    for (int i = 0; i < (int)vec_len; i++) {
        tmplen += vec[i].iov_len;
    }
    DEBUG("[nrfmin_gnrc] send: send packet, len is %i\n", (int)tmplen);
    for (int i = 0; i < (int)vec_len; i++) {
        dump(vec[i].iov_base, vec[i].iov_len);
    }

    /* and finally send out the data and release the packet */
    res = dev->dev->driver->send(dev->dev, vec, vec_len);
    gnrc_pktbuf_release(vec_snip);

    return res;
}

static gnrc_pktsnip_t *gnrc_nrfmin_recv(gnrc_netif_t *dev)
{
    int pktsize;
    nrfmin_hdr_t *nrfmin;
    gnrc_netif_hdr_t *netif;
    gnrc_pktsnip_t *pkt_snip;
    gnrc_pktsnip_t *hdr_snip;
    gnrc_pktsnip_t *netif_snip;

    /* get the size of the new packet */
    pktsize = nrfmin_dev.driver->recv(NULL, NULL, 0, NULL);
    if (pktsize <= 0) {
        DEBUG("[nrfmin_gnrc] recv: error: tried to read empty packet\n");
        return NULL;
    }

    DEBUG("[nrfmin_gnrc] recv: got packet len: %i\n", (int)pktsize);

    /* allocate space in the packet buffer */
    pkt_snip = gnrc_pktbuf_add(NULL, NULL, pktsize, GNRC_NETTYPE_UNDEF);
    if (pkt_snip == NULL) {
        DEBUG("[nrfmin_gnrc] recv: unable to allocate pktsnip\n");
        return NULL;
    }

    /* read the incoming data into the packet buffer */
    nrfmin_dev.driver->recv(NULL, pkt_snip->data, pktsize, NULL);

    dump(pkt_snip->data, pktsize);

    /* now we mark the nrfmin header */
    hdr_snip = gnrc_pktbuf_mark(pkt_snip, NRFMIN_HDR_LEN, GNRC_NETTYPE_UNDEF);
    if (hdr_snip == NULL) {
        DEBUG("[nrfmin_gnrc] recv: unable to mark the nrfmin header\n");
        gnrc_pktbuf_release(pkt_snip);
        return NULL;
    }

    /* allocate the generic netif header and populate it with data from the
       nrfmin header */
    nrfmin = (nrfmin_hdr_t *)hdr_snip->data;
    netif_snip = gnrc_netif_hdr_build((uint8_t *)&nrfmin->src_addr, 2,
                                      (uint8_t *)&nrfmin->dst_addr, 2);
    if (netif_snip == NULL) {
        DEBUG("[nrfmin_gnrc] recv: unable to allocate netif header\n");
        gnrc_pktbuf_release(pkt_snip);
        return NULL;
    }

    netif = (gnrc_netif_hdr_t *)netif_snip->data;
    if (nrfmin->dst_addr == NRFMIN_ADDR_BCAST) {
        netif->flags |= GNRC_NETIF_HDR_FLAGS_BROADCAST;
    }
    netif->lqi = 0;
    netif->rssi = 0;
    netif->if_pid = dev->pid;
    pkt_snip->type = nrfmin->proto;

    /* finally: remove the nrfmin header and append the netif header */
    gnrc_pktbuf_remove_snip(pkt_snip, hdr_snip);
    LL_APPEND(pkt_snip, netif_snip);

    return pkt_snip;
}

static const gnrc_netif_ops_t gnrc_nrfmin_ops = {
    .send = gnrc_nrfmin_send,
    .recv = gnrc_nrfmin_recv,
    .get = gnrc_netif_get_from_netdev,
    .set = gnrc_netif_set_from_netdev,
};

void gnrc_nrfmin_init(void)
{
    /* setup the NRFMIN driver */
    nrfmin_setup();
    gnrc_netif_create(stack, sizeof(stack), NRFMIN_GNRC_THREAD_PRIO, "nrfmin",
                      (netdev_t *)&nrfmin_dev, &gnrc_nrfmin_ops);
}
