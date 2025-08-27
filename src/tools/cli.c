#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

const char* ENCODE_OPT[] = {"encode", "enc"};
const char* DECODE_OPT[] = {"decode", "dec"};
const char* REPAIR_OPT[] = {"repair", "ecc"};
const char* PLAY_OPT[] = {"play", "p"};
const char* METADATA_OPT[] = {"meta", "metadata"};
const char* HELP_OPT[] = {"help", "h", "?"};

void init_cli_params(CliParams* params) {
    params->output = NULL;
    params->pcm = PCM_F64BE;
    params->bits = 0;
    params->srate = 0;
    params->channels = 0;
    params->frame_size = 2048;
    params->little_endian = false;
    params->profile = 4;
    params->overlap_ratio = 16;
    params->losslevel = 0;
    params->enable_ecc = false;
    params->ecc_ratio[0] = 96;
    params->ecc_ratio[1] = 24;
    params->overwrite = false;
    params->overwrite_repair = false;
    params->meta_count = 0;
    params->image_path = NULL;
    params->loglevel = 0;
    params->speed = 1.0;
}

void free_cli_params(CliParams* params) {
    if (params->output) free(params->output);
    if (params->image_path) free(params->image_path);
    for (int i = 0; i < params->meta_count; i++) {
        free(params->meta[i][0]);
        free(params->meta[i][1]);
    }
}

static bool is_in(const char* str, const char* arr[], int size) {
    for (int i = 0; i < size; i++) {
        if (strcmp(str, arr[i]) == 0) {
            return true;
        }
    }
    return false;
}

