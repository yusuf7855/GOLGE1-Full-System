// GOLGE-1 Flash Depolama

#include "golge1_flash.h"
#include "golge1_comms.h"
#include "golge1_fdir.h"
#include <string.h>

static uint32_t telem_log_write_offset = 0;
static uint32_t telem_log_count = 0;

bool FLASH_EraseSector(uint32_t sector)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef erase_config;
    erase_config.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_config.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase_config.Sector = sector;
    erase_config.NbSectors = 1;

    uint32_t sector_error = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_config, &sector_error);

    HAL_FLASH_Lock();

    return (status == HAL_OK);
}

bool FLASH_WriteData(uint32_t address, const uint8_t *data, uint32_t length)
{
    HAL_FLASH_Unlock();

    HAL_StatusTypeDef status = HAL_OK;

    uint32_t i = 0;
    while (i < length && status == HAL_OK) {
        if (i + 4 <= length) {
            uint32_t word;
            memcpy(&word, &data[i], 4);
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word);
            i += 4;
        } else {
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, data[i]);
            i += 1;
        }
    }

    HAL_FLASH_Lock();

    return (status == HAL_OK);
}

void FLASH_ReadData(uint32_t address, uint8_t *data, uint32_t length)
{
    memcpy(data, (const void *)address, length); // memory-mapped
}

void FLASH_DefaultConfig(FlashConfig_t *config)
{
    memset(config, 0, sizeof(FlashConfig_t));

    config->magic = FLASH_MAGIC_CONFIG;
    config->version = 1;
    config->boot_count = 0;
    config->beacon_interval_ms = COMMS_BEACON_INTERVAL_MS;
    config->default_scan_mode = 0;
    config->gs_latitude = GS_LATITUDE;
    config->gs_longitude = GS_LONGITUDE;

    memset(config->aes_key, 0x42, 32);  // varsayilan anahtar
    memset(config->hmac_key, 0x43, 32);

    config->crc = FDIR_CRC32((const uint8_t *)config,
                              sizeof(FlashConfig_t) - sizeof(uint32_t));
}

bool FLASH_ReadConfig(FlashConfig_t *config)
{
    FLASH_ReadData(FLASH_DATA_BASE_ADDR + FLASH_CONFIG_OFFSET,
                   (uint8_t *)config, sizeof(FlashConfig_t));

    if (config->magic != FLASH_MAGIC_CONFIG)
        return false;

    uint32_t calc_crc = FDIR_CRC32((const uint8_t *)config,
                                    sizeof(FlashConfig_t) - sizeof(uint32_t));
    if (calc_crc != config->crc)
        return false;

    return true;
}

bool FLASH_WriteConfig(const FlashConfig_t *config)
{
    // yedekle
    uint8_t backup[sizeof(FlashConfig_t)];
    FLASH_ReadData(FLASH_DATA_BASE_ADDR + FLASH_CONFIG_OFFSET,
                   backup, sizeof(FlashConfig_t));
    FLASH_WriteData(FLASH_BACKUP_BASE_ADDR + FLASH_CONFIG_OFFSET,
                    backup, sizeof(FlashConfig_t));

    // yaz
    bool ok = FLASH_WriteData(FLASH_DATA_BASE_ADDR + FLASH_CONFIG_OFFSET,
                               (const uint8_t *)config,
                               sizeof(FlashConfig_t));

    // dogrula (read-back)
    if (ok) {
        FlashConfig_t verify;
        FLASH_ReadData(FLASH_DATA_BASE_ADDR + FLASH_CONFIG_OFFSET,
                       (uint8_t *)&verify, sizeof(FlashConfig_t));
        ok = (memcmp(config, &verify, sizeof(FlashConfig_t)) == 0);
    }

    return ok;
}

// Boot sayaci - wear leveling ile

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t boot_count;
    uint32_t slot_writes;
} BootCountEntry_t;

uint32_t FLASH_ReadBootCount(void)
{
    uint32_t max_count = 0;

    for (uint8_t i = 0; i < FLASH_WEAR_LEVEL_SLOTS; i++) {
        BootCountEntry_t entry;
        uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_BOOT_COUNTER_OFFSET +
                        i * sizeof(BootCountEntry_t);
        FLASH_ReadData(addr, (uint8_t *)&entry, sizeof(entry));

        if (entry.magic == FLASH_MAGIC_BOOT_CNT && entry.boot_count > max_count) {
            max_count = entry.boot_count;
        }
    }

    return max_count;
}

uint32_t FLASH_IncrementBootCount(void)
{
    uint32_t current = FLASH_ReadBootCount();
    uint32_t new_count = current + 1;

    // en az yazilan slot'u bul
    uint32_t min_writes = 0xFFFFFFFF;
    uint8_t  best_slot = 0;

    for (uint8_t i = 0; i < FLASH_WEAR_LEVEL_SLOTS; i++) {
        BootCountEntry_t entry;
        uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_BOOT_COUNTER_OFFSET +
                        i * sizeof(BootCountEntry_t);
        FLASH_ReadData(addr, (uint8_t *)&entry, sizeof(entry));

        if (entry.magic != FLASH_MAGIC_BOOT_CNT || entry.slot_writes < min_writes) {
            min_writes = entry.slot_writes;
            best_slot = i;
        }
    }

    BootCountEntry_t new_entry;
    new_entry.magic = FLASH_MAGIC_BOOT_CNT;
    new_entry.boot_count = new_count;
    new_entry.slot_writes = (min_writes == 0xFFFFFFFF) ? 1 : min_writes + 1;

    uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_BOOT_COUNTER_OFFSET +
                    best_slot * sizeof(BootCountEntry_t);
    FLASH_WriteData(addr, (const uint8_t *)&new_entry, sizeof(new_entry));

    return new_count;
}

