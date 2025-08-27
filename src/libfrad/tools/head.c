#include "head.h"
#include "../common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Block type signatures
static const uint8_t COMMENT_SIG[] = {0xfa, 0xaa};
static const uint8_t IMAGE_SIG[] = {0xf5};

#define COMMENT_HEAD_LENGTH 12
#define IMAGE_HEAD_LENGTH 10

metadata_vec* metadata_vec_new(void) {
    metadata_vec* vec = (metadata_vec*)malloc(sizeof(metadata_vec));
    if (!vec) return NULL;

    vec->entries = NULL;
    vec->count = 0;
    vec->capacity = 0;
    return vec;
}

void metadata_vec_free(metadata_vec* vec) {
    if (vec) {
        if (vec->entries) {
            for (size_t i = 0; i < vec->count; i++) {
                free(vec->entries[i].title);
                vec_u8_free(vec->entries[i].data);
            }
            free(vec->entries);
        }
        free(vec);
    }
}

void metadata_vec_push(metadata_vec* vec, const char* title, const vec_u8* data) {
    if (!vec) return;

    if (vec->count >= vec->capacity) {
        size_t new_capacity = vec->capacity == 0 ? 1 : vec->capacity * 2;
        metadata_entry* new_entries = (metadata_entry*)realloc(vec->entries, new_capacity * sizeof(metadata_entry));
        if (!new_entries) return;
        vec->entries = new_entries;
        vec->capacity = new_capacity;
    }

    vec->entries[vec->count].title = title ? strdup(title) : NULL;
    if (data) {
        vec->entries[vec->count].data = vec_u8_new(data->size);
        for (size_t i = 0; i < data->size; i++) {
            vec_u8_push(vec->entries[vec->count].data, data->data[i]);
        }
    } else {
        vec->entries[vec->count].data = NULL;
    }
    vec->count++;
}

// Helper function to create a comment block
static vec_u8* create_comment_block(const char* title, const vec_u8* data) {
    if (!title || !data) return NULL;

    vec_u8* block = vec_u8_new(0);
    if (!block) return NULL;

    // Calculate sizes
    size_t title_len = strlen(title);
    size_t data_len = data->size;
    size_t block_len = COMMENT_HEAD_LENGTH + title_len + data_len;

    // Write COMMENT signature
    for (size_t i = 0; i < sizeof(COMMENT_SIG); i++) {
        vec_u8_push(block, COMMENT_SIG[i]);
    }

    // Write block length (6 bytes, big-endian)
    uint64_t block_size = title_len + data_len + 12;  // Total block size including header
    vec_u8_push(block, (block_size >> 40) & 0xFF);
    vec_u8_push(block, (block_size >> 32) & 0xFF);
    vec_u8_push(block, (block_size >> 24) & 0xFF);
    vec_u8_push(block, (block_size >> 16) & 0xFF);
    vec_u8_push(block, (block_size >> 8) & 0xFF);
    vec_u8_push(block, block_size & 0xFF);

    // Write title length (4 bytes, big-endian)
    vec_u8_push(block, (title_len >> 24) & 0xFF);
    vec_u8_push(block, (title_len >> 16) & 0xFF);
    vec_u8_push(block, (title_len >> 8) & 0xFF);
    vec_u8_push(block, title_len & 0xFF);

    // Write title
    for (size_t i = 0; i < title_len; i++) {
        vec_u8_push(block, title[i]);
    }

    // Write data
    for (size_t i = 0; i < data_len; i++) {
        vec_u8_push(block, data->data[i]);
    }

    return block;
}

