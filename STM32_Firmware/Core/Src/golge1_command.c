// GOLGE-1 Komut Isleyici

#include "golge1_command.h"
#include "golge1_comms.h"
#include "golge1_fdir.h"
#include "golge1_eps.h"
#include <string.h>

// HMAC anahtari (gercekte flash'tan okunur)
static const uint8_t hmac_key[32] = {
    0x48, 0x4D, 0x41, 0x43, 0x5F, 0x47, 0x4F, 0x4C,
    0x47, 0x45, 0x31, 0x5F, 0x53, 0x45, 0x43, 0x52,
    0x45, 0x54, 0x5F, 0x4B, 0x45, 0x59, 0x5F, 0x32,
    0x30, 0x32, 0x36, 0x5F, 0x54, 0x55, 0x41, 0x21
};

// Anti-replay state
static uint16_t replay_window[CMD_REPLAY_WINDOW] = {0};
static uint8_t  replay_head = 0;
static uint16_t highest_seq = 0;

// UART RX state machine
static RxState_t rx_state = RX_WAIT_SYNC1;
static uint8_t   rx_buffer[CMD_RX_BUFFER_SIZE];
static uint16_t  rx_index = 0;
static uint8_t   rx_expected_len = 0;
static volatile bool rx_packet_ready = false;

extern Golge1_Telemetry_t  satellite;
extern TMR_CriticalData_t  tmr_data;

void CMD_Init(void)
{
    rx_state = RX_WAIT_SYNC1;
    rx_index = 0;
    rx_packet_ready = false;
    highest_seq = 0;
    memset(replay_window, 0, sizeof(replay_window));
}

void CMD_ProcessRxByte(uint8_t byte)
{
    switch (rx_state) {
        case RX_WAIT_SYNC1:
            if (byte == CMD_SYNC_BYTE_1) {
                rx_buffer[0] = byte;
                rx_index = 1;
                rx_state = RX_WAIT_SYNC2;
            }
            break;

        case RX_WAIT_SYNC2:
            if (byte == CMD_SYNC_BYTE_2) {
                rx_buffer[1] = byte;
                rx_index = 2;
                rx_state = RX_WAIT_LENGTH;
            } else {
                rx_state = RX_WAIT_SYNC1;
            }
            break;

        case RX_WAIT_LENGTH:
            rx_buffer[2] = byte;
            rx_index = 3;
            rx_expected_len = CMD_HEADER_SIZE + byte + CMD_AUTH_SIZE;
            if (rx_expected_len > CMD_MAX_PACKET_SIZE || rx_expected_len < CMD_MIN_PACKET_SIZE) {
                rx_state = RX_WAIT_SYNC1;
            } else {
                rx_state = RX_RECEIVE_DATA;
            }
            break;

        case RX_RECEIVE_DATA:
            if (rx_index < CMD_RX_BUFFER_SIZE) {
                rx_buffer[rx_index++] = byte;
            }
            if (rx_index >= rx_expected_len) {
                rx_state = RX_COMPLETE;
                rx_packet_ready = true;
            }
            break;

        case RX_COMPLETE:
            break;
    }
}

bool CMD_IsPacketReady(void)
{
    return rx_packet_ready;
}

void CMD_GetReceivedPacket(uint8_t *buffer, uint16_t *length)
{
    memcpy(buffer, rx_buffer, rx_index);
    *length = rx_index;

    rx_state = RX_WAIT_SYNC1;
    rx_index = 0;
    rx_packet_ready = false;
}

// Hafif MAC (SipHash benzeri ARX)
void CMD_ComputeHMAC(const uint8_t *data, uint16_t data_len,
                     uint8_t tag[CMD_AUTH_SIZE])
{
    uint32_t h = 0x5A5A5A5A;

    for (int i = 0; i < 32; i++) {
        h ^= hmac_key[i];
        h = (h << 7) | (h >> 25);
        h += 0x9E3779B9;           // golden ratio
    }

    for (uint16_t i = 0; i < data_len; i++) {
        h ^= data[i];
        h = (h << 13) | (h >> 19);
        h *= 0x5BD1E995;           // murmur sabiti
        h ^= (h >> 15);
    }

    // avalanche
    h ^= (h >> 16);
    h *= 0x85EBCA6B;
    h ^= (h >> 13);
    h *= 0xC2B2AE35;
    h ^= (h >> 16);

    tag[0] = (uint8_t)(h >> 24);
    tag[1] = (uint8_t)(h >> 16);
    tag[2] = (uint8_t)(h >> 8);
    tag[3] = (uint8_t)(h);
}

