#include "app_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>

// Pipe and null device definitions
const char* PIPEIN[] = {"-", "/dev/stdin", "dev/fd/0", NULL};
const char* PIPEOUT[] = {"-", "/dev/stdout", "dev/fd/1", NULL};

bool is_pipe_in(const char* path) {
    if (!path) return false;
    for (int i = 0; PIPEIN[i]; i++) {
        if (strcmp(path, PIPEIN[i]) == 0) return true;
    }
    return false;
}

bool is_pipe_out(const char* path) {
    if (!path) return false;
    for (int i = 0; PIPEOUT[i]; i++) {
        if (strcmp(path, PIPEOUT[i]) == 0) return true;
    }
    return false;
}

size_t read_exact(FILE* file, uint8_t* buf, size_t size) {
    if (!file || !buf) return 0;

    size_t total_read = 0;
    while (total_read < size) {
        size_t read_size = fread(buf + total_read, 1, size - total_read, file);
        if (read_size == 0) break;
        total_read += read_size;
    }
    return total_read;
}

bool write_safe(FILE* file, const uint8_t* buf, size_t size) {
    if (!file || !buf) return false;

    size_t written = fwrite(buf, 1, size, file);
    if (written != size) {
        if (ferror(file)) {
            // Check for broken pipe
            if (errno == EPIPE) {
                exit(0);  // Exit gracefully on broken pipe
            }
            return false;
        }
    }
    return true;
}

char* get_file_stem(const char* file_path) {
    if (!file_path) return strdup("unknown");

    // Check if it's a pipe
    if (is_pipe_in(file_path) || is_pipe_out(file_path)) {
        return strdup("pipe");
    }

    // Create a copy to work with
    char* path_copy = strdup(file_path);
    char* base = basename(path_copy);

    // Find the last dot for extension
    char* dot = strrchr(base, '.');
    if (dot && dot != base) {
        *dot = '\0';  // Remove extension
    }

    char* result = strdup(base);
    free(path_copy);
    return result;
}

char* format_time(double n) {
    static char buffer[64];

    if (n < 0.0) {
        char* positive = format_time(-n);
        snprintf(buffer, sizeof(buffer), "-%s", positive);
        return buffer;
    }

    uint16_t julian = (uint16_t)(n / 31557600.0);
    n = fmod(n, 31557600.0);
    uint16_t days = (uint16_t)(n / 86400.0);
    n = fmod(n, 86400.0);
    uint8_t hours = (uint8_t)(n / 3600.0);
    n = fmod(n, 3600.0);
    uint8_t minutes = (uint8_t)(n / 60.0);
    n = fmod(n, 60.0);

    if (julian > 0) {
        snprintf(buffer, sizeof(buffer), "J%d.%03d:%02d:%02d:%06.3f", julian, days, hours, minutes, n);
    } else if (days > 0) {
        snprintf(buffer, sizeof(buffer), "%d:%02d:%02d:%06.3f", days, hours, minutes, n);
    } else if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%d:%02d:%06.3f", hours, minutes, n);
    } else if (minutes > 0) {
        snprintf(buffer, sizeof(buffer), "%d:%06.3f", minutes, n);
    } else if (n >= 1.0) {
        snprintf(buffer, sizeof(buffer), "%.3f s", n);
    } else if (n >= 0.001) {
        snprintf(buffer, sizeof(buffer), "%.3f ms", n * 1000.0);
    } else if (n >= 0.000001) {
        snprintf(buffer, sizeof(buffer), "%.3f µs", n * 1000000.0);
    } else if (n > 0.0) {
        snprintf(buffer, sizeof(buffer), "%.3f ns", n * 1000000000.0);
    } else {
        snprintf(buffer, sizeof(buffer), "0");
    }

    return buffer;
}

static const char* SI_UNITS[] = {"", "k", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"};
static const int SI_UNITS_COUNT = 11;

char* format_si(double n) {
    static char buffer[32];

    double abs_n = fabs(n);
    if (abs_n == 0.0) {
        snprintf(buffer, sizeof(buffer), "0.000 ");
        return buffer;
    }

    double exp = floor(log10(abs_n) / 3.0);
    if (exp < 0.0) exp = 0.0;
    if (exp >= SI_UNITS_COUNT - 1) exp = SI_UNITS_COUNT - 1;

    double value = n / pow(1000.0, exp);
    snprintf(buffer, sizeof(buffer), "%.3f %s", value, SI_UNITS[(int)exp]);

    return buffer;
}

char* format_speed(double n) {
    static char buffer[32];

    if (n >= 100.0) {
        snprintf(buffer, sizeof(buffer), "%.0f", n);
    } else if (n >= 10.0) {
        snprintf(buffer, sizeof(buffer), "%.1f", n);
    } else if (n >= 1.0) {
        snprintf(buffer, sizeof(buffer), "%.2f", n);
    } else {
        snprintf(buffer, sizeof(buffer), "%.3f", n);
    }

    return buffer;
}

void move_all(FILE* readfile, FILE* writefile, size_t buffer_size) {
    if (!readfile || !writefile) return;

    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) return;

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, buffer_size, readfile)) > 0) {
        fwrite(buffer, 1, bytes_read, writefile);
    }

    free(buffer);
}

bool check_overwrite(const char* file_path, bool overwrite) {
    if (!file_path) return false;

    // Skip check for pipes
    if (is_pipe_out(file_path)) return true;

    struct stat st;
    if (stat(file_path, &st) == 0) {
        // File exists
        if (!overwrite) {
            if (isatty(STDIN_FILENO)) {
                fprintf(stderr, "Output file '%s' already exists. Overwrite? (y/n): ", file_path);
                char response = getchar();
                if (response != 'y' && response != 'Y') {
                    fprintf(stderr, "Operation cancelled.\n");
                    exit(1);
                }
            } else {
                fprintf(stderr, "Output file '%s' already exists and overwrite not enabled.\n", file_path);
                exit(1);
            }
        }
    }

    return true;
}

bool is_same_file(const char* file1, const char* file2) {
    if (!file1 || !file2) return false;

    struct stat st1, st2;
    if (stat(file1, &st1) != 0 || stat(file2, &st2) != 0) {
        return false;
    }

    return (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino);
}