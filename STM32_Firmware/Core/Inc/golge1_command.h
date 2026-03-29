// GOLGE-1 Komut Alici

#ifndef __GOLGE1_COMMAND_H
#define __GOLGE1_COMMAND_H

#include "main.h"

#define CMD_SYNC_BYTE_1         0xEB
#define CMD_SYNC_BYTE_2         0x90
#define CMD_MAX_DATA_LEN        32
#define CMD_HEADER_SIZE         6       // SYNC(2)+LEN(1)+SEQ(2)+CMD(1)
#define CMD_AUTH_SIZE            4
#define CMD_MIN_PACKET_SIZE     (CMD_HEADER_SIZE + CMD_AUTH_SIZE)
#define CMD_MAX_PACKET_SIZE     (CMD_MIN_PACKET_SIZE + CMD_MAX_DATA_LEN)

#define CMD_RX_BUFFER_SIZE      128
#define CMD_RX_TIMEOUT_MS       5000

#define CMD_REPLAY_WINDOW       16

typedef enum {
    /* Sistem */
    CMD_PING            = 0x00,
    CMD_GET_TELEMETRY   = 0x01,
    CMD_GET_STATUS      = 0x02,
    CMD_SET_TIME        = 0x03,

    /* Durum */
    CMD_FORCE_STATE     = 0x10,
    CMD_ENTER_SAFE      = 0x11,
    CMD_EXIT_SAFE       = 0x12,
    CMD_REBOOT_OBC      = 0x13,

    /* Gorev */
    CMD_START_OPTICAL   = 0x20,
    CMD_START_RF        = 0x21,
    CMD_STOP_SCAN       = 0x22,
    CMD_DOWNLINK_STORED = 0x23,
    CMD_CLEAR_TARGETS   = 0x24,

    /* Haberlesme */
    CMD_SET_TX_POWER    = 0x30,
    CMD_SET_FREQ_TABLE  = 0x31,
    CMD_SET_BEACON_RATE = 0x32,

    /* Guvenlik */
    CMD_UPDATE_AES_KEY  = 0x40,
    CMD_UPDATE_HMAC_KEY = 0x41,
    CMD_RESET_SEQ       = 0x42,

    /* ADCS */
    CMD_SET_POINTING    = 0x50,
    CMD_SET_TARGET_COORD= 0x51,

    /* Debug */
    CMD_DUMP_MEMORY     = 0xF0,
    CMD_INJECT_ERROR    = 0xF1,
} CmdCode_Extended_t;

typedef enum {
    CMD_RESULT_OK           = 0x00,
    CMD_RESULT_INVALID_SYNC = 0x01,
    CMD_RESULT_BAD_LENGTH   = 0x02,
    CMD_RESULT_AUTH_FAIL    = 0x03,
    CMD_RESULT_REPLAY       = 0x04,
    CMD_RESULT_UNKNOWN_CMD  = 0x05,
    CMD_RESULT_PARAM_ERROR  = 0x06,
    CMD_RESULT_BUSY         = 0x07,
    CMD_RESULT_DENIED       = 0x08,
} CmdResult_t;

typedef struct {
    uint16_t sequence;
    uint8_t  cmd_id;
    uint8_t  data[CMD_MAX_DATA_LEN];
    uint8_t  data_length;
    uint8_t  auth_tag[CMD_AUTH_SIZE];
    bool     is_valid;
} ParsedCommand_t;

typedef struct {
    uint8_t  sync[2];
    uint16_t seq_echo;
    uint8_t  cmd_echo;
    uint8_t  result;
    uint8_t  data[16];
    uint8_t  data_length;
} CmdResponse_t;

typedef enum {
    RX_WAIT_SYNC1 = 0,
    RX_WAIT_SYNC2,
    RX_WAIT_LENGTH,
    RX_RECEIVE_DATA,
    RX_COMPLETE
} RxState_t;

void CMD_Init(void);

CmdResult_t CMD_Parse(const uint8_t *raw_data, uint16_t raw_len,
                      ParsedCommand_t *parsed);

bool CMD_VerifyHMAC(const uint8_t *data, uint16_t data_len,
                    const uint8_t received_tag[CMD_AUTH_SIZE]);

void CMD_ComputeHMAC(const uint8_t *data, uint16_t data_len,
                     uint8_t tag[CMD_AUTH_SIZE]);

bool CMD_CheckReplay(uint16_t seq);

CmdResult_t CMD_Execute(const ParsedCommand_t *cmd, CmdResponse_t *response);

void CMD_BuildResponse(const CmdResponse_t *response,
                       uint8_t *output, uint16_t *out_len);

void CMD_ProcessRxByte(uint8_t byte);

bool CMD_IsPacketReady(void);

void CMD_GetReceivedPacket(uint8_t *buffer, uint16_t *length);

#endif /* __GOLGE1_COMMAND_H */
