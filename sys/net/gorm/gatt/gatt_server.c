/*
 * Copyright (C) 2018 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     net_gorm
 * @{
 *
 * @file
 * @brief       Gorm's GATT server implementation
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

#include <stdint.h>
#include <limits.h>

#include "mutex.h"
#include "net/gorm/util.h"
#include "net/gorm/l2cap.h"
#include "net/gorm/gatt.h"
#include "net/gorm/gatt/tab.h"
#include "net/gorm/pdupool.h"

/* REOMVE! */
#include "xtimer.h"

#define ENABLE_DEBUG                (1)
#include "debug.h"

static size_t _write_service_attr_data(uint8_t *buf, gorm_gatt_entry_t *entry)
{
    gorm_util_htoles(buf, entry->handle);
    gorm_util_htoles(&buf[2], gorm_gatt_tab_get_end_handle(entry));
    return gorm_uuid_to_buf(&buf[4], &entry->service->uuid) + 4;
}

static size_t _write_char_attr_data(uint8_t *buf, gorm_gatt_entry_t *entry,
                                    uint16_t num)
{
    const gorm_gatt_char_t *c = &entry->service->chars[num];
    /* | handle | prop 1b | val handle | uuid | */
    gorm_util_htoles(&buf[0], gorm_gatt_tab_get_char_handle(entry, num));
    buf[2] = c->perm;
    gorm_util_htoles(&buf[3], gorm_gatt_tab_get_val_handle(entry, num));
    return gorm_uuid_to_buf(&buf[5], &c->type) + 5;
}

static void _error(gorm_ll_connection_t *con, gorm_buf_t *buf, uint8_t *data,
                   uint16_t handle, uint8_t code)
{
    data[1] = data[0];      /* copy request opcode */
    data[0] = BLE_ATT_ERROR_RESP;
    gorm_util_htoles(&data[2], handle);
    data[4] = code;
    gorm_l2cap_reply(con, buf, 5);
}

/* TODO: support changing the MTU, therefore the currently used MTU size must
         be part of the connection_t struct or similar... */
static void _on_mtu_req(gorm_ll_connection_t *con, gorm_buf_t *buf,
                        uint8_t *data, size_t len)
{
    if (len != 3) {
        DEBUG("[gatt_server] _on_mtu_req: invalid PDU len\n");
        _error(con, buf, data, 0, BLE_ATT_INVALID_PDU);
        return;
    }

    // uint16_t req_mtu = gorm_util_letohs(&buf->mtu[5]);
    // con->mtu_size = (GORM_GATT_MAX_MTU < req_mtu) ? req_mtu : GORM_GATT_MAX_MTU;
    data[0] = BLE_ATT_MTU_RESP;
    // gorm_util_htoles(&data[1], GORM_GATT_MAX_MTU);
    gorm_util_htoles(&data[1], GORM_GATT_DEFAULT_MTU);
    DEBUG("[gatt_server] _on_mtu_req: sending reply now\n");
    gorm_l2cap_reply(con, buf, 3);
}

/* discover all primary services on the server */
static void _on_read_by_group_type_req(gorm_ll_connection_t *con,
                                       gorm_buf_t *buf,
                                       uint8_t *data, size_t len)
{
    /* we only allow discovery of primary (0x2800) and secondary (0x2801)
     * service through this method, hence we use only 16-bit UUIDs here */
    if (len != 7) {
        DEBUG("[gatt_server] _on_read_by_group_type_req: invalid PDU len\n");
        _error(con, buf, data, 0, BLE_ATT_INVALID_PDU);
        return;
    }

    /* parse request data */
    uint16_t start_handle = gorm_util_letohs(&data[1]);
    uint16_t end_handle = gorm_util_letohs(&data[3]);
    gorm_uuid_t uuid;
    gorm_uuid_from_buf(&uuid, &data[5], 2);

    DEBUG("[gatt_server] start: 0x%04x, end: 0x%04x, uuid16: 0x%04x\n",
          (int)start_handle, (int)end_handle, (int)uuid.uuid16);

    /* so far we only support primary services */
    if ((uuid.uuid16 != BLE_DECL_PRI_SERVICE)) {
        DEBUG("[gatt_server] _on_read_by_group_type_req: not primary service\n");
        goto error;
    }

    /* prepare response, start by getting the first viable entry */
    gorm_gatt_entry_t *entry = gorm_gatt_tab_find_service(start_handle);
    if (entry == NULL) {
        DEBUG("[gatt_server] _on_read_by_group_type_req: no entry found\n");
        goto error;
    }

    /* now we prepare the response and add the previously found service as
       first entry */
    data[0] = BLE_ATT_READ_BY_GROUP_TYPE_RESP;
    data[1] = ((uint8_t)gorm_uuid_len(&entry->service->uuid) + 4);
    size_t pos = _write_service_attr_data(&data[2], entry) + 2;

    /* try to fit as many services with the same length UUID into this
     * response */
    entry = entry->next;
    unsigned limit = ((GORM_GATT_DEFAULT_MTU - pos) / data[1]);
    while (entry && (entry->handle <= end_handle) && (limit > 0)) {
        pos += _write_service_attr_data(&data[pos], entry);
        entry = entry->next;
        --limit;
    }

    DEBUG("[gatt_server] _on_read_by_group_type_req: sending %i byte resp\n",
          (int)pos);

    gorm_l2cap_reply(con, buf, pos);
    return;

error:
    _error(con, buf, data, start_handle, BLE_ATT_ATTRIBUTE_NOT_FOUND);
}

