#include "encoder.h"
#include "../tools/pcmproc.h"
#include "fourier/profiles.h"
#include "fourier/compact.h"
#include "fourier/profile0.h"
#include "fourier/profile1.h"
#include "fourier/profile2.h"
#include "fourier/profile4.h"
#include "tools/ecc/ecc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// Available profiles and bit depths
const uint8_t AVAILABLE_PROFILES[] = {0, 1, 2, 4};
const size_t AVAILABLE_PROFILES_COUNT = 4;

// Bit depths for each profile
const uint16_t* BIT_DEPTHS[] = {
    PROFILE0_DEPTHS,  // Profile 0
    PROFILE1_DEPTHS,  // Profile 1
    PROFILE2_DEPTHS,  // Profile 2
    NULL,             // Profile 3 (not available)
    PROFILE4_DEPTHS   // Profile 4
};

// Bit depth counts for each profile (not including the terminating 0)
#define BIT_DEPTHS_COUNT_P0 6
#define BIT_DEPTHS_COUNT_P1 7
#define BIT_DEPTHS_COUNT_P2 7
#define BIT_DEPTHS_COUNT_P3 0
#define BIT_DEPTHS_COUNT_P4 6

const size_t BIT_DEPTHS_COUNT[] = {
    BIT_DEPTHS_COUNT_P0,  // Profile 0
    BIT_DEPTHS_COUNT_P1,  // Profile 1
    BIT_DEPTHS_COUNT_P2,  // Profile 2
    BIT_DEPTHS_COUNT_P3,  // Profile 3
    BIT_DEPTHS_COUNT_P4   // Profile 4
};

// Maximum samples per frame for each profile
const uint32_t SEGMAX[] = {
    UINT32_MAX,  // Profile 0
    28672,       // Profile 1
    28672,       // Profile 2
    0,           // Profile 3
    UINT32_MAX   // Profile 4
};

// Helper function to check if profile is compact
static bool is_compact_profile(uint8_t profile) {
    return (profile == 1 || profile == 2);
}

// Encoder structure
struct encoder {
    ASFH* asfh;
    vec_f64* buffer;  // Changed to store f64 samples directly

    uint16_t bit_depth;
    uint16_t channels;
    uint32_t fsize;
    uint32_t srate;

    vec_f64* overlap_fragment;

    double loss_level;
    bool init;
};

// Create new encoder
encoder_t* encoder_new(encoder_params_t* params) {
    if (!params) return NULL;

    encoder_t* enc = calloc(1, sizeof(encoder_t));
    if (!enc) return NULL;

    enc->asfh = asfh_new();
    enc->buffer = vec_f64_new(0);  // Changed to f64 buffer
    enc->overlap_fragment = vec_f64_new(0);

    if (!enc->asfh || !enc->buffer || !enc->overlap_fragment) {
        encoder_free(enc);
        return NULL;
    }

    enc->loss_level = 0.5;
    enc->init = false;

    // Set initial profile
    const char* err = encoder_set_profile(enc, params);
    if (err != NULL) {
        // Error occurred, but we'll return the encoder anyway
        // The user can check the error and retry
    }

    return enc;
}

// Free encoder
void encoder_free(encoder_t* enc) {
    if (enc) {
        asfh_free(enc->asfh);
        vec_f64_free(enc->buffer);  // Changed from vec_u8_free
        vec_f64_free(enc->overlap_fragment);
        free(enc);
    }
}

// Overlap function - matches Rust implementation exactly
static vec_f64* overlap(encoder_t* enc, vec_f64* frame, bool flush) {
    if (!enc || !frame) return NULL;

    size_t channels = enc->channels;

    // 1. If overlap fragment is not empty, prepend the fragment to the frame
    if (enc->overlap_fragment && enc->overlap_fragment->size > 0) {
        vec_f64_prepend(frame, enc->overlap_fragment);
    }

    // 2. If overlap is enabled and profile uses overlap
    vec_f64* next_overlap = vec_f64_new(0);
    bool next_flag = !flush &&
                     is_compact_profile(enc->asfh->profile) &&
                     enc->asfh->overlap_ratio > 1;

    if (next_flag && frame->size > 0) {
        // Copy the last olap samples to the next overlap fragment
        size_t overlap_ratio = enc->asfh->overlap_ratio;
        // Samples * (Overlap ratio - 1) / Overlap ratio
        // e.g., ([2048], overlap_ratio=16) -> [1920, 128]
        size_t cutoff = (frame->size / channels) * (overlap_ratio - 1) / overlap_ratio;

        // Copy the end portion to next_overlap
        for (size_t i = cutoff * channels; i < frame->size; i++) {
            vec_f64_push(next_overlap, frame->data[i]);
        }
    }

    // Replace overlap fragment
    vec_f64_free(enc->overlap_fragment);
    enc->overlap_fragment = next_overlap;

    return frame;
}

