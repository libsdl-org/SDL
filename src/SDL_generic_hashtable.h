/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#include <SDL3/SDL_stdinc.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef SDL_HASHTABLE_TYPENAME
#error SDL_HASHTABLE_TYPENAME is not set
#endif

#ifndef SDL_HASHTABLE_KEY_TYPE
#error SDL_HASHTABLE_KEY_TYPE is not set
#endif

#ifndef SDL_HASHTABLE_HASH_KEY
#error SDL_HASHTABLE_HASH_KEY is not set
#endif

#ifndef SDL_HASHTABLE_KEYS_EQUAL
#error SDL_HASHTABLE_KEYS_EQUAL is not set
#endif

#ifndef SDL_HASHTABLE_FREE_ITEM
#ifdef SDL_HASHTABLE_VALUE_TYPE
#define SDL_HASHTABLE_FREE_ITEM(userdata, key, value) ;
#else
#define SDL_HASHTABLE_FREE_ITEM(userdata, key) ;
#endif
#endif

#ifndef SDL_HASHTABLE_MAX_LOAD_FACTOR
#define SDL_HASHTABLE_MAX_LOAD_FACTOR 128
#endif

#define CONCAT(a, b)        a##_##b
#define EXPAND_CONCAT(a, b) CONCAT(a, b)
#define ADD_PREFIX(name)    EXPAND_CONCAT(SDL_HASHTABLE_TYPENAME, name)

typedef struct ADD_PREFIX(HashItem)
{
    SDL_HASHTABLE_KEY_TYPE key;
#ifdef SDL_HASHTABLE_VALUE_TYPE
    SDL_HASHTABLE_VALUE_TYPE value;
#endif
    Uint32 hash;
    Uint32 probe_len; // equals to 0 when item is not live
} ADD_PREFIX(HashItem);

// Anything larger than this will cause integer overflow
#define SDL_MAX_HASHTABLE_SIZE (0x80000000u / sizeof(ADD_PREFIX(HashItem)))

typedef struct SDL_HASHTABLE_TYPENAME
{
#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_RWLock *lock;
#endif
    ADD_PREFIX(HashItem) *table;
    void *userdata;
    Uint32 hash_mask;
    Uint32 num_occupied_slots;
} SDL_HASHTABLE_TYPENAME;

static SDL_INLINE Uint32 ADD_PREFIX(internal_hash_buckets_from_estimate)(int estimated_capacity)
{
    if (estimated_capacity <= 0) {
        return 4; // start small, grow as necessary.
    }

    const Uint32 estimated32 = (Uint32)estimated_capacity;
    Uint32 buckets = ((Uint32)1) << SDL_MostSignificantBitIndex32(estimated32);
    if (!SDL_HasExactlyOneBitSet32(estimated32)) {
        buckets <<= 1; // need next power of two up to fit overflow capacity bits.
    }

    return SDL_min(buckets, SDL_MAX_HASHTABLE_SIZE);
}

static SDL_INLINE void ADD_PREFIX(Destroy)(SDL_HASHTABLE_TYPENAME *table);

