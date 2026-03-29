// GOLGE-1 Haberlesme Alt Sistemi

#include "golge1_comms.h"
#include <string.h>
#include <stdio.h>

// AES S-Box
static const uint8_t aes_sbox[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5,
    0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC,
    0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B,
    0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85,
    0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17,
    0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88,
    0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9,
    0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6,
    0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94,
    0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68,
    0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

// AES ters S-Box
static const uint8_t aes_inv_sbox[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38,
    0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,
    0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D,
    0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2,
    0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA,
    0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A,
    0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,
    0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA,
    0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85,
    0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20,
    0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31,
    0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,
    0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0,
    0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26,
    0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

// Rcon tablosu
static const uint8_t rcon[15] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
    0x80, 0x1B, 0x36, 0x6C, 0xD8, 0xAB, 0x4D
};

// Sifreleme anahtari (256-bit)
static const uint8_t master_key[CRYPTO_AES_KEY_BYTES] = {
    0x47, 0x4F, 0x4C, 0x47, 0x45, 0x31, 0x5F, 0x54,
    0x55, 0x41, 0x5F, 0x41, 0x53, 0x54, 0x52, 0x4F,
    0x5F, 0x48, 0x41, 0x43, 0x4B, 0x41, 0x54, 0x48,
    0x4F, 0x4E, 0x5F, 0x32, 0x30, 0x32, 0x36, 0x21
};

// CBC baslangic vektoru
static const uint8_t default_iv[CRYPTO_IV_BYTES] = {
    0x61, 0x54, 0x55, 0x41, 0x32, 0x30, 0x32, 0x36,
    0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE
};

// GF(2^8) xtime
static uint8_t gf_mul2(uint8_t a)
{
    return (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1B : 0x00));
}

static uint8_t gf_mul3(uint8_t a)
{
    return gf_mul2(a) ^ a;
}

// GF(2^8) genel carpma
static uint8_t gf_mul(uint8_t a, uint8_t b)
{
    uint8_t result = 0;
    uint8_t hi_bit;
    for (int i = 0; i < 8; i++) {
        if (b & 1)
            result ^= a;
        hi_bit = a & 0x80;
        a <<= 1;
        if (hi_bit)
            a ^= 0x1B;
        b >>= 1;
    }
    return result;
}

// AES-256 anahtar genisletme
void AES256_KeyExpansion(const uint8_t key[CRYPTO_AES_KEY_BYTES],
                         uint8_t round_keys[CRYPTO_AES_EXPANDED_SIZE])
{
    uint8_t temp[4];
    int i;

    memcpy(round_keys, key, CRYPTO_AES_KEY_BYTES);

    for (i = 8; i < 60; i++) {
        temp[0] = round_keys[(i - 1) * 4 + 0];
        temp[1] = round_keys[(i - 1) * 4 + 1];
        temp[2] = round_keys[(i - 1) * 4 + 2];
        temp[3] = round_keys[(i - 1) * 4 + 3];

        if (i % 8 == 0) {
            // RotWord
            uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;

            // SubWord
            temp[0] = aes_sbox[temp[0]];
            temp[1] = aes_sbox[temp[1]];
            temp[2] = aes_sbox[temp[2]];
            temp[3] = aes_sbox[temp[3]];

            temp[0] ^= rcon[i / 8];
        }
        else if (i % 8 == 4) {
            // AES-256 ek SubWord
            temp[0] = aes_sbox[temp[0]];
            temp[1] = aes_sbox[temp[1]];
            temp[2] = aes_sbox[temp[2]];
            temp[3] = aes_sbox[temp[3]];
        }

        round_keys[i * 4 + 0] = round_keys[(i - 8) * 4 + 0] ^ temp[0];
        round_keys[i * 4 + 1] = round_keys[(i - 8) * 4 + 1] ^ temp[1];
        round_keys[i * 4 + 2] = round_keys[(i - 8) * 4 + 2] ^ temp[2];
        round_keys[i * 4 + 3] = round_keys[(i - 8) * 4 + 3] ^ temp[3];
    }
}

static void aes_sub_bytes(uint8_t state[16])
{
    for (int i = 0; i < 16; i++)
        state[i] = aes_sbox[state[i]];
}

static void aes_inv_sub_bytes(uint8_t state[16])
{
    for (int i = 0; i < 16; i++)
        state[i] = aes_inv_sbox[state[i]];
}

