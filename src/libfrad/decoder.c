#include "decoder.h"
#include "tools/asfh.h"
#include "tools/ecc/ecc.h"
#include "backend/backend.h"
#include "fourier/profile0.h"
#include "fourier/profile1.h"
#include "fourier/profile2.h"
#include "fourier/profile4.h"
#include "common.h"
#include <stdlib.h>
#include <string.h>

struct decoder {
    ASFH* asfh;
    ASFH* info;
    vec_u8* buffer;
    vec_f64* overlap_fragment;
    bool fix_error;
    bool broken_frame;
};

// Apply overlap to the decoded PCM (implementation of Rust version)
static vec_f64* overlap(decoder_t* dec, vec_f64* frame) {
    size_t channels = (dec->asfh->channels > 0) ? dec->asfh->channels : 1;

    // 1. If overlap buffer not empty, apply Forward linear overlap-add
    if (dec->overlap_fragment && dec->overlap_fragment->size > 0) {
        size_t overlap_len = dec->overlap_fragment->size / channels;
        size_t frame_samples = frame->size / channels;
        size_t actual_overlap_len = (overlap_len < frame_samples) ? overlap_len : frame_samples;

        vec_f64* fade_in = hanning_in_overlap(actual_overlap_len);

        for (size_t i = 0; i < actual_overlap_len; i++) {
            for (size_t j = 0; j < channels; j++) {
                size_t idx = i * channels + j;
                frame->data[idx] *= fade_in->data[i];
                frame->data[idx] += dec->overlap_fragment->data[idx] * fade_in->data[fade_in->size - i - 1];
            }
        }

        vec_f64_free(fade_in);
    }

    // 2. If COMPACT profile and overlap is enabled, split this frame
    vec_f64* next_overlap = vec_f64_new(0);
    if ((dec->asfh->profile == 1 || dec->asfh->profile == 2) && dec->asfh->overlap_ratio != 0) {
        size_t overlap_ratio = dec->asfh->overlap_ratio;
        // Samples * (Overlap ratio - 1) / Overlap ratio
        // e.g., ([2048], overlap_ratio=16) -> [1920, 128]
        size_t frame_cutout = (frame->size / channels) * (overlap_ratio - 1) / overlap_ratio;

        // Copy the end portion to next_overlap
        for (size_t i = frame_cutout * channels; i < frame->size; i++) {
            vec_f64_push(next_overlap, frame->data[i]);
        }

        // Truncate frame
        frame->size = frame_cutout * channels;
    }

    // Replace overlap fragment
    vec_f64_free(dec->overlap_fragment);
    dec->overlap_fragment = next_overlap;

    return frame;
}

decoder_t* decoder_new(bool fix_error) {
    decoder_t* dec = calloc(1, sizeof(decoder_t));
    if (!dec) return NULL;

    dec->asfh = asfh_new();
    dec->info = asfh_new();
    dec->buffer = vec_u8_new(0);
    dec->overlap_fragment = vec_f64_new(0);

    if (!dec->asfh || !dec->info || !dec->buffer || !dec->overlap_fragment) {
        decoder_free(dec);
        return NULL;
    }

    dec->fix_error = fix_error;
    dec->broken_frame = false;

    return dec;
}

void decoder_free(decoder_t* dec) {
    if (!dec) return;

    if (dec->asfh) asfh_free(dec->asfh);
    if (dec->info) asfh_free(dec->info);
    if (dec->buffer) vec_u8_free(dec->buffer);
    if (dec->overlap_fragment) vec_f64_free(dec->overlap_fragment);
    free(dec);
}