void parse_cli(int argc, char** argv, char** action, char** metaaction, char** input, CliParams* params) {
    if (argc < 2) {
        *action = "";
        *input = "";
        return;
    }

    *action = argv[1];

    if (is_in(*action, METADATA_OPT, 2)) {
        if (argc < 3) {
            fprintf(stderr, "Metadata action not specified.\n");
            exit(1);
        }
        *metaaction = argv[2];
        if (argc < 4) {
            *input = "";
            return;
        }
        *input = argv[3];
    } else {
        *metaaction = "";
        if (argc < 3) {
            *input = "";
            return;
        }
        *input = argv[2];
    }

    int i = (is_in(*action, METADATA_OPT, 2)) ? 4 : 3;

    while (i < argc) {
        char* key = argv[i];
        i++;

        if (key[0] == '-') {
            key++;
            if (key[0] == '-') key++;

            if (strcmp(key, "output") == 0 || strcmp(key, "out") == 0 || strcmp(key, "o") == 0) {
                if (i < argc) params->output = strdup(argv[i++]);
            } else if (strcmp(key, "ecc") == 0 || strcmp(key, "enable-ecc") == 0 || strcmp(key, "e") == 0) {
                params->enable_ecc = true;
            } else if (strcmp(key, "y") == 0 || strcmp(key, "force") == 0) {
                params->overwrite = true;
            } else if (strcmp(key, "f") == 0 || strcmp(key, "format") == 0 || strcmp(key, "fmt") == 0) {
                if (i < argc) {
                    char* format = argv[i++];
                    if (strcmp(format, "s8") == 0) {
                        params->pcm = PCM_S8;
                        params->bits = 8;
                    } else if (strcmp(format, "s16le") == 0) {
                        params->pcm = PCM_S16LE;
                        params->bits = 16;
                        params->little_endian = true;
                    } else if (strcmp(format, "s16be") == 0) {
                        params->pcm = PCM_S16BE;
                        params->bits = 16;
                        params->little_endian = false;
                    } else if (strcmp(format, "s24le") == 0) {
                        params->pcm = PCM_S24LE;
                        params->bits = 24;
                        params->little_endian = true;
                    } else if (strcmp(format, "s24be") == 0) {
                        params->pcm = PCM_S24BE;
                        params->bits = 24;
                        params->little_endian = false;
                    } else if (strcmp(format, "s32le") == 0) {
                        params->pcm = PCM_S32LE;
                        params->bits = 32;
                        params->little_endian = true;
                    } else if (strcmp(format, "s32be") == 0) {
                        params->pcm = PCM_S32BE;
                        params->bits = 32;
                        params->little_endian = false;
                    } else if (strcmp(format, "f32le") == 0) {
                        params->pcm = PCM_F32LE;
                        params->bits = 32;
                        params->little_endian = true;
                    } else if (strcmp(format, "f32be") == 0) {
                        params->pcm = PCM_F32BE;
                        params->bits = 32;
                        params->little_endian = false;
                    } else if (strcmp(format, "f64le") == 0) {
                        params->pcm = PCM_F64LE;
                        params->bits = 64;
                        params->little_endian = true;
                    } else if (strcmp(format, "f64be") == 0) {
                        params->pcm = PCM_F64BE;
                        params->bits = 64;
                        params->little_endian = false;
                    } else if (strcmp(format, "u8") == 0) {
                        params->pcm = PCM_U8;
                        params->bits = 8;
                    } else if (strcmp(format, "u16le") == 0) {
                        params->pcm = PCM_U16LE;
                        params->bits = 16;
                        params->little_endian = true;
                    } else if (strcmp(format, "u16be") == 0) {
                        params->pcm = PCM_U16BE;
                        params->bits = 16;
                        params->little_endian = false;
                    } else if (strcmp(format, "u24le") == 0) {
                        params->pcm = PCM_U24LE;
                        params->bits = 24;
                        params->little_endian = true;
                    } else if (strcmp(format, "u24be") == 0) {
                        params->pcm = PCM_U24BE;
                        params->bits = 24;
                        params->little_endian = false;
                    } else if (strcmp(format, "u32le") == 0) {
                        params->pcm = PCM_U32LE;
                        params->bits = 32;
                        params->little_endian = true;
                    } else if (strcmp(format, "u32be") == 0) {
                        params->pcm = PCM_U32BE;
                        params->bits = 32;
                        params->little_endian = false;
                    } else if (strcmp(format, "s64le") == 0) {
                        params->pcm = PCM_S64LE;
                        params->bits = 64;
                        params->little_endian = true;
                    } else if (strcmp(format, "s64be") == 0) {
                        params->pcm = PCM_S64BE;
                        params->bits = 64;
                        params->little_endian = false;
                    } else if (strcmp(format, "u64le") == 0) {
                        params->pcm = PCM_U64LE;
                        params->bits = 64;
                        params->little_endian = true;
                    } else if (strcmp(format, "u64be") == 0) {
                        params->pcm = PCM_U64BE;
                        params->bits = 64;
                        params->little_endian = false;
                    } else if (strcmp(format, "f16le") == 0) {
                        params->pcm = PCM_F16LE;
                        params->bits = 16;
                        params->little_endian = true;
                    } else if (strcmp(format, "f16be") == 0) {
                        params->pcm = PCM_F16BE;
                        params->bits = 16;
                        params->little_endian = false;
                    }
                }
            } else if (strcmp(key, "bits") == 0 || strcmp(key, "bit") == 0 || strcmp(key, "b") == 0) {
                if (i < argc) params->bits = atoi(argv[i++]);
            } else if (strcmp(key, "srate") == 0 || strcmp(key, "sample-rate") == 0 || strcmp(key, "sr") == 0) {
                if (i < argc) params->srate = atoi(argv[i++]);
            } else if (strcmp(key, "chnl") == 0 || strcmp(key, "channels") == 0 || strcmp(key, "ch") == 0) {
                if (i < argc) params->channels = atoi(argv[i++]);
            } else if (strcmp(key, "frame-size") == 0 || strcmp(key, "fsize") == 0 || strcmp(key, "fr") == 0) {
                if (i < argc) params->frame_size = atoi(argv[i++]);
            } else if (strcmp(key, "le") == 0 || strcmp(key, "little-endian") == 0) {
                params->little_endian = true;
            } else if (strcmp(key, "profile") == 0 || strcmp(key, "prf") == 0 || strcmp(key, "p") == 0) {
                if (i < argc) params->profile = atoi(argv[i++]);
            } else if (strcmp(key, "losslevel") == 0 || strcmp(key, "loss") == 0 || strcmp(key, "l") == 0) {
                if (i < argc) params->losslevel = atoi(argv[i++]);
            } else if (strcmp(key, "overlap") == 0 || strcmp(key, "overlap-ratio") == 0 || strcmp(key, "olap") == 0) {
                if (i < argc) params->overlap_ratio = atoi(argv[i++]);
            } else if (strcmp(key, "overwrite") == 0 || strcmp(key, "ow") == 0 || strcmp(key, "y") == 0) {
                params->overwrite = true;
            } else if (strcmp(key, "overwrite-repair") == 0) {
                params->overwrite_repair = true;
            } else if (strcmp(key, "log") == 0 || strcmp(key, "v") == 0) {
                params->loglevel = 1;
                if (i < argc && argv[i][0] >= '0' && argv[i][0] <= '2') {
                    params->loglevel = atoi(argv[i++]);
                }
            } else if (strcmp(key, "fix") == 0 || strcmp(key, "fix-error") == 0) {
                params->fix_error = true;
            } else if (strcmp(key, "tag") == 0 || strcmp(key, "meta") == 0 || strcmp(key, "m") == 0) {
                // Parse metadata in format key=value
                if (i < argc) {
                    char* meta_str = strdup(argv[i++]);
                    char* equals = strchr(meta_str, '=');
                    if (equals && params->meta_count < 256) {
                        *equals = '\0';
                        params->meta[params->meta_count][0] = strdup(meta_str);
                        params->meta[params->meta_count][1] = strdup(equals + 1);
                        params->meta_count++;
                    }
                    free(meta_str);
                }
            } else if (strcmp(key, "img") == 0 || strcmp(key, "image") == 0) {
                if (i < argc) params->image_path = strdup(argv[i++]);
            } else if (strcmp(key, "speed") == 0 || strcmp(key, "spd") == 0) {
                // TODO: Implement speed control
            } else if (strcmp(key, "keys") == 0 || strcmp(key, "key") == 0 || strcmp(key, "k") == 0) {
                // TODO: Implement key transposition
            }
        }
    }
}
