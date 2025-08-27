#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <unistd.h>
#include "libfrad/encoder.h"
#include "tools/pcmproc.h"
#include "tools/cli.h"
#include "tools/process.h"
#include "app_common.h"

static void logging_encode(uint8_t loglevel, process_info_t* log, bool linefeed) {
    if (loglevel == 0) return;

    // Format each value separately to avoid static buffer conflicts
    char size_str[64], time_str[64], bitrate_str[64];
    snprintf(size_str, sizeof(size_str), "%s", format_si(process_info_get_total_size(log)));
    snprintf(time_str, sizeof(time_str), "%s", format_time(process_info_get_duration(log)));
    snprintf(bitrate_str, sizeof(bitrate_str), "%s", format_si(process_info_get_bitrate(log)));

    fprintf(stderr, "size=%sB time=%s bitrate=%sbit/s speed=%.2fx    ",
            size_str, time_str, bitrate_str,
            process_info_get_speed(log));

    if (linefeed) {
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "\r");
    }
}

void encode(const char* input, CliParams* params) {
    // Handle pipe input
    FILE* in_file = NULL;
    bool is_stdin = is_pipe_in(input);

    if (is_stdin) {
        in_file = stdin;
    } else {
        in_file = fopen(input, "rb");
        if (!in_file) {
            fprintf(stderr, "Error: Cannot open input file '%s'\n", input);
            return;
        }
    }

    // Create encoder parameters
    encoder_params_t enc_params;
    enc_params.profile = params->profile;
    enc_params.srate = params->srate;
    enc_params.channels = params->channels;
    enc_params.bit_depth = params->bits;
    enc_params.frame_size = params->frame_size;

    // Use PCM format from params (set by -f option)
    PCMFormat pcm_format = params->pcm;

    // If bits wasn't set explicitly, use a default FrAD encoding bit depth
    if (params->bits == 0) {
        params->bits = 16;  // Default FrAD encoding bit depth
        enc_params.bit_depth = params->bits;
    }

    // Create encoder (no longer needs PCM format)
    encoder_t* encoder = encoder_new(&enc_params);
    if (!encoder) {
        fprintf(stderr, "Error: Failed to create encoder\n");
        fclose(in_file);
        return;
    }
    
    // Create PCM processor for converting input to f64
    PCMProcessor* pcm_processor = pcm_processor_new(pcm_format);
    if (!pcm_processor) {
        fprintf(stderr, "Error: Failed to create PCM processor\n");
        encoder_free(encoder);
        fclose(in_file);
        return;
    }

    // Set encoder options
    encoder_set_ecc(encoder, params->enable_ecc, params->ecc_ratio[0], params->ecc_ratio[1]);
    encoder_set_little_endian(encoder, params->little_endian);
    double loss_level = pow(1.25, params->losslevel) / 19.0 + 0.5;
    encoder_set_loss_level(encoder, loss_level);
    encoder_set_overlap_ratio(encoder, params->overlap_ratio);

    // Set output filename if not specified
    char* output_file = params->output;
    char* auto_output = NULL;
    if (!output_file || strlen(output_file) == 0) {
        // Generate output filename from input filename
        if (!is_stdin) {
            const char* ext = ".frad";
            size_t input_len = strlen(input);
            size_t ext_len = strlen(ext);
            auto_output = malloc(input_len + ext_len + 1);
            strcpy(auto_output, input);

            // Remove existing extension if it's a known audio extension
            char* dot = strrchr(auto_output, '.');
            if (dot && (strcasecmp(dot, ".pcm") == 0 || strcasecmp(dot, ".raw") == 0 ||
                       strcasecmp(dot, ".wav") == 0 || strcasecmp(dot, ".aiff") == 0)) {
                *dot = '\0';
            }
            strcat(auto_output, ext);
            output_file = auto_output;
        } else {
            output_file = "output.frad";
        }
    }

    // Handle pipe output
    FILE* out_file = NULL;
    bool is_stdout = is_pipe_out(output_file);

    if (is_stdout) {
        out_file = stdout;
    } else {
        // Check for overwrite
        check_overwrite(output_file, params->overwrite);

        // Check for same file
        if (!is_stdin && is_same_file(input, output_file)) {
            fprintf(stderr, "Error: Input and output files cannot be the same\n");
            encoder_free(encoder);
            if (!is_stdin) fclose(in_file);
            if (auto_output) free(auto_output);
            return;
        }

        out_file = fopen(output_file, "wb");
        if (!out_file) {
            fprintf(stderr, "Error: Cannot create output file '%s'\n", output_file);
            encoder_free(encoder);
            if (!is_stdin) fclose(in_file);
            if (auto_output) free(auto_output);
            return;
        }
    }

    // Write FrAD header
    const char* frad_header = "fRad";
    fwrite(frad_header, 1, 4, out_file);

    // Process input in chunks
    size_t chunk_size = 32768;  // Fixed chunk size like Rust version
    uint8_t* buffer = malloc(chunk_size);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(out_file);
        pcm_processor_free(pcm_processor);
        encoder_free(encoder);
        fclose(in_file);
        return;
    }

    // Create process info for logging
    process_info_t* procinfo = process_info_new();
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, chunk_size, in_file)) > 0) {
        // Convert PCM bytes to f64 samples
        size_t sample_count;
        double* samples = pcm_processor_into_f64(pcm_processor, buffer, bytes_read, &sample_count);
        
        if (samples && sample_count > 0) {
            encode_result_t* result = encoder_process(encoder, samples, sample_count);
            free(samples);
            
            if (result) {
                if (result->data && result->data->size > 0) {
                    fwrite(result->data->data, 1, result->data->size, out_file);
                    process_info_update(procinfo, result->data->size, result->samples, enc_params.srate);
                }
                encode_result_free(result);
            }
        }
        logging_encode(params->loglevel, procinfo, false);
    }

    // Flush encoder
    encode_result_t* result = encoder_flush(encoder);
    if (result) {
        if (result->data && result->data->size > 0) {
            fwrite(result->data->data, 1, result->data->size, out_file);
            process_info_update(procinfo, result->data->size, result->samples, enc_params.srate);
        }
        encode_result_free(result);
    }
    logging_encode(params->loglevel, procinfo, true);

    // Cleanup
    process_info_free(procinfo);
    free(buffer);
    if (!is_stdout) fclose(out_file);
    pcm_processor_free(pcm_processor);
    encoder_free(encoder);
    if (!is_stdin) fclose(in_file);
    if (auto_output) free(auto_output);
}