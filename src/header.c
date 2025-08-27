#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "tools/cli.h"
#include "libfrad/tools/head.h"
#include "libfrad/common.h"
#include "libfrad/backend/backend.h"
#include "app_common.h"

// Base64 encoding table
static const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64 encode function
static char* base64_encode(const uint8_t* data, size_t len) {
    size_t out_len = ((len + 2) / 3) * 4;
    char* out = malloc(out_len + 1);
    if (!out) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i < len; i += 3, j += 4) {
        uint32_t n = data[i] << 16;
        if (i + 1 < len) n |= data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];

        out[j] = base64_chars[(n >> 18) & 63];
        out[j + 1] = base64_chars[(n >> 12) & 63];
        out[j + 2] = (i + 1 < len) ? base64_chars[(n >> 6) & 63] : '=';
        out[j + 3] = (i + 2 < len) ? base64_chars[n & 63] : '=';
    }
    out[out_len] = '\0';
    return out;
}

// Helper to check if data is valid UTF-8
static bool is_valid_utf8(const uint8_t* data, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (data[i] <= 0x7F) {
            i++;
        } else if ((data[i] & 0xE0) == 0xC0) {
            if (i + 1 >= len || (data[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((data[i] & 0xF0) == 0xE0) {
            if (i + 2 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((data[i] & 0xF8) == 0xF0) {
            if (i + 3 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

// Get file extension from image data - using infer-like detection
static const char* get_image_extension(const uint8_t* data, size_t len) {
    if (len < 4) return "img";

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (len >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
        data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) return "png";

    // JPEG: FF D8 FF
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return "jpg";

    // GIF: GIF87a or GIF89a
    if (len >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
        data[3] == '8' && (data[4] == '7' || data[4] == '9') && data[5] == 'a') return "gif";

    // BMP: BM
    if (len >= 2 && data[0] == 'B' && data[1] == 'M') return "bmp";

    // WebP: RIFF....WEBP
    if (len >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0) return "webp";

    // TIFF: II*\0 or MM\0*
    if (len >= 4 && ((data[0] == 0x49 && data[1] == 0x49 && data[2] == 0x2A && data[3] == 0x00) ||
                     (data[0] == 0x4D && data[1] == 0x4D && data[2] == 0x00 && data[3] == 0x2A))) return "tiff";

    // ICO: 00 00 01 00
    if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00) return "ico";

    return "img";
}


void modify_metadata(const char* input, const char* metaaction, CliParams* params) {
    if (!input || strlen(input) == 0) {
        fprintf(stderr, "Input file must be given\n");
        return;
    }

    if (!metaaction) {
        fprintf(stderr, "Error: No metadata action specified\n");
        return;
    }

    // Check if file exists
    if (access(input, F_OK) != 0) {
        fprintf(stderr, "Input file does not exist\n");
        return;
    }

    // Read file header
    FILE* rfile = fopen(input, "rb");
    if (!rfile) {
        fprintf(stderr, "Failed to open input file\n");
        return;
    }

    uint8_t head[64];
    size_t bytes_read = fread(head, 1, 64, rfile);
    if (bytes_read < 16) {
        fprintf(stderr, "File too small to be a valid FrAD file\n");
        fclose(rfile);
        return;
    }

    // Check for FrAD signature
    uint64_t head_len = 0;
    if (memcmp(head, SIGNATURE, 4) == 0) {
        // Extract header length (8 bytes, big-endian, at offset 8)
        head_len = ((uint64_t)head[8] << 56) | ((uint64_t)head[9] << 48) |
                   ((uint64_t)head[10] << 40) | ((uint64_t)head[11] << 32) |
                   ((uint64_t)head[12] << 24) | ((uint64_t)head[13] << 16) |
                   ((uint64_t)head[14] << 8) | ((uint64_t)head[15]);
    } else if (memcmp(head, FRM_SIGN, 4) == 0) {
        head_len = 0;
    } else {
        fprintf(stderr, "It seems this is not a valid FrAD file.\n");
        fclose(rfile);
        return;
    }

    // Read full header
    fseek(rfile, 0, SEEK_SET);
    vec_u8* head_old = vec_u8_new(head_len);
    if (head_len > 0) {
        uint8_t* temp_buf = malloc(head_len);
        fread(temp_buf, 1, head_len, rfile);
        for (size_t i = 0; i < head_len; i++) {
            vec_u8_push(head_old, temp_buf[i]);
        }
        free(temp_buf);
    }

    // Parse existing header
    parser_result* parsed = head_parser(head_old);
    vec_u8_free(head_old);

    if (strcmp(metaaction, "parse") == 0 || strcmp(metaaction, "show") == 0) {
        // Output metadata as JSON
        printf("[\n");
        if (parsed->meta) {
            for (size_t i = 0; i < parsed->meta->count; i++) {
                printf("  {\n");
                printf("    \"key\": \"%s\",\n", parsed->meta->entries[i].title);

                // Check if data is valid UTF-8
                if (is_valid_utf8(parsed->meta->entries[i].data->data, parsed->meta->entries[i].data->size)) {
                    printf("    \"type\": \"string\",\n");
                    printf("    \"value\": \"");
                    for (size_t j = 0; j < parsed->meta->entries[i].data->size; j++) {
                        char c = parsed->meta->entries[i].data->data[j];
                        if (c == '"') printf("\\\"");
                        else if (c == '\\') printf("\\\\");
                        else if (c == '\n') printf("\\n");
                        else if (c == '\r') printf("\\r");
                        else if (c == '\t') printf("\\t");
                        else if (c >= 32 && c < 127) printf("%c", c);
                        else printf("\\u%04x", (unsigned char)c);
                    }
                    printf("\"\n");
                } else {
                    printf("    \"type\": \"base64\",\n");
                    char* encoded = base64_encode(parsed->meta->entries[i].data->data, parsed->meta->entries[i].data->size);
                    printf("    \"value\": \"%s\"\n", encoded);
                    free(encoded);
                }

                if (i < parsed->meta->count - 1) {
                    printf("  },\n");
                } else {
                    printf("  }\n");
                }
            }
        }
        printf("]\n");

        // Save image if present and output file specified
        if (parsed->img && parsed->img->size > 0 && params->output && strlen(params->output) > 0) {
            const char* ext = get_image_extension(parsed->img->data, parsed->img->size);
            char img_filename[512];
            snprintf(img_filename, sizeof(img_filename), "%s.%s", params->output, ext);

            FILE* img_file = fopen(img_filename, "wb");
            if (img_file) {
                fwrite(parsed->img->data, 1, parsed->img->size, img_file);
                fclose(img_file);
            }
        }

        parser_result_free(parsed);
        fclose(rfile);
        return;
    }

    // For modification actions, prepare new metadata
    metadata_vec* meta_new = metadata_vec_new();
    vec_u8* img_new = NULL;

    if (strcmp(metaaction, "add") == 0) {
        // Add existing metadata first
        if (parsed->meta) {
            for (size_t i = 0; i < parsed->meta->count; i++) {
                metadata_vec_push(meta_new, parsed->meta->entries[i].title, parsed->meta->entries[i].data);
            }
        }
        // Add new metadata from params
        for (int i = 0; i < params->meta_count; i++) {
            if (params->meta[i][0] && params->meta[i][1]) {
                vec_u8* data = vec_u8_new(strlen(params->meta[i][1]));
                for (size_t j = 0; j < strlen(params->meta[i][1]); j++) {
                    vec_u8_push(data, params->meta[i][1][j]);
                }
                metadata_vec_push(meta_new, params->meta[i][0], data);
                vec_u8_free(data);
            }
        }
        // Keep existing image or add new one
        if (params->image_path && strlen(params->image_path) > 0) {
            FILE* img_file = fopen(params->image_path, "rb");
            if (img_file) {
                fseek(img_file, 0, SEEK_END);
                size_t img_size = ftell(img_file);
                fseek(img_file, 0, SEEK_SET);

                img_new = vec_u8_new(img_size);
                uint8_t* img_buf = malloc(img_size);
                fread(img_buf, 1, img_size, img_file);
                for (size_t i = 0; i < img_size; i++) {
                    vec_u8_push(img_new, img_buf[i]);
                }
                free(img_buf);
                fclose(img_file);
            } else {
                fprintf(stderr, "Image not found\n");
            }
        } else if (parsed->img && parsed->img->size > 0) {
            img_new = vec_u8_new(parsed->img->size);
            for (size_t i = 0; i < parsed->img->size; i++) {
                vec_u8_push(img_new, parsed->img->data[i]);
            }
        }
    } else if (strcmp(metaaction, "remove") == 0) {
        // Remove specified metadata keys
        if (parsed->meta) {
            for (size_t i = 0; i < parsed->meta->count; i++) {
                bool should_remove = false;
                for (int j = 0; j < params->meta_count; j++) {
                    if (params->meta[j][0] && strcmp(parsed->meta->entries[i].title, params->meta[j][0]) == 0) {
                        should_remove = true;
                        break;
                    }
                }
                if (!should_remove) {
                    metadata_vec_push(meta_new, parsed->meta->entries[i].title, parsed->meta->entries[i].data);
                }
            }
        }
        // Keep existing image
        if (parsed->img && parsed->img->size > 0) {
            img_new = vec_u8_new(parsed->img->size);
            for (size_t i = 0; i < parsed->img->size; i++) {
                vec_u8_push(img_new, parsed->img->data[i]);
            }
        }
    } else if (strcmp(metaaction, "rmimg") == 0) {
        // Keep metadata but remove image
        if (parsed->meta) {
            for (size_t i = 0; i < parsed->meta->count; i++) {
                metadata_vec_push(meta_new, parsed->meta->entries[i].title, parsed->meta->entries[i].data);
            }
        }
        // img_new stays NULL
    } else if (strcmp(metaaction, "overwrite") == 0 || strcmp(metaaction, "set") == 0) {
        // Replace all metadata
        for (int i = 0; i < params->meta_count; i++) {
            if (params->meta[i][0] && params->meta[i][1]) {
                vec_u8* data = vec_u8_new(strlen(params->meta[i][1]));
                for (size_t j = 0; j < strlen(params->meta[i][1]); j++) {
                    vec_u8_push(data, params->meta[i][1][j]);
                }
                metadata_vec_push(meta_new, params->meta[i][0], data);
                vec_u8_free(data);
            }
        }
        // Set new image if provided
        if (params->image_path && strlen(params->image_path) > 0) {
            FILE* img_file = fopen(params->image_path, "rb");
            if (img_file) {
                fseek(img_file, 0, SEEK_END);
                size_t img_size = ftell(img_file);
                fseek(img_file, 0, SEEK_SET);

                img_new = vec_u8_new(img_size);
                uint8_t* img_buf = malloc(img_size);
                fread(img_buf, 1, img_size, img_file);
                for (size_t i = 0; i < img_size; i++) {
                    vec_u8_push(img_new, img_buf[i]);
                }
                free(img_buf);
                fclose(img_file);
            }
        }
    } else {
        fprintf(stderr, "Invalid modification type: %s\n", metaaction);
        metadata_vec_free(meta_new);
        parser_result_free(parsed);
        fclose(rfile);
        return;
    }

    // Build new header
    vec_u8* head_new = head_builder(meta_new, img_new, parsed->itype);

    // Create temporary file
    char temp_filename[512];
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", input);
    FILE* temp_file = fopen(temp_filename, "wb");
    if (!temp_file) {
        fprintf(stderr, "Failed to create temporary file\n");
        metadata_vec_free(meta_new);
        vec_u8_free(img_new);
        vec_u8_free(head_new);
        parser_result_free(parsed);
        fclose(rfile);
        return;
    }

    // Copy audio data to temp file
    move_all(rfile, temp_file, 16777216);
    fclose(rfile);
    fclose(temp_file);

    // Write new file
    FILE* wfile = fopen(input, "wb");
    if (!wfile) {
        fprintf(stderr, "Failed to open output file for writing\n");
        metadata_vec_free(meta_new);
        vec_u8_free(img_new);
        vec_u8_free(head_new);
        parser_result_free(parsed);
        remove(temp_filename);
        return;
    }

    // Write new header
    fwrite(head_new->data, 1, head_new->size, wfile);

    // Copy audio data from temp file
    temp_file = fopen(temp_filename, "rb");
    if (temp_file) {
        move_all(temp_file, wfile, 16777216);
        fclose(temp_file);
        remove(temp_filename);
    }

    fclose(wfile);

    // Cleanup
    metadata_vec_free(meta_new);
    vec_u8_free(img_new);
    vec_u8_free(head_new);
    parser_result_free(parsed);
}