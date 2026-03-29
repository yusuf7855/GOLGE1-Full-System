// GOLGE-1 Flash Depolama Surucu

#ifndef __GOLGE1_FLASH_H
#define __GOLGE1_FLASH_H

#include "main.h"

// Sektor tanimlari
#define FLASH_SECTOR_DATA_1         FLASH_SECTOR_8
#define FLASH_SECTOR_DATA_2         FLASH_SECTOR_9
#define FLASH_SECTOR_BACKUP_1       FLASH_SECTOR_10
#define FLASH_SECTOR_BACKUP_2       FLASH_SECTOR_11

// Adresler
#define FLASH_DATA_BASE_ADDR        0x08080000  // Sektor 8
#define FLASH_DATA_SIZE             (256 * 1024)
#define FLASH_BACKUP_BASE_ADDR      0x080C0000  // Sektor 10

// Bolum offsetleri
#define FLASH_CONFIG_OFFSET         0x00000
#define FLASH_CONFIG_SIZE           4096
#define FLASH_BOOT_COUNTER_OFFSET   0x01000
#define FLASH_BOOT_COUNTER_SIZE     4096
#define FLASH_SF_BUFFER_OFFSET      0x02000
#define FLASH_SF_BUFFER_SIZE        (32 * 1024)
#define FLASH_TELEM_LOG_OFFSET      0x0A000
#define FLASH_TELEM_LOG_SIZE        (220 * 1024)

#define FLASH_PAGE_SIZE             256
#define FLASH_WEAR_LEVEL_SLOTS      8

// Magic numberlar
#define FLASH_MAGIC_CONFIG          0x474C4701  // "GLG\x01"
#define FLASH_MAGIC_SF_PACKET       0x53465000  // "SFP\0"
#define FLASH_MAGIC_TELEM_LOG       0x544C4F47  // "TLOG"
#define FLASH_MAGIC_BOOT_CNT        0x424F4F54  // "BOOT"

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint32_t boot_count;
    uint8_t  aes_key[32];
    uint8_t  hmac_key[32];
    uint16_t beacon_interval_ms;
    uint8_t  default_scan_mode;
    uint8_t  fhss_seed[4];
    float    gs_latitude;
    float    gs_longitude;
    uint16_t reserved[16];
    uint32_t crc;
} FlashConfig_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t data_length;
    uint8_t  priority;
    uint8_t  retry_count;
    uint32_t timestamp;
    uint32_t crc;
} FlashSFHeader_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t timestamp;
    uint8_t  state;
    float    battery;
    int8_t   temp_mcu;
    int8_t   temp_ai;
    uint8_t  target_count;
    uint16_t seq;
    uint32_t crc;
} FlashTelemLog_t;

typedef struct __attribute__((packed)) {
    uint32_t write_count;
    uint32_t last_write_time;
    uint8_t  is_active;
} WearLevelSlot_t;

bool FLASH_Init(void);

bool FLASH_ReadConfig(FlashConfig_t *config);
bool FLASH_WriteConfig(const FlashConfig_t *config);
void FLASH_DefaultConfig(FlashConfig_t *config);

uint32_t FLASH_ReadBootCount(void);
uint32_t FLASH_IncrementBootCount(void);

bool FLASH_WriteSFPacket(const SF_Packet_t *packet, uint8_t index);
bool FLASH_ReadSFPacket(uint8_t index, SF_Packet_t *packet);
uint8_t FLASH_FlushSFBuffer(const SF_Packet_t *buffer, uint8_t count);
uint8_t FLASH_LoadSFBuffer(SF_Packet_t *buffer, uint8_t max_count);

bool FLASH_WriteTelemLog(const FlashTelemLog_t *log);
uint8_t FLASH_ReadTelemLogs(FlashTelemLog_t *logs, uint8_t max_count);

bool FLASH_EraseSector(uint32_t sector);
bool FLASH_WriteData(uint32_t address, const uint8_t *data, uint32_t length);
void FLASH_ReadData(uint32_t address, uint8_t *data, uint32_t length);

#endif /* __GOLGE1_FLASH_H */
