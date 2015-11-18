

#include "net/ethernet.h"
#include "net/ethertype.h"

#include "net/mia.h"
#include "net/mia/eth.h"
#include "net/mia/arp.h"
#include "net/mia/ip.h"


#define ENABLE_DEBUG            (0)
#include "debug.h"



uint8_t mia_mac[ETHERNET_ADDR_LEN];


static void eth_flush(uint16_t len)
{
    struct iovec data;

    data.iov_base = (void *)mia_buf;
    data.iov_len = (size_t)len + MIA_ETH_HDR_LEN;
    mia_dev->driver->send(mia_dev, &data, 1);
}

void mia_eth_process(void)
{
    /* only allow packets addressed to me or broadcast packets */
    if (!(memcmp(mia_ptr(MIA_ETH_DST), mia_mac, ETHERNET_ADDR_LEN) == 0 ||
          memcmp(mia_ptr(MIA_ETH_DST), mia_bcast, ETHERNET_ADDR_LEN) == 0)) {
        return;
    }

    DEBUG("[mia] eth: got valid packet, processing it now...\n");

    /* IPv4 or ARP */
    switch (mia_ntos(MIA_ETH_TYPE)) {
        case ETHERTYPE_ARP:
            mia_arp_process();
            break;
        case ETHERTYPE_IPV4:
            mia_ip_process();
            break;
        default:
            DEBUG("[mia] eth: got packet that we can not handle\n");
            break;
    }
}

void mia_eth_reply(uint16_t len)
{
    memcpy(mia_ptr(MIA_ETH_DST), mia_ptr(MIA_ETH_SRC), ETHERNET_ADDR_LEN);
    memcpy(mia_ptr(MIA_ETH_SRC), mia_mac, ETHERNET_ADDR_LEN);
    eth_flush(len);
}

void mia_eth_send(uint8_t *mac, uint16_t ethertype, uint16_t len)
{
    memcpy(mia_ptr(MIA_ETH_DST), mac, ETHERNET_ADDR_LEN);
    memcpy(mia_ptr(MIA_ETH_SRC), mia_mac, ETHERNET_ADDR_LEN);
    mia_ston(MIA_ETH_TYPE, ethertype);
    eth_flush(len);
}
