/*
 * Copyright (C) 2016 Michel Rottleuthner <michel.rottleuthner@haw-hamburg.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     drivers
 * @brief       Low-level driver for reading and writing sd-cards via spi interface.
 * @{
 *
 * @file
 * @brief       Low-level driver for reading and writing sd-cards via spi interface.
 *              For details of the sd card standard and the spi mode refer to
 *              "SD Specifications Part 1 Physical Layer Simplified Specification".
 *              References to the sd specs in this file apply to Version 5.00 from August 10, 2016.
 *              See https://www.sdcard.org/downloads/pls/pdf/part1_500.pdf for further details.
 *
 * @author      Michel Rottleuthner <michel.rottleuthner@haw-hamburg.de>
 */

#ifndef SDCARD_SPI_H
#define SDCARD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "periph/spi.h"
#include "periph/gpio.h"
#include "stdbool.h"    


/* number of clocks that should be applied to the card on init
   before taking furter actions (see sd spec. 6.4.1.1 Power Up Time of Card) */
#define SD_POWERSEQUENCE_CLOCK_COUNT 74

/* R1 response bits (see sd spec. 7.3.2.1 Format R1) */
#define SD_R1_RESPONSE_PARAM_ERROR       0b01000000
#define SD_R1_RESPONSE_ADDR_ERROR        0b00100000
#define SD_R1_RESPONSE_ERASE_SEQ_ERROR   0b00010000
#define SD_R1_RESPONSE_CMD_CRC_ERROR     0b00001000
#define SD_R1_RESPONSE_ILLEGAL_CMD_ERROR 0b00000100
#define SD_R1_RESPONSE_ERASE_RESET       0b00000010
#define SD_R1_RESPONSE_IN_IDLE_STATE     0b00000001
#define SD_INVALID_R1_RESPONSE           0b10000000

#define IS_VALID_R1_BYTE(X)   (((X) >> 7) == 0)
#define R1_HAS_PARAM_ERR(X)   ((((X) &SD_R1_RESPONSE_PARAM_ERROR) != 0))
#define R1_HAS_ADDR_ERR(X)    ((((X) &SD_R1_RESPONSE_ADDR_ERROR) != 0))
#define R1_HAS_ERASE_ERR(X)   ((((X) &SD_R1_RESPONSE_ERASE_SEQ_ERROR) != 0))
#define R1_HAS_CMD_CRC_ERR(X) ((((X) &SD_R1_RESPONSE_CMD_CRC_ERROR) != 0))
#define R1_HAS_ILL_CMD_ERR(X) ((((X) &SD_R1_RESPONSE_ILLEGAL_CMD_ERROR) != 0))
#define IS_R1_IDLE_BIT_SET(X) (((X) &SD_R1_RESPONSE_IN_IDLE_STATE) != 0)
#define R1_HAS_ERROR(X) (R1_HAS_PARAM_ERR(X) || R1_HAS_ADDR_ERR(X) || R1_HAS_ERASE_ERR(X) || R1_HAS_CMD_CRC_ERR(X) || R1_HAS_ILL_CMD_ERR(X))

/* see sd spec. 7.3.3.1 Data Response Token */
#define DATA_RESPONSE_IS_VALID(X)  (((X) & 0b00010001) == 0b00000001)
#define DATA_RESPONSE_ACCEPTED(X)  (((X) & 0b00001110) == 0b00000100)
#define DATA_RESPONSE_CRC_ERR(X)  (((X) & 0b00001110) == 0b00001010)
#define DATA_RESPONSE_WRITE_ERR(X) (((X) & 0b00001110) == 0b00001100)

/* see sd spec. 5.1 OCR register */
#define OCR_VOLTAGE_3_2_TO_3_3 (1 << 20)
#define OCR_VOLTAGE_3_3_TO_3_4 (1 << 21)
#define OCR_CCS (1 << 30)               /* card capacity status (CCS=0 means that the card is SDSD. CCS=1 means that the card is SDHC or SDXC.) */
#define OCR_POWER_UP_STATUS (1 << 31)   /* This bit is set to low if the card has not finished power up routine */

