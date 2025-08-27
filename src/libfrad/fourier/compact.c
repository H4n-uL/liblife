#include "compact.h"
#include <stddef.h>

// Sample rate table
const uint32_t COMPACT_SRATES[12] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000
};

// Sample count table
const uint32_t COMPACT_SAMPLES[32] = {
      128,   160,   192,   224,
      256,   320,   384,   448,
      512,   640,   768,   896,
     1024,  1280,  1536,  1792,
     2048,  2560,  3072,  3584,
     4096,  5120,  6144,  7168,
     8192, 10240, 12288, 14336,
    16384, 20480, 24576, 28672
};

// Maximum sample count
const uint32_t COMPACT_MAX_SMPL = 28672;

// Get valid sample rate eq or larger than given sample rate
uint32_t get_valid_srate(uint32_t srate) {
    uint32_t max = COMPACT_SRATES[0];
    if (srate > max) return max;

    // Find the first sample rate that is >= srate, starting from the end
    for (int i = COMPACT_SRATES_SIZE - 1; i >= 0; i--) {
        if (COMPACT_SRATES[i] >= srate) {
            return COMPACT_SRATES[i];
        }
    }

    return COMPACT_SRATES[0];
}

// Get sample rate index of given sample rate
uint16_t get_srate_index(uint32_t srate) {
    uint16_t best_index = 0;
    uint32_t best_rate = COMPACT_SRATES[0];

    for (size_t i = 0; i < COMPACT_SRATES_SIZE; i++) {
        if (COMPACT_SRATES[i] >= srate) {
            if (COMPACT_SRATES[i] < best_rate) {
                best_rate = COMPACT_SRATES[i];
                best_index = i;
            }
        }
    }

    return best_index;
}

// Get minimum sample count greater than or equal to given value
uint32_t get_samples_min_ge(uint32_t value) {
    for (size_t i = 0; i < COMPACT_SAMPLES_SIZE; i++) {
        if (COMPACT_SAMPLES[i] >= value) {
            return COMPACT_SAMPLES[i];
        }
    }
    return 0;
}

// Get sample count index of given value
uint16_t get_samples_index(uint32_t value) {
    value = get_samples_min_ge(value);
    for (size_t i = 0; i < COMPACT_SAMPLES_SIZE; i++) {
        if (COMPACT_SAMPLES[i] == value) {
            return i;
        }
    }
    return 0;
}