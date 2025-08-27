#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdbool.h>

typedef struct audio_player audio_player_t;

// Create a new audio player
// Parameters: channels, sample rate
audio_player_t* audio_player_new(int channels, int sample_rate);

// Free the audio player
void audio_player_free(audio_player_t* player);

// Queue audio samples for playback
// Parameters: player, samples (float64), sample count
void audio_player_queue(audio_player_t* player, const double* samples, size_t count);

// Check if the player queue is empty
bool audio_player_is_empty(audio_player_t* player);

// Wait for playback to finish
void audio_player_wait(audio_player_t* player);

// Set playback speed (1.0 = normal speed)
void audio_player_set_speed(audio_player_t* player, float speed);

#endif // AUDIO_H