static void aes_shift_rows(uint8_t state[16])
{
    uint8_t t;

    // Satir 1: 1 sola
    t = state[1];
    state[1]  = state[5];
    state[5]  = state[9];
    state[9]  = state[13];
    state[13] = t;

    // Satir 2: 2 sola
    t = state[2];
    state[2]  = state[10];
    state[10] = t;
    t = state[6];
    state[6]  = state[14];
    state[14] = t;

    // Satir 3: 3 sola
    t = state[13 + 2];
    t = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7]  = state[3];
    state[3]  = t;
}

static void aes_inv_shift_rows(uint8_t state[16])
{
    uint8_t t;

    t = state[13];
    state[13] = state[9];
    state[9]  = state[5];
    state[5]  = state[1];
    state[1]  = t;

    t = state[2];
    state[2]  = state[10];
    state[10] = t;
    t = state[6];
    state[6]  = state[14];
    state[14] = t;

    t = state[3];
    state[3]  = state[7];
    state[7]  = state[11];
    state[11] = state[15];
    state[15] = t;
}

static void aes_mix_columns(uint8_t state[16])
{
    for (int c = 0; c < 4; c++) {
        int i = c * 4;
        uint8_t s0 = state[i + 0];
        uint8_t s1 = state[i + 1];
        uint8_t s2 = state[i + 2];
        uint8_t s3 = state[i + 3];

        state[i + 0] = gf_mul2(s0) ^ gf_mul3(s1) ^ s2        ^ s3;
        state[i + 1] = s0        ^ gf_mul2(s1) ^ gf_mul3(s2) ^ s3;
        state[i + 2] = s0        ^ s1        ^ gf_mul2(s2) ^ gf_mul3(s3);
        state[i + 3] = gf_mul3(s0) ^ s1        ^ s2        ^ gf_mul2(s3);
    }
}

static void aes_inv_mix_columns(uint8_t state[16])
{
    for (int c = 0; c < 4; c++) {
        int i = c * 4;
        uint8_t s0 = state[i + 0];
        uint8_t s1 = state[i + 1];
        uint8_t s2 = state[i + 2];
        uint8_t s3 = state[i + 3];

        state[i + 0] = gf_mul(s0, 0x0E) ^ gf_mul(s1, 0x0B) ^ gf_mul(s2, 0x0D) ^ gf_mul(s3, 0x09);
        state[i + 1] = gf_mul(s0, 0x09) ^ gf_mul(s1, 0x0E) ^ gf_mul(s2, 0x0B) ^ gf_mul(s3, 0x0D);
        state[i + 2] = gf_mul(s0, 0x0D) ^ gf_mul(s1, 0x09) ^ gf_mul(s2, 0x0E) ^ gf_mul(s3, 0x0B);
        state[i + 3] = gf_mul(s0, 0x0B) ^ gf_mul(s1, 0x0D) ^ gf_mul(s2, 0x09) ^ gf_mul(s3, 0x0E);
    }
}

static void aes_add_round_key(uint8_t state[16], const uint8_t *round_key)
{
    for (int i = 0; i < 16; i++)
        state[i] ^= round_key[i];
}

// Tek blok AES-256 sifreleme
static void aes256_encrypt_block(const uint8_t input[16],
                                  uint8_t output[16],
                                  const uint8_t round_keys[CRYPTO_AES_EXPANDED_SIZE])
{
    uint8_t state[16];
    memcpy(state, input, 16);

    aes_add_round_key(state, &round_keys[0]);

    for (int round = 1; round < CRYPTO_AES_ROUNDS; round++) {
        aes_sub_bytes(state);
        aes_shift_rows(state);
        aes_mix_columns(state);
        aes_add_round_key(state, &round_keys[round * 16]);
    }

    // Son round, MixColumns yok
    aes_sub_bytes(state);
    aes_shift_rows(state);
    aes_add_round_key(state, &round_keys[CRYPTO_AES_ROUNDS * 16]);

    memcpy(output, state, 16);
}

