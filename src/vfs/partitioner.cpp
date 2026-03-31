#pragma GCC optimize ("Os")

#include <string.h>
#include <Arduino.h>

#include "partitioner.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp32s3/rom/spi_flash.h"
#include "esp_partition.h"

// Sector size for partition table
#define PARTITION_SIZE 4096
static const uint32_t PARTITION_ADDR = 0x00008000;   // Partition table addr
static const uint32_t PARTITION_SECTOR = PARTITION_ADDR / 0x1000;
static const size_t LAUNCHER_SPIFFS_SIZE = 1 * 1024 * 1024; // 1 MB

// Gamestation with 4,5Mb of SPIFFS for the Launcher
const uint8_t gamestation[192] = {
    0xAA, 0x50, 0x01, 0x02, 0x00, 0x90, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x6E, 0x76, 0x73, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAA, 0x50, 0x00, 0x20, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x16, 0x00, 0x61, 0x70, 0x70, 0x30,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAA, 0x50, 0x00, 0x10, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x20, 0x00, 0x61, 0x70, 0x70, 0x31,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAA, 0x50, 0x01, 0x82, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00, 0x48, 0x00, 0x73, 0x70, 0x69, 0x66,
    0x66, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xAA, 0x50, 0x01, 0x03, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x63, 0x6F, 0x72, 0x65,
    0x64, 0x75, 0x6D, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xEB, 0xEB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x48, 0xF3, 0xAB, 0xB2, 0x54, 0x6D, 0xCB, 0x5E, 0xF2, 0x78, 0x96, 0xD4, 0xF6, 0x64, 0x3C, 0x1F
};

static bool write_gamestation_partition() {
    printf("[ROM] Preparing Game Station partition buffer...\n");

    uint32_t *buf = (uint32_t *)heap_caps_malloc(
        PARTITION_SIZE,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT
    );
    if (!buf) {
        printf("[ROM][ERROR] Failed to allocate partition buffer\n");
        return false;
    }

    uint8_t *buf8 = reinterpret_cast<uint8_t *>(buf);
    memset(buf8, 0xFF, PARTITION_SIZE);
    memcpy(buf8, gamestation, sizeof(gamestation));

    printf("[ROM] Erasing sector %u (addr 0x%08X)...\n",
           (unsigned)PARTITION_SECTOR, (unsigned)PARTITION_ADDR);

    int rc = esp_rom_spiflash_erase_sector(PARTITION_SECTOR);
    if (rc != 0) {
        printf("[ROM][ERROR] esp_rom_spiflash_erase_sector failed, rc=%d\n", rc);
        heap_caps_free(buf);
        return false;
    }

    printf("[ROM] Writing %u bytes at 0x%08X...\n",
           (unsigned)PARTITION_SIZE, (unsigned)PARTITION_ADDR);

    rc = esp_rom_spiflash_write(PARTITION_ADDR, buf, PARTITION_SIZE);
    heap_caps_free(buf);

    if (rc != 0) {
        printf("[ROM][ERROR] esp_rom_spiflash_write failed, rc=%d\n", rc);
        return false;
    }

    printf("[ROM] Write done.\n");
    return true;
}

// Detect if we are running in Launcher (1 MB SPIFFS)
bool isLauncherLayout() {
    const esp_partition_t *spiffs =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                 ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                 "spiffs");

    if (!spiffs) {
        printf("[GUARD] No SPIFFS partition with label 'spiffs' found, aborting.\n");
        return false;
    }

    printf("[GUARD] Current SPIFFS size: 0x%X (%u KB)\n",
           (unsigned)spiffs->size, (unsigned)(spiffs->size / 1024));

    if (spiffs->size != LAUNCHER_SPIFFS_SIZE) {
        printf("[GUARD] SPIFFS size != 1 MiB (launcher layout), aborting.\n");
        return false;
    }

    printf("[GUARD] Launcher layout detected (SPIFFS = 1 MiB).\n");
    return true;
}

bool flashGameStationPartition() {
    printf("Writing Game Station partition (ROM hack)...\n");

    // Ecriture
    if (!write_gamestation_partition()) {
        printf("[ERROR] Game Station partition write FAILED\n");
        return false;
    }

    printf("[INFO] Partition write OK, verifying content...\n");

    uint8_t verifyBuf[sizeof(gamestation)];
    esp_err_t err = spi_flash_read(PARTITION_ADDR, verifyBuf, sizeof(verifyBuf));
    if (err != ESP_OK) {
        printf("[ERROR] spi_flash_read failed during verification: 0x%x\n", err);
        return false;
    }

    // Verif
    for (size_t i = 0; i < sizeof(gamestation); ++i) {
        if (verifyBuf[i] != gamestation[i]) {
            printf("[ERROR] Verification mismatch at byte %u: flash=0x%02X expected=0x%02X\n",
                   (unsigned)i, verifyBuf[i], gamestation[i]);
            printf("[ERROR] Verification FAILED — partition table NOT valid.\n");
            return false;
        }
    }

    printf("[OK] Game Station partition verified successfully.\n");
    return true;
}

