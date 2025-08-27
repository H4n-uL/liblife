#include "audio.h"
#include <portaudio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#define BUFFER_SIZE 8192
#define RING_BUFFER_SIZE (BUFFER_SIZE * 8)

typedef struct {
    float* buffer;
    size_t write_pos;
    size_t read_pos;
    size_t size;
    size_t capacity;
    pthread_mutex_t mutex;
    pthread_cond_t cond_not_empty;
    pthread_cond_t cond_not_full;
} ring_buffer_t;

struct audio_player {
    PaStream* stream;
    ring_buffer_t* ring_buffer;
    int channels;
    int sample_rate;
    float speed;
    bool finished;
};

static ring_buffer_t* ring_buffer_new(size_t capacity) {
    ring_buffer_t* rb = calloc(1, sizeof(ring_buffer_t));
    if (!rb) return NULL;

    rb->buffer = calloc(capacity, sizeof(float));
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }

    rb->capacity = capacity;
    rb->size = 0;
    rb->write_pos = 0;
    rb->read_pos = 0;
    pthread_mutex_init(&rb->mutex, NULL);
    pthread_cond_init(&rb->cond_not_empty, NULL);
    pthread_cond_init(&rb->cond_not_full, NULL);

    return rb;
}

static void ring_buffer_free(ring_buffer_t* rb) {
    if (!rb) return;

    pthread_mutex_destroy(&rb->mutex);
    pthread_cond_destroy(&rb->cond_not_empty);
    pthread_cond_destroy(&rb->cond_not_full);
    free(rb->buffer);
    free(rb);
}

static size_t ring_buffer_write(ring_buffer_t* rb, const float* data, size_t count) {
    pthread_mutex_lock(&rb->mutex);

    // Wait while buffer is full
    while (rb->size >= rb->capacity) {
        pthread_cond_wait(&rb->cond_not_full, &rb->mutex);
    }

    size_t available = rb->capacity - rb->size;
    size_t to_write = (count < available) ? count : available;

    for (size_t i = 0; i < to_write; i++) {
        rb->buffer[rb->write_pos] = data[i];
        rb->write_pos = (rb->write_pos + 1) % rb->capacity;
    }

    rb->size += to_write;
    pthread_cond_signal(&rb->cond_not_empty);
    pthread_mutex_unlock(&rb->mutex);

    return to_write;
}

static size_t ring_buffer_read(ring_buffer_t* rb, float* data, size_t count, bool blocking) {
    pthread_mutex_lock(&rb->mutex);

    if (blocking) {
        // Wait while buffer is empty
        while (rb->size == 0) {
            pthread_cond_wait(&rb->cond_not_empty, &rb->mutex);
        }
    }

    size_t to_read = (count < rb->size) ? count : rb->size;

    for (size_t i = 0; i < to_read; i++) {
        data[i] = rb->buffer[rb->read_pos];
        rb->read_pos = (rb->read_pos + 1) % rb->capacity;
    }

    rb->size -= to_read;
    pthread_cond_signal(&rb->cond_not_full);
    pthread_mutex_unlock(&rb->mutex);

    return to_read;
}

static size_t ring_buffer_size(ring_buffer_t* rb) {
    pthread_mutex_lock(&rb->mutex);
    size_t size = rb->size;
    pthread_mutex_unlock(&rb->mutex);
    return size;
}