/* to ensure the voltage range check on init is done properly you need to
   define this according to your actual interface/wiring with the sd-card */
#define SYSTEM_VOLTAGE (OCR_VOLTAGE_3_2_TO_3_3 | OCR_VOLTAGE_3_2_TO_3_3)


/* see sd spec. 7.3.1.3 Detailed Command Description */
#define SD_CMD_PREFIX_MASK 0b01000000

#define SD_CMD_0_IDX 0      /* Resets the SD Memory Card */
#define SD_CMD_1_IDX 1      /* Sends host capacity support information and activates the card's initialization process */
#define SD_CMD_8_IDX 8      /* Sends SD Memory Card interface condition that includes host supply voltage information */
#define SD_CMD_9_IDX 9      /* Asks  the  selected  card  to  send  its card-specific data (CSD) */
#define SD_CMD_10_IDX 10    /* Asks  the  selected  card  to  send  its card identification (CID) */
#define SD_CMD_12_IDX 12    /* Forces  the  card to  stop  transmission in Multiple Block Read Operation */

#define SD_CMD_16_IDX 16    /* In case of SDSC Card, block length is set by this command */
#define SD_CMD_17_IDX 17    /* Reads a block of the size selected by the SET_BLOCKLEN command */
#define SD_CMD_18_IDX 18    /* Continuously  transfers  data  blocks from card to host until interrupted by a STOP_TRANSMISSION command */
#define SD_CMD_24_IDX 24    /* Writes a block of the size selected by the SET_BLOCKLEN command */
#define SD_CMD_25_IDX 25    /* Continuously writes blocks of data until 'Stop Tran'token is sent */
#define SD_CMD_41_IDX 41    /* Reserved (used for ACMD41) */
#define SD_CMD_55_IDX 55    /* Defines to the card that the next commmand is an application specific command rather than a standard command */
#define SD_CMD_58_IDX 58    /* Reads the OCR register of a card */
#define SD_CMD_59_IDX 59    /* Turns the CRC option on or off. A '1'in the CRC option bit will turn the option on, a '0'will turn it off */

#define SD_CMD_8_VHS_2_7_V_TO_3_6_V 0b00000001
#define SD_CMD_8_CHECK_PATTERN      0b10110101
#define SD_CMD_ARG_NONE   0x00000000
#define SD_ACMD_41_ARG_HC 0x40000000
#define SD_CMD_59_ARG_ENABLE  0x00000001
#define SD_CMD_59_ARG_DISABLE 0x00000000

/* see sd spec. 7.3.3 Control Tokens */
#define SD_DATA_TOKEN_CMD_17_18_24 0b11111110
#define SD_DATA_TOKEN_CMD_25       0b11111100
#define SD_DATA_TOKEN_CMD_25_STOP  0b11111101

#define SD_SIZE_OF_CID_AND_CSD_REG 16
#define SD_GET_CSD_STRUCTURE(CSD_RAW_DATA) ((CSD_RAW_DATA)[0] >> 6)
#define SD_CSD_V1 0
#define SD_CSD_V2 1
#define SD_CSD_VUNSUPPORTED -1

/* the retry counters below are used as timeouts for specific actions.
   The values may need some adjustments to either give the card more time to respond
   to commands or to achieve a lower delay / avoid infinite blocking. */
#define R1_POLLING_RETRY_CNT    10000
#define SD_DATA_TOKEN_RETRY_CNT 10000
#define INIT_CMD_RETRY_CNT      1000
#define INIT_CMD0_RETRY_CNT     3
#define SD_WAIT_FOR_NOT_BUSY_CNT 10000   /* setting this to -1 leads to full blocking until the card isn't busy anymore */
#define SD_BLOCK_READ_CMD_RETRIES 10    /* this only accounts for sending of the command not the whole read transaction! */
#define SD_BLOCK_WRITE_CMD_RETRIES 10   /* this only accounts for sending of the command not the whole write transaction! */

