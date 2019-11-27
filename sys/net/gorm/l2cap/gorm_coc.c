/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_gorm_l2cap_coc
 * @{
 *
 * @file
 * @brief       Gorm's L2CAP connection oriented channel (COC) implementation
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @}
 */

gorm_coc_on_data(coc, llid, buf, cid, llid, data, data_len)

void gorm_coc_on_data(gorm_ctx_t *con, gorm_buf_t *buf,
                      uint16_t cid, uint8_t llid, uint8_t *data, size_t data_len)
{
    gorm_coc_t *coc = con->l2cap.cocs;

    while (coc != NULL) {
        if (coc->cid_src == cid) {
            DEBUG("[gorm_l2cap] _find_cid: data on channel 0x%04x\n", (int)cid);
            gorm_coc_on_data(coc, llid, buf, cid, llid, data, data_len);
        }
        coc = coc->next;
    }

    if (coc == NULL) {
        DEBUG("[gorm_l2cap] _find_cid: data on invalid channel 0x%04x\n", (int)cid);
        gorm_buf_return(buf);
    }
}


void gorm_coc_on_data(gorm_coc_t *coc, uint8_t llid?, gorm_buf_t *data);
