#include "process.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// HashMap node for storing srate -> value mappings
typedef struct hash_node {
    uint32_t key;
    uint64_t value;
    struct hash_node* next;
} hash_node_t;

// Simple HashMap implementation
typedef struct {
    hash_node_t* buckets[32];
} hashmap_t;

struct process_info {
    struct timeval start_time;
    struct timeval* t_block;
    uint64_t total_size;
    hashmap_t* duration;
    hashmap_t* bitrate;
};

// HashMap functions
static hashmap_t* hashmap_new() {
    hashmap_t* map = calloc(1, sizeof(hashmap_t));
    return map;
}

static void hashmap_free(hashmap_t* map) {
    if (!map) return;
    for (int i = 0; i < 32; i++) {
        hash_node_t* node = map->buckets[i];
        while (node) {
            hash_node_t* next = node->next;
            free(node);
            node = next;
        }
    }
    free(map);
}

static uint64_t hashmap_get(hashmap_t* map, uint32_t key) {
    if (!map) return 0;
    uint32_t bucket = key % 32;
    hash_node_t* node = map->buckets[bucket];
    while (node) {
        if (node->key == key) return node->value;
        node = node->next;
    }
    return 0;
}

static void hashmap_set(hashmap_t* map, uint32_t key, uint64_t value) {
    if (!map) return;
    uint32_t bucket = key % 32;
    hash_node_t* node = map->buckets[bucket];

    // Check if key exists
    while (node) {
        if (node->key == key) {
            node->value = value;
            return;
        }
        node = node->next;
    }

    // Add new node
    hash_node_t* new_node = malloc(sizeof(hash_node_t));
    new_node->key = key;
    new_node->value = value;
    new_node->next = map->buckets[bucket];
    map->buckets[bucket] = new_node;
}

static double hashmap_sum_duration(hashmap_t* map) {
    if (!map) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < 32; i++) {
        hash_node_t* node = map->buckets[i];
        while (node) {
            if (node->key != 0) {
                sum += (double)node->value / (double)node->key;
            }
            node = node->next;
        }
    }
    return sum;
}

static uint64_t hashmap_sum_values(hashmap_t* map) {
    if (!map) return 0;
    uint64_t sum = 0;
    for (int i = 0; i < 32; i++) {
        hash_node_t* node = map->buckets[i];
        while (node) {
            sum += node->value;
            node = node->next;
        }
    }
    return sum;
}

// ProcessInfo implementation
process_info_t* process_info_new() {
    process_info_t* info = calloc(1, sizeof(process_info_t));
    if (!info) return NULL;

    gettimeofday(&info->start_time, NULL);
    info->t_block = NULL;
    info->total_size = 0;
    info->duration = hashmap_new();
    info->bitrate = hashmap_new();

    return info;
}

void process_info_free(process_info_t* info) {
    if (!info) return;
    if (info->t_block) free(info->t_block);
    hashmap_free(info->duration);
    hashmap_free(info->bitrate);
    free(info);
}

void process_info_update(process_info_t* info, size_t size, size_t samples, uint32_t srate) {
    if (!info) return;

    info->total_size += size;
    if (srate == 0) return;

    uint64_t current_duration = hashmap_get(info->duration, srate);
    hashmap_set(info->duration, srate, current_duration + samples);

    uint64_t current_bitrate = hashmap_get(info->bitrate, srate);
    hashmap_set(info->bitrate, srate, current_bitrate + size);
}

double process_info_get_duration(process_info_t* info) {
    if (!info) return 0.0;
    return hashmap_sum_duration(info->duration);
}

double process_info_get_bitrate(process_info_t* info) {
    if (!info) return 0.0;

    double total_bits = (double)hashmap_sum_values(info->bitrate) * 8.0;
    double total_duration = process_info_get_duration(info);

    return (total_duration > 0.0) ? (total_bits / total_duration) : 0.0;
}

double process_info_get_speed(process_info_t* info) {
    if (!info) return 0.0;

    struct timeval now;
    gettimeofday(&now, NULL);

    double encoding_time = (now.tv_sec - info->start_time.tv_sec) +
                          (now.tv_usec - info->start_time.tv_usec) / 1000000.0;
    double total_duration = process_info_get_duration(info);

    return (encoding_time > 0.0) ? (total_duration / encoding_time) : 0.0;
}

uint64_t process_info_get_total_size(process_info_t* info) {
    return info ? info->total_size : 0;
}

void process_info_block(process_info_t* info) {
    if (!info) return;
    if (info->t_block) free(info->t_block);
    info->t_block = malloc(sizeof(struct timeval));
    gettimeofday(info->t_block, NULL);
}

void process_info_unblock(process_info_t* info) {
    if (!info || !info->t_block) return;

    struct timeval now;
    gettimeofday(&now, NULL);

    long sec_diff = now.tv_sec - info->t_block->tv_sec;
    long usec_diff = now.tv_usec - info->t_block->tv_usec;

    // Add the blocked time to start_time to effectively remove it from elapsed time
    info->start_time.tv_sec += sec_diff;
    info->start_time.tv_usec += usec_diff;

    // Normalize the time
    if (info->start_time.tv_usec >= 1000000) {
        info->start_time.tv_sec++;
        info->start_time.tv_usec -= 1000000;
    } else if (info->start_time.tv_usec < 0) {
        info->start_time.tv_sec--;
        info->start_time.tv_usec += 1000000;
    }

    free(info->t_block);
    info->t_block = NULL;
}

double process_info_get_elapsed(process_info_t* info) {
    if (!info) return 0.0;

    struct timeval now;
    gettimeofday(&now, NULL);

    return (now.tv_sec - info->start_time.tv_sec) +
           (now.tv_usec - info->start_time.tv_usec) / 1000000.0;
}