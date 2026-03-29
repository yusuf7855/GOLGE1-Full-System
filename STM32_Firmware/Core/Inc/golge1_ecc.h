// GOLGE-1 ECC - Hamming(7,4) SEC-DED

#ifndef __GOLGE1_ECC_H
#define __GOLGE1_ECC_H

#include "main.h"

/* Hamming(7,4) parametreleri */
#define ECC_DATA_BITS           4
#define ECC_PARITY_BITS         3
#define ECC_TOTAL_BITS          8   // 7 + 1 genel parite
#define ECC_EXPANSION_RATIO     2   // 1 byte -> 2 byte

/* Hata turleri */
#define ECC_NO_ERROR            0
#define ECC_SINGLE_ERROR        1   // duzeltilir
#define ECC_DOUBLE_ERROR        2   // sadece tespit
#define ECC_MULTI_ERROR         3

/* Hamming encode/decode */
uint8_t ECC_HammingEncode(uint8_t nibble);
uint8_t ECC_HammingDecode(uint8_t code, uint8_t *decoded);

/* Buffer encode/decode */
void ECC_EncodeBuffer(const uint8_t *input, uint16_t in_len,
                      uint8_t *output, uint16_t *out_len);

bool ECC_DecodeBuffer(const uint8_t *input, uint16_t in_len,
                      uint8_t *output, uint16_t *out_len,
                      uint16_t *corrections, uint16_t *uncorrectable);

/* Bellek koruma (EDAC) */
void ECC_ProtectMemory(const uint8_t *data, uint16_t data_len,
                       uint8_t *shadow);

bool ECC_VerifyAndRepair(const uint8_t *shadow, uint16_t shadow_len,
                         uint8_t *data, uint16_t data_len,
                         uint16_t *errors_fixed);

/* FEC - iletisim hata duzeltme */
void ECC_FEC_Encode(const uint8_t *input, uint16_t in_len,
                    uint8_t *output, uint16_t *out_len);

bool ECC_FEC_Decode(const uint8_t *input, uint16_t in_len,
                    uint8_t *output, uint16_t *out_len,
                    uint16_t *errors_fixed);

#endif /* __GOLGE1_ECC_H */
