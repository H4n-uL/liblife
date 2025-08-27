#include "pcmproc.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

// PCM format utility functions
size_t pcm_bit_depth(PCMFormat fmt) {
    return (size_t)fmt >> 3;
}

bool pcm_is_float(PCMFormat fmt) {
    return ((size_t)fmt & 0b100) == 0;
}

bool pcm_is_signed(PCMFormat fmt) {
    return ((size_t)fmt & 0b010) != 0;
}

double pcm_scale(PCMFormat fmt) {
    if (pcm_is_float(fmt)) return 1.0;
    size_t bits = pcm_bit_depth(fmt);
    if (bits == 0) return 1.0;
    return (double)(1LL << (bits - 1));
}

// Internal conversion helpers
static double norm_into(double x, PCMFormat pcm_fmt) {
    if (pcm_is_float(pcm_fmt)) return x;
    x /= pcm_scale(pcm_fmt);
    return pcm_is_signed(pcm_fmt) ? x : x - 1.0;
}

static double norm_from(double x, PCMFormat pcm_fmt) {
    if (pcm_is_float(pcm_fmt)) return x;
    x = pcm_is_signed(pcm_fmt) ? x : x + 1.0;
    return round(x * pcm_scale(pcm_fmt));
}

static double i24_to_f64(const uint8_t* bytes, bool little_endian, bool signed_val) {
    uint8_t sign_bit = little_endian ? bytes[2] & 0x80 : bytes[0] & 0x80;
    uint32_t val;
    if (little_endian) {
        val = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
    } else {
        val = (bytes[0] << 16) | (bytes[1] << 8) | bytes[2];
    }
    if (signed_val && sign_bit) {
        val |= 0xFF000000;
    }
    return (double)(int32_t)val;
}

static void f64_to_i24(double x, bool little_endian, bool signed_val, uint8_t* out) {
    int32_t val = (int32_t)x;
    if (little_endian) {
        out[0] = val & 0xFF;
        out[1] = (val >> 8) & 0xFF;
        out[2] = (val >> 16) & 0xFF;
    } else {
        out[0] = (val >> 16) & 0xFF;
        out[1] = (val >> 8) & 0xFF;
        out[2] = val & 0xFF;
    }
}

