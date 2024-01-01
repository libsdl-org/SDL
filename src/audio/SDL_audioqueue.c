/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_audioqueue.h"

#define AUDIO_SPECS_EQUAL(x, y) (((x).format == (y).format) && ((x).channels == (y).channels) && ((x).freq == (y).freq))

struct SDL_AudioTrack
{
    SDL_AudioSpec spec;
    SDL_bool flushed;
    SDL_AudioTrack *next;

    size_t (*avail)(void *ctx);
    int (*write)(void *ctx, const Uint8 *buf, size_t len);
    size_t (*read)(void *ctx, Uint8 *buf, size_t len, SDL_bool advance);
    void (*destroy)(void *ctx);
};

struct SDL_AudioQueue
{
    SDL_AudioTrack *head;
    SDL_AudioTrack *tail;
    size_t chunk_size;
};

typedef struct SDL_AudioChunk SDL_AudioChunk;

struct SDL_AudioChunk
{
    SDL_AudioChunk *next;
    size_t head;
    size_t tail;
    Uint8 data[SDL_VARIABLE_LENGTH_ARRAY];
};

typedef struct SDL_ChunkedAudioTrack
{
    SDL_AudioTrack track;

    size_t chunk_size;

    SDL_AudioChunk *head;
    SDL_AudioChunk *tail;
    size_t queued_bytes;

    SDL_AudioChunk *free_chunks;
    size_t num_free_chunks;
} SDL_ChunkedAudioTrack;

static void DestroyAudioChunk(SDL_AudioChunk *chunk)
{
    SDL_free(chunk);
}

static void DestroyAudioChunks(SDL_AudioChunk *chunk)
{
    while (chunk) {
        SDL_AudioChunk *next = chunk->next;
        DestroyAudioChunk(chunk);
        chunk = next;
    }
}

static void ResetAudioChunk(SDL_AudioChunk *chunk)
{
    chunk->next = NULL;
    chunk->head = 0;
    chunk->tail = 0;
}

static SDL_AudioChunk *CreateAudioChunk(size_t chunk_size)
{
    SDL_AudioChunk *chunk = (SDL_AudioChunk *)SDL_malloc(sizeof(*chunk) + chunk_size);

    if (!chunk) {
        return NULL;
    }

    ResetAudioChunk(chunk);

    return chunk;
}

static void DestroyAudioTrackChunk(SDL_ChunkedAudioTrack *track, SDL_AudioChunk *chunk)
{
    // Keeping a list of free chunks reduces memory allocations,
    // But also increases the amount of work to perform when freeing the track.
    const size_t max_free_bytes = 64 * 1024;

    if (track->chunk_size * track->num_free_chunks < max_free_bytes) {
        chunk->next = track->free_chunks;
        track->free_chunks = chunk;
        ++track->num_free_chunks;
    } else {
        DestroyAudioChunk(chunk);
    }
}

static SDL_AudioChunk *CreateAudioTrackChunk(SDL_ChunkedAudioTrack *track)
{
    if (track->num_free_chunks > 0) {
        SDL_AudioChunk *chunk = track->free_chunks;

        track->free_chunks = chunk->next;
        --track->num_free_chunks;

        ResetAudioChunk(chunk);

        return chunk;
    }

    return CreateAudioChunk(track->chunk_size);
}

static size_t AvailChunkedAudioTrack(void *ctx)
{
    SDL_ChunkedAudioTrack *track = ctx;

    return track->queued_bytes;
}

static int WriteToChunkedAudioTrack(void *ctx, const Uint8 *data, size_t len)
{
    SDL_ChunkedAudioTrack *track = ctx;

    SDL_AudioChunk *chunk = track->tail;

    // Handle the first chunk
    if (!chunk) {
        chunk = CreateAudioTrackChunk(track);

        if (!chunk) {
            return -1;
        }

        SDL_assert((track->head == NULL) && (track->tail == NULL) && (track->queued_bytes == 0));
        track->head = chunk;
        track->tail = chunk;
    }

    size_t total = 0;
    size_t old_tail = chunk->tail;
    size_t chunk_size = track->chunk_size;

    while (chunk) {
        size_t to_write = chunk_size - chunk->tail;
        to_write = SDL_min(to_write, len - total);
        SDL_memcpy(&chunk->data[chunk->tail], &data[total], to_write);
        total += to_write;

        chunk->tail += to_write;

        if (total == len) {
            break;
        }

        SDL_AudioChunk *next = CreateAudioTrackChunk(track);
        chunk->next = next;
        chunk = next;
    }

    // Roll back the changes if we couldn't write all the data
    if (!chunk) {
        chunk = track->tail;

        SDL_AudioChunk *next = chunk->next;
        chunk->next = NULL;
        chunk->tail = old_tail;

        DestroyAudioChunks(next);

        return -1;
    }

    track->tail = chunk;
    track->queued_bytes += total;

    return 0;
}