// Tek blok AES-256 cozme
static void aes256_decrypt_block(const uint8_t input[16],
                                  uint8_t output[16],
                                  const uint8_t round_keys[CRYPTO_AES_EXPANDED_SIZE])
{
    uint8_t state[16];
    memcpy(state, input, 16);

    aes_add_round_key(state, &round_keys[CRYPTO_AES_ROUNDS * 16]);

    for (int round = CRYPTO_AES_ROUNDS - 1; round >= 1; round--) {
        aes_inv_shift_rows(state);
        aes_inv_sub_bytes(state);
        aes_add_round_key(state, &round_keys[round * 16]);
        aes_inv_mix_columns(state);
    }

    aes_inv_shift_rows(state);
    aes_inv_sub_bytes(state);
    aes_add_round_key(state, &round_keys[0]);

    memcpy(output, state, 16);
}

// AES-256-CBC sifreleme (PKCS#7 padding)
void AES256_CBC_Encrypt(const uint8_t *plaintext, uint16_t len,
                        const uint8_t key[CRYPTO_AES_KEY_BYTES],
                        const uint8_t iv[CRYPTO_IV_BYTES],
                        uint8_t *ciphertext, uint16_t *out_len)
{
    uint8_t round_keys[CRYPTO_AES_EXPANDED_SIZE];
    uint8_t block[16];
    uint8_t prev_cipher[16];

    AES256_KeyExpansion(key, round_keys);

    uint8_t pad_value = 16 - (len % 16);
    uint16_t padded_len = len + pad_value;
    *out_len = padded_len;

    memcpy(prev_cipher, iv, 16);

    uint16_t num_blocks = padded_len / 16;
    for (uint16_t b = 0; b < num_blocks; b++) {
        for (int i = 0; i < 16; i++) {
            uint16_t idx = b * 16 + i;
            if (idx < len) {
                block[i] = plaintext[idx];
            } else {
                block[i] = pad_value; // PKCS#7
            }
        }

        for (int i = 0; i < 16; i++)
            block[i] ^= prev_cipher[i];

        aes256_encrypt_block(block, &ciphertext[b * 16], round_keys);
        memcpy(prev_cipher, &ciphertext[b * 16], 16);
    }
}

// AES-256-CBC cozme
void AES256_CBC_Decrypt(const uint8_t *ciphertext, uint16_t len,
                        const uint8_t key[CRYPTO_AES_KEY_BYTES],
                        const uint8_t iv[CRYPTO_IV_BYTES],
                        uint8_t *plaintext, uint16_t *out_len)
{
    uint8_t round_keys[CRYPTO_AES_EXPANDED_SIZE];
    uint8_t decrypted[16];
    uint8_t prev_cipher[16];

    AES256_KeyExpansion(key, round_keys);
    memcpy(prev_cipher, iv, 16);

    uint16_t num_blocks = len / 16;
    for (uint16_t b = 0; b < num_blocks; b++) {
        aes256_decrypt_block(&ciphertext[b * 16], decrypted, round_keys);

        for (int i = 0; i < 16; i++)
            plaintext[b * 16 + i] = decrypted[i] ^ prev_cipher[i];

        memcpy(prev_cipher, &ciphertext[b * 16], 16);
    }

    // PKCS#7 padding kaldir
    uint8_t pad = plaintext[len - 1];
    if (pad > 0 && pad <= 16)
        *out_len = len - pad;
    else
        *out_len = len;
}

// CRC-16-CCITT lookup tablosu (poly: 0x1021)
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x4864, 0x5845, 0x6826, 0x7807, 0x08E0, 0x18C1, 0x28A2, 0x38A3,
    0xC94C, 0xD96D, 0xE90E, 0xF92F, 0x89C8, 0x99E9, 0xA98A, 0xB9AB,
    0x5A75, 0x4A54, 0x7A37, 0x6A16, 0x1AF1, 0x0AD0, 0x3AB3, 0x2A92,
    0xDB7D, 0xCB5C, 0xFB3F, 0xEB1E, 0x9BF9, 0x8BD8, 0xBBBB, 0xAB9A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

uint16_t CRC16_CCITT(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;

    for (uint16_t i = 0; i < length; i++) {
        uint8_t idx = (uint8_t)((crc >> 8) ^ data[i]);
        crc = (crc << 8) ^ crc16_table[idx];
    }

    return crc;
}

bool CRC16_Verify(const uint8_t *data, uint16_t length, uint16_t expected_crc)
{
    return CRC16_CCITT(data, length) == expected_crc;
}

