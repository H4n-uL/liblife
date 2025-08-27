#include "asfh.h"
#include "../common.h"
#include "../fourier/profiles.h"
#include "../fourier/compact.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static uint8_t encode_pfb(uint8_t profile, bool enable_ecc, bool little_endian, uint16_t bit_depth_index) {
    return (profile << 5) | ((enable_ecc & 1) << 4) | ((little_endian & 1) << 3) | (bit_depth_index & 0x07);
}

static void encode_css(uint16_t channels, uint32_t srate, uint32_t fsize, bool force_flush, uint8_t* out) {
    uint16_t chnl = (channels - 1) << 10;
    uint16_t srate_idx = get_srate_index(srate) << 6;
    uint16_t fsize_idx = get_samples_index(fsize) << 1;
    uint16_t val = chnl | srate_idx | fsize_idx | (force_flush ? 1 : 0);

    // Store as big-endian
    out[0] = (val >> 8) & 0xFF;
    out[1] = val & 0xFF;
}

ASFH* asfh_new() {
    ASFH* asfh = (ASFH*)calloc(1, sizeof(ASFH));
    if (!asfh) return NULL;
    asfh->buffer = vec_u8_new(0);
    if (!asfh->buffer) {
        free(asfh);
        return NULL;
    }
    return asfh;
}

void asfh_free(ASFH* asfh) {
    if (asfh) {
        if (asfh->buffer) vec_u8_free(asfh->buffer);
        free(asfh);
    }
}

vec_u8* asfh_write(ASFH* asfh, vec_u8* frad) {
    vec_u8* fhead = vec_u8_new(32);
    for (size_t i = 0; i < 4; i++) vec_u8_push(fhead, FRM_SIGN[i]);

    uint32_t frad_len = frad->size;
    for (int i = 3; i >= 0; i--) vec_u8_push(fhead, (frad_len >> (i * 8)) & 0xFF);
    vec_u8_push(fhead, encode_pfb(asfh->profile, asfh->ecc, asfh->endian, asfh->bit_depth_index));

    if (asfh->profile == 1 || asfh->profile == 2) { // Compact profiles
        uint8_t css[2];
        encode_css(asfh->channels, asfh->srate, asfh->fsize, false, css);
        vec_u8_push(fhead, css[0]);
        vec_u8_push(fhead, css[1]);
        vec_u8_push(fhead, (asfh->overlap_ratio > 0 ? asfh->overlap_ratio - 1 : 0));
        if (asfh->ecc) {
            vec_u8_push(fhead, asfh->ecc_ratio[0]);
            vec_u8_push(fhead, asfh->ecc_ratio[1]);
            uint16_t crc = crc16_ansi(0, frad->data, frad->size);
            vec_u8_push(fhead, (crc >> 8) & 0xFF);
            vec_u8_push(fhead, crc & 0xFF);
        }
    } else { // Lossless profiles
        vec_u8_push(fhead, asfh->channels - 1);
        vec_u8_push(fhead, asfh->ecc_ratio[0]);
        vec_u8_push(fhead, asfh->ecc_ratio[1]);
        for (int i = 3; i >= 0; i--) vec_u8_push(fhead, (asfh->srate >> (i * 8)) & 0xFF);
        for (int i = 0; i < 8; i++) vec_u8_push(fhead, 0); // reserved
        for (int i = 3; i >= 0; i--) vec_u8_push(fhead, (asfh->fsize >> (i * 8)) & 0xFF);
        uint32_t crc = frad_crc32(0, frad->data, frad->size);
        for (int i = 3; i >= 0; i--) vec_u8_push(fhead, (crc >> (i * 8)) & 0xFF);
    }

    vec_u8* frame = vec_u8_new(fhead->size + frad->size);
    memcpy(frame->data, fhead->data, fhead->size);
    frame->size += fhead->size;
    memcpy(frame->data + frame->size, frad->data, frad->size);
    frame->size += frad->size;

    vec_u8_free(fhead);
    return frame;
}

vec_u8* asfh_force_flush(ASFH* asfh) {
    vec_u8* fhead = vec_u8_new(12);
    for (size_t i = 0; i < 4; i++) vec_u8_push(fhead, FRM_SIGN[i]);
    for (int i = 0; i < 4; i++) vec_u8_push(fhead, 0); // frad length = 0
    vec_u8_push(fhead, encode_pfb(asfh->profile, asfh->ecc, asfh->endian, asfh->bit_depth_index));

    if (asfh->profile == 1 || asfh->profile == 2) { // Compact profiles
        uint16_t channels = asfh->channels > 1 ? asfh->channels : 1;
        uint8_t css[2];
        encode_css(channels, asfh->srate, asfh->fsize, true, css);
        vec_u8_push(fhead, css[0]);
        vec_u8_push(fhead, css[1]);
        vec_u8_push(fhead, 0);
    } else {
        vec_u8_free(fhead);
        return vec_u8_new(0);
    }

    return fhead;
}

// Helper function to decode PFB byte
static void decode_pfb(uint8_t pfb, uint8_t* profile, bool* ecc, bool* endian, uint16_t* bit_depth_index) {
    *profile = pfb >> 5;
    *ecc = (pfb >> 4) & 1;
    *endian = (pfb >> 3) & 1;
    *bit_depth_index = pfb & 0x07;
}