static SDL_INLINE SDL_HASHTABLE_TYPENAME *ADD_PREFIX(Create)(int estimated_capacity, void *userdata)
{
    const Uint32 num_buckets = ADD_PREFIX(internal_hash_buckets_from_estimate)(estimated_capacity);
    SDL_HASHTABLE_TYPENAME *table = (SDL_HASHTABLE_TYPENAME *)SDL_calloc(1, sizeof(SDL_HASHTABLE_TYPENAME));
    if (!table) {
        return NULL;
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    table->lock = SDL_CreateRWLock();
    if (!table->lock) {
        ADD_PREFIX(Destroy)(table);
        return NULL;
    }
#endif

    table->table = (ADD_PREFIX(HashItem) *)SDL_calloc(num_buckets, sizeof(ADD_PREFIX(HashItem)));
    if (!table->table) {
        ADD_PREFIX(Destroy)(table);
        return NULL;
    }

    table->hash_mask = num_buckets - 1;
    table->userdata = userdata;
    return table;
}

static SDL_INLINE Uint32 ADD_PREFIX(internal_get_probe_length)(Uint32 zero_idx, Uint32 actual_idx, Uint32 num_buckets)
{
    // returns the probe sequence length from zero_idx to actual_idx
    if (actual_idx < zero_idx) {
        return num_buckets - zero_idx + actual_idx + 1;
    }

    return actual_idx - zero_idx + 1;
}

static SDL_INLINE ADD_PREFIX(HashItem) * ADD_PREFIX(internal_find_item)(const SDL_HASHTABLE_TYPENAME *ht, SDL_HASHTABLE_KEY_TYPE key, Uint32 hash)
{
    Uint32 hash_mask = ht->hash_mask;
    ADD_PREFIX(HashItem) *table = ht->table;
    Uint32 i = hash & hash_mask;
    Uint32 probe_len = 0;

    while (true) {
        ADD_PREFIX(HashItem) *item = table + i;
        Uint32 item_probe_len = item->probe_len;
        if (++probe_len > item_probe_len) {
            return NULL;
        }

        Uint32 item_hash = item->hash;
        if (item_hash == hash && SDL_HASHTABLE_KEYS_EQUAL(ht->userdata, item->key, key)) {
            return item;
        }

        SDL_assert(item_probe_len == ADD_PREFIX(internal_get_probe_length)(item_hash & hash_mask, (Uint32)(item - table), hash_mask + 1));
        i = (i + 1) & hash_mask;
    }
}

static SDL_INLINE void ADD_PREFIX(internal_insert_item)(ADD_PREFIX(HashItem) * item_to_insert, ADD_PREFIX(HashItem) * table, Uint32 hash_mask)
{
    const Uint32 num_buckets = hash_mask + 1;
    Uint32 idx = item_to_insert->hash & hash_mask;
    ADD_PREFIX(HashItem) *target = NULL;
    ADD_PREFIX(HashItem) temp_item;

    while (true) {
        ADD_PREFIX(HashItem) *candidate = table + idx;

        if (candidate->probe_len == 0) {
            // Found an empty slot. Put it here and we're done.
            *candidate = *item_to_insert;

            if (target == NULL) {
                target = candidate;
            }

            const Uint32 probe_len = ADD_PREFIX(internal_get_probe_length)(candidate->hash & hash_mask, idx, num_buckets);
            candidate->probe_len = probe_len;

            break;
        }

        const Uint32 candidate_probe_len = candidate->probe_len;
        SDL_assert(candidate_probe_len == ADD_PREFIX(internal_get_probe_length)(candidate->hash & hash_mask, idx, num_buckets));
        const Uint32 new_probe_len = ADD_PREFIX(internal_get_probe_length)(item_to_insert->hash & hash_mask, idx, num_buckets);

        if (candidate_probe_len < new_probe_len) {
            // Robin Hood hashing: the item at idx has a better probe length than our item would at this position.
            // Evict it and put our item in its place, then continue looking for a new spot for the displaced item.
            // This algorithm significantly reduces clustering in the table, making lookups take very few probes.

            temp_item = *candidate;
            *candidate = *item_to_insert;

            if (target == NULL) {
                target = candidate;
            }

            *item_to_insert = temp_item;

            SDL_assert(new_probe_len == ADD_PREFIX(internal_get_probe_length)(candidate->hash & hash_mask, idx, num_buckets));
            candidate->probe_len = new_probe_len;
        }

        idx = (idx + 1) & hash_mask;
    }
}

static SDL_INLINE void ADD_PREFIX(internal_delete_item)(SDL_HASHTABLE_TYPENAME *ht, ADD_PREFIX(HashItem) * item)
{
    const Uint32 hash_mask = ht->hash_mask;
    ADD_PREFIX(HashItem) *table = ht->table;

#ifdef SDL_HASHTABLE_VALUE_TYPE
    SDL_HASHTABLE_FREE_ITEM(ht->userdata, item->key, item->value);
#else
    SDL_HASHTABLE_FREE_ITEM(ht->userdata, item->key);
#endif

    SDL_assert(ht->num_occupied_slots > 0);
    ht->num_occupied_slots--;

    Uint32 idx = (Uint32)(item - ht->table);

    while (true) {
        idx = (idx + 1) & hash_mask;
        ADD_PREFIX(HashItem) *next_item = table + idx;

        if (next_item->probe_len <= 1) {
            SDL_zerop(item);
            return;
        }

        *item = *next_item;
        item->probe_len -= 1;
        item = next_item;
    }
}

static SDL_INLINE bool ADD_PREFIX(internal_resize)(SDL_HASHTABLE_TYPENAME *ht, Uint32 new_size)
{
    const Uint32 new_hash_mask = new_size - 1;
    ADD_PREFIX(HashItem) *new_table = (ADD_PREFIX(HashItem) *)SDL_calloc(new_size, sizeof(*new_table));

    if (!new_table) {
        return false;
    }

    ADD_PREFIX(HashItem) *old_table = ht->table;
    const Uint32 old_size = ht->hash_mask + 1;

    ht->hash_mask = new_hash_mask;
    ht->table = new_table;

    for (Uint32 i = 0; i < old_size; ++i) {
        ADD_PREFIX(HashItem) *item = old_table + i;
        if (item->probe_len != 0) {
            ADD_PREFIX(internal_insert_item)
            (item, new_table, new_hash_mask);
        }
    }

    SDL_free(old_table);
    return true;
}

static SDL_INLINE bool ADD_PREFIX(internal_maybe_resize)(SDL_HASHTABLE_TYPENAME *ht)
{
    const Uint32 capacity = ht->hash_mask + 1;
    if (capacity >= SDL_MAX_HASHTABLE_SIZE) {
        return false;
    }

    const Uint32 resize_threshold = (Uint32)((SDL_HASHTABLE_MAX_LOAD_FACTOR * (Uint64)capacity) >> 8);
    if (ht->num_occupied_slots > resize_threshold) {
        return ADD_PREFIX(internal_resize)(ht, capacity * 2);
    }

    return true;
}

#ifdef SDL_HASHTABLE_VALUE_TYPE
static SDL_INLINE bool ADD_PREFIX(Insert)(SDL_HASHTABLE_TYPENAME *table, SDL_HASHTABLE_KEY_TYPE key, SDL_HASHTABLE_VALUE_TYPE value, bool replace)
#else
static SDL_INLINE bool ADD_PREFIX(Insert)(SDL_HASHTABLE_TYPENAME *table, SDL_HASHTABLE_KEY_TYPE key, bool replace)
#endif
{
    if (!table) {
        return SDL_InvalidParamError("table");
    }

    bool result = false;

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_LockRWLockForWriting(table->lock);
#endif

    const Uint32 hash = SDL_HASHTABLE_HASH_KEY(table->userdata, key);
    ADD_PREFIX(HashItem) *item = ADD_PREFIX(internal_find_item)(table, key, hash);
    bool do_insert = true;

    if (item) {
        if (replace) {
            ADD_PREFIX(internal_delete_item)(table, item);
        } else {
            SDL_SetError("key already exists and replace is disabled");
            do_insert = false;
        }
    }

    if (do_insert) {
        ADD_PREFIX(HashItem)
        new_item;
        new_item.key = key;
#ifdef SDL_HASHTABLE_VALUE_TYPE
        new_item.value = value;
#endif
        new_item.hash = hash;
        new_item.probe_len = 1;

        table->num_occupied_slots++;

        if (!ADD_PREFIX(internal_maybe_resize)(table)) {
            table->num_occupied_slots--;
        } else {
            ADD_PREFIX(internal_insert_item)(&new_item, table->table, table->hash_mask);
            result = true;
        }
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_UnlockRWLock(table->lock);
#endif
    return result;
}

#ifdef SDL_HASHTABLE_VALUE_TYPE
static SDL_INLINE bool ADD_PREFIX(Find)(const SDL_HASHTABLE_TYPENAME *table, SDL_HASHTABLE_KEY_TYPE key, SDL_HASHTABLE_VALUE_TYPE *value)
#else
static SDL_INLINE bool ADD_PREFIX(Find)(const SDL_HASHTABLE_TYPENAME *table, SDL_HASHTABLE_KEY_TYPE key)
#endif
{
    if (!table) {
        return SDL_InvalidParamError("table");
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_LockRWLockForReading(table->lock);
#endif

    bool result = false;
    const Uint32 hash = SDL_HASHTABLE_HASH_KEY(table->userdata, key);
    ADD_PREFIX(HashItem) *i = ADD_PREFIX(internal_find_item)(table, key, hash);
    if (i) {
#ifdef SDL_HASHTABLE_VALUE_TYPE
        if (value) {
            *value = i->value;
        }
#endif
        result = true;
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_UnlockRWLock(table->lock);
#endif
    return result;
}

static SDL_INLINE bool ADD_PREFIX(Remove)(SDL_HASHTABLE_TYPENAME *table, const SDL_HASHTABLE_KEY_TYPE key)
{
    if (!table) {
        return SDL_InvalidParamError("table");
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_LockRWLockForWriting(table->lock);
#endif

    bool result = false;
    const Uint32 hash = SDL_HASHTABLE_HASH_KEY(table->userdata, key);
    ADD_PREFIX(HashItem) *item = ADD_PREFIX(internal_find_item)(table, key, hash);
    if (item) {
        ADD_PREFIX(internal_delete_item)(table, item);
        result = true;
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_UnlockRWLock(table->lock);
#endif
    return result;
}

#ifdef SDL_HASHTABLE_VALUE_TYPE
typedef bool(SDLCALL *ADD_PREFIX(IterateCallback))(void *userdata, const SDL_HASHTABLE_TYPENAME *table, SDL_HASHTABLE_KEY_TYPE key, SDL_HASHTABLE_VALUE_TYPE value);
#else
typedef bool(SDLCALL *ADD_PREFIX(IterateCallback))(void *userdata, const SDL_HASHTABLE_TYPENAME *table, SDL_HASHTABLE_KEY_TYPE key);
#endif

static SDL_INLINE bool ADD_PREFIX(Iterate)(const SDL_HASHTABLE_TYPENAME *table, ADD_PREFIX(IterateCallback) callback, void *userdata)
{
    if (!table) {
        return SDL_InvalidParamError("table");
    }
    if (!callback) {
        return SDL_InvalidParamError("callback");
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_LockRWLockForReading(table->lock);
#endif
    ADD_PREFIX(HashItem) *end = table->table + (table->hash_mask + 1);
    Uint32 num_iterated = 0;

    for (ADD_PREFIX(HashItem) *item = table->table; item < end; item++) {
        if (item->probe_len != 0) {
#ifdef SDL_HASHTABLE_VALUE_TYPE
            bool res = callback(userdata, table, item->key, item->value);
#else
            bool res = callback(userdata, table, item->key);
#endif
            if (!res || ++num_iterated >= table->num_occupied_slots) {
                break;
            }
        }
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_UnlockRWLock(table->lock);
#endif
    return true;
}

static SDL_INLINE bool ADD_PREFIX(Empty)(SDL_HASHTABLE_TYPENAME *table)
{
    if (!table) {
        return SDL_InvalidParamError("table");
    }

#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_LockRWLockForReading(table->lock);
#endif
    const bool retval = (table->num_occupied_slots == 0);
#ifdef SDL_HASHTABLE_THREAD_SAFE
    SDL_UnlockRWLock(table->lock);
#endif
    return retval;
}

static SDL_INLINE void ADD_PREFIX(internal_destroy_all)(SDL_HASHTABLE_TYPENAME *table)
{
    void *userdata = table->userdata;
    ADD_PREFIX(HashItem) *end = table->table + (table->hash_mask + 1);
    for (ADD_PREFIX(HashItem) *i = table->table; i < end; ++i) {
        if (i->probe_len != 0) {
            i->probe_len = 0;
#ifdef SDL_HASHTABLE_VALUE_TYPE
            SDL_HASHTABLE_FREE_ITEM(userdata, i->key, i->value);
#else
            SDL_HASHTABLE_FREE_ITEM(userdata, i->key);
#endif
        }
    }
}

static SDL_INLINE void ADD_PREFIX(Clear)(SDL_HASHTABLE_TYPENAME *table)
{
    if (table) {
#ifdef SDL_HASHTABLE_THREAD_SAFE
        SDL_LockRWLockForWriting(table->lock);
#endif
        ADD_PREFIX(internal_destroy_all)(table);
        SDL_memset(table->table, 0, sizeof(*table->table) * (table->hash_mask + 1));
        table->num_occupied_slots = 0;
#ifdef SDL_HASHTABLE_THREAD_SAFE
        SDL_UnlockRWLock(table->lock);
#endif
    }
}

static SDL_INLINE void ADD_PREFIX(Destroy)(SDL_HASHTABLE_TYPENAME *table)
{
    if (table) {
        ADD_PREFIX(internal_destroy_all)(table);
#ifdef SDL_HASHTABLE_THREAD_SAFE
        SDL_DestroyRWLock(table->lock);
#endif
        SDL_free(table->table);
        SDL_free(table);
    }
}

#undef SDL_HASHTABLE_TYPENAME
#undef SDL_HASHTABLE_KEY_TYPE
#undef SDL_HASHTABLE_VALUE_TYPE
#undef SDL_HASHTABLE_HASH_KEY
#undef SDL_HASHTABLE_KEYS_EQUAL
#undef SDL_HASHTABLE_FREE_ITEM
#undef SDL_HASHTABLE_THREAD_SAFE
#undef SDL_HASHTABLE_MAX_LOAD_FACTOR

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>