// PortAudio callback function
static int audio_callback(const void* input_buffer,
                         void* output_buffer,
                         unsigned long frames_per_buffer,
                         const PaStreamCallbackTimeInfo* time_info,
                         PaStreamCallbackFlags status_flags,
                         void* user_data) {
    (void)input_buffer;
    (void)time_info;
    (void)status_flags;

    audio_player_t* player = (audio_player_t*)user_data;
    float* out = (float*)output_buffer;

    size_t frames_needed = frames_per_buffer * player->channels;
    size_t frames_read = ring_buffer_read(player->ring_buffer, out, frames_needed, false);

    // Apply speed adjustment if needed (simple pitch scaling)
    if (player->speed != 1.0f && frames_read > 0) {
        // This is a simplified speed adjustment - a proper implementation would use resampling
        // For now, we just play samples faster/slower by skipping/repeating
        if (player->speed > 1.0f) {
            // Speed up - skip samples
            size_t new_frames = (size_t)(frames_read / player->speed);
            for (size_t i = 0; i < new_frames; i++) {
                size_t src_idx = (size_t)(i * player->speed);
                if (src_idx < frames_read) {
                    out[i] = out[src_idx];
                }
            }
            frames_read = new_frames;
        }
    }

    // Fill remaining buffer with silence if needed
    if (frames_read < frames_needed) {
        memset(out + frames_read, 0, (frames_needed - frames_read) * sizeof(float));

        // If buffer is empty and we're finished, stop the stream
        if (player->finished && ring_buffer_size(player->ring_buffer) == 0) {
            return paComplete;
        }
    }

    return paContinue;
}

audio_player_t* audio_player_new(int channels, int sample_rate) {
    audio_player_t* player = calloc(1, sizeof(audio_player_t));
    if (!player) return NULL;

    player->channels = channels;
    player->sample_rate = sample_rate;
    player->speed = 1.0f;
    player->finished = false;

    // Create ring buffer
    player->ring_buffer = ring_buffer_new(RING_BUFFER_SIZE * channels);
    if (!player->ring_buffer) {
        free(player);
        return NULL;
    }

    // Initialize PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        ring_buffer_free(player->ring_buffer);
        free(player);
        return NULL;
    }

    // Open audio stream
    err = Pa_OpenDefaultStream(&player->stream,
                               0,              // no input channels
                               channels,       // output channels
                               paFloat32,      // sample format
                               sample_rate,    // sample rate
                               256,            // frames per buffer
                               audio_callback, // callback function
                               player);        // user data

    if (err != paNoError) {
        Pa_Terminate();
        ring_buffer_free(player->ring_buffer);
        free(player);
        return NULL;
    }

    // Start the stream
    err = Pa_StartStream(player->stream);
    if (err != paNoError) {
        Pa_CloseStream(player->stream);
        Pa_Terminate();
        ring_buffer_free(player->ring_buffer);
        free(player);
        return NULL;
    }

    return player;
}

void audio_player_free(audio_player_t* player) {
    if (!player) return;

    player->finished = true;

    if (player->stream) {
        Pa_StopStream(player->stream);
        Pa_CloseStream(player->stream);
    }

    Pa_Terminate();
    ring_buffer_free(player->ring_buffer);
    free(player);
}

void audio_player_queue(audio_player_t* player, const double* samples, size_t count) {
    if (!player || !samples || count == 0) return;

    // Convert double to float
    float* float_samples = malloc(count * sizeof(float));
    if (!float_samples) return;

    for (size_t i = 0; i < count; i++) {
        // Clamp to [-1, 1]
        double sample = samples[i];
        if (sample > 1.0) sample = 1.0;
        else if (sample < -1.0) sample = -1.0;
        float_samples[i] = (float)sample;
    }

    // Write to ring buffer (may block if buffer is full)
    size_t written = 0;
    while (written < count) {
        written += ring_buffer_write(player->ring_buffer, float_samples + written, count - written);
    }

    free(float_samples);
}

bool audio_player_is_empty(audio_player_t* player) {
    if (!player) return true;
    return ring_buffer_size(player->ring_buffer) == 0;
}

void audio_player_wait(audio_player_t* player) {
    if (!player) return;

    player->finished = true;

    // Wait for stream to finish
    while (Pa_IsStreamActive(player->stream) == 1) {
        Pa_Sleep(100);
    }
}

void audio_player_set_speed(audio_player_t* player, float speed) {
    if (!player) return;

    if (speed < 0.1f) speed = 0.1f;
    if (speed > 10.0f) speed = 10.0f;

    player->speed = speed;
}