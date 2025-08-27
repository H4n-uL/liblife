#include "repairer.h"
#include "common.h"
#include "tools/asfh.h"
#include "tools/ecc/ecc.h"
#include "backend/backend.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Profile groups (from profiles.h)
static const uint8_t LOSSLESS[] = {0, 4};
static const uint8_t COMPACT[] = {1, 2};

static bool is_lossless(uint8_t profile) {
    for (size_t i = 0; i < sizeof(LOSSLESS); i++) {
        if (LOSSLESS[i] == profile) return true;
    }
    return false;
}

static bool is_compact(uint8_t profile) {
    for (size_t i = 0; i < sizeof(COMPACT); i++) {
        if (COMPACT[i] == profile) return true;
    }
    return false;
}

struct repairer {
    ASFH* asfh;
    vec_u8* buffer;
    uint8_t ecc_ratio[2];
    bool broken_frame;
};

repairer_t* repairer_new(uint8_t data_size, uint8_t check_size) {
    repairer_t* ctx = (repairer_t*)malloc(sizeof(repairer_t));
    if (!ctx) return NULL;

    // Validate ECC parameters
    if (data_size == 0) {
        fprintf(stderr, "ECC data size must not be zero\n");
        fprintf(stderr, "Setting ECC to default 96 24\n");
        data_size = 96;
        check_size = 24;
    }

    if ((uint16_t)data_size + (uint16_t)check_size > 255) {
        fprintf(stderr, "ECC data size and check size must not exceed 255, given: %u and %u\n",
                data_size, check_size);
        fprintf(stderr, "Setting ECC to default 96 24\n");
        data_size = 96;
        check_size = 24;
    }

    ctx->asfh = asfh_new();
    ctx->buffer = vec_u8_new(0);
    ctx->ecc_ratio[0] = data_size;
    ctx->ecc_ratio[1] = check_size;
    ctx->broken_frame = false;

    if (!ctx->asfh || !ctx->buffer) {
        repairer_free(ctx);
        return NULL;
    }

    return ctx;
}

void repairer_free(repairer_t* ctx) {
    if (ctx) {
        asfh_free(ctx->asfh);
        vec_u8_free(ctx->buffer);
        free(ctx);
    }
}

bool repairer_is_empty(repairer_t* ctx) {
    if (!ctx) return true;
    return ctx->buffer->size < 4 || ctx->broken_frame;
}

const ASFH* repairer_get_asfh(repairer_t* ctx) {
    return ctx ? ctx->asfh : NULL;
}

