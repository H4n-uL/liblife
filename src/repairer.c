#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <time.h>
#include "libfrad/repairer.h"
#include "tools/cli.h"
#include "tools/process.h"
#include "app_common.h"

static void logging_repair(uint8_t loglevel, process_info_t* log, bool linefeed) {
    if (loglevel == 0) return;

    double total_size = (double)process_info_get_total_size(log);
    double elapsed = process_info_get_elapsed(log);
    double speed = elapsed > 0 ? total_size / elapsed : 0;

    // Format each value separately to avoid static buffer conflicts
    char size_str[64], speed_str[64];
    snprintf(size_str, sizeof(size_str), "%s", format_si(total_size));
    snprintf(speed_str, sizeof(speed_str), "%s", format_si(speed));

    fprintf(stderr, "size=%sB speed=%sB/s    ",
            size_str, speed_str);

    if (linefeed) {
        fprintf(stderr, "\n");
    } else {
        fprintf(stderr, "\r");
    }
}

void repair(const char* input, CliParams* params) {
    // Check for stdin/stdout
    bool is_stdin = (strcmp(input, "-") == 0 || strlen(input) == 0);
    bool is_stdout = (strcmp(params->output, "-") == 0);

    // Open input file or stdin
    FILE* in_file = is_stdin ? stdin : fopen(input, "rb");
    if (!in_file) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", input);
        return;
    }

    // Create repairer with ECC parameters
    repairer_t* repairer = repairer_new(params->ecc_ratio[0], params->ecc_ratio[1]);
    if (!repairer) {
        fprintf(stderr, "Error: Failed to create repairer\n");
        fclose(in_file);
        return;
    }

    // Set output filename if not specified
    char* output_file = params->output;
    char* auto_output = NULL;

    if (is_stdout) {
        output_file = NULL;  // Will use stdout
    } else if (!output_file || strlen(output_file) == 0) {
        if (is_stdin) {
            // Default output for stdin input
            auto_output = strdup("repaired.frad");
        } else {
            // Generate output filename from input filename
            const char* suffix = "_repaired";
            size_t input_len = strlen(input);
            size_t suffix_len = strlen(suffix);
            auto_output = malloc(input_len + suffix_len + 10);  // +10 for extension variations
            strcpy(auto_output, input);

            // Find the extension position
            char* dot = strrchr(auto_output, '.');
            if (dot && (strcasecmp(dot, ".frad") == 0 || strcasecmp(dot, ".fra") == 0 ||
                       strcasecmp(dot, ".dsin") == 0 || strcasecmp(dot, ".dsn") == 0)) {
                // Insert "_repaired" before the extension
                char ext[10];
                strcpy(ext, dot);
                *dot = '\0';
                strcat(auto_output, suffix);
                strcat(auto_output, ext);
            } else {
                // No known extension, just append "_repaired.frad"
                strcat(auto_output, suffix);
                strcat(auto_output, ".frad");
            }
        }
        output_file = auto_output;
    }

    // Open output file or stdout
    FILE* out_file = is_stdout ? stdout : fopen(output_file, "wb");
    if (!out_file) {
        fprintf(stderr, "Error: Cannot create output file '%s'\n", output_file);
        repairer_free(repairer);
        if (!is_stdin) fclose(in_file);
        if (auto_output) free(auto_output);
        return;
    }

    // Create process info for logging
    process_info_t* procinfo = process_info_new();

    // Process input
    uint8_t buffer[32768];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), in_file)) > 0) {
        vec_u8* result = repairer_process(repairer, buffer, bytes_read);
        if (result && result->size > 0) {
            fwrite(result->data, 1, result->size, out_file);
            process_info_update(procinfo, result->size, 0, 0);
            vec_u8_free(result);
        }
        logging_repair(params->loglevel, procinfo, false);
    }

    // Flush repairer
    vec_u8* result = repairer_flush(repairer);
    if (result && result->size > 0) {
        fwrite(result->data, 1, result->size, out_file);
        process_info_update(procinfo, result->size, 0, 0);
        vec_u8_free(result);
    }
    logging_repair(params->loglevel, procinfo, true);

    // Cleanup
    process_info_free(procinfo);
    if (!is_stdout) fclose(out_file);
    repairer_free(repairer);
    if (!is_stdin) fclose(in_file);
    if (auto_output) free(auto_output);
}