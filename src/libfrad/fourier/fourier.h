#ifndef FOURIER_H
#define FOURIER_H

#include <stdint.h>
#include <limits.h>

// Available profiles
#define AVAILABLE_PROFILES_COUNT 3
extern const uint8_t AVAILABLE_PROFILES[AVAILABLE_PROFILES_COUNT];

// Maximum segment sizes for each profile
extern const uint32_t SEGMAX[8];

// Bit depths for each profile
extern const uint16_t* BIT_DEPTHS[8];

#endif