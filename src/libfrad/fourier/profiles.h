#ifndef PROFILES_H
#define PROFILES_H

#include <stdint.h>
#include <stddef.h>

extern const uint32_t COMPACT_SRATES[];
extern const size_t COMPACT_SRATES_SIZE;

extern const uint16_t PROFILE0_DEPTHS[];
extern const uint16_t PROFILE1_DEPTHS[];
extern const uint16_t PROFILE2_DEPTHS[];
extern const uint16_t PROFILE4_DEPTHS[];

uint32_t get_samples_min_ge(uint32_t samples);

#endif // PROFILES_H
