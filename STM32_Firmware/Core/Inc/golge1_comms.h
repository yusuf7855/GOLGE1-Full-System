// GOLGE-1 Haberlesme Alt Sistemi

#ifndef __GOLGE1_COMMS_H
#define __GOLGE1_COMMS_H

#include "main.h"

// AES-256-CBC
void AES256_KeyExpansion(const uint8_t key[CRYPTO_AES_KEY_BYTES],
                         uint8_t round_keys[CRYPTO_AES_EXPANDED_SIZE]);

void AES256_CBC_Encrypt(const uint8_t *plaintext, uint16_t len,
                        const uint8_t key[CRYPTO_AES_KEY_BYTES],
                        const uint8_t iv[CRYPTO_IV_BYTES],
                        uint8_t *ciphertext, uint16_t *out_len);

void AES256_CBC_Decrypt(const uint8_t *ciphertext, uint16_t len,
                        const uint8_t key[CRYPTO_AES_KEY_BYTES],
                        const uint8_t iv[CRYPTO_IV_BYTES],
                        uint8_t *plaintext, uint16_t *out_len);

// CRC-16-CCITT
uint16_t CRC16_CCITT(const uint8_t *data, uint16_t length);
bool CRC16_Verify(const uint8_t *data, uint16_t length, uint16_t expected_crc);

// CCSDS Space Packet
typedef struct __attribute__((packed)) {
    uint16_t packet_id;        // Version(3)|Type(1)|SecHdr(1)|APID(11)
    uint16_t sequence_ctrl;    // SeqFlags(2)|SeqCount(14)
    uint16_t data_length;      // veri uzunlugu - 1
} CCSDS_PrimaryHeader_t;

void CCSDS_CreatePacket(uint16_t apid, uint16_t seq_count,
                        const uint8_t *payload, uint16_t payload_len,
                        uint8_t *output, uint16_t *out_len);

// AX.25 cerceveleme
void AX25_CreateFrame(const char dest_call[6], const char src_call[6],
                      const uint8_t *payload, uint16_t payload_len,
                      uint8_t *frame, uint16_t *frame_len);

bool AX25_ParseFrame(const uint8_t *frame, uint16_t frame_len,
                     uint8_t *payload, uint16_t *payload_len);

// FHSS
void FHSS_Init(uint32_t seed);
uint8_t FHSS_NextChannel(void);
uint8_t FHSS_GetCurrentChannel(void);

// Ust duzey haberlesme API
void COMMS_Init(void);

void COMMS_PrepareSecurePacket(const char *json_input,
                               uint8_t *output, uint16_t *out_len,
                               uint16_t seq_num);

void COMMS_CreateBeacon(float bat_percent, uint8_t state,
                        uint8_t *output, uint16_t *out_len);

int COMMS_TelemetryToJSON(const Golge1_Telemetry_t *tlm,
                          char *buffer, uint16_t buf_size);

int COMMS_IntelToJSON(const IntelData_t *targets, uint8_t count,
                      char *buffer, uint16_t buf_size);

#endif /* __GOLGE1_COMMS_H */