static void _on_read_by_type_req(gorm_ll_connection_t *con,
                                 gorm_buf_t *buf,
                                 uint8_t *data, size_t len)
{
    /* this message type is used in GATT to
     * - find included services (not supported in Gorm for now)
     * - discover all characteristics of a service
     * - discover characteristic by UUID
     * - read char using characteristic UUID (?) */

    /* expected size is 7 or 21 byte (16- or 128-bit UUID) */
    if ((len != 7) && (len != 21)) {
        DEBUG("[gatt_server] read_by_type_req: invalid request length\n");
        _error(con, buf, data, 0, BLE_ATT_INVALID_PDU);
        return;
    }

    /* parse the request data */
    uint16_t start_handle = gorm_util_letohs(&data[1]);
    uint16_t end_handle = gorm_util_letohs(&data[3]);
    gorm_uuid_t uuid;
    gorm_uuid_from_buf(&uuid, &data[5], (len - 5));

    if (gorm_uuid_eq16(&uuid, BLE_DECL_CHAR)) {
        gorm_gatt_entry_t *entry = gorm_gatt_tab_get_service(start_handle);
        if (entry == NULL) {
            goto error;
        }
        int c = gorm_gatt_tab_find_char(entry, start_handle);
        if (c < 0) {
            goto error;
        }

        size_t ulen = gorm_uuid_len(&entry->service->chars[c].type);
        data[0] = BLE_ATT_READ_BY_TYPE_RESP;
        data[1] = ((uint8_t)ulen + 5);
        size_t pos = _write_char_attr_data(&data[2], entry, c) + 2;
        unsigned limit = ((GORM_GATT_DEFAULT_MTU - 2) / data[1]) - 1;

        c++;
        while ((entry->service->chars[c].cb != NULL) &&
               (gorm_uuid_len(&entry->service->chars[c].type) == ulen) &&
               (limit > 0) &&
               (gorm_gatt_tab_get_char_handle(entry, (uint16_t)c) <= end_handle)) {
            pos += _write_char_attr_data(&data[pos], entry, c);
            c++;
            limit--;
        }

        gorm_l2cap_reply(con, buf, pos);
        return;
    }

error:
    /* if nothing fitting was found, we return the default error here */
    _error(con, buf, data, start_handle, BLE_ATT_ATTRIBUTE_NOT_FOUND);
}

static void _on_read_req(gorm_ll_connection_t *con, gorm_buf_t *buf,
                         uint8_t *data, size_t len)
{
    /* this message type is used in GATT for:
     * - read characteristic descriptor
     * - read characteristic value */
    if (len != 3) {
        _error(con, buf, data, 0, BLE_ATT_INVALID_PDU);
        return;
    }

    /* parse handle from request */
    gorm_gatt_tab_iter_t iter;
    iter.handle = gorm_util_letohs(&data[1]);

    /* read data */
    gorm_gatt_tab_get_by_handle(&iter);
    data[0] = BLE_ATT_READ_RESP;

    DEBUG("ON_READ: e %p - %p - %p - 0x%04x\n", iter.e, iter.c, iter.d, (int)iter.handle);

    /* if handle belongs to a characteristic value, check permissions and read
     * that value when allowed */
    if (gorm_gatt_tab_is_char_val(&iter)) {
        DEBUG("-> is char val\n");
        /* TODO: check read permissions and authen/author */
        /* check if characteristic value is readable */
        if (iter.c->perm & BLE_ATT_READ) {
            size_t len = iter.c->cb(iter.c, GORM_GATT_READ,
                                    &data[1], (GORM_GATT_DEFAULT_MTU - 1));
            gorm_l2cap_reply(con, buf, (len + 1));
        }
        else {
            _error(con, buf, data, iter.handle, BLE_ATT_READ_NOT_PERMITTED);
        }
    }
    /* if the handle belongs to a descriptor, we read it */
    else if (gorm_gatt_tab_is_decl(&iter)) {
        DEBUG("-> is desc\n");
        /* note: we assume all descriptors are readable without permissions */
        size_t len = iter.d->cb(iter.d, &data[1], (GORM_GATT_DEFAULT_MTU - 1));
        gorm_l2cap_reply(con, buf, (len + 1));
    }
    else {
        _error(con, buf, data, iter.handle, BLE_ATT_ATTRIBUTE_NOT_FOUND);
    }
}