static size_t ReadFromChunkedAudioTrack(void *ctx, Uint8 *data, size_t len, SDL_bool advance)
{
    SDL_ChunkedAudioTrack *track = ctx;
    SDL_AudioChunk *chunk = track->head;

    size_t total = 0;
    size_t head = 0;

    while (chunk) {
        head = chunk->head;

        size_t to_read = chunk->tail - head;
        to_read = SDL_min(to_read, len - total);
        SDL_memcpy(&data[total], &chunk->data[head], to_read);
        total += to_read;

        SDL_AudioChunk *next = chunk->next;

        if (total == len) {
            head += to_read;
            break;
        }

        if (advance) {
            DestroyAudioTrackChunk(track, chunk);
        }

        chunk = next;
    }

    if (advance) {
        if (chunk) {
            chunk->head = head;
            track->head = chunk;
        } else {
            track->head = NULL;
            track->tail = NULL;
        }

        track->queued_bytes -= total;
    }

    return total;
}

static void DestroyChunkedAudioTrack(void *ctx)
{
    SDL_ChunkedAudioTrack *track = ctx;
    DestroyAudioChunks(track->head);
    DestroyAudioChunks(track->free_chunks);
    SDL_free(track);
}

static SDL_AudioTrack *CreateChunkedAudioTrack(const SDL_AudioSpec *spec, size_t chunk_size)
{
    SDL_ChunkedAudioTrack *track = (SDL_ChunkedAudioTrack *)SDL_calloc(1, sizeof(*track));

    if (!track) {
        return NULL;
    }

    SDL_copyp(&track->track.spec, spec);
    track->track.avail = AvailChunkedAudioTrack;
    track->track.write = WriteToChunkedAudioTrack;
    track->track.read = ReadFromChunkedAudioTrack;
    track->track.destroy = DestroyChunkedAudioTrack;

    track->chunk_size = chunk_size;

    return &track->track;
}

SDL_AudioQueue *SDL_CreateAudioQueue(size_t chunk_size)
{
    SDL_AudioQueue *queue = (SDL_AudioQueue *)SDL_calloc(1, sizeof(*queue));

    if (!queue) {
        return NULL;
    }

    queue->chunk_size = chunk_size;

    return queue;
}

void SDL_DestroyAudioQueue(SDL_AudioQueue *queue)
{
    SDL_ClearAudioQueue(queue);

    SDL_free(queue);
}

void SDL_ClearAudioQueue(SDL_AudioQueue *queue)
{
    SDL_AudioTrack *track = queue->head;
    queue->head = NULL;
    queue->tail = NULL;

    while (track) {
        SDL_AudioTrack *next = track->next;
        track->destroy(track);
        track = next;
    }
}

static void SDL_FlushAudioTrack(SDL_AudioTrack *track)
{
    track->flushed = SDL_TRUE;
    track->write = NULL;
}

void SDL_FlushAudioQueue(SDL_AudioQueue *queue)
{
    SDL_AudioTrack *track = queue->tail;

    if (track) {
        SDL_FlushAudioTrack(track);
    }
}

void SDL_PopAudioQueueHead(SDL_AudioQueue *queue)
{
    SDL_AudioTrack *track = queue->head;

    for (;;) {
        SDL_bool flushed = track->flushed;

        SDL_AudioTrack *next = track->next;
        track->destroy(track);
        track = next;

        if (flushed) {
            break;
        }
    }

    queue->head = track;

    if (!track) {
        queue->tail = NULL;
    }
}

