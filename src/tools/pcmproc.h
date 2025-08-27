#ifndef PCMPROC_H
#define PCMPROC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// PCM Format definitions
typedef enum {
    PCM_F16BE = (16 << 3) | 0b010,
    PCM_F16LE = (16 << 3) | 0b011,
    PCM_F32BE = (32 << 3) | 0b010,
    PCM_F32LE = (32 << 3) | 0b011,
    PCM_F64BE = (64 << 3) | 0b010,
    PCM_F64LE = (64 << 3) | 0b011,

    PCM_S8    = ( 8 << 3) | 0b110,
    PCM_S16BE = (16 << 3) | 0b110,
    PCM_S16LE = (16 << 3) | 0b111,
    PCM_S24BE = (24 << 3) | 0b110,
    PCM_S24LE = (24 << 3) | 0b111,
    PCM_S32BE = (32 << 3) | 0b110,
    PCM_S32LE = (32 << 3) | 0b111,
    PCM_S64BE = (64 << 3) | 0b110,
    PCM_S64LE = (64 << 3) | 0b111,

    PCM_U8    = ( 8 << 3) | 0b100,
    PCM_U16BE = (16 << 3) | 0b100,
    PCM_U16LE = (16 << 3) | 0b101,
    PCM_U24BE = (24 << 3) | 0b100,
    PCM_U24LE = (24 << 3) | 0b101,
    PCM_U32BE = (32 << 3) | 0b100,
    PCM_U32LE = (32 << 3) | 0b101,
    PCM_U64BE = (64 << 3) | 0b100,
    PCM_U64LE = (64 << 3) | 0b101
} PCMFormat;

// PCM format utility functions
size_t pcm_bit_depth(PCMFormat fmt);
bool pcm_is_float(PCMFormat fmt);
bool pcm_is_signed(PCMFormat fmt);
double pcm_scale(PCMFormat fmt);

// Conversion functions
double any_to_f64(const uint8_t* bytes, PCMFormat pcm_fmt);
uint8_t* f64_to_any(double x, PCMFormat pcm_fmt, size_t* byte_count);

// PCM Processor structure (matching Rust implementation)
typedef struct {
    PCMFormat fmt;
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_capacity;
} PCMProcessor;

// PCM Processor functions
PCMProcessor* pcm_processor_new(PCMFormat fmt);
void pcm_processor_free(PCMProcessor* processor);

// Convert f64 samples to bytes
uint8_t* pcm_processor_from_f64(const PCMProcessor* processor, const double* samples, size_t sample_count, size_t* byte_count);

// Convert bytes to f64 samples
double* pcm_processor_into_f64(PCMProcessor* processor, const uint8_t* bytes, size_t byte_count, size_t* sample_count);

#endif