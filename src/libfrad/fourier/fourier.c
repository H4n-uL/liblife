#include "fourier.h"
#include "profiles.h"
#include "profile0.h"
#include "profile1.h"
#include "profile2.h"
#include "profile4.h"

// Available profiles
const uint8_t AVAILABLE_PROFILES[AVAILABLE_PROFILES_COUNT] = {0, 1, 4};

// Maximum segment sizes for each profile
const uint32_t SEGMAX[8] = {
    UINT32_MAX,         // Profile 0
    COMPACT_MAX_SMPL,   // Profile 1
    COMPACT_MAX_SMPL,   // Profile 2
    0,                  // Profile 3
    UINT32_MAX,         // Profile 4
    0,                  // Profile 5
    0,                  // Profile 6
    0                   // Profile 7
};

// Placeholder for unavailable profiles
static const uint16_t EMPTY_DEPTHS[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// Bit depths for each profile
const uint16_t* BIT_DEPTHS[8] = {
    PROFILE0_DEPTHS,
    PROFILE1_DEPTHS,
    PROFILE2_DEPTHS,
    EMPTY_DEPTHS,
    PROFILE4_DEPTHS,
    EMPTY_DEPTHS,
    EMPTY_DEPTHS,
    EMPTY_DEPTHS
};