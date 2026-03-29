// GOLGE-1 ECC - Hamming(7,4) SEC-DED

#include "golge1_ecc.h"
#include <string.h>

// nibble -> 8-bit hamming lookup
static const uint8_t hamming_encode_table[16] = {
    0x00, 0x71, 0x62, 0x13, 0x54, 0x25, 0x36, 0x47,
    0x38, 0x49, 0x5A, 0x2B, 0x6C, 0x1D, 0x0E, 0x7F
};

// sendrom -> hatali bit pozisyonu
static const int8_t syndrome_to_bit[8] = {
    -1, 6, 5, 4, 3, 2, 1, 0
};

uint8_t ECC_HammingEncode(uint8_t nibble)
{
    return hamming_encode_table[nibble & 0x0F];
}

uint8_t ECC_HammingDecode(uint8_t code, uint8_t *decoded)
{
    uint8_t p1 = (code >> 6) & 1;
    uint8_t p2 = (code >> 5) & 1;
    uint8_t d1 = (code >> 4) & 1;
    uint8_t p3 = (code >> 3) & 1;
    uint8_t d2 = (code >> 2) & 1;
    uint8_t d3 = (code >> 1) & 1;
    uint8_t d4 = (code >> 0) & 1;

    // sendrom hesapla
    uint8_t s1 = p1 ^ d1 ^ d2 ^ d4;
    uint8_t s2 = p2 ^ d1 ^ d3 ^ d4;
    uint8_t s3 = p3 ^ d2 ^ d3 ^ d4;
    uint8_t syndrome = (s1 << 2) | (s2 << 1) | s3;

    // genel parite
    uint8_t p0_check = (code >> 7) & 1;
    uint8_t parity = p0_check ^ p1 ^ p2 ^ d1 ^ p3 ^ d2 ^ d3 ^ d4;

    uint8_t error_type = ECC_NO_ERROR;

    if (syndrome == 0 && parity == 0) {
        error_type = ECC_NO_ERROR;
    }
    else if (syndrome == 0 && parity == 1) {
        error_type = ECC_SINGLE_ERROR; // sadece p0 hatasi
    }
    else if (syndrome != 0 && parity == 1) {
        // tek bit hatasi, duzelt
        int8_t bit_pos = syndrome_to_bit[syndrome];
        if (bit_pos >= 0) {
            code ^= (1 << bit_pos);
        }
        error_type = ECC_SINGLE_ERROR;
    }
    else {
        error_type = ECC_DOUBLE_ERROR; // cift bit, duzeltilmez
    }

    // veri bitlerini cikar
    *decoded = ((code >> 4) & 1) << 3 |
               ((code >> 2) & 1) << 2 |
               ((code >> 1) & 1) << 1 |
               ((code >> 0) & 1);

    return error_type;
}

/* --- Buffer ECC --- */

void ECC_EncodeBuffer(const uint8_t *input, uint16_t in_len,
                      uint8_t *output, uint16_t *out_len)
{
    uint16_t out_idx = 0;

    for (uint16_t i = 0; i < in_len; i++) {
        uint8_t hi_nibble = (input[i] >> 4) & 0x0F;
        uint8_t lo_nibble = input[i] & 0x0F;

        output[out_idx++] = ECC_HammingEncode(hi_nibble);
        output[out_idx++] = ECC_HammingEncode(lo_nibble);
    }

    *out_len = out_idx;
}

bool ECC_DecodeBuffer(const uint8_t *input, uint16_t in_len,
                      uint8_t *output, uint16_t *out_len,
                      uint16_t *corrections, uint16_t *uncorrectable)
{
    *corrections = 0;
    *uncorrectable = 0;
    uint16_t out_idx = 0;
    bool all_ok = true;

    for (uint16_t i = 0; i + 1 < in_len; i += 2) {
        uint8_t hi_decoded, lo_decoded;
        uint8_t err_hi = ECC_HammingDecode(input[i], &hi_decoded);
        uint8_t err_lo = ECC_HammingDecode(input[i + 1], &lo_decoded);

        if (err_hi == ECC_SINGLE_ERROR || err_lo == ECC_SINGLE_ERROR)
            (*corrections)++;

        if (err_hi == ECC_DOUBLE_ERROR || err_lo == ECC_DOUBLE_ERROR) {
            (*uncorrectable)++;
            all_ok = false;
        }

        output[out_idx++] = (hi_decoded << 4) | lo_decoded;
    }

    *out_len = out_idx;
    return all_ok;
}

/* --- EDAC --- */

void ECC_ProtectMemory(const uint8_t *data, uint16_t data_len,
                       uint8_t *shadow)
{
    uint16_t shadow_len;
    ECC_EncodeBuffer(data, data_len, shadow, &shadow_len);
}

bool ECC_VerifyAndRepair(const uint8_t *shadow, uint16_t shadow_len,
                         uint8_t *data, uint16_t data_len,
                         uint16_t *errors_fixed)
{
    uint16_t out_len;
    uint16_t corrections = 0;
    uint16_t uncorrectable = 0;

    bool ok = ECC_DecodeBuffer(shadow, shadow_len,
                                data, &out_len,
                                &corrections, &uncorrectable);

    *errors_fixed = corrections;
    return ok;
}

/* --- FEC + Interleaving --- */

#define FEC_INTERLEAVE_DEPTH  4

void ECC_FEC_Encode(const uint8_t *input, uint16_t in_len,
                    uint8_t *output, uint16_t *out_len)
{
    uint8_t hamming_buf[COMMS_MAX_PACKET_SIZE * 2];
    uint16_t hamming_len;
    ECC_EncodeBuffer(input, in_len, hamming_buf, &hamming_len);

    // interleave
    uint16_t row_size = (hamming_len + FEC_INTERLEAVE_DEPTH - 1) / FEC_INTERLEAVE_DEPTH;
    uint16_t idx = 0;

    for (uint16_t col = 0; col < row_size; col++) {
        for (uint8_t row = 0; row < FEC_INTERLEAVE_DEPTH; row++) {
            uint16_t src_idx = row * row_size + col;
            if (src_idx < hamming_len) {
                output[idx++] = hamming_buf[src_idx];
            } else {
                output[idx++] = 0x00;
            }
        }
    }

    *out_len = idx;
}

bool ECC_FEC_Decode(const uint8_t *input, uint16_t in_len,
                    uint8_t *output, uint16_t *out_len,
                    uint16_t *errors_fixed)
{
    // de-interleave
    uint16_t row_size = (in_len + FEC_INTERLEAVE_DEPTH - 1) / FEC_INTERLEAVE_DEPTH;
    uint8_t deinterleaved[COMMS_MAX_PACKET_SIZE * 2];
    uint16_t hamming_len = 0;

    for (uint8_t row = 0; row < FEC_INTERLEAVE_DEPTH; row++) {
        for (uint16_t col = 0; col < row_size; col++) {
            uint16_t src_idx = col * FEC_INTERLEAVE_DEPTH + row;
            if (src_idx < in_len) {
                deinterleaved[hamming_len++] = input[src_idx];
            }
        }
    }

    uint16_t uncorrectable = 0;
    bool ok = ECC_DecodeBuffer(deinterleaved, hamming_len,
                                output, out_len,
                                errors_fixed, &uncorrectable);

    return ok;
}