#define SD_HC_FIXED_BLOCK_SIZE 512
#define SD_CSD_V2_C_SIZE_BLOCK_MULT 1024 /* memory capacity in bytes = (C_SIZE+1) * SD_CSD_V2_C_SIZE_BLOCK_MULT * BLOCK_LEN */


#define SD_CARD_DEFAULT_SPI_CONF SPI_CONF_FIRST_RISING
#define SD_CARD_SPI_SPEED_PREINIT SPI_SPEED_100KHZ  /* this speed setting is only used while the init procedure is performed */
#define SD_CARD_SPI_SPEED_POSTINIT SPI_SPEED_10MHZ  /* after init procedure is finished the driver auto sets the card to this speed */

#define SD_CARD_DUMMY_BYTE 0xFF

#ifndef NUM_OF_SD_CARDS
    #define NUM_OF_SD_CARDS 1
#endif

typedef enum {
    SD_V2,
    SD_V1,
    MMC_V3,
    SD_UNKNOWN
} sd_version_t;

typedef enum {
    SD_INIT_START,
    SD_INIT_SPI_POWER_SEQ,
    SD_INIT_SEND_CMD0,
    SD_INIT_SEND_CMD8,
    SD_INIT_CARD_UNKNOWN,
    SD_INIT_SEND_ACMD41_HCS,
    SD_INIT_SEND_ACMD41,
    SD_INIT_SEND_CMD1,
    SD_INIT_SEND_CMD58,
    SD_INIT_SEND_CMD16,
    SD_INIT_ENABLE_CRC,
    SD_INIT_READ_CID,
    SD_INIT_READ_CSD,
    SD_INIT_SET_MAX_SPI_SPEED,
    SD_INIT_FINISH
} sd_init_fsm_state_t;

typedef enum {
    SD_RW_OK = 0,
    SD_RW_NO_TOKEN,
    SD_RW_TIMEOUT,
    SD_RW_RX_TX_ERROR,
    SD_RW_WRITE_ERROR,
    SD_RW_CRC_MISMATCH,
    SD_RW_NOT_SUPPORTED
} sd_rw_response_t;

struct {
    uint8_t MID;            /* Manufacturer ID */
    char OID[2];            /* OEM/Application ID*/
    char PNM[5];            /* Product name */
    uint8_t PRV;            /* Product revision */
    uint32_t PSN;           /* Product serial number */
    uint16_t MDT;           /* Manufacturing date */
    uint8_t CRC;            /* CRC7 checksum */
} typedef cid_t;

/* see sd spec. 5.3.2 CSD Register (CSD Version 1.0) */
struct {
    uint8_t CSD_STRUCTURE : 2;
    uint8_t TAAC : 8;
    uint8_t NSAC : 8;
    uint8_t TRAN_SPEED : 8;
    uint16_t CCC : 12;
    uint8_t READ_BL_LEN : 4;
    uint8_t READ_BL_PARTIAL : 1;
    uint8_t WRITE_BLK_MISALIGN : 1;
    uint8_t READ_BLK_MISALIGN : 1;
    uint8_t DSR_IMP : 1;
    uint16_t C_SIZE : 12;
    uint8_t VDD_R_CURR_MIN : 3;
    uint8_t VDD_R_CURR_MAX : 3;
    uint8_t VDD_W_CURR_MIN : 3;
    uint8_t VDD_W_CURR_MAX : 3;
    uint8_t C_SIZE_MULT : 3;
    uint8_t ERASE_BLK_EN : 1;
    uint8_t SECTOR_SIZE : 7;
    uint8_t WP_GRP_SIZE : 7;
    uint8_t WP_GRP_ENABLE : 1;
    uint8_t R2W_FACTOR : 3;
    uint8_t WRITE_BL_LEN : 4;
    uint8_t WRITE_BL_PARTIAL : 1;
    uint8_t FILE_FORMAT_GRP : 1;
    uint8_t COPY : 1;
    uint8_t PERM_WRITE_PROTECT : 1;
    uint8_t TMP_WRITE_PROTECT : 1;
    uint8_t FILE_FORMAT : 2;
    uint8_t CRC : 8;
} typedef csd_v1_t;

