#ifndef ASFH_H
#define ASFH_H

#include <stdint.h>
#include <stdbool.h>
#include "../backend/backend.h"

typedef struct {
    uint64_t frmbytes;
    vec_u8* buffer;
    bool all_set;
    size_t header_bytes;

    bool endian;
    uint16_t bit_depth_index;
    uint16_t channels;
    uint32_t srate;
    uint32_t fsize;

    bool ecc;
    uint8_t ecc_ratio[2];

    uint8_t profile;

    uint32_t crc32;
    uint16_t crc16;
    uint16_t overlap_ratio;
} ASFH;

// Parse result enumeration
typedef enum {
    PARSE_COMPLETE,
    PARSE_INCOMPLETE,
    PARSE_FORCE_FLUSH
} ParseResult;

ASFH* asfh_new();
void asfh_free(ASFH* asfh);
vec_u8* asfh_write(ASFH* asfh, vec_u8* frad);
vec_u8* asfh_force_flush(ASFH* asfh);

// Missing parsing functions
ParseResult asfh_parse(ASFH* asfh, vec_u8* buffer);
void asfh_clear(ASFH* asfh);
bool asfh_criteq(const ASFH* a, const ASFH* b);

#endif // ASFH_H
