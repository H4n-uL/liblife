#ifndef APP_COMMON_H
#define APP_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <errno.h>

// Pipe and null device arrays
extern const char* PIPEIN[];
extern const char* PIPEOUT[];

// Check if path is a pipe input/output
bool is_pipe_in(const char* path);
bool is_pipe_out(const char* path);

// Read exact number of bytes from file
// Returns: Number of bytes actually read
size_t read_exact(FILE* file, uint8_t* buf, size_t size);

// Write data safely with broken pipe handling
// Returns: true on success, false on error
bool write_safe(FILE* file, const uint8_t* buf, size_t size);

// Get file stem (filename without extension)
// Returns: Newly allocated string (caller must free)
char* get_file_stem(const char* file_path);

// Format time in seconds to human-readable format
// Returns: Static buffer (do not free)
char* format_time(double seconds);

// Format number with SI prefix (k, M, G, etc.)
// Returns: Static buffer (do not free)
char* format_si(double n);

// Format speed value for display
// Returns: Static buffer (do not free)
char* format_speed(double n);

// Move all data from one file to another
void move_all(FILE* readfile, FILE* writefile, size_t buffer_size);

// Check if output file exists and handle overwrite
// Returns: true if ok to proceed, exits on cancel
bool check_overwrite(const char* file_path, bool overwrite);

// Check if two paths refer to the same file
bool is_same_file(const char* file1, const char* file2);

#endif // APP_COMMON_H