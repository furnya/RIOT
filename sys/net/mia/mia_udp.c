

#include <stdint.h>
#include <stdlib.h>

#include "net/protnum.h"

#include "net/mia.h"
#include "net/mia/ip.h"

extern const mia_bind_t mia_udp_bindings[];

void mia_udp_process(void)
{
    int i = 0;
    while (mia_udp_bindings[i].port != 0) {
        if (mia_udp_bindings[i].port == mia_ntos(MIA_UDP_DST)) {
            mia_udp_bindings[i].cb();
            return;
        }
        ++i;
    }
}

void mia_udp_reply(void)
{
    uint8_t tmp[2];

    /* switch UDP ports, set csum to zero and adjust length */
    memcpy(tmp, mia_ptr(MIA_UDP_SRC), 2);
    memcpy(mia_ptr(MIA_UDP_SRC), mia_ptr(MIA_UDP_DST), 2);
    memcpy(mia_ptr(MIA_UDP_DST), tmp, 2);
    memset(mia_ptr(MIA_UDP_CSUM), 0, 2);
    mia_ip_reply(mia_ntos(MIA_UDP_LEN));
}

int mia_udp_send(uint8_t *ip, uint16_t src, uint16_t dst)
{
    mia_ston(MIA_UDP_SRC, src);
    mia_ston(MIA_UDP_DST, dst);
    memset(mia_ptr(MIA_UDP_CSUM), 0, 2);
    return mia_ip_send(ip, PROTNUM_UDP, mia_ntos(MIA_UDP_LEN));
}
