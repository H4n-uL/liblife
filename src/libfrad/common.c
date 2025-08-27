#include "common.h"

const uint8_t SIGNATURE[] = { 0x66, 0x52, 0x61, 0x64 };
const uint8_t FRM_SIGN[] = { 0xff, 0xd0, 0xd2, 0x97 };

static uint16_t crc16_table[256];
static uint32_t crc32_table[256];

static void build_crc16_table() {
    for (uint16_t i = 0; i < 256; i++) {
        uint16_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
        crc16_table[i] = crc;
    }
}

static void build_crc32_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
}

static int crc_tables_built = 0;

static void build_crc_tables() {
    if (!crc_tables_built) {
        build_crc16_table();
        build_crc32_table();
        crc_tables_built = 1;
    }
}

uint16_t crc16_ansi(uint16_t crc, const uint8_t* data, size_t len) {
    build_crc_tables();
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc16_table[(crc & 0xFF) ^ data[i]];
    }
    return crc;
}

uint32_t frad_crc32(uint32_t crc, const uint8_t* data, size_t len) {
    build_crc_tables();
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ data[i]];
    }
    return ~crc;
}