size_t SDL_GetAudioQueueChunkSize(SDL_AudioQueue *queue)
{
    return queue->chunk_size;
}

SDL_AudioTrack *SDL_CreateChunkedAudioTrack(const SDL_AudioSpec *spec, const Uint8 *data, size_t len, size_t chunk_size)
{
    SDL_AudioTrack *track = CreateChunkedAudioTrack(spec, chunk_size);

    if (!track) {
        return NULL;
    }

    if (track->write(track, data, len) != 0) {
        track->destroy(track);
        return NULL;
    }

    return track;
}

void SDL_AddTrackToAudioQueue(SDL_AudioQueue *queue, SDL_AudioTrack *track)
{
    SDL_AudioTrack *tail = queue->tail;

    if (tail) {
        // If the spec has changed, make sure to flush the previous track
        if (!AUDIO_SPECS_EQUAL(tail->spec, track->spec)) {
            SDL_FlushAudioTrack(tail);
        }

        tail->next = track;
    } else {
        queue->head = track;
    }

    queue->tail = track;
}

int SDL_WriteToAudioQueue(SDL_AudioQueue *queue, const SDL_AudioSpec *spec, const Uint8 *data, size_t len)
{
    if (len == 0) {
        return 0;
    }

    SDL_AudioTrack *track = queue->tail;

    if ((track) && !AUDIO_SPECS_EQUAL(track->spec, *spec)) {
        SDL_FlushAudioTrack(track);
    }

    if ((!track) || (!track->write)) {
        SDL_AudioTrack *new_track = CreateChunkedAudioTrack(spec, queue->chunk_size);

        if (!new_track) {
            return -1;
        }

        if (track) {
            track->next = new_track;
        } else {
            queue->head = new_track;
        }

        queue->tail = new_track;

        track = new_track;
    }

    return track->write(track, data, len);
}

void *SDL_BeginAudioQueueIter(SDL_AudioQueue *queue)
{
    return queue->head;
}

size_t SDL_NextAudioQueueIter(SDL_AudioQueue *queue, void **inout_iter, SDL_AudioSpec *out_spec, SDL_bool *out_flushed)
{
    SDL_AudioTrack *iter = *inout_iter;
    SDL_assert(iter != NULL);

    SDL_copyp(out_spec, &iter->spec);

    SDL_bool flushed = SDL_FALSE;
    size_t queued_bytes = 0;

    while (iter) {
        SDL_AudioTrack *track = iter;
        iter = iter->next;

        size_t avail = track->avail(track);

        if (avail >= SDL_SIZE_MAX - queued_bytes) {
            queued_bytes = SDL_SIZE_MAX;
            flushed = SDL_FALSE;
            break;
        }

        queued_bytes += avail;
        flushed = track->flushed;

        if (flushed) {
            break;
        }
    }

    *inout_iter = iter;
    *out_flushed = flushed;

    return queued_bytes;
}

int SDL_ReadFromAudioQueue(SDL_AudioQueue *queue, Uint8 *data, size_t len)
{
    size_t total = 0;
    SDL_AudioTrack *track = queue->head;

    for (;;) {
        if (!track) {
            return SDL_SetError("Reading past end of queue");
        }

        total += track->read(track, &data[total], len - total, SDL_TRUE);

        if (total == len) {
            return 0;
        }

        if (track->flushed) {
            return SDL_SetError("Reading past end of flushed track");
        }

        SDL_AudioTrack *next = track->next;

        if (!next) {
            return SDL_SetError("Reading past end of incomplete track");
        }

        queue->head = next;

        track->destroy(track);
        track = next;
    }
}

int SDL_PeekIntoAudioQueue(SDL_AudioQueue *queue, Uint8 *data, size_t len)
{
    size_t total = 0;
    SDL_AudioTrack *track = queue->head;

    for (;;) {
        if (!track) {
            return SDL_SetError("Peeking past end of queue");
        }

        total += track->read(track, &data[total], len - total, SDL_FALSE);

        if (total == len) {
            return 0;
        }

        if (track->flushed) {
            // If we have run out of data, fill the rest with silence.
            SDL_memset(&data[total], SDL_GetSilenceValueForFormat(track->spec.format), len - total);
            return 0;
        }

        track = track->next;
    }
}
