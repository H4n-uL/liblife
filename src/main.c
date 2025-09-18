#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tools/cli.h"
#include "encoder.h"
#include "decoder.h"
#include "repairer.h"
#include "header.h"

// Include generated help headers
#include "help/general.h"
#include "help/encode.h"
#include "help/decode.h"
#include "help/repair.h"
#include "help/play.h"
#include "help/metadata.h"
#include "help/jsonmeta.h"
#include "help/vorbismeta.h"
#include "help/profiles.h"

const char* BANNER =
"LIFE Isn't FrAD Encoder, version 1.0.0\n"
"Copyright (C) 2024-2025 Definitely not HaמuL, unless...?\n"
"\n"
"This program is free software; you can redistribute it and/or\n"
"modify it under the terms of the GNU Affero General Public License\n"
"as published by the Free Software Foundation; either version 3\n"
"of the License, or (at your option) any later version.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU Affero General Public License for more details.\n"
"\n"
"You should have received a copy of the GNU General Public License along\n"
"with this program; if not, write to the Free Software Foundation, Inc.,\n"
"51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.\n";

// Helper function to replace {frad} with executable name
static void print_help_with_executable(const char* help_text, size_t help_len, const char* executable) {
    char* help_copy = malloc(help_len + 1);
    memcpy(help_copy, help_text, help_len);
    help_copy[help_len] = '\0';

    char* pos = strstr(help_copy, "{frad}");
    while (pos) {
        char* temp = malloc(strlen(help_copy) + strlen(executable) + 1);
        size_t prefix_len = pos - help_copy;
        strncpy(temp, help_copy, prefix_len);
        temp[prefix_len] = '\0';
        strcat(temp, executable);
        strcat(temp, pos + 6);  // Skip "{frad}"
        free(help_copy);
        help_copy = temp;
        pos = strstr(help_copy, "{frad}");
    }
    printf("%s", help_copy);
    free(help_copy);
}

int main(int argc, char** argv) {    
    char* action;
    char* metaaction;
    char* input;
    CliParams params;

    init_cli_params(&params);
    parse_cli(argc, argv, &action, &metaaction, &input, &params);

    // Main actions
    if (strcmp(action, "encode") == 0 || strcmp(action, "enc") == 0) {
        encode(input, &params);
    } else if (strcmp(action, "decode") == 0 || strcmp(action, "dec") == 0) {
        decode(input, &params, false);
    } else if (strcmp(action, "play") == 0 || strcmp(action, "p") == 0) {
        decode(input, &params, true);
    } else if (strcmp(action, "repair") == 0 || strcmp(action, "ecc") == 0) {
        repair(input, &params);
    } else if (strcmp(action, "meta") == 0 || strcmp(action, "metadata") == 0) {
        modify_metadata(input, metaaction, &params);
    // Help-only actions (like in Rust version)
    } else if (strcmp(action, "jsonmeta") == 0 || strcmp(action, "jm") == 0) {
        printf("%s\n", BANNER);
        print_help_with_executable((char*)src_help_jsonmeta_txt, src_help_jsonmeta_txt_len, argv[0]);
        printf("\n");
    } else if (strcmp(action, "vorbismeta") == 0 || strcmp(action, "vm") == 0) {
        printf("%s\n", BANNER);
        print_help_with_executable((char*)src_help_vorbismeta_txt, src_help_vorbismeta_txt_len, argv[0]);
        printf("\n");
    } else if (strcmp(action, "profiles") == 0 || strcmp(action, "prf") == 0) {
        printf("%s\n", BANNER);
        print_help_with_executable((char*)src_help_profiles_txt, src_help_profiles_txt_len, argv[0]);
        printf("\n");
    } else if (strcmp(action, "help") == 0 || strcmp(action, "h") == 0 || strcmp(action, "?") == 0) {
        printf("%s\n", BANNER);
        // Check if specific help is requested
        if (input != NULL) {
            if (strcmp(input, "encode") == 0 || strcmp(input, "enc") == 0) {
                print_help_with_executable((char*)src_help_encode_txt, src_help_encode_txt_len, argv[0]);
            } else if (strcmp(input, "decode") == 0 || strcmp(input, "dec") == 0) {
                print_help_with_executable((char*)src_help_decode_txt, src_help_decode_txt_len, argv[0]);
            } else if (strcmp(input, "repair") == 0 || strcmp(input, "ecc") == 0) {
                print_help_with_executable((char*)src_help_repair_txt, src_help_repair_txt_len, argv[0]);
            } else if (strcmp(input, "play") == 0 || strcmp(input, "p") == 0) {
                print_help_with_executable((char*)src_help_play_txt, src_help_play_txt_len, argv[0]);
            } else if (strcmp(input, "meta") == 0 || strcmp(input, "metadata") == 0) {
                print_help_with_executable((char*)src_help_metadata_txt, src_help_metadata_txt_len, argv[0]);
            } else if (strcmp(input, "jsonmeta") == 0 || strcmp(input, "jm") == 0) {
                print_help_with_executable((char*)src_help_jsonmeta_txt, src_help_jsonmeta_txt_len, argv[0]);
            } else if (strcmp(input, "vorbismeta") == 0 || strcmp(input, "vm") == 0) {
                print_help_with_executable((char*)src_help_vorbismeta_txt, src_help_vorbismeta_txt_len, argv[0]);
            } else if (strcmp(input, "profiles") == 0 || strcmp(input, "prf") == 0) {
                print_help_with_executable((char*)src_help_profiles_txt, src_help_profiles_txt_len, argv[0]);
            } else {
                print_help_with_executable((char*)src_help_general_txt, src_help_general_txt_len, argv[0]);
            }
        } else {
            print_help_with_executable((char*)src_help_general_txt, src_help_general_txt_len, argv[0]);
        }
        printf("\n\n");
    } else {
        printf("%s\n", BANNER);
        fprintf(stderr, "Usage: %s COMMAND [OPTION]... [FILE]...\n", argv[0]);
        fprintf(stderr, "Try '%s help' for more information.\n", argv[0]);
        fprintf(stderr, "\n");
    }

    free_cli_params(&params);

    return 0;
}
