#ifndef CLI_H
#define CLI_H

#include "../libfrad/libfrad.h"
#include <stdbool.h>

// CLI Parameters
typedef struct {
    char* output;
    PCMFormat pcm;
    int bits;
    int srate;
    int channels;
    int frame_size;
    bool little_endian;
    int profile;
    int overlap_ratio;
    int losslevel;
    bool enable_ecc;
    int ecc_ratio[2];
    bool overwrite;
    bool overwrite_repair;
    bool fix_error;
    char* meta[256][2]; // Max 256 metadata entries
    int meta_count;
    char* image_path;
    int loglevel;
    double speed;
} CliParams;

void init_cli_params(CliParams* params);
void free_cli_params(CliParams* params);

// Parse CLI arguments
void parse_cli(int argc, char** argv, char** action, char** metaaction, char** input, CliParams* params);

#endif // CLI_H