decode_result_t* decoder_process(decoder_t* dec, const uint8_t* stream, size_t stream_len) {
    if (!dec) return NULL;

    // Extend buffer with new stream data
    if (stream && stream_len > 0) {
        for (size_t i = 0; i < stream_len; i++) {
            vec_u8_push(dec->buffer, stream[i]);
        }
    }

    vec_f64* ret_pcm = vec_f64_new(0);
    size_t frames = 0;
    bool crit = false;

    while (true) {
        // If every parameter in the ASFH struct is set
        /* 1. Decoding FrAD Frame */
        if (dec->asfh->all_set) {
            // 1.0. If the buffer is not enough to decode the frame, break
            // 1.0.1. If the stream is empty while ASFH is set (which means broken frame), break
            dec->broken_frame = false;
            if (dec->buffer->size < dec->asfh->frmbytes) {
                if (!stream || stream_len == 0) { dec->broken_frame = true; }
                break;
            }

            // 1.1. Split out the frame data
            vec_u8* frad = vec_u8_split_front(dec->buffer, (size_t)dec->asfh->frmbytes);

            // 1.2. Correct the error if ECC is enabled
            if (dec->asfh->ecc) {
                bool repair = dec->fix_error && (
                    // and if CRC mismatch
                    ((dec->asfh->profile == 0 || dec->asfh->profile == 4) && frad_crc32(0, frad->data, frad->size) != dec->asfh->crc32) ||
                    ((dec->asfh->profile == 1 || dec->asfh->profile == 2) && crc16_ansi(0, frad->data, frad->size) != dec->asfh->crc16)
                );
                vec_u8* corrected = ecc_decode(frad, dec->asfh->ecc_ratio, repair);
                vec_u8_free(frad);
                frad = corrected;
            }

            // 1.3. Decode the FrAD frame
            vec_f64* pcm = NULL;
            switch (dec->asfh->profile) {
                case 1:
                    pcm = profile1_digital(frad->data, frad->size, dec->asfh->bit_depth_index,
                                          dec->asfh->channels, dec->asfh->srate, dec->asfh->fsize, dec->asfh->endian);
                    break;
                case 2:
                    pcm = profile2_digital(frad->data, frad->size, dec->asfh->bit_depth_index,
                                          dec->asfh->channels, dec->asfh->srate, dec->asfh->fsize, dec->asfh->endian);
                    break;
                case 4:
                    pcm = profile4_digital(frad->data, frad->size, dec->asfh->bit_depth_index,
                                          dec->asfh->channels, dec->asfh->endian);
                    break;
                default: // Profile 0
                    pcm = profile0_digital(frad->data, frad->size, dec->asfh->bit_depth_index,
                                          dec->asfh->channels, dec->asfh->endian);
                    break;
            }

            vec_u8_free(frad);

            if (pcm) {
                // 1.4. Apply overlap
                pcm = overlap(dec, pcm);

                // 1.5. Append the decoded PCM and clear header
                for (size_t i = 0; i < pcm->size; i++) {
                    vec_f64_push(ret_pcm, pcm->data[i]);
                }
                vec_f64_free(pcm);
                frames++;
            }

            asfh_clear(dec->asfh);
        }

        /* 2. Finding header / Gathering more data to parse */
        else {
            // 2.1. If the header buffer not found, find the header buffer
            if (dec->asfh->buffer->size < 4 ||
                memcmp(dec->asfh->buffer->data, FRM_SIGN, 4) != 0) {

                int pattern_pos = vec_u8_find_pattern(dec->buffer, FRM_SIGN, 4);
                if (pattern_pos >= 0) {
                    // 2.1.1. Split out the buffer to the header buffer
                    vec_u8* discarded = vec_u8_split_front(dec->buffer, pattern_pos);
                    vec_u8_free(discarded);

                    // Take the FRM_SIGN bytes and put them in ASFH buffer
                    vec_u8* sign_bytes = vec_u8_split_front(dec->buffer, 4);
                    if (sign_bytes) {
                        vec_u8_free(dec->asfh->buffer);
                        dec->asfh->buffer = sign_bytes;
                    }
                } else {
                    // 2.1.2. else, Split out the buffer to the last 3 bytes and return
                    if (dec->buffer->size > 3) {
                        vec_u8_free(vec_u8_split_front(dec->buffer, dec->buffer->size - 3));
                    }
                    break;
                }
            }

            // 2.2. If header buffer found, try parsing the header
            ParseResult header_result = asfh_parse(dec->asfh, dec->buffer);

            // 2.3. Check header parsing result
            switch (header_result) {
                // 2.3.1. If header is complete and not forced to flush, continue
                case PARSE_COMPLETE:
                    // 2.3.1.1. If any critical parameter has changed, flush the overlap buffer
                    if (!asfh_criteq(dec->asfh, dec->info)) {
                        uint32_t old_srate = dec->info->srate;
                        uint16_t old_channels = dec->info->channels;

                        // Copy current ASFH to info
                        dec->info->channels = dec->asfh->channels;
                        dec->info->srate = dec->asfh->srate;

                        if (old_srate != 0 || old_channels != 0) { // If the info struct is not empty
                            // Flush the overlap buffer and return with critical flag
                            for (size_t i = 0; i < dec->overlap_fragment->size; i++) {
                                vec_f64_push(ret_pcm, dec->overlap_fragment->data[i]);
                            }
                            dec->overlap_fragment->size = 0;

                            decode_result_t* result = calloc(1, sizeof(decode_result_t));
                            if (result) {
                                result->pcm = ret_pcm;
                                result->channels = old_channels;
                                result->srate = old_srate;
                                result->frames = frames;
                                result->crit = true;
                            }
                            return result;
                        }
                    }
                    break;

                // 2.3.2. If header is complete and forced to flush, flush and return
                case PARSE_FORCE_FLUSH:
                    for (size_t i = 0; i < dec->overlap_fragment->size; i++) {
                        vec_f64_push(ret_pcm, dec->overlap_fragment->data[i]);
                    }
                    dec->overlap_fragment->size = 0;
                    goto end_loop;

                // 2.3.3. If header is incomplete, return
                case PARSE_INCOMPLETE:
                    goto end_loop;
            }
        }
    }

end_loop:
    ;
    decode_result_t* result = calloc(1, sizeof(decode_result_t));
    if (result) {
        result->pcm = ret_pcm;
        result->channels = dec->asfh->channels;
        result->srate = dec->asfh->srate;
        result->frames = frames;
        result->crit = crit;
    } else {
        vec_f64_free(ret_pcm);
    }

    return result;
}

decode_result_t* decoder_flush(decoder_t* dec) {
    if (!dec) return NULL;

    decode_result_t* result = calloc(1, sizeof(decode_result_t));
    if (!result) return NULL;

    // Extract the overlap buffer
    result->pcm = vec_f64_new(dec->overlap_fragment->size);
    for (size_t i = 0; i < dec->overlap_fragment->size; i++) {
        vec_f64_push(result->pcm, dec->overlap_fragment->data[i]);
    }

    result->channels = dec->asfh->channels;
    result->srate = dec->asfh->srate;
    result->frames = 0;
    result->crit = true;

    // Clear the overlap buffer and ASFH struct
    dec->overlap_fragment->size = 0;
    asfh_clear(dec->asfh);

    return result;
}

void decode_result_free(decode_result_t* result) {
    if (!result) return;

    if (result->pcm) vec_f64_free(result->pcm);
    free(result);
}

bool decoder_is_empty(decoder_t* dec) {
    if (!dec || !dec->buffer) return true;
    return dec->buffer->size < 4 || dec->broken_frame;
}

const ASFH* decoder_get_asfh(decoder_t* dec) {
    if (!dec) return NULL;
    return dec->asfh;
}