double any_to_f64(const uint8_t* bytes, PCMFormat pcm_fmt) {
    size_t depth = pcm_bit_depth(pcm_fmt);
    if (depth == 0) return 0.0;

    double val = 0.0;
    bool is_little_endian = ((size_t)pcm_fmt & 0x01) != 0;

    switch (pcm_fmt) {
        case PCM_F16BE: case PCM_F16LE: {
            // Note: f16 conversion not yet supported, return 0
            return 0.0;
        }
        case PCM_F32BE: case PCM_F32LE: {
            float f32_val;
            if (is_little_endian) {
                uint32_t bits = ((uint32_t)bytes[0]) | ((uint32_t)bytes[1] << 8) |
                               ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
                memcpy(&f32_val, &bits, 4);
            } else {
                uint32_t bits = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
                               ((uint32_t)bytes[2] << 8) | ((uint32_t)bytes[3]);
                memcpy(&f32_val, &bits, 4);
            }
            val = (double)f32_val;
            break;
        }
        case PCM_F64BE: case PCM_F64LE: {
            if (is_little_endian) {
                uint64_t bits = ((uint64_t)bytes[0]) | ((uint64_t)bytes[1] << 8) |
                               ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24) |
                               ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) |
                               ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);
                memcpy(&val, &bits, 8);
            } else {
                uint64_t bits = ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) |
                               ((uint64_t)bytes[2] << 40) | ((uint64_t)bytes[3] << 32) |
                               ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16) |
                               ((uint64_t)bytes[6] << 8) | ((uint64_t)bytes[7]);
                memcpy(&val, &bits, 8);
            }
            break;
        }
        case PCM_S8: val = (double)(*(int8_t*)bytes); break;
        case PCM_S16BE: case PCM_S16LE: {
            int16_t i16_val;
            if (is_little_endian) {
                i16_val = (int16_t)(bytes[0] | (bytes[1] << 8));
            } else {
                i16_val = (int16_t)((bytes[0] << 8) | bytes[1]);
            }
            val = (double)i16_val;
            break;
        }
        case PCM_S24BE: case PCM_S24LE: val = i24_to_f64(bytes, is_little_endian, true); break;
        case PCM_S32BE: case PCM_S32LE: {
            int32_t i32_val;
            if (is_little_endian) {
                i32_val = (int32_t)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
            } else {
                i32_val = (int32_t)((bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3]);
            }
            val = (double)i32_val;
            break;
        }
        case PCM_S64BE: case PCM_S64LE: {
            int64_t i64_val;
            if (is_little_endian) {
                i64_val = (int64_t)((uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) |
                                   ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24) |
                                   ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) |
                                   ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56));
            } else {
                i64_val = (int64_t)(((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) |
                                   ((uint64_t)bytes[2] << 40) | ((uint64_t)bytes[3] << 32) |
                                   ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16) |
                                   ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7]);
            }
            val = (double)i64_val;
            break;
        }
        case PCM_U8: val = (double)(*bytes); break;
        case PCM_U16BE: case PCM_U16LE: {
            uint16_t u16_val;
            if (is_little_endian) {
                u16_val = (uint16_t)(bytes[0] | (bytes[1] << 8));
            } else {
                u16_val = (uint16_t)((bytes[0] << 8) | bytes[1]);
            }
            val = (double)u16_val;
            break;
        }
        case PCM_U24BE: case PCM_U24LE: val = i24_to_f64(bytes, is_little_endian, false); break;
        case PCM_U32BE: case PCM_U32LE: {
            uint32_t u32_val;
            if (is_little_endian) {
                u32_val = (uint32_t)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24));
            } else {
                u32_val = (uint32_t)((bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3]);
            }
            val = (double)u32_val;
            break;
        }
        case PCM_U64BE: case PCM_U64LE: {
            uint64_t u64_val;
            if (is_little_endian) {
                u64_val = (uint64_t)bytes[0] | ((uint64_t)bytes[1] << 8) |
                         ((uint64_t)bytes[2] << 16) | ((uint64_t)bytes[3] << 24) |
                         ((uint64_t)bytes[4] << 32) | ((uint64_t)bytes[5] << 40) |
                         ((uint64_t)bytes[6] << 48) | ((uint64_t)bytes[7] << 56);
            } else {
                u64_val = ((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) |
                         ((uint64_t)bytes[2] << 40) | ((uint64_t)bytes[3] << 32) |
                         ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16) |
                         ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7];
            }
            val = (double)u64_val;
            break;
        }
        default: break;
    }

    return norm_into(val, pcm_fmt);
}

