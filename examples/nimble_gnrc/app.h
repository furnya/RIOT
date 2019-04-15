

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#define APP_ADV_ITVL            (100000U / BLE_HCI_ADV_ITVL)    /* 100ms */

#define APP_ADV_NAME_DEFAULT    "RIOT-GNRC-man"

void app_ble_init(void);

int app_ble_cmd(int argc, char **argv);

int app_udp_cmd(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
/** @} */