// CCSDS paket olustur
void CCSDS_CreatePacket(uint16_t apid, uint16_t seq_count,
                        const uint8_t *payload, uint16_t payload_len,
                        uint8_t *output, uint16_t *out_len)
{
    uint16_t packet_id = 0;
    packet_id |= (CCSDS_VERSION & 0x07) << 13;
    packet_id |= (CCSDS_TYPE_TLM & 0x01) << 12;
    packet_id |= (CCSDS_SEC_HDR_PRESENT & 0x01) << 11;
    packet_id |= (apid & 0x07FF);

    uint16_t seq_ctrl = 0;
    seq_ctrl |= (CCSDS_SEQ_STANDALONE & 0x03) << 14;
    seq_ctrl |= (seq_count & 0x3FFF);

    uint16_t data_len = CCSDS_SEC_HDR_SIZE + payload_len + 2 - 1; // +2 CRC

    // Primary header (big-endian)
    uint16_t idx = 0;
    output[idx++] = (uint8_t)(packet_id >> 8);
    output[idx++] = (uint8_t)(packet_id & 0xFF);
    output[idx++] = (uint8_t)(seq_ctrl >> 8);
    output[idx++] = (uint8_t)(seq_ctrl & 0xFF);
    output[idx++] = (uint8_t)(data_len >> 8);
    output[idx++] = (uint8_t)(data_len & 0xFF);

    // Secondary header: timestamp
    uint32_t ts = HAL_GetTick() / 1000;
    output[idx++] = (uint8_t)(ts >> 24);
    output[idx++] = (uint8_t)(ts >> 16);
    output[idx++] = (uint8_t)(ts >> 8);
    output[idx++] = (uint8_t)(ts & 0xFF);

    memcpy(&output[idx], payload, payload_len);
    idx += payload_len;

    uint16_t crc = CRC16_CCITT(output, idx);
    output[idx++] = (uint8_t)(crc >> 8);
    output[idx++] = (uint8_t)(crc & 0xFF);

    *out_len = idx;
}

// AX.25 UI cerceve olustur
void AX25_CreateFrame(const char dest_call[6], const char src_call[6],
                      const uint8_t *payload, uint16_t payload_len,
                      uint8_t *frame, uint16_t *frame_len)
{
    uint16_t idx = 0;

    frame[idx++] = AX25_FLAG;

    // Hedef adresi
    for (int i = 0; i < 6; i++)
        frame[idx++] = (uint8_t)(dest_call[i] << 1);
    frame[idx++] = AX25_SSID_COMMAND;

    // Kaynak adresi
    for (int i = 0; i < 6; i++)
        frame[idx++] = (uint8_t)(src_call[i] << 1);
    frame[idx++] = AX25_SSID_RESPONSE | 0x01; // son adres

    frame[idx++] = AX25_CONTROL_UI;
    frame[idx++] = AX25_PID_NO_L3;

    memcpy(&frame[idx], payload, payload_len);
    idx += payload_len;

    // FCS
    uint16_t fcs = CRC16_CCITT(&frame[1], idx - 1);
    frame[idx++] = (uint8_t)(fcs & 0xFF);
    frame[idx++] = (uint8_t)((fcs >> 8) & 0xFF);

    frame[idx++] = AX25_FLAG;

    *frame_len = idx;
}

bool AX25_ParseFrame(const uint8_t *frame, uint16_t frame_len,
                     uint8_t *payload, uint16_t *payload_len)
{
    if (frame_len < 20) // min cerceve boyutu
        return false;

    if (frame[0] != AX25_FLAG || frame[frame_len - 1] != AX25_FLAG)
        return false;

    uint16_t data_end = frame_len - 3;
    uint16_t received_fcs = (uint16_t)frame[data_end] |
                           ((uint16_t)frame[data_end + 1] << 8);
    uint16_t calc_fcs = CRC16_CCITT(&frame[1], data_end - 1);

    if (received_fcs != calc_fcs)
        return false;

    uint16_t hdr_size = 1 + 7 + 7 + 1 + 1; // Flag+2xAddr+Ctrl+PID
    *payload_len = data_end - hdr_size;
    memcpy(payload, &frame[hdr_size], *payload_len);

    return true;
}

// FHSS
static uint8_t fhss_hop_table[FHSS_CHANNEL_COUNT];
static uint8_t fhss_current_index = 0;

