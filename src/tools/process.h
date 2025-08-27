#ifndef TOOLS_PROCESS_H
#define TOOLS_PROCESS_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration
typedef struct process_info process_info_t;

// Create new process info
process_info_t* process_info_new();

// Free process info
void process_info_free(process_info_t* info);

// Update process info with new data
// Parameters: info, size in bytes, number of samples, sample rate
void process_info_update(process_info_t* info, size_t size, size_t samples, uint32_t srate);

// Get total duration in seconds
double process_info_get_duration(process_info_t* info);

// Get average bitrate in bits per second
double process_info_get_bitrate(process_info_t* info);

// Get processing speed (duration/elapsed time)
double process_info_get_speed(process_info_t* info);

// Get total size in bytes
uint64_t process_info_get_total_size(process_info_t* info);

// Block/unblock timing (for pausing)
void process_info_block(process_info_t* info);
void process_info_unblock(process_info_t* info);

// Get elapsed time since start (in seconds)
double process_info_get_elapsed(process_info_t* info);

#endif // TOOLS_PROCESS_H