bool FLASH_WriteSFPacket(const SF_Packet_t *packet, uint8_t index)
{
    if (index >= SF_MAX_PACKETS)
        return false;

    FlashSFHeader_t header;
    header.magic = FLASH_MAGIC_SF_PACKET;
    header.data_length = packet->data_length;
    header.priority = packet->priority;
    header.retry_count = packet->retry_count;
    header.timestamp = packet->timestamp;
    header.crc = FDIR_CRC32(packet->data, packet->data_length);

    uint32_t slot_size = sizeof(FlashSFHeader_t) + SF_PACKET_DATA_SIZE;
    uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_SF_BUFFER_OFFSET +
                    index * slot_size;

    bool ok = FLASH_WriteData(addr, (const uint8_t *)&header, sizeof(header));

    if (ok) {
        ok = FLASH_WriteData(addr + sizeof(header), packet->data,
                             packet->data_length);
    }

    return ok;
}

bool FLASH_ReadSFPacket(uint8_t index, SF_Packet_t *packet)
{
    if (index >= SF_MAX_PACKETS)
        return false;

    uint32_t slot_size = sizeof(FlashSFHeader_t) + SF_PACKET_DATA_SIZE;
    uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_SF_BUFFER_OFFSET +
                    index * slot_size;

    FlashSFHeader_t header;
    FLASH_ReadData(addr, (uint8_t *)&header, sizeof(header));

    if (header.magic != FLASH_MAGIC_SF_PACKET)
        return false;

    packet->data_length = header.data_length;
    packet->priority = header.priority;
    packet->retry_count = header.retry_count;
    packet->timestamp = header.timestamp;
    packet->is_valid = 1;

    FLASH_ReadData(addr + sizeof(header), packet->data, header.data_length);

    uint32_t calc_crc = FDIR_CRC32(packet->data, packet->data_length);
    if (calc_crc != header.crc) {
        packet->is_valid = 0;
        return false;
    }

    return true;
}

uint8_t FLASH_FlushSFBuffer(const SF_Packet_t *buffer, uint8_t count)
{
    uint8_t written = 0;
    for (uint8_t i = 0; i < count && i < SF_MAX_PACKETS; i++) {
        if (buffer[i].is_valid) {
            if (FLASH_WriteSFPacket(&buffer[i], i))
                written++;
        }
    }
    return written;
}

uint8_t FLASH_LoadSFBuffer(SF_Packet_t *buffer, uint8_t max_count)
{
    uint8_t loaded = 0;
    for (uint8_t i = 0; i < max_count && i < SF_MAX_PACKETS; i++) {
        if (FLASH_ReadSFPacket(i, &buffer[i]))
            loaded++;
        else
            buffer[i].is_valid = 0;
    }
    return loaded;
}

// Dairesel telemetri logu

bool FLASH_WriteTelemLog(const FlashTelemLog_t *log)
{
    uint32_t max_entries = FLASH_TELEM_LOG_SIZE / sizeof(FlashTelemLog_t);

    if (telem_log_count >= max_entries) {
        telem_log_write_offset = 0; // basa don
    }

    uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_TELEM_LOG_OFFSET +
                    telem_log_write_offset;

    bool ok = FLASH_WriteData(addr, (const uint8_t *)log, sizeof(FlashTelemLog_t));

    if (ok) {
        telem_log_write_offset += sizeof(FlashTelemLog_t);
        telem_log_count++;
    }

    return ok;
}

uint8_t FLASH_ReadTelemLogs(FlashTelemLog_t *logs, uint8_t max_count)
{
    uint8_t read = 0;
    uint32_t max_entries = FLASH_TELEM_LOG_SIZE / sizeof(FlashTelemLog_t);
    uint32_t start_entry = (telem_log_count > max_count) ?
                           (telem_log_count - max_count) : 0;

    for (uint32_t i = start_entry; i < telem_log_count && read < max_count; i++) {
        uint32_t entry_idx = i % max_entries;
        uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_TELEM_LOG_OFFSET +
                        entry_idx * sizeof(FlashTelemLog_t);

        FLASH_ReadData(addr, (uint8_t *)&logs[read], sizeof(FlashTelemLog_t));

        if (logs[read].magic == FLASH_MAGIC_TELEM_LOG)
            read++;
    }

    return read;
}

bool FLASH_Init(void)
{
    FlashConfig_t config;
    if (!FLASH_ReadConfig(&config)) {
        FLASH_DefaultConfig(&config); // ilk boot veya bozuk
    }

    FLASH_IncrementBootCount();

    // telem log indeksini bul
    telem_log_write_offset = 0;
    telem_log_count = 0;

    uint32_t max_entries = FLASH_TELEM_LOG_SIZE / sizeof(FlashTelemLog_t);
    for (uint32_t i = 0; i < max_entries; i++) {
        FlashTelemLog_t entry;
        uint32_t addr = FLASH_DATA_BASE_ADDR + FLASH_TELEM_LOG_OFFSET +
                        i * sizeof(FlashTelemLog_t);
        FLASH_ReadData(addr, (uint8_t *)&entry, sizeof(entry));

        if (entry.magic == FLASH_MAGIC_TELEM_LOG) {
            telem_log_count++;
            telem_log_write_offset = (i + 1) * sizeof(FlashTelemLog_t);
        } else {
            break;
        }
    }

    return true;
}