// Helper function to decode CSS bytes
static void decode_css(const uint8_t* css, uint16_t* channels, uint32_t* srate, uint32_t* fsize, bool* force_flush) {
    uint16_t css_int = (css[0] << 8) | css[1]; // big-endian
    *channels = (css_int >> 10) + 1;
    *srate = COMPACT_SRATES[(css_int >> 6) & 0x0F];
    *fsize = COMPACT_SAMPLES[(css_int >> 1) & 0x1F];
    *force_flush = css_int & 1;
}

// Fill buffer helper function
static bool fill_buffer(ASFH* asfh, vec_u8* buffer, size_t target_size) {
    if (asfh->buffer->size < target_size) {
        size_t needed = target_size - asfh->buffer->size;
        if (buffer->size < needed) {
            // Not enough data in input buffer
            for (size_t i = 0; i < buffer->size; i++) {
                vec_u8_push(asfh->buffer, buffer->data[i]);
            }
            buffer->size = 0; // Clear input buffer
            return false;
        }

        // Take needed bytes from input buffer
        for (size_t i = 0; i < needed; i++) {
            vec_u8_push(asfh->buffer, buffer->data[i]);
        }

        // Remove taken bytes from input buffer
        memmove(buffer->data, buffer->data + needed, buffer->size - needed);
        buffer->size -= needed;
    }
    asfh->header_bytes = target_size;
    return true;
}

// Main parsing function
ParseResult asfh_parse(ASFH* asfh, vec_u8* buffer) {
    if (!fill_buffer(asfh, buffer, 9)) return PARSE_INCOMPLETE; // Need at least 9 bytes for basic header

    asfh->frmbytes = (asfh->buffer->data[4] << 24) | (asfh->buffer->data[5] << 16) |
                     (asfh->buffer->data[6] << 8) | asfh->buffer->data[7];

    decode_pfb(asfh->buffer->data[8], &asfh->profile, &asfh->ecc, &asfh->endian, &asfh->bit_depth_index);

    // Check if it's a compact profile
    if (asfh->profile == 1 || asfh->profile == 2) {
        if (!fill_buffer(asfh, buffer, 12)) return PARSE_INCOMPLETE;

        bool force_flush;
        decode_css(&asfh->buffer->data[9], &asfh->channels, &asfh->srate, &asfh->fsize, &force_flush);

        if (force_flush) {
            asfh->all_set = true;
            return PARSE_FORCE_FLUSH;
        }

        asfh->overlap_ratio = asfh->buffer->data[11];
        if (asfh->overlap_ratio != 0) {
            asfh->overlap_ratio += 1;
        }

        if (asfh->ecc) {
            if (!fill_buffer(asfh, buffer, 16)) return PARSE_INCOMPLETE;

            asfh->ecc_ratio[0] = asfh->buffer->data[12];
            asfh->ecc_ratio[1] = asfh->buffer->data[13];
            asfh->crc16 = (asfh->buffer->data[14] << 8) | asfh->buffer->data[15];
        }
    } else {
        // Lossless profile
        if (!fill_buffer(asfh, buffer, 32)) return PARSE_INCOMPLETE;

        asfh->channels = asfh->buffer->data[9] + 1;
        asfh->ecc_ratio[0] = asfh->buffer->data[10];
        asfh->ecc_ratio[1] = asfh->buffer->data[11];
        asfh->srate = (asfh->buffer->data[12] << 24) | (asfh->buffer->data[13] << 16) |
                      (asfh->buffer->data[14] << 8) | asfh->buffer->data[15];
        // bytes 16-23 are reserved (8 bytes)
        asfh->fsize = (asfh->buffer->data[24] << 24) | (asfh->buffer->data[25] << 16) |
                      (asfh->buffer->data[26] << 8) | asfh->buffer->data[27];
        asfh->crc32 = (asfh->buffer->data[28] << 24) | (asfh->buffer->data[29] << 16) |
                      (asfh->buffer->data[30] << 8) | asfh->buffer->data[31];
    }

    // Handle extended frame size
    if (asfh->frmbytes == 0xFFFFFFFF) {
        if (!fill_buffer(asfh, buffer, asfh->header_bytes + 8)) return PARSE_INCOMPLETE;

        size_t offset = asfh->buffer->size - 8;
        asfh->frmbytes = ((uint64_t)asfh->buffer->data[offset] << 56) |
                         ((uint64_t)asfh->buffer->data[offset+1] << 48) |
                         ((uint64_t)asfh->buffer->data[offset+2] << 40) |
                         ((uint64_t)asfh->buffer->data[offset+3] << 32) |
                         ((uint64_t)asfh->buffer->data[offset+4] << 24) |
                         ((uint64_t)asfh->buffer->data[offset+5] << 16) |
                         ((uint64_t)asfh->buffer->data[offset+6] << 8) |
                         ((uint64_t)asfh->buffer->data[offset+7]);
    }

    asfh->all_set = true;
    return PARSE_COMPLETE;
}

// Clear ASFH buffer and reset state
void asfh_clear(ASFH* asfh) {
    if (asfh && asfh->buffer) {
        asfh->all_set = false;
        asfh->buffer->size = 0;
        asfh->header_bytes = 0;
    }
}

// Compare critical parameters between two ASFH structures
bool asfh_criteq(const ASFH* a, const ASFH* b) {
    if (!a || !b) return false;
    return a->channels == b->channels && a->srate == b->srate;
}