void FHSS_Init(uint32_t seed)
{
    for (uint8_t i = 0; i < FHSS_CHANNEL_COUNT; i++)
        fhss_hop_table[i] = i;

    // Fisher-Yates shuffle
    uint32_t rng = seed;
    for (int i = FHSS_CHANNEL_COUNT - 1; i > 0; i--) {
        rng = rng * 1103515245 + 12345; // LCG
        uint8_t j = (uint8_t)((rng >> 16) % (i + 1));
        uint8_t temp = fhss_hop_table[i];
        fhss_hop_table[i] = fhss_hop_table[j];
        fhss_hop_table[j] = temp;
    }

    fhss_current_index = 0;
}

uint8_t FHSS_NextChannel(void)
{
    uint8_t ch = fhss_hop_table[fhss_current_index];
    fhss_current_index = (fhss_current_index + 1) % FHSS_CHANNEL_COUNT;
    return ch;
}

uint8_t FHSS_GetCurrentChannel(void)
{
    return fhss_hop_table[fhss_current_index];
}

// JSON serializasyon
int COMMS_TelemetryToJSON(const Golge1_Telemetry_t *tlm,
                          char *buffer, uint16_t buf_size)
{
    int len = snprintf(buffer, buf_size,
        "{"
        "\"h\":{"
            "\"sat\":\"%s\","
            "\"ver\":%d,"
            "\"st\":%d,"
            "\"seq\":%u,"
            "\"ts\":%lu,"
            "\"typ\":%u"
        "},"
        "\"eps\":{"
            "\"bat\":%.1f,"
            "\"bv\":%.2f,"
            "\"bi\":%.1f,"
            "\"sv\":%.2f,"
            "\"sp\":%.0f,"
            "\"pd\":%.0f,"
            "\"pm\":%.0f,"
            "\"ecl\":%d"
        "},"
        "\"thm\":{"
            "\"mcu\":%d,"
            "\"ai\":%d,"
            "\"sdr\":%d,"
            "\"bat\":%d,"
            "\"ext\":%d"
        "},"
        "\"adcs\":{"
            "\"q\":[%.3f,%.3f,%.3f,%.3f],"
            "\"w\":[%.3f,%.3f,%.3f],"
            "\"pm\":%d,"
            "\"stb\":%d,"
            "\"err\":%.2f"
        "},"
        "\"sub\":{"
            "\"jet\":%d,"
            "\"sdr\":%d,"
            "\"cam\":%d,"
            "\"ir\":%d,"
            "\"adcs\":%d"
        "},"
        "\"stat\":{"
            "\"up\":%lu,"
            "\"boot\":%lu,"
            "\"err\":%lu,"
            "\"pkt\":%lu,"
            "\"tgt\":%lu,"
            "\"tmr\":%lu"
        "},",
        tlm->header.sat_id,
        tlm->header.version,
        tlm->header.state,
        tlm->header.sequence,
        (unsigned long)tlm->header.timestamp,
        tlm->header.packet_type,
        tlm->eps.bat_percent,
        tlm->eps.bat_voltage,
        tlm->eps.bat_current,
        tlm->eps.solar_voltage,
        tlm->eps.solar_power_mw,
        tlm->eps.total_draw_mw,
        tlm->eps.power_margin_mw,
        tlm->eps.in_eclipse,
        tlm->thermal.mcu,
        tlm->thermal.ai,
        tlm->thermal.sdr,
        tlm->thermal.battery,
        tlm->thermal.external,
        tlm->adcs.quaternion[0], tlm->adcs.quaternion[1],
        tlm->adcs.quaternion[2], tlm->adcs.quaternion[3],
        tlm->adcs.angular_rate[0], tlm->adcs.angular_rate[1],
        tlm->adcs.angular_rate[2],
        tlm->adcs.pointing_mode,
        tlm->adcs.is_stable,
        tlm->adcs.pointing_error,
        tlm->subsystems.jetson,
        tlm->subsystems.sdr,
        tlm->subsystems.camera,
        tlm->subsystems.ir_sensor,
        tlm->subsystems.adcs,
        (unsigned long)tlm->stats.uptime_sec,
        (unsigned long)tlm->stats.boot_count,
        (unsigned long)tlm->stats.error_count,
        (unsigned long)tlm->stats.packets_sent,
        (unsigned long)tlm->stats.targets_detected,
        (unsigned long)tlm->stats.tmr_corrections
    );

    len += snprintf(buffer + len, buf_size - len, "\"intel\":[");

    for (int i = 0; i < tlm->target_count && i < PAYLOAD_MAX_TARGETS; i++) {
        const IntelData_t *t = &tlm->targets[i];
        len += snprintf(buffer + len, buf_size - len,
            "%s{"
            "\"id\":\"%s\","
            "\"typ\":\"%s\","
            "\"cls\":\"%s\","
            "\"lat\":%.6f,"
            "\"lon\":%.6f,"
            "\"alt\":%.1f,"
            "\"hdg\":%u,"
            "\"spd\":%u,"
            "\"cnf\":%.2f,"
            "\"pri\":%d,"
            "\"sen\":%d,"
            "\"sig\":%.1f"
            "}",
            (i > 0) ? "," : "",
            t->id, t->type, t->classification,
            t->lat, t->lon, t->alt,
            t->heading, t->speed, t->confidence,
            t->priority, t->sensor_id, t->signal_strength
        );
    }

    len += snprintf(buffer + len, buf_size - len, "]}\n");

    return len;
}

