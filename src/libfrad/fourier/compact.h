#ifndef COMPACT_H
#define COMPACT_H

#include <stdint.h>

// Sample rate table
#define COMPACT_SRATES_SIZE 12
extern const uint32_t COMPACT_SRATES[COMPACT_SRATES_SIZE];

// Sample count table
#define COMPACT_SAMPLES_SIZE 32
extern const uint32_t COMPACT_SAMPLES[COMPACT_SAMPLES_SIZE];

// Get valid sample rate eq or larger than given sample rate
uint32_t get_valid_srate(uint32_t srate);

// Get sample rate index of given sample rate
uint16_t get_srate_index(uint32_t srate);

// Get minimum sample count greater than or equal to given value
uint32_t get_samples_min_ge(uint32_t value);

// Get sample count index of given value
uint16_t get_samples_index(uint32_t value);

// Maximum sample count
extern const uint32_t COMPACT_MAX_SMPL;

#endif