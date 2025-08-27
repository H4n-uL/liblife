#ifndef LIBFRAD_H
#define LIBFRAD_H

#ifdef __cplusplus
extern "C" {
#endif

// Version information
#define LIBFRAD_VERSION_MAJOR 1
#define LIBFRAD_VERSION_MINOR 0
#define LIBFRAD_VERSION_PATCH 0
#define LIBFRAD_VERSION_STRING "1.0.0"

// Core modules
#include "encoder.h"
#include "decoder.h"
#include "repairer.h"

// Backend utilities
#include "backend/backend.h"
#include "../tools/pcmproc.h"
#include "backend/bitcvt.h"

// Common structures
#include "common.h"

// Tools
#include "tools/asfh.h"
#include "tools/head.h"
#include "tools/ecc/ecc.h"

// Available profiles
#define FRAD_PROFILE_0  0  // Lossless
#define FRAD_PROFILE_1  1  // Lossy with psychoacoustic masking
#define FRAD_PROFILE_2  2  // Lossy with TNS (Temporal Noise Shaping)
#define FRAD_PROFILE_4  4  // Lossless (alternative)

// Profile categories
#define FRAD_IS_LOSSLESS(p) ((p) == 0 || (p) == 4)
#define FRAD_IS_COMPACT(p)  ((p) == 1 || (p) == 2)

// Convenience functions

// Get version string
static inline const char* libfrad_version(void) {
    return LIBFRAD_VERSION_STRING;
}

// Check if profile is available
static inline bool libfrad_profile_available(uint8_t profile) {
    return (profile == 0 || profile == 1 || profile == 2 || profile == 4);
}

// Get profile name
static inline const char* libfrad_profile_name(uint8_t profile) {
    switch (profile) {
        case 0: return "Profile 0 (Lossless)";
        case 1: return "Profile 1 (Psychoacoustic)";
        case 2: return "Profile 2 (TNS)";
        case 4: return "Profile 4 (Lossless Alt)";
        default: return "Unknown Profile";
    }
}

// Error codes
typedef enum {
    FRAD_OK = 0,
    FRAD_ERROR_INVALID_PARAM = -1,
    FRAD_ERROR_OUT_OF_MEMORY = -2,
    FRAD_ERROR_INVALID_PROFILE = -3,
    FRAD_ERROR_INVALID_SRATE = -4,
    FRAD_ERROR_INVALID_CHANNELS = -5,
    FRAD_ERROR_INVALID_BIT_DEPTH = -6,
    FRAD_ERROR_INVALID_FRAME_SIZE = -7,
    FRAD_ERROR_DECODE_FAILED = -8,
    FRAD_ERROR_ENCODE_FAILED = -9,
    FRAD_ERROR_CRC_MISMATCH = -10,
    FRAD_ERROR_ECC_FAILED = -11,
    FRAD_ERROR_IO = -12
} frad_error_t;

// Get error description
static inline const char* libfrad_error_string(frad_error_t error) {
    switch (error) {
        case FRAD_OK: return "Success";
        case FRAD_ERROR_INVALID_PARAM: return "Invalid parameter";
        case FRAD_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case FRAD_ERROR_INVALID_PROFILE: return "Invalid profile";
        case FRAD_ERROR_INVALID_SRATE: return "Invalid sample rate";
        case FRAD_ERROR_INVALID_CHANNELS: return "Invalid channel count";
        case FRAD_ERROR_INVALID_BIT_DEPTH: return "Invalid bit depth";
        case FRAD_ERROR_INVALID_FRAME_SIZE: return "Invalid frame size";
        case FRAD_ERROR_DECODE_FAILED: return "Decode failed";
        case FRAD_ERROR_ENCODE_FAILED: return "Encode failed";
        case FRAD_ERROR_CRC_MISMATCH: return "CRC mismatch";
        case FRAD_ERROR_ECC_FAILED: return "ECC correction failed";
        case FRAD_ERROR_IO: return "I/O error";
        default: return "Unknown error";
    }
}

#ifdef __cplusplus
}
#endif

#endif // LIBFRAD_H