static void _on_write_req(gorm_ll_connection_t *con, gorm_buf_t *buf,
                          uint8_t *data, size_t len)
{
    if (len < 3) {
        _error(con, buf, data, 0, BLE_ATT_INVALID_PDU);
        return;
    }

    gorm_gatt_tab_iter_t iter;
    iter.handle = gorm_util_letohs(&data[1]);

    /* write data if applicable */
    gorm_gatt_tab_get_by_handle(&iter);
    if (gorm_gatt_tab_is_char_val(&iter)) {
        /* make sure the value is writable and we are allowed to do so */
        /* TODO: check encryption, authentication, and authorization... */
        if (iter.c->perm & BLE_ATT_WRITE) {
            iter.c->cb(iter.c, GORM_GATT_WRITE, &data[3], (len - 3));
            data[0] = BLE_ATT_WRITE_RESP;
            gorm_l2cap_reply(con, buf, 1);
        }
        else {
            _error(con, buf, data, iter.handle, BLE_ATT_WRITE_NOT_PERMITTED);
        }
    }
    else {
        _error(con, buf, data, iter.handle, BLE_ATT_ATTRIBUTE_NOT_FOUND);
    }
}

static void _on_find_info_req(gorm_ll_connection_t *con, gorm_buf_t *buf,
                              uint8_t *data, size_t len)
{
    gorm_gatt_tab_iter_t iter;

    if (len != 5) {
        _error(con, buf, data, 0, BLE_ATT_INVALID_PDU);
        return;
    }

    /* get input parameters */
    iter.handle = gorm_util_letohs(&data[1]);
    uint16_t end_handle = gorm_util_letohs(&data[3]);

    /* read given start handle and check if it belongs to a descriptor */
    gorm_gatt_tab_get_by_handle(&iter);
    if (iter.d == NULL) {
        _error(con, buf, data, iter.handle, BLE_ATT_ATTRIBUTE_NOT_FOUND);
        return;
    }

    data[0] = BLE_ATT_FIND_INFO_RESP;
    data[1] = BLE_ATT_FORMAT_U16;
    size_t pos = 2;
    size_t limit = ((GORM_GATT_DEFAULT_MTU - 2) / 4);

    while (iter.d && (limit > 0) && (iter.handle <= end_handle)) {
        gorm_util_htoles(&data[pos], iter.handle);
        gorm_util_htoles(&data[pos + 2], iter.d->type);
        pos += 4;
        limit--;
        gorm_gatt_tab_get_next(&iter);
    }

    gorm_l2cap_reply(con, buf, pos);
}

void _on_find_by_type_val(gorm_ll_connection_t *con, gorm_buf_t *buf,
                          uint8_t *data, size_t len)
{
    if ((len != 9) && (len != 23)) {
        _error(con, buf, data, 0, BLE_ATT_INVALID_PDU);
        DEBUG("[gorm_gatt] _on_find_by_type_val: invalid PDU\n");
        return;
    }

    /* parse PDU */
    gorm_gatt_tab_iter_t iter;
    uint16_t handle = gorm_util_letohs(&data[1]);
    iter.handle = handle;
    uint16_t end_handle = gorm_util_letohs(&data[3]);
    uint16_t type = gorm_util_letohs(&data[5]);
    gorm_uuid_t uuid;
    gorm_uuid_from_buf(&uuid, &data[7], (len - 7));

    DEBUG("[gorm_gatt] _on_find_by_type_val: start 0x%02x, end 0x%02x\n",
          (int)handle, (int)end_handle);

    /* only allow this type of request to find primary services */
    if (type != BLE_DECL_PRI_SERVICE) {
        DEBUG("[gorm_gatt] _on_find_by_type_val: type not PRIMARY_SERVICE\n");
        goto error;
    }

    /* find first handle */
    /* TODO: make sure we are not above end_handle */
    gorm_gatt_tab_get_service_by_uuid(&iter, &uuid);

    if (iter.e == NULL) {
        DEBUG("[gorm_gatt] _on_find_by_type_val: no service with UUID found\n");
        goto error;
    }

    /* prepare results */
    data[0] = BLE_ATT_FIND_BY_VAL_RESP;
    gorm_util_htoles(&data[1], iter.e->handle);
    gorm_util_htoles(&data[3], gorm_gatt_tab_get_end_handle(iter.e));
    gorm_l2cap_reply(con, buf, 5);
    DEBUG("[grom_gatt] _on_find_by_type_val: found service 0x%02x\n", (int)iter.handle);
    return;

error:
    _error(con, buf, data, handle, BLE_ATT_ATTRIBUTE_NOT_FOUND);
}