int COMMS_IntelToJSON(const IntelData_t *targets, uint8_t count,
                      char *buffer, uint16_t buf_size)
{
    int len = snprintf(buffer, buf_size, "{\"intel\":[");

    for (int i = 0; i < count; i++) {
        const IntelData_t *t = &targets[i];
        len += snprintf(buffer + len, buf_size - len,
            "%s{"
            "\"id\":\"%s\","
            "\"typ\":\"%s\","
            "\"cls\":\"%s\","
            "\"lat\":%.6f,"
            "\"lon\":%.6f,"
            "\"spd\":%u,"
            "\"cnf\":%.2f,"
            "\"pri\":%d,"
            "\"sig\":%.1f"
            "}",
            (i > 0) ? "," : "",
            t->id, t->type, t->classification,
            t->lat, t->lon, t->speed,
            t->confidence, t->priority, t->signal_strength
        );
    }

    len += snprintf(buffer + len, buf_size - len, "]}\n");
    return len;
}

// Ust duzey haberlesme API
static uint16_t comms_sequence_counter = 0;

void COMMS_Init(void)
{
    comms_sequence_counter = 0;
    uint32_t seed = 0x474C4731; // "GLG1"
    FHSS_Init(seed);
}

void COMMS_PrepareSecurePacket(const char *json_input,
                               uint8_t *output, uint16_t *out_len,
                               uint16_t seq_num)
{
    uint8_t encrypted[COMMS_MAX_PAYLOAD_SIZE];
    uint16_t encrypted_len = 0;
    uint8_t ccsds_packet[COMMS_MAX_PAYLOAD_SIZE];
    uint16_t ccsds_len = 0;

    // 1) AES-256-CBC sifreleme
    AES256_CBC_Encrypt(
        (const uint8_t *)json_input,
        (uint16_t)strlen(json_input),
        master_key,
        default_iv,
        encrypted,
        &encrypted_len
    );

    // 2) CCSDS cerceveleme
    CCSDS_CreatePacket(
        GOLGE1_CCSDS_APID,
        seq_num,
        encrypted,
        encrypted_len,
        ccsds_packet,
        &ccsds_len
    );

    // 3) AX.25 cerceveleme
    AX25_CreateFrame(
        GROUND_CALLSIGN,
        GOLGE1_SAT_ID "  ",
        ccsds_packet,
        ccsds_len,
        output,
        out_len
    );
}

void COMMS_CreateBeacon(float bat_percent, uint8_t state,
                        uint8_t *output, uint16_t *out_len)
{
    char beacon[64];
    int len = snprintf(beacon, sizeof(beacon),
        "{\"bcn\":\"%s\",\"bat\":%.1f,\"st\":%d,\"ch\":%d}\n",
        GOLGE1_SAT_ID,
        bat_percent,
        state,
        FHSS_GetCurrentChannel()
    );

    // Beacon sifrelenmez, sadece cercevelenir
    uint8_t ccsds_pkt[128];
    uint16_t ccsds_len;
    CCSDS_CreatePacket(GOLGE1_CCSDS_APID, comms_sequence_counter++,
                       (uint8_t *)beacon, len,
                       ccsds_pkt, &ccsds_len);

    AX25_CreateFrame(GROUND_CALLSIGN, GOLGE1_SAT_ID "  ",
                     ccsds_pkt, ccsds_len,
                     output, out_len);
}