bool CMD_VerifyHMAC(const uint8_t *data, uint16_t data_len,
                    const uint8_t received_tag[CMD_AUTH_SIZE])
{
    uint8_t computed_tag[CMD_AUTH_SIZE];
    CMD_ComputeHMAC(data, data_len, computed_tag);

    // sabit zamanli karsilastirma
    uint8_t diff = 0;
    for (int i = 0; i < CMD_AUTH_SIZE; i++) {
        diff |= computed_tag[i] ^ received_tag[i];
    }
    return (diff == 0);
}

bool CMD_CheckReplay(uint16_t seq)
{
    if (seq <= highest_seq && highest_seq - seq < CMD_REPLAY_WINDOW) {
        for (uint8_t i = 0; i < CMD_REPLAY_WINDOW; i++) {
            if (replay_window[i] == seq)
                return false; // replay tespit
        }
    }

    if (seq < highest_seq && (highest_seq - seq) >= CMD_REPLAY_WINDOW) {
        if (highest_seq > 0xFF00 && seq < 0x0100) {
            // 16-bit wrap: kabul
        } else {
            return false;
        }
    }

    replay_window[replay_head] = seq;
    replay_head = (replay_head + 1) % CMD_REPLAY_WINDOW;
    if (seq > highest_seq)
        highest_seq = seq;

    return true;
}

CmdResult_t CMD_Parse(const uint8_t *raw_data, uint16_t raw_len,
                      ParsedCommand_t *parsed)
{
    memset(parsed, 0, sizeof(ParsedCommand_t));

    if (raw_len < CMD_MIN_PACKET_SIZE)
        return CMD_RESULT_BAD_LENGTH;

    if (raw_data[0] != CMD_SYNC_BYTE_1 || raw_data[1] != CMD_SYNC_BYTE_2)
        return CMD_RESULT_INVALID_SYNC;

    uint8_t payload_len = raw_data[2];
    uint16_t expected_total = CMD_HEADER_SIZE + payload_len + CMD_AUTH_SIZE;
    if (raw_len < expected_total)
        return CMD_RESULT_BAD_LENGTH;

    parsed->sequence = ((uint16_t)raw_data[3] << 8) | raw_data[4];
    parsed->cmd_id = raw_data[5];
    parsed->data_length = payload_len;
    if (payload_len > 0) {
        memcpy(parsed->data, &raw_data[CMD_HEADER_SIZE], payload_len);
    }

    uint16_t auth_offset = CMD_HEADER_SIZE + payload_len;
    memcpy(parsed->auth_tag, &raw_data[auth_offset], CMD_AUTH_SIZE);

    // HMAC dogrula (SYNC haric)
    if (!CMD_VerifyHMAC(&raw_data[2], CMD_HEADER_SIZE - 2 + payload_len,
                        parsed->auth_tag)) {
        return CMD_RESULT_AUTH_FAIL;
    }

    if (!CMD_CheckReplay(parsed->sequence)) {
        return CMD_RESULT_REPLAY;
    }

    parsed->is_valid = true;
    return CMD_RESULT_OK;
}