uint8_t* f64_to_any(double x, PCMFormat pcm_fmt, size_t* byte_count) {
    *byte_count = pcm_bit_depth(pcm_fmt) / 8;
    if (*byte_count == 0) return NULL;

    uint8_t* bytes = malloc(*byte_count);
    if (!bytes) return NULL;

    x = norm_from(x, pcm_fmt);
    bool is_little_endian = ((size_t)pcm_fmt & 0x01) != 0;

    switch (pcm_fmt) {
        case PCM_F16BE: case PCM_F16LE: {
            // Note: f16 conversion not yet supported
            free(bytes);
            return NULL;
        }
        case PCM_F32BE: case PCM_F32LE: {
            float f32_val = (float)x;
            uint32_t bits;
            memcpy(&bits, &f32_val, 4);
            if (is_little_endian) {
                bytes[0] = bits & 0xFF;
                bytes[1] = (bits >> 8) & 0xFF;
                bytes[2] = (bits >> 16) & 0xFF;
                bytes[3] = (bits >> 24) & 0xFF;
            } else {
                bytes[0] = (bits >> 24) & 0xFF;
                bytes[1] = (bits >> 16) & 0xFF;
                bytes[2] = (bits >> 8) & 0xFF;
                bytes[3] = bits & 0xFF;
            }
            break;
        }
        case PCM_F64BE: case PCM_F64LE: {
            uint64_t bits;
            memcpy(&bits, &x, 8);
            if (is_little_endian) {
                for (int i = 0; i < 8; i++) {
                    bytes[i] = (bits >> (i * 8)) & 0xFF;
                }
            } else {
                for (int i = 0; i < 8; i++) {
                    bytes[i] = (bits >> ((7 - i) * 8)) & 0xFF;
                }
            }
            break;
        }
        case PCM_S8: *(int8_t*)bytes = (int8_t)x; break;
        case PCM_S16BE: case PCM_S16LE: {
            int16_t i16_val = (int16_t)x;
            if (is_little_endian) {
                bytes[0] = i16_val & 0xFF;
                bytes[1] = (i16_val >> 8) & 0xFF;
            } else {
                bytes[0] = (i16_val >> 8) & 0xFF;
                bytes[1] = i16_val & 0xFF;
            }
            break;
        }
        case PCM_S24BE: case PCM_S24LE: f64_to_i24(x, is_little_endian, true, bytes); break;
        case PCM_S32BE: case PCM_S32LE: {
            int32_t i32_val = (int32_t)x;
            if (is_little_endian) {
                bytes[0] = i32_val & 0xFF;
                bytes[1] = (i32_val >> 8) & 0xFF;
                bytes[2] = (i32_val >> 16) & 0xFF;
                bytes[3] = (i32_val >> 24) & 0xFF;
            } else {
                bytes[0] = (i32_val >> 24) & 0xFF;
                bytes[1] = (i32_val >> 16) & 0xFF;
                bytes[2] = (i32_val >> 8) & 0xFF;
                bytes[3] = i32_val & 0xFF;
            }
            break;
        }
        case PCM_S64BE: case PCM_S64LE: {
            int64_t i64_val = (int64_t)x;
            if (is_little_endian) {
                for (int i = 0; i < 8; i++) {
                    bytes[i] = (i64_val >> (i * 8)) & 0xFF;
                }
            } else {
                for (int i = 0; i < 8; i++) {
                    bytes[i] = (i64_val >> ((7 - i) * 8)) & 0xFF;
                }
            }
            break;
        }
        case PCM_U8: *bytes = (uint8_t)x; break;
        case PCM_U16BE: case PCM_U16LE: {
            uint16_t u16_val = (uint16_t)x;
            if (is_little_endian) {
                bytes[0] = u16_val & 0xFF;
                bytes[1] = (u16_val >> 8) & 0xFF;
            } else {
                bytes[0] = (u16_val >> 8) & 0xFF;
                bytes[1] = u16_val & 0xFF;
            }
            break;
        }
        case PCM_U24BE: case PCM_U24LE: f64_to_i24(x, is_little_endian, false, bytes); break;
        case PCM_U32BE: case PCM_U32LE: {
            uint32_t u32_val = (uint32_t)x;
            if (is_little_endian) {
                bytes[0] = u32_val & 0xFF;
                bytes[1] = (u32_val >> 8) & 0xFF;
                bytes[2] = (u32_val >> 16) & 0xFF;
                bytes[3] = (u32_val >> 24) & 0xFF;
            } else {
                bytes[0] = (u32_val >> 24) & 0xFF;
                bytes[1] = (u32_val >> 16) & 0xFF;
                bytes[2] = (u32_val >> 8) & 0xFF;
                bytes[3] = u32_val & 0xFF;
            }
            break;
        }
        case PCM_U64BE: case PCM_U64LE: {
            uint64_t u64_val = (uint64_t)x;
            if (is_little_endian) {
                for (int i = 0; i < 8; i++) {
                    bytes[i] = (u64_val >> (i * 8)) & 0xFF;
                }
            } else {
                for (int i = 0; i < 8; i++) {
                    bytes[i] = (u64_val >> ((7 - i) * 8)) & 0xFF;
                }
            }
            break;
        }
        default: free(bytes); return NULL;
    }

    return bytes;
}

