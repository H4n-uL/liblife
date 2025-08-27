#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "libfrad/decoder.h"
#include "tools/pcmproc.h"
#include "tools/cli.h"
#include "tools/audio.h"
#include "tools/process.h"
#include "app_common.h"

static void logging_decode(uint8_t loglevel, process_info_t* log, bool linefeed, const ASFH* asfh) {
    if (loglevel == 0) return;

    int line_count = 0;

    // Format each value separately to avoid static buffer conflicts
    char size_str[64], time_str[64], bitrate_str[64];
    snprintf(size_str, sizeof(size_str), "%s", format_si(process_info_get_total_size(log)));
    snprintf(time_str, sizeof(time_str), "%s", format_time(process_info_get_duration(log)));
    snprintf(bitrate_str, sizeof(bitrate_str), "%s", format_si(process_info_get_bitrate(log)));

    // First line - basic info
    fprintf(stderr, "size=%sB time=%s bitrate=%sbit/s speed=%.2fx    ",
            size_str, time_str, bitrate_str,
            process_info_get_speed(log));

    // Additional info at higher log levels (on new line)
    if (loglevel > 1 && asfh) {
        fprintf(stderr, "\nProfile %d, %dch@%dHz, ECC=%s    ",
                asfh->profile, asfh->channels, asfh->srate,
                asfh->ecc ? "enabled" : "disabled");
        line_count++;
    }

    if (linefeed) {
        fprintf(stderr, "\n");
    } else {
        // Move cursor back up for multi-line output
        for (int i = 0; i < line_count; i++) {
            fprintf(stderr, "\x1b[1A");  // Move up one line
        }
        fprintf(stderr, "\r");  // Return to beginning of line
    }
}

void decode(const char* input, CliParams* params, bool play) {
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

    // Create decoder - assume fix_errors is true if ECC is enabled
    decoder_t* decoder = decoder_new(params->enable_ecc);
    if (!decoder) {
        fprintf(stderr, "Error: Failed to create decoder\n");
        fclose(in_file);
        return;
    }
    
    // Create PCM processor for converting f64 to output format
    PCMProcessor* pcm_processor = pcm_processor_new(params->pcm);
    if (!pcm_processor) {
        fprintf(stderr, "Error: Failed to create PCM processor\n");
        decoder_free(decoder);
        fclose(in_file);
        return;
    }

    // Set output filename if not specified
    char* output_file = params->output;
    char* auto_output = NULL;
    if (!play && (!output_file || strlen(output_file) == 0)) {
        // Generate output filename from input filename
        if (!is_stdin) {
            const char* ext = ".pcm";
            size_t input_len = strlen(input);
            size_t ext_len = strlen(ext);
            auto_output = malloc(input_len + ext_len + 1);
            strcpy(auto_output, input);

            // Remove existing extension if it's a known FrAD extension
            char* dot = strrchr(auto_output, '.');
            if (dot && (strcasecmp(dot, ".frad") == 0 || strcasecmp(dot, ".fra") == 0 ||
                       strcasecmp(dot, ".dsin") == 0 || strcasecmp(dot, ".dsn") == 0)) {
                *dot = '\0';
            }
            strcat(auto_output, ext);
            output_file = auto_output;
        } else {
            output_file = "output.pcm";
        }
    }

    FILE* out_file = NULL;
    bool is_stdout = false;
    if (!play) {
        // Handle pipe output
        is_stdout = is_pipe_out(output_file);

        if (is_stdout) {
            out_file = stdout;
        } else {
            // Check for overwrite
            check_overwrite(output_file, params->overwrite);

            // Check for same file
            if (!is_stdin && is_same_file(input, output_file)) {
                fprintf(stderr, "Error: Input and output files cannot be the same\n");
                decoder_free(decoder);
                if (!is_stdin) fclose(in_file);
                if (auto_output) free(auto_output);
                return;
            }

            out_file = fopen(output_file, "wb");
            if (!out_file) {
                fprintf(stderr, "Error: Cannot create output file '%s'\n", output_file);
                decoder_free(decoder);
                if (!is_stdin) fclose(in_file);
                if (auto_output) free(auto_output);
                return;
            }
        }
    }

    // Initialize audio player if needed
    audio_player_t* audio_player = NULL;
    bool audio_initialized = false;

    // Create process info for logging
    process_info_t* procinfo = process_info_new();

    // Read and process input
    uint8_t buffer[32768];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), in_file)) > 0) {
        decode_result_t* result = decoder_process(decoder, buffer, bytes_read);
        if (result) {
            if (result->pcm && result->pcm->size > 0) {
                // Update process info (samples = pcm->size / channels)
                size_t samples = result->pcm->size / (result->channels > 0 ? result->channels : 1);
                process_info_update(procinfo, bytes_read, samples, result->srate);

                if (play) {
                    // Initialize audio player on first frame
                    if (!audio_initialized && result->channels > 0 && result->srate > 0) {
                        audio_player = audio_player_new(result->channels, result->srate);
                        if (audio_player) {
                            audio_player_set_speed(audio_player, params->speed);
                            audio_initialized = true;
                        } else {
                            play = false;  // Fall back to file output
                        }
                    }

                    // Queue samples for playback
                    if (audio_player) {
                        audio_player_queue(audio_player, result->pcm->data, result->pcm->size);
                    }
                } else {
                    // Convert float64 PCM to output format
                    size_t byte_count;
                    uint8_t* bytes = pcm_processor_from_f64(pcm_processor, result->pcm->data, result->pcm->size, &byte_count);
                    if (bytes) {
                        fwrite(bytes, 1, byte_count, out_file);
                        free(bytes);
                    }
                }
            }

            decode_result_free(result);
        }
        logging_decode(params->loglevel, procinfo, false, decoder_get_asfh(decoder));
    }

    // Flush decoder
    decode_result_t* result = decoder_flush(decoder);
    if (result) {
        if (result->pcm && result->pcm->size > 0) {
            size_t samples = result->pcm->size / (result->channels > 0 ? result->channels : 1);
            process_info_update(procinfo, 0, samples, result->srate);

            if (play && audio_player) {
                audio_player_queue(audio_player, result->pcm->data, result->pcm->size);
            } else if (!play) {
                size_t byte_count;
                uint8_t* bytes = pcm_processor_from_f64(pcm_processor, result->pcm->data, result->pcm->size, &byte_count);
                if (bytes) {
                    fwrite(bytes, 1, byte_count, out_file);
                    free(bytes);
                }
            }
        }
        decode_result_free(result);
    }
    logging_decode(params->loglevel, procinfo, true, decoder_get_asfh(decoder));

    // Wait for audio playback to finish
    if (audio_player) {
        audio_player_wait(audio_player);
        audio_player_free(audio_player);
    }

    // Cleanup
    process_info_free(procinfo);
    if (out_file && !is_stdout) {
        fclose(out_file);
    }
    pcm_processor_free(pcm_processor);
    decoder_free(decoder);
    if (!is_stdin) fclose(in_file);
    if (auto_output) free(auto_output);
}