CmdResult_t CMD_Execute(const ParsedCommand_t *cmd, CmdResponse_t *response)
{
    response->sync[0] = CMD_SYNC_BYTE_1;
    response->sync[1] = CMD_SYNC_BYTE_2;
    response->seq_echo = cmd->sequence;
    response->cmd_echo = cmd->cmd_id;
    response->data_length = 0;

    Golge1_State_t current_state = TMR_ReadState(&tmr_data);

    switch (cmd->cmd_id) {

        case CMD_PING:
            response->result = CMD_RESULT_OK;
            response->data[0] = (uint8_t)current_state;
            response->data[1] = (uint8_t)(satellite.eps.bat_percent);
            response->data_length = 2;
            break;

        case CMD_GET_STATUS:
            response->result = CMD_RESULT_OK;
            response->data[0] = (uint8_t)current_state;
            response->data[1] = (uint8_t)(satellite.eps.bat_percent);
            response->data[2] = satellite.thermal.mcu;
            response->data[3] = satellite.thermal.ai;
            response->data[4] = satellite.target_count;
            response->data[5] = (uint8_t)(satellite.stats.packets_sent & 0xFF);
            response->data_length = 6;
            break;

        case CMD_SET_TIME:
            if (cmd->data_length >= 4) {
                uint32_t new_time = ((uint32_t)cmd->data[0] << 24) |
                                   ((uint32_t)cmd->data[1] << 16) |
                                   ((uint32_t)cmd->data[2] << 8) |
                                    (uint32_t)cmd->data[3];
                TMR_WriteTimestamp(&tmr_data, new_time);
                satellite.stats.last_ground_contact = satellite.stats.uptime_sec;
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_PARAM_ERROR;
            }
            break;

        case CMD_FORCE_STATE:
            if (cmd->data_length >= 1) {
                Golge1_State_t new_state = (Golge1_State_t)cmd->data[0];
                TMR_WriteState(&tmr_data, new_state);
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_PARAM_ERROR;
            }
            break;

        case CMD_ENTER_SAFE:
            TMR_WriteState(&tmr_data, STATE_SAFE_MODE);
            EPS_EmergencyShutdown();
            response->result = CMD_RESULT_OK;
            break;

        case CMD_EXIT_SAFE:
            if (current_state == STATE_SAFE_MODE) {
                TMR_WriteState(&tmr_data, STATE_IDLE_CHARGE);
                satellite.stats.reboot_count = 0;
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_DENIED;
            }
            break;

        case CMD_REBOOT_OBC:
            response->result = CMD_RESULT_OK;
            break;

        case CMD_START_OPTICAL:
            if (current_state == STATE_IDLE_CHARGE &&
                satellite.eps.bat_percent >= EPS_BAT_NOMINAL) {
                TMR_WriteState(&tmr_data, STATE_OPTICAL_SCAN);
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_DENIED;
            }
            break;

        case CMD_START_RF:
            if (current_state == STATE_IDLE_CHARGE &&
                satellite.eps.bat_percent >= EPS_BAT_NOMINAL) {
                TMR_WriteState(&tmr_data, STATE_RF_SCAN);
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_DENIED;
            }
            break;

        case CMD_STOP_SCAN:
            if (current_state == STATE_OPTICAL_SCAN ||
                current_state == STATE_RF_SCAN) {
                TMR_WriteState(&tmr_data, STATE_IDLE_CHARGE);
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_DENIED;
            }
            break;

        case CMD_CLEAR_TARGETS:
            satellite.target_count = 0;
            response->result = CMD_RESULT_OK;
            break;

        case CMD_DOWNLINK_STORED:
            if (current_state == STATE_IDLE_CHARGE ||
                current_state == STATE_COMMS_RELAY) {
                TMR_WriteState(&tmr_data, STATE_COMMS_RELAY);
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_DENIED;
            }
            break;

        case CMD_SET_FREQ_TABLE:
            if (cmd->data_length >= 4) {
                uint32_t seed = ((uint32_t)cmd->data[0] << 24) |
                               ((uint32_t)cmd->data[1] << 16) |
                               ((uint32_t)cmd->data[2] << 8) |
                                (uint32_t)cmd->data[3];
                FHSS_Init(seed);
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_PARAM_ERROR;
            }
            break;

        case CMD_SET_POINTING:
            if (cmd->data_length >= 1) {
                satellite.adcs.pointing_mode = cmd->data[0];
                response->result = CMD_RESULT_OK;
            } else {
                response->result = CMD_RESULT_PARAM_ERROR;
            }
            break;

        default:
            response->result = CMD_RESULT_UNKNOWN_CMD;
            break;
    }

    satellite.stats.last_ground_contact = satellite.stats.uptime_sec;

    return response->result;
}

void CMD_BuildResponse(const CmdResponse_t *response,
                       uint8_t *output, uint16_t *out_len)
{
    uint16_t idx = 0;

    output[idx++] = CMD_SYNC_BYTE_1;
    output[idx++] = CMD_SYNC_BYTE_2;
    output[idx++] = (uint8_t)(response->seq_echo >> 8);
    output[idx++] = (uint8_t)(response->seq_echo & 0xFF);
    output[idx++] = response->cmd_echo;
    output[idx++] = response->result;

    if (response->data_length > 0) {
        memcpy(&output[idx], response->data, response->data_length);
        idx += response->data_length;
    }

    // HMAC (SYNC haric)
    uint8_t tag[CMD_AUTH_SIZE];
    CMD_ComputeHMAC(&output[2], idx - 2, tag);
    memcpy(&output[idx], tag, CMD_AUTH_SIZE);
    idx += CMD_AUTH_SIZE;

    *out_len = idx;
}