// Helper function to create an image block
static vec_u8* create_image_block(const vec_u8* img, uint8_t itype) {
    if (!img || img->size == 0) return NULL;

    vec_u8* block = vec_u8_new(0);
    if (!block) return NULL;

    // Validate and default image type
    if (itype > 20) itype = 3;  // Default to "Cover (front)"

    // Write IMAGE signature
    for (size_t i = 0; i < sizeof(IMAGE_SIG); i++) {
        vec_u8_push(block, IMAGE_SIG[i]);
    }

    // Write picture type with flags (1 byte)
    vec_u8_push(block, 0x40 | itype);  // 0b01000000 | picture_type

    // Write data length (8 bytes, big-endian)
    uint64_t data_len = img->size + IMAGE_HEAD_LENGTH;
    vec_u8_push(block, (data_len >> 56) & 0xFF);
    vec_u8_push(block, (data_len >> 48) & 0xFF);
    vec_u8_push(block, (data_len >> 40) & 0xFF);
    vec_u8_push(block, (data_len >> 32) & 0xFF);
    vec_u8_push(block, (data_len >> 24) & 0xFF);
    vec_u8_push(block, (data_len >> 16) & 0xFF);
    vec_u8_push(block, (data_len >> 8) & 0xFF);
    vec_u8_push(block, data_len & 0xFF);

    // Write image data
    for (size_t i = 0; i < img->size; i++) {
        vec_u8_push(block, img->data[i]);
    }

    return block;
}

vec_u8* head_builder(const metadata_vec* meta, const vec_u8* img, uint8_t itype) {
    vec_u8* blocks = vec_u8_new(0);
    if (!blocks) return NULL;

    // Add metadata blocks
    if (meta && meta->count > 0) {
        for (size_t i = 0; i < meta->count; i++) {
            vec_u8* comment_block = create_comment_block(meta->entries[i].title, meta->entries[i].data);
            if (comment_block) {
                for (size_t j = 0; j < comment_block->size; j++) {
                    vec_u8_push(blocks, comment_block->data[j]);
                }
                vec_u8_free(comment_block);
            }
        }
    }

    // Add image block
    if (img && img->size > 0) {
        vec_u8* image_block = create_image_block(img, itype);
        if (image_block) {
            for (size_t j = 0; j < image_block->size; j++) {
                vec_u8_push(blocks, image_block->data[j]);
            }
            vec_u8_free(image_block);
        }
    }

    // Build final header (matches Rust version exactly)
    vec_u8* header = vec_u8_new(0);

    // Write signature (4 bytes)
    for (size_t i = 0; i < 4; i++) {
        vec_u8_push(header, SIGNATURE[i]);
    }

    // Write 4 reserved bytes
    for (int i = 0; i < 4; i++) {
        vec_u8_push(header, 0);
    }

    // Write header size (8 bytes, big-endian) - total size is 64 + blocks
    uint64_t header_size = 64 + blocks->size;
    vec_u8_push(header, (header_size >> 56) & 0xFF);
    vec_u8_push(header, (header_size >> 48) & 0xFF);
    vec_u8_push(header, (header_size >> 40) & 0xFF);
    vec_u8_push(header, (header_size >> 32) & 0xFF);
    vec_u8_push(header, (header_size >> 24) & 0xFF);
    vec_u8_push(header, (header_size >> 16) & 0xFF);
    vec_u8_push(header, (header_size >> 8) & 0xFF);
    vec_u8_push(header, header_size & 0xFF);

    // Write 48 reserved bytes
    for (int i = 0; i < 48; i++) {
        vec_u8_push(header, 0);
    }

    // Append blocks
    for (size_t i = 0; i < blocks->size; i++) {
        vec_u8_push(header, blocks->data[i]);
    }

    vec_u8_free(blocks);
    return header;
}