/* see sd spec. 5.3.3 CSD Register (CSD Version 2.0) */
struct {
    uint8_t CSD_STRUCTURE : 2;
    uint8_t TAAC : 8;
    uint8_t NSAC : 8;
    uint8_t TRAN_SPEED : 8;
    uint16_t CCC : 12;
    uint8_t READ_BL_LEN : 4;
    uint8_t READ_BL_PARTIAL : 1;
    uint8_t WRITE_BLK_MISALIGN : 1;
    uint8_t READ_BLK_MISALIGN : 1;
    uint8_t DSR_IMP : 1;
    uint32_t C_SIZE : 22;
    uint8_t ERASE_BLK_EN : 1;
    uint8_t SECTOR_SIZE : 7;
    uint8_t WP_GRP_SIZE : 7;
    uint8_t WP_GRP_ENABLE : 1;
    uint8_t R2W_FACTOR : 3;
    uint8_t WRITE_BL_LEN : 4;
    uint8_t WRITE_BL_PARTIAL : 1;
    uint8_t FILE_FORMAT_GRP : 1;
    uint8_t COPY : 1;
    uint8_t PERM_WRITE_PROTECT : 1;
    uint8_t TMP_WRITE_PROTECT : 1;
    uint8_t FILE_FORMAT : 2;
    uint8_t CRC : 8;
} typedef csd_v2_t;

union {
    csd_v1_t v1;
    csd_v2_t v2;
} typedef csd_t;

struct {
    spi_t spi_dev;
    gpio_t cs_pin;
    bool use_block_addr;
    bool init_done;
    sd_version_t card_type;
    int csd_structure;
    cid_t cid;
    csd_t csd;
} typedef sd_card_t;


/**
 * @brief              Initializes the sd-card with the given parameters in sd_card_t structure.
 *                     The init procedure also takes care of initializing the spi peripheral to master mode and performing all neccecary
 *                     steps to set the sd-card to spi-mode. Reading the CID and CSD registers is also done within this routine and their
 *                     values are copied to the given sd_card_t struct.
 *
 * @param[in] card     struct that contains the pre-set spi_dev (e.g. SPI_1) and cs_pin that are connected to the sd card.
 *                     Initialisation of spi_dev and cs_pin are done within this driver.
 *
 * @return             true if the card could be initialized successfully
 * @return             false if an error occured while initializing the card
 */
bool sdcard_spi_init(sd_card_t *card);

/**
 * @brief                 Sends a cmd to the sd card.
 *
 * @param[in] card        Initialized sd-card struct
 * @param[in] sd_cmd_idx  A supported sd-card command index for SPI-mode like defined in "7.3.1.3 Detailed Command Description" of sd spec.
 *                        (for CMD<X> this parameter is simply the integer value <X>).
 * @param[in] argument    The argument for the given cmd. As described by "7.3.1.1 Command Format". This argument is transmitted byte wise with most significant byte first.
 * @param[in] max_retry   Specifies how often the command should be retried if an error occures. Use 0 to try only once, -1 to try forever, or n to retry n times.
 *
 * @return                R1 response of the command if no (low-level) communication error occured
 * @return                SD_INVALID_R1_RESPONSE if either waiting for the card to enter not-busy-state timed out or spi communication failed
 */
char sdcard_spi_send_cmd(sd_card_t *card, char sd_cmd_idx, uint32_t argument, int max_retry);