// Inner encoder loop - matches Rust implementation exactly
static encode_result_t* encoder_inner(encoder_t* enc, const double* samples, size_t sample_count, bool flush) {
    if (!enc) return NULL;

    // Extend buffer with new samples
    if (samples && sample_count > 0) {
        for (size_t i = 0; i < sample_count; i++) {
            vec_f64_push(enc->buffer, samples[i]);
        }
    }

    encode_result_t* result = calloc(1, sizeof(encode_result_t));
    if (!result) return NULL;

    result->data = vec_u8_new(0);
    result->samples = 0;

    if (!enc->init) {
        return result;
    }

    while (true) {
        // 0. Set read length in samples
        size_t overlap_len = enc->overlap_fragment->size / enc->channels;
        size_t rlen = (enc->fsize > overlap_len) ? enc->fsize : overlap_len;

        if (is_compact_profile(enc->asfh->profile)) {
            rlen = get_samples_min_ge(rlen);
        }
        rlen -= overlap_len;

        size_t read_samples = rlen * enc->channels;

        if (enc->buffer->size < read_samples && !flush) break;

        // 1. Cut out the frame from the buffer
        vec_f64* frame = vec_f64_split_front(enc->buffer, (read_samples < enc->buffer->size) ? read_samples : enc->buffer->size);
        size_t samples_in_frame = frame->size / enc->channels;

        // 2. Overlap the frame with the previous overlap fragment
        frame = overlap(enc, frame, flush);
        if (!frame || frame->size == 0) {
            // If this frame is empty, break
            vec_u8* flush_data = asfh_force_flush(enc->asfh);
            if (flush_data) {
                for (size_t i = 0; i < flush_data->size; i++) {
                    vec_u8_push(result->data, flush_data->data[i]);
                }
                vec_u8_free(flush_data);
            }
            if (frame) vec_f64_free(frame);
            break;
        }

        result->samples += samples_in_frame;
        uint32_t fsize = frame->size / enc->channels;

        // 3. Encode the frame
        encoded_packet* packet = NULL;

        switch (enc->asfh->profile) {
            case 0:
                packet = profile0_analogue(frame->data, frame->size, enc->bit_depth,
                                          enc->channels, enc->srate, enc->asfh->endian);
                break;
            case 1:
                packet = profile1_analogue(frame->data, frame->size, enc->bit_depth,
                                          enc->channels, enc->srate, enc->loss_level, enc->asfh->endian);
                break;
            case 2:
                packet = profile2_analogue(frame->data, frame->size, enc->bit_depth,
                                          enc->channels, enc->srate, enc->asfh->endian);
                break;
            case 4:
                packet = profile4_analogue(frame->data, frame->size, enc->bit_depth,
                                          enc->channels, enc->srate, enc->asfh->endian);
                break;
        }

        vec_f64_free(frame);

        if (!packet) {
            break;
        }

        // 4. Create Reed-Solomon error correction code
        vec_u8* frad = packet->data;
        if (enc->asfh->ecc) {
            vec_u8* encoded = ecc_encode(frad, enc->asfh->ecc_ratio);
            if (encoded) {
                frad = encoded;
            }
        }

        // 5. Write the frame to the buffer
        enc->asfh->bit_depth_index = packet->bit_depth_index;
        enc->asfh->channels = packet->channels;
        enc->asfh->fsize = fsize;
        enc->asfh->srate = packet->sample_rate;

        vec_u8* frame_data = asfh_write(enc->asfh, frad);
        if (frame_data) {
            for (size_t i = 0; i < frame_data->size; i++) {
                vec_u8_push(result->data, frame_data->data[i]);
            }
            vec_u8_free(frame_data);
        }

        if (flush) {
            vec_u8* flush_data = asfh_force_flush(enc->asfh);
            if (flush_data) {
                for (size_t i = 0; i < flush_data->size; i++) {
                    vec_u8_push(result->data, flush_data->data[i]);
                }
                vec_u8_free(flush_data);
            }
        }

        // Clean up
        if (enc->asfh->ecc && frad != packet->data) {
            vec_u8_free(frad);
        }
        free(packet);
    }

    return result;
}

// Process input stream (now takes f64 samples)
encode_result_t* encoder_process(encoder_t* enc, const double* samples, size_t sample_count) {
    return encoder_inner(enc, samples, sample_count, false);
}

