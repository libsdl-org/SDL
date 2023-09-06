/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifndef SDL_audioqueue_h_
#define SDL_audioqueue_h_

// Internal functions used by SDL_AudioStream for queueing audio.

typedef struct SDL_AudioQueue SDL_AudioQueue;
typedef struct SDL_AudioTrack SDL_AudioTrack;

// Create a new audio queue
SDL_AudioQueue *SDL_CreateAudioQueue(size_t chunk_size);

// Destroy an audio queue
void SDL_DestroyAudioQueue(SDL_AudioQueue *queue);

// Completely clear the queue
void SDL_ClearAudioQueue(SDL_AudioQueue *queue);

// Mark the last track as flushed
void SDL_FlushAudioQueue(SDL_AudioQueue *queue);

// Pop the current head track
// REQUIRES: The head track must exist, and must have been flushed
void SDL_PopAudioQueueHead(SDL_AudioQueue *queue);

// Get the chunk size, mostly for use with SDL_CreateChunkedAudioTrack
// This can be called from any thread
size_t SDL_GetAudioQueueChunkSize(SDL_AudioQueue *queue);

// Write data to the end of queue
// REQUIRES: If the spec has changed, the last track must have been flushed
int SDL_WriteToAudioQueue(SDL_AudioQueue *queue, const SDL_AudioSpec *spec, const Uint8 *data, size_t len);

// Create a track without needing to hold any locks
SDL_AudioTrack *SDL_CreateChunkedAudioTrack(const SDL_AudioSpec *spec, const Uint8 *data, size_t len, size_t chunk_size);

// Add a track to the end of the queue
// REQUIRES: `track != NULL`
void SDL_AddTrackToAudioQueue(SDL_AudioQueue *queue, SDL_AudioTrack *track);

// Iterate over the tracks in the queue
void *SDL_BeginAudioQueueIter(SDL_AudioQueue *queue);

// Query and update the track iterator
// REQUIRES: `*inout_iter != NULL` (a valid iterator)
size_t SDL_NextAudioQueueIter(SDL_AudioQueue *queue, void **inout_iter, SDL_AudioSpec *out_spec, SDL_bool *out_flushed);

// Read data from the start of the queue
// REQUIRES: There must be enough data in the queue
int SDL_ReadFromAudioQueue(SDL_AudioQueue *queue, Uint8 *data, size_t len);

// Peek into the start of the queue
// REQUIRES: There must be enough data in the queue, unless it has been flushed, in which case missing data is filled with silence.
int SDL_PeekIntoAudioQueue(SDL_AudioQueue *queue, Uint8 *data, size_t len);

#endif // SDL_audioqueue_h_