void gorm_gatt_server_init(void)
{
    gorm_gatt_tab_init();
    DEBUG("[gorm_gatt] initialization successful\n");
}

void gorm_gatt_on_data(gorm_ll_connection_t *con, gorm_buf_t *buf,
                       uint8_t *data, size_t len)
{
    uint32_t now = xtimer_now_usec();

    switch (data[0]) {
        /* TODO: restore order ... */
        case BLE_ATT_MTU_REQ:
            DEBUG("[gatt_server] _on_mtu_req()\n");
            _on_mtu_req(con, buf, data, len);
            break;
        case BLE_ATT_READ_BY_GROUP_TYPE_REQ:
            DEBUG("[gatt_server] _on_read_by_group_type_req()\n");
            _on_read_by_group_type_req(con, buf, data, len);
            break;
        case BLE_ATT_READ_BY_TYPE_REQ:
            DEBUG("[gatt_server] _on_read_by_type_req()\n");
            _on_read_by_type_req(con, buf, data, len);
            break;
        case BLE_ATT_READ_REQ:
            DEBUG("[gatt_server] _on_read_req()\n");
            _on_read_req(con, buf, data, len);
            break;
        case BLE_ATT_FIND_INFO_REQ:
            DEBUG("[gatt_server] _on_find_info_req()\n");
            _on_find_info_req(con, buf, data, len);
            break;
        case BLE_ATT_WRITE_REQ:
            DEBUG("[gatt_server] _on_write_req()\n");
            _on_write_req(con, buf, data, len);
            break;
        case BLE_ATT_FIND_BY_VAL_REQ:
            DEBUG("[gatt_server] _on_find_by_type_val()\n");
            _on_find_by_type_val(con, buf, data, len);
            break;
        case BLE_ATT_READ_BLOB_REQ:
        case BLE_ATT_READ_MUL_REQ:

        case BLE_ATT_PREP_WRITE_REQ:
        case BLE_ATT_WRITE_COMMAND:
        case BLE_ATT_EXEC_WRITE_REQ:
        case BLE_ATT_VAL_NOTIFICATION:
        case BLE_ATT_VAL_INDICATION:
        case BLE_ATT_VAL_CONFIRMATION:
        case BLE_ATT_SIGNED_WRITE_CMD:
            DEBUG("[gorm_gatt] on_data: unknown opcode %i\n", (int)data[0]);
            _error(con, buf, data, 0, BLE_ATT_REQUEST_NOT_SUP);
            gorm_pdupool_return(buf);
            break;

        case BLE_ATT_ERROR_RESP:
        case BLE_ATT_MTU_RESP:
        case BLE_ATT_FIND_INFO_RESP:
        case BLE_ATT_FIND_BY_VAL_RESP:
        case BLE_ATT_READ_BY_TYPE_RESP:
        case BLE_ATT_READ_RESP:
        case BLE_ATT_READ_BLOB_RESP:
        case BLE_ATT_READ_MUL_RESP:
        case BLE_ATT_READ_BY_GROUP_TYPE_RESP:
        case BLE_ATT_WRITE_RESP:
        case BLE_ATT_PREP_WRITE_RESP:
        case BLE_ATT_EXEC_WRITE_RESP:
        default:
            /* we silently drop any response we get (as we are the server...) */
            DEBUG("[gorm_gatt] on_data: got undefined response, ignoring it\n");
            gorm_pdupool_return(buf);
            break;
    }

    uint32_t diff = xtimer_now_usec() - now;
    DEBUG("[gatt_server] on_data() done (took %u us)\n", (unsigned)diff);
}