// Flush encoder
encode_result_t* encoder_flush(encoder_t* enc) {
    return encoder_inner(enc, NULL, 0, true);
}

// Free encode result
void encode_result_free(encode_result_t* result) {
    if (result) {
        vec_u8_free(result->data);
        free(result);
    }
}

// Verify functions
static const char* verify_profile(uint8_t profile) {
    for (size_t i = 0; i < AVAILABLE_PROFILES_COUNT; i++) {
        if (AVAILABLE_PROFILES[i] == profile) return NULL;
    }
    return "Invalid profile";
}

static const char* verify_srate(uint8_t profile, uint32_t srate) {
    if (is_compact_profile(profile)) {
        for (size_t i = 0; i < COMPACT_SRATES_SIZE; i++) {
            if (COMPACT_SRATES[i] == srate) return NULL;
        }
        return "Invalid sample rate for compact profile";
    }
    return NULL;
}

static const char* verify_channels(uint8_t profile, uint16_t channels) {
    (void)profile;  // Unused in current implementation, like Rust
    if (channels == 0) return "Channel count cannot be zero";
    return NULL;
}

static const char* verify_bit_depth(uint8_t profile, uint16_t bit_depth) {
    if (bit_depth == 0) return "Bit depth cannot be zero";
    if (profile >= 5) return "Invalid profile";

    const uint16_t* depths = BIT_DEPTHS[profile];
    size_t count = BIT_DEPTHS_COUNT[profile];

    if (!depths || count == 0) return "Profile not available";

    for (size_t i = 0; i < count; i++) {
        if (depths[i] == bit_depth) return NULL;
    }
    return "Invalid bit depth for profile";
}

static const char* verify_frame_size(uint8_t profile, uint32_t frame_size) {
    if (frame_size == 0) return "Frame size cannot be zero";
    if (profile >= 5) return "Invalid profile";
    if (frame_size > SEGMAX[profile]) return "Frame size exceeds maximum";
    return NULL;
}

// Setters and getters
const char* encoder_set_profile(encoder_t* enc, encoder_params_t* params) {
    if (!enc || !params) return "Invalid parameters";

    const char* err;
    if ((err = verify_profile(params->profile))) return err;
    if ((err = verify_srate(params->profile, params->srate))) return err;
    if ((err = verify_channels(params->profile, params->channels))) return err;
    if ((err = verify_bit_depth(params->profile, params->bit_depth))) return err;
    if ((err = verify_frame_size(params->profile, params->frame_size))) return err;

    // Flush if critical parameters changed
    if (enc->channels != params->channels || enc->srate != params->srate) {
        encode_result_t* flush_result = encoder_flush(enc);
        encode_result_free(flush_result);
    }

    enc->asfh->profile = params->profile;
    enc->srate = params->srate;
    enc->channels = params->channels;
    enc->bit_depth = params->bit_depth;
    enc->fsize = params->frame_size;
    enc->init = true;

    return NULL;
}

uint8_t encoder_get_profile(encoder_t* enc) {
    return enc ? enc->asfh->profile : 0;
}

uint16_t encoder_get_channels(encoder_t* enc) {
    return enc ? enc->channels : 0;
}

uint32_t encoder_get_srate(encoder_t* enc) {
    return enc ? enc->srate : 0;
}

uint32_t encoder_get_frame_size(encoder_t* enc) {
    return enc ? enc->fsize : 0;
}

uint16_t encoder_get_bit_depth(encoder_t* enc) {
    return enc ? enc->bit_depth : 0;
}

// Non-critical setters
void encoder_set_ecc(encoder_t* enc, bool ecc, uint8_t data_size, uint8_t check_size) {
    if (!enc) return;

    enc->asfh->ecc = ecc;

    if (data_size == 0 || data_size + check_size > 255) {
        // Set default 96/24
        enc->asfh->ecc_ratio[0] = 96;
        enc->asfh->ecc_ratio[1] = 24;
    } else {
        enc->asfh->ecc_ratio[0] = data_size;
        enc->asfh->ecc_ratio[1] = check_size;
    }
}

void encoder_set_little_endian(encoder_t* enc, bool little_endian) {
    if (enc) enc->asfh->endian = little_endian;
}

void encoder_set_loss_level(encoder_t* enc, double loss_level) {
    if (enc) enc->loss_level = fmax(fabs(loss_level), 0.125);
}

void encoder_set_overlap_ratio(encoder_t* enc, uint16_t overlap_ratio) {
    if (!enc) return;

    if (overlap_ratio != 0) {
        overlap_ratio = fmax(2, fmin(256, overlap_ratio));
    }
    enc->asfh->overlap_ratio = overlap_ratio;
}