parser_result* head_parser(vec_u8* header) {
    if (!header || header->size < 16) return NULL;

    parser_result* result = (parser_result*)calloc(1, sizeof(parser_result));
    if (!result) return NULL;

    result->meta = metadata_vec_new();
    result->img = NULL;
    result->itype = 0;

    size_t pos = 0;

    // Check for FrAD signature and skip header prefix
    if (header->size >= 64 && memcmp(header->data, SIGNATURE, 4) == 0) {
        pos = 64;  // Skip signature (4) + reserved (4) + header size (8) + 48 reserved bytes
    }

    // Parse blocks
    while (pos < header->size) {
        // Need at least 2 bytes for smallest signature (COMMENT)
        if (pos + 2 > header->size) break;

        // Check for COMMENT block
        if (header->size >= pos + 2 && memcmp(&header->data[pos], COMMENT_SIG, sizeof(COMMENT_SIG)) == 0) {
            pos += sizeof(COMMENT_SIG);

            // Read block length (6 bytes, big-endian)
            if (pos + 6 > header->size) break;
            uint64_t block_len = ((uint64_t)header->data[pos] << 40) |
                                ((uint64_t)header->data[pos+1] << 32) |
                                ((uint64_t)header->data[pos+2] << 24) |
                                ((uint64_t)header->data[pos+3] << 16) |
                                ((uint64_t)header->data[pos+4] << 8) |
                                ((uint64_t)header->data[pos+5]);
            pos += 6;

            // Read title length (4 bytes, big-endian)
            if (pos + 4 > header->size) break;
            uint32_t title_len = ((uint32_t)header->data[pos] << 24) |
                                ((uint32_t)header->data[pos+1] << 16) |
                                ((uint32_t)header->data[pos+2] << 8) |
                                ((uint32_t)header->data[pos+3]);
            pos += 4;

            // Read title
            if (pos + title_len > header->size) break;
            char* title = (char*)malloc(title_len + 1);
            memcpy(title, &header->data[pos], title_len);
            title[title_len] = '\0';
            pos += title_len;

            // Read data
            size_t data_len = block_len - 12 - title_len;  // Subtract header (2+6+4) and title
            if (pos + data_len > header->size) {
                free(title);
                break;
            }

            vec_u8* data = vec_u8_new(0);
            for (size_t i = 0; i < data_len; i++) {
                vec_u8_push(data, header->data[pos + i]);
            }
            pos += data_len;

            metadata_vec_push(result->meta, title, data);
            free(title);
            vec_u8_free(data);
        }
        // Check for IMAGE block
        else if (header->size >= pos + 1 && memcmp(&header->data[pos], IMAGE_SIG, sizeof(IMAGE_SIG)) == 0) {
            pos += sizeof(IMAGE_SIG);

            // Read picture type (1 byte)
            if (pos >= header->size) break;
            result->itype = header->data[pos] & 0x1F;  // Extract lower 5 bits
            pos++;

            // Read data length (8 bytes, big-endian)
            if (pos + 8 > header->size) break;
            uint64_t data_len = ((uint64_t)header->data[pos] << 56) |
                               ((uint64_t)header->data[pos+1] << 48) |
                               ((uint64_t)header->data[pos+2] << 40) |
                               ((uint64_t)header->data[pos+3] << 32) |
                               ((uint64_t)header->data[pos+4] << 24) |
                               ((uint64_t)header->data[pos+5] << 16) |
                               ((uint64_t)header->data[pos+6] << 8) |
                               ((uint64_t)header->data[pos+7]);
            pos += 8;

            // Calculate actual image data length
            size_t img_len = data_len - IMAGE_HEAD_LENGTH;
            if (pos + img_len > header->size) break;

            // Read image data
            result->img = vec_u8_new(0);
            for (size_t i = 0; i < img_len; i++) {
                vec_u8_push(result->img, header->data[pos + i]);
            }
            pos += img_len;
        }
        // Check for frame signature (end of header)
        else if (header->size >= pos + 4 && memcmp(&header->data[pos], FRM_SIGN, 4) == 0) {
            break;  // Found frame data, stop parsing header
        }
        else {
            // Unknown block or corruption, try to skip
            pos++;
        }
    }

    return result;
}

void parser_result_free(parser_result* result) {
    if (result) {
        metadata_vec_free(result->meta);
        vec_u8_free(result->img);
        free(result);
    }
}