vec_u8* repairer_process(repairer_t* ctx, const uint8_t* stream, size_t stream_len) {
    if (!ctx || !stream) return vec_u8_new(0);

    // Extend buffer with input stream
    for (size_t i = 0; i < stream_len; i++) {
        vec_u8_push(ctx->buffer, stream[i]);
    }

    vec_u8* ret = vec_u8_new(0);
    if (!ret) return NULL;

    while (1) {
        // 1. If every parameter in the ASFH struct is set, repair the frame
        if (ctx->asfh->all_set) {
            // 1.0. Check if we have enough data
            // 1.0.1. If the stream is empty while ASFH is set (which means broken frame), break
            if (ctx->buffer->size < ctx->asfh->frmbytes) {
                if (stream_len == 0) { ctx->broken_frame = true; }
                break;
            }
            ctx->broken_frame = false;

            // 1.1. Split out the frame data
            vec_u8* frad = vec_u8_split_front(ctx->buffer, ctx->asfh->frmbytes);
            if (!frad) break;

            // 1.2. Correct the error if ECC is enabled
            if (ctx->asfh->ecc) {
                // Check if CRC mismatch (matching Rust's || logic)
                bool repair = (is_lossless(ctx->asfh->profile) &&
                              frad_crc32(0, frad->data, frad->size) != ctx->asfh->crc32) ||
                             (is_compact(ctx->asfh->profile) &&
                              crc16_ansi(0, frad->data, frad->size) != ctx->asfh->crc16);

                // Always decode with repair flag
                vec_u8* decoded = ecc_decode(frad, ctx->asfh->ecc_ratio, repair);
                if (decoded) {
                    vec_u8_free(frad);
                    frad = decoded;
                }
            }

            // 1.3. Create Reed-Solomon error correction code with new ratio
            vec_u8* encoded = ecc_encode(frad, ctx->ecc_ratio);
            vec_u8_free(frad);

            if (!encoded) break;

            // Update ASFH with new ECC parameters
            ctx->asfh->ecc = true;
            ctx->asfh->ecc_ratio[0] = ctx->ecc_ratio[0];
            ctx->asfh->ecc_ratio[1] = ctx->ecc_ratio[1];

            // 1.4. Write the frame data to the output
            vec_u8* frame_output = asfh_write(ctx->asfh, encoded);
            vec_u8_free(encoded);

            if (frame_output) {
                for (size_t i = 0; i < frame_output->size; i++) {
                    vec_u8_push(ret, frame_output->data[i]);
                }
                vec_u8_free(frame_output);
            }

            // 1.5. Clear the ASFH struct
            asfh_clear(ctx->asfh);
        }
        // 2. Finding header / Gathering more data to parse
        else {
            // 2.1. If the header buffer not found, find the header buffer
            vec_u8* asfh_buffer = ctx->asfh->buffer;
            bool starts_with_sign = (asfh_buffer->size >= 4 &&
                                    memcmp(asfh_buffer->data, FRM_SIGN, 4) == 0);

            if (!starts_with_sign) {
                int pattern_idx = vec_u8_find_pattern(ctx->buffer, FRM_SIGN, 4);

                if (pattern_idx >= 0) {
                    // 2.1.1. Split out the buffer to the header buffer
                    vec_u8* prefix = vec_u8_split_front(ctx->buffer, pattern_idx);
                    if (prefix) {
                        for (size_t i = 0; i < prefix->size; i++) {
                            vec_u8_push(ret, prefix->data[i]);
                        }
                        vec_u8_free(prefix);
                    }

                    // Get the frame signature
                    vec_u8* sign = vec_u8_split_front(ctx->buffer, 4);
                    if (sign) {
                        // Replace asfh buffer with new sign
                        vec_u8_free(ctx->asfh->buffer);
                        ctx->asfh->buffer = sign;
                    }
                } else {
                    // 2.1.2. Split out the buffer to the last 3 bytes and return
                    size_t split_size = ctx->buffer->size > 3 ?
                                       ctx->buffer->size - 3 : 0;

                    vec_u8* prefix = vec_u8_split_front(ctx->buffer, split_size);
                    if (prefix) {
                        for (size_t i = 0; i < prefix->size; i++) {
                            vec_u8_push(ret, prefix->data[i]);
                        }
                        vec_u8_free(prefix);
                    }
                    break;
                }
            }

            // 2.2. If header buffer found, try parsing the header
            ParseResult parse_result = asfh_parse(ctx->asfh, ctx->buffer);

            // 2.3. Check header parsing result
            switch (parse_result) {
                case PARSE_COMPLETE:
                    // 2.3.1. If header is complete and not forced to flush, continue
                    break;

                case PARSE_FORCE_FLUSH:
                    // 2.3.2. If header is complete and forced to flush, flush and return
                    {
                        vec_u8* flush_data = asfh_force_flush(ctx->asfh);
                        if (flush_data) {
                            for (size_t i = 0; i < flush_data->size; i++) {
                                vec_u8_push(ret, flush_data->data[i]);
                            }
                            vec_u8_free(flush_data);
                        }
                    }
                    goto exit_loop;

                case PARSE_INCOMPLETE:
                    // 2.3.3. If header is incomplete, return
                    goto exit_loop;
            }
        }
    }

exit_loop:
    return ret;
}

vec_u8* repairer_flush(repairer_t* ctx) {
    if (!ctx || !ctx->buffer) return vec_u8_new(0);

    vec_u8* ret = vec_u8_new(ctx->buffer->size);
    if (!ret) return NULL;

    for (size_t i = 0; i < ctx->buffer->size; i++) {
        vec_u8_push(ret, ctx->buffer->data[i]);
    }

    ctx->buffer->size = 0;
    return ret;
}