/**
 * @brief                 Sends an acmd to the sd card. ACMD<n> consists of sending CMD55 + CMD<n>
 *
 * @param[in] card        Initialized sd-card struct
 * @param[in] sd_cmd_idx  A supported sd-card command index for SPI-mode like defined in "7.3.1.3 Detailed Command Description" of sd spec.
 *                        (for ACMD<X> this parameter is simply the integer value <X>).
 * @param[in] argument    The argument for the given cmd. As described by "7.3.1.1 Command Format". This argument is transmitted byte wise with most significant byte first.
 * @param[in] max_retry   Specifies how often the command should be retried if an error occures. Use 0 to try only once, -1 to try forever, or n to retry n times.
 *
 * @return                R1 response of the command if no (low-level) communication error occured
 * @return                SD_INVALID_R1_RESPONSE if either waiting for the card to enter not-busy-state timed out or spi communication failed
 */
char sdcard_spi_send_acmd(sd_card_t *card, char sd_cmd_idx, uint32_t argument, int max_retry);

/**
 * @brief                 Reads data blocks (usually multiples of 512 Bytes) from the sd card to the given buffer.
 *
 * @param[in] card        Initialized sd-card struct
 * @param[in] blockaddr   Start adress to read from. Independet of the actual adressing scheme of the used card the adress needs to be given as block address (e.g. 0, 1, 2... NOT: 0, 512... ).
 *                        The driver takes care of mapping to byte adressing if needed.
 * @param[out] data       Buffer to store the read data in. The user is responsible for providing a suitable buffer size.
 * @param[in]  blocksize  Size of data blocks. For now only 512 byte blocks are supported because only older (SDSC) cards support variable blocksizes anyway.
 *                        With SDHC/SDXC-cards this is always fixed to 512 bytes. SDSC cards are automatically forced to use 512 byte as blocksize by the init procedure for now.
 * @param[in]  nblocks    Number of blocks to read
 * @param[out] state      Contains information about the error state if something went wrong (if return value is lower than nblocks).
 *
 * @return                number of sucessfully read blocks (0 if no block was read).
 */
int sdcard_spi_read_blocks(sd_card_t *card, int blockaddr, char *data, int blocksize, int nblocks, sd_rw_response_t *state);

/**
 * @brief                 Writes data blocks (usually multiples of 512 Bytes) from the buffer to the card.
 *
 * @param[in] card        Initialized sd-card struct
 * @param[in] blockaddr   Start adress to read from. Independet of the actual adressing scheme of the used card the adress needs to be given as block address (e.g. 0, 1, 2... NOT: 0, 512... ).
 *                        The driver takes care of mapping to byte adressing if needed.
 * @param[out] data       Buffer that contains the data to be sent.
 * @param[in]  blocksize  Size of data blocks. For now only 512 byte blocks are supported because only older (SDSC) cards support variable blocksizes anyway.
 *                        With SDHC/SDXC-cards this is always fixed to 512 bytes. SDSC cards are automatically forced to use 512 byte as blocksize by the init procedure for now.
 * @param[in]  nblocks    Number of blocks to write
 * @param[out] state      Contains information about the error state if something went wrong (if return value is lower than nblocks).
 *
 * @return                number of sucessfully written blocks (0 if no block was written).
 */
int sdcard_spi_write_blocks(sd_card_t *card, int blockaddr, char *data, int blocksize, int nblocks, sd_rw_response_t *state);

/**
 * @brief                 Gets the capacity of the card.
 *
 * @param[in] card        Initialized sd-card struct
 *
 * @return                capacity of the card in bytes
 */
uint64_t sdcard_spi_get_capacity(sd_card_t *card);

/**
 * @brief                 Gets the sector count of the card.
 *
 * @param[in] card        Initialized sd-card struct
 *
 * @return                number of available sectors
 */
uint32_t sdcard_spi_get_sector_count(sd_card_t *card);


#ifdef __cplusplus
}
#endif

#endif /* SDCARD_SPI_H */
/** @} */