// PCM Processor implementation
PCMProcessor* pcm_processor_new(PCMFormat fmt) {
    PCMProcessor* processor = malloc(sizeof(PCMProcessor));
    if (!processor) return NULL;
    
    processor->fmt = fmt;
    processor->buffer = NULL;
    processor->buffer_size = 0;
    processor->buffer_capacity = 0;
    
    return processor;
}

void pcm_processor_free(PCMProcessor* processor) {
    if (processor) {
        if (processor->buffer) {
            free(processor->buffer);
        }
        free(processor);
    }
}

// Convert f64 samples to bytes (matching Rust's from_f64)
uint8_t* pcm_processor_from_f64(const PCMProcessor* processor, const double* samples, size_t sample_count, size_t* byte_count) {
    if (!processor || !samples || sample_count == 0) {
        *byte_count = 0;
        return NULL;
    }
    
    size_t bytes_per_sample = pcm_bit_depth(processor->fmt) / 8;
    *byte_count = sample_count * bytes_per_sample;
    
    uint8_t* output = malloc(*byte_count);
    if (!output) {
        *byte_count = 0;
        return NULL;
    }
    
    size_t output_idx = 0;
    for (size_t i = 0; i < sample_count; i++) {
        size_t sample_bytes;
        uint8_t* bytes = f64_to_any(samples[i], processor->fmt, &sample_bytes);
        if (bytes) {
            memcpy(output + output_idx, bytes, sample_bytes);
            output_idx += sample_bytes;
            free(bytes);
        }
    }
    
    return output;
}

// Convert bytes to f64 samples (matching Rust's into_f64)
double* pcm_processor_into_f64(PCMProcessor* processor, const uint8_t* bytes, size_t byte_count, size_t* sample_count) {
    if (!processor || !bytes || byte_count == 0) {
        *sample_count = 0;
        return NULL;
    }
    
    // Extend internal buffer
    size_t new_size = processor->buffer_size + byte_count;
    if (new_size > processor->buffer_capacity) {
        size_t new_capacity = (new_size * 3) / 2;  // Grow by 1.5x
        uint8_t* new_buffer = realloc(processor->buffer, new_capacity);
        if (!new_buffer) {
            *sample_count = 0;
            return NULL;
        }
        processor->buffer = new_buffer;
        processor->buffer_capacity = new_capacity;
    }
    
    // Copy new bytes to buffer
    memcpy(processor->buffer + processor->buffer_size, bytes, byte_count);
    processor->buffer_size = new_size;
    
    // Calculate how many complete samples we can extract
    size_t bytes_per_sample = pcm_bit_depth(processor->fmt) / 8;
    size_t complete_samples = processor->buffer_size / bytes_per_sample;
    size_t bytes_to_process = complete_samples * bytes_per_sample;
    
    if (complete_samples == 0) {
        *sample_count = 0;
        return NULL;
    }
    
    // Allocate output array
    double* samples = malloc(complete_samples * sizeof(double));
    if (!samples) {
        *sample_count = 0;
        return NULL;
    }
    
    // Convert bytes to samples
    for (size_t i = 0; i < complete_samples; i++) {
        samples[i] = any_to_f64(processor->buffer + i * bytes_per_sample, processor->fmt);
    }
    
    // Keep remaining bytes in buffer
    size_t remaining = processor->buffer_size - bytes_to_process;
    if (remaining > 0) {
        memmove(processor->buffer, processor->buffer + bytes_to_process, remaining);
    }
    processor->buffer_size = remaining;
    
    *sample_count = complete_samples;
    return samples;
}