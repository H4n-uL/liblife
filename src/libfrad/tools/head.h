#ifndef LIBFRAD_TOOLS_HEAD_H
#define LIBFRAD_TOOLS_HEAD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../backend/backend.h"

// Metadata entry structure
typedef struct {
    char* title;
    vec_u8* data;
} metadata_entry;

// Metadata collection structure
typedef struct {
    metadata_entry* entries;
    size_t count;
    size_t capacity;
} metadata_vec;

// Parser result structure
typedef struct {
    metadata_vec* meta;
    vec_u8* img;
    uint8_t itype;
} parser_result;

// Function declarations

// Builds a header from metadata and image
// Parameters: Metadata vector, Image data, Image type (optional, use 0 for default)
// Returns: FrAD Header as vec_u8
vec_u8* head_builder(const metadata_vec* meta, const vec_u8* img, uint8_t itype);

// Parses a header into metadata and image
// Parameters: Header data
// Returns: Parser result containing metadata, image, and image type
parser_result* head_parser(vec_u8* header);

// Helper functions for metadata_vec
metadata_vec* metadata_vec_new(void);
void metadata_vec_free(metadata_vec* vec);
void metadata_vec_push(metadata_vec* vec, const char* title, const vec_u8* data);

// Helper function to free parser_result
void parser_result_free(parser_result* result);

#endif // LIBFRAD_TOOLS_HEAD_H