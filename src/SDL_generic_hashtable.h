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

#ifndef SDL_generic_hashtable_h_
#define SDL_generic_hashtable_h_

#include <SDL3/SDL_stdinc.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_GenericHashItem
{
    const void *key;
    const void *value;
    Uint32 hash;
    Uint32 probe_len; // equals to 0 when item is not live
} SDL_GenericHashItem;

// Must be a power of 2 >= sizeof(SDL_GenericHashItem)
#define SDL_GHT_MAX_HASHITEM_SIZEOF 32u
SDL_COMPILE_TIME_ASSERT(sizeof_SDL_GenericHashItem, sizeof(SDL_GenericHashItem) <= SDL_GHT_MAX_HASHITEM_SIZEOF);

// Anything larger than this will cause integer overflows
#define SDL_GHT_MAX_HASHTABLE_SIZE (0x80000000u / (SDL_GHT_MAX_HASHITEM_SIZEOF))

#define SDL_MAKE_HASHTABLE_TYPE(type, hash_callback, keymatch_callback, destroy_callback, threadsafe) \
    SDL_MAKE_HASHTABLE_TYPE_EXT(type, hash_callback, keymatch_callback, destroy_callback, threadsafe, 128)

#define SDL_MAKE_HASHTABLE_TYPE_EXT(type, hash_callback, keymatch_callback, destroy_callback, threadsafe, max_load_factor)                \
    typedef struct type                                                                                                                   \
    {                                                                                                                                     \
        SDL_RWLock *lock; /* NULL if not created threadsafe */                                                                            \
        SDL_GenericHashItem *table;                                                                                                       \
        void *userdata;                                                                                                                   \
        Uint32 hash_mask;                                                                                                                 \
        Uint32 num_occupied_slots;                                                                                                        \
    } type;                                                                                                                               \
                                                                                                                                          \
    typedef bool(SDLCALL * type##_IterateCallback)(void *userdata, const type *table, const void *key, const void *value);                \
                                                                                                                                          \
    static void type##_Destroy(type *table);                                                                                              \
                                                                                                                                          \
    static SDL_INLINE Uint32 type##_internal_hash_buckets_from_estimate(int estimated_capacity)                                           \
    {                                                                                                                                     \
        if (estimated_capacity <= 0) {                                                                                                    \
            return 4; /* start small, grow as necessary. */                                                                               \
        }                                                                                                                                 \
                                                                                                                                          \
        const Uint32 estimated32 = (Uint32)estimated_capacity;                                                                            \
        Uint32 buckets = ((Uint32)1) << SDL_MostSignificantBitIndex32(estimated32);                                                       \
        if (!SDL_HasExactlyOneBitSet32(estimated32)) {                                                                                    \
            buckets <<= 1; /* need next power of two up to fit overflow capacity bits. */                                                 \
        }                                                                                                                                 \
                                                                                                                                          \
        return SDL_min(buckets, SDL_GHT_MAX_HASHTABLE_SIZE);                                                                              \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE type *type##_Create(int estimated_capacity, void *userdata)                                                         \
    {                                                                                                                                     \
        const Uint32 num_buckets = type##_internal_hash_buckets_from_estimate(estimated_capacity);                                        \
        type *table = (type *)SDL_calloc(1, sizeof(type));                                                                                \
        if (!table) {                                                                                                                     \
            return NULL;                                                                                                                  \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            table->lock = SDL_CreateRWLock();                                                                                             \
            if (!table->lock) {                                                                                                           \
                type##_Destroy(table);                                                                                                    \
                return NULL;                                                                                                              \
            }                                                                                                                             \
        }                                                                                                                                 \
                                                                                                                                          \
        table->table = (SDL_GenericHashItem *)SDL_calloc(num_buckets, sizeof(SDL_GenericHashItem));                                       \
        if (!table->table) {                                                                                                              \
            type##_Destroy(table);                                                                                                        \
            return NULL;                                                                                                                  \
        }                                                                                                                                 \
                                                                                                                                          \
        table->hash_mask = num_buckets - 1;                                                                                               \
        table->userdata = userdata;                                                                                                       \
        return table;                                                                                                                     \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE Uint32 type##_internal_calc_hash(const type *table, const void *key)                                                \
    {                                                                                                                                     \
        const Uint32 BitMixer = 0x9E3779B1u;                                                                                              \
        return hash_callback(table->userdata, key) * BitMixer;                                                                            \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE Uint32 type##_internal_get_probe_length(Uint32 zero_idx, Uint32 actual_idx, Uint32 num_buckets)                     \
    {                                                                                                                                     \
        /* returns the probe sequence length from zero_idx to actual_idx */                                                               \
        if (actual_idx < zero_idx) {                                                                                                      \
            return num_buckets - zero_idx + actual_idx + 1;                                                                               \
        }                                                                                                                                 \
                                                                                                                                          \
        return actual_idx - zero_idx + 1;                                                                                                 \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE SDL_GenericHashItem *type##_internal_find_item(const type *ht, const void *key, Uint32 hash)                        \
    {                                                                                                                                     \
        Uint32 hash_mask = ht->hash_mask;                                                                                                 \
        SDL_GenericHashItem *table = ht->table;                                                                                           \
        Uint32 i = hash & hash_mask;                                                                                                      \
        Uint32 probe_len = 0;                                                                                                             \
                                                                                                                                          \
        while (true) {                                                                                                                    \
            SDL_GenericHashItem *item = table + i;                                                                                        \
            Uint32 item_probe_len = item->probe_len;                                                                                      \
            if (++probe_len > item_probe_len) {                                                                                           \
                return NULL;                                                                                                              \
            }                                                                                                                             \
                                                                                                                                          \
            Uint32 item_hash = item->hash;                                                                                                \
            if (item_hash == hash && keymatch_callback(ht->userdata, item->key, key)) {                                                   \
                return item;                                                                                                              \
            }                                                                                                                             \
                                                                                                                                          \
            SDL_assert(item_probe_len == type##_internal_get_probe_length(item_hash & hash_mask, (Uint32)(item - table), hash_mask + 1)); \
            i = (i + 1) & hash_mask;                                                                                                      \
        }                                                                                                                                 \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE void type##_internal_insert_item(SDL_GenericHashItem *item_to_insert, SDL_GenericHashItem *table, Uint32 hash_mask) \
    {                                                                                                                                     \
        const Uint32 num_buckets = hash_mask + 1;                                                                                         \
        Uint32 idx = item_to_insert->hash & hash_mask;                                                                                    \
        SDL_GenericHashItem *target = NULL;                                                                                               \
        SDL_GenericHashItem temp_item;                                                                                                    \
                                                                                                                                          \
        while (true) {                                                                                                                    \
            SDL_GenericHashItem *candidate = table + idx;                                                                                 \
                                                                                                                                          \
            if (candidate->probe_len == 0) {                                                                                              \
                /* Found an empty slot. Put it here and we're done. */                                                                    \
                *candidate = *item_to_insert;                                                                                             \
                                                                                                                                          \
                if (target == NULL) {                                                                                                     \
                    target = candidate;                                                                                                   \
                }                                                                                                                         \
                                                                                                                                          \
                const Uint32 probe_len = type##_internal_get_probe_length(candidate->hash & hash_mask, idx, num_buckets);                 \
                candidate->probe_len = probe_len;                                                                                         \
                                                                                                                                          \
                break;                                                                                                                    \
            }                                                                                                                             \
                                                                                                                                          \
            const Uint32 candidate_probe_len = candidate->probe_len;                                                                      \
            SDL_assert(candidate_probe_len == type##_internal_get_probe_length(candidate->hash & hash_mask, idx, num_buckets));           \
            const Uint32 new_probe_len = type##_internal_get_probe_length(item_to_insert->hash & hash_mask, idx, num_buckets);            \
                                                                                                                                          \
            if (candidate_probe_len < new_probe_len) {                                                                                    \
                /* Robin Hood hashing: the item at idx has a better probe length than our item would at this position. */                 \
                /* Evict it and put our item in its place, then continue looking for a new spot for the displaced item. */                \
                /* This algorithm significantly reduces clustering in the table, making lookups take very few probes. */                  \
                                                                                                                                          \
                temp_item = *candidate;                                                                                                   \
                *candidate = *item_to_insert;                                                                                             \
                                                                                                                                          \
                if (target == NULL) {                                                                                                     \
                    target = candidate;                                                                                                   \
                }                                                                                                                         \
                                                                                                                                          \
                *item_to_insert = temp_item;                                                                                              \
                                                                                                                                          \
                SDL_assert(new_probe_len == type##_internal_get_probe_length(candidate->hash & hash_mask, idx, num_buckets));             \
                candidate->probe_len = new_probe_len;                                                                                     \
            }                                                                                                                             \
                                                                                                                                          \
            idx = (idx + 1) & hash_mask;                                                                                                  \
        }                                                                                                                                 \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE void type##_internal_delete_item(type *ht, SDL_GenericHashItem *item)                                               \
    {                                                                                                                                     \
        const Uint32 hash_mask = ht->hash_mask;                                                                                           \
        SDL_GenericHashItem *table = ht->table;                                                                                           \
                                                                                                                                          \
        destroy_callback(ht->userdata, item->key, item->value);                                                                           \
                                                                                                                                          \
        SDL_assert(ht->num_occupied_slots > 0);                                                                                           \
        ht->num_occupied_slots--;                                                                                                         \
                                                                                                                                          \
        Uint32 idx = (Uint32)(item - ht->table);                                                                                          \
                                                                                                                                          \
        while (true) {                                                                                                                    \
            idx = (idx + 1) & hash_mask;                                                                                                  \
            SDL_GenericHashItem *next_item = table + idx;                                                                                 \
                                                                                                                                          \
            if (next_item->probe_len <= 1) {                                                                                              \
                SDL_zerop(item);                                                                                                          \
                return;                                                                                                                   \
            }                                                                                                                             \
                                                                                                                                          \
            *item = *next_item;                                                                                                           \
            item->probe_len -= 1;                                                                                                         \
            item = next_item;                                                                                                             \
        }                                                                                                                                 \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE bool type##_internal_resize(type *ht, Uint32 new_size)                                                              \
    {                                                                                                                                     \
        const Uint32 new_hash_mask = new_size - 1;                                                                                        \
        SDL_GenericHashItem *new_table = (SDL_GenericHashItem *)SDL_calloc(new_size, sizeof(*new_table));                                 \
                                                                                                                                          \
        if (!new_table) {                                                                                                                 \
            return false;                                                                                                                 \
        }                                                                                                                                 \
                                                                                                                                          \
        SDL_GenericHashItem *old_table = ht->table;                                                                                       \
        const Uint32 old_size = ht->hash_mask + 1;                                                                                        \
                                                                                                                                          \
        ht->hash_mask = new_hash_mask;                                                                                                    \
        ht->table = new_table;                                                                                                            \
                                                                                                                                          \
        for (Uint32 i = 0; i < old_size; ++i) {                                                                                           \
            SDL_GenericHashItem *item = old_table + i;                                                                                    \
            if (item->probe_len != 0) {                                                                                                   \
                type##_internal_insert_item(item, new_table, new_hash_mask);                                                              \
            }                                                                                                                             \
        }                                                                                                                                 \
                                                                                                                                          \
        SDL_free(old_table);                                                                                                              \
        return true;                                                                                                                      \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE bool type##_internal_maybe_resize(type *ht)                                                                         \
    {                                                                                                                                     \
        const Uint32 capacity = ht->hash_mask + 1;                                                                                        \
        if (capacity >= SDL_GHT_MAX_HASHTABLE_SIZE) {                                                                                     \
            return false;                                                                                                                 \
        }                                                                                                                                 \
                                                                                                                                          \
        const Uint32 resize_threshold = (Uint32)((max_load_factor * (Uint64)capacity) >> 8);                                              \
        if (ht->num_occupied_slots > resize_threshold) {                                                                                  \
            return type##_internal_resize(ht, capacity * 2);                                                                              \
        }                                                                                                                                 \
                                                                                                                                          \
        return true;                                                                                                                      \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE bool type##_Insert(type *table, const void *key, const void *value, bool replace)                                   \
    {                                                                                                                                     \
        if (!table) {                                                                                                                     \
            return SDL_InvalidParamError("table");                                                                                        \
        }                                                                                                                                 \
                                                                                                                                          \
        bool result = false;                                                                                                              \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_LockRWLockForWriting(table->lock);                                                                                        \
        }                                                                                                                                 \
                                                                                                                                          \
        const Uint32 hash = type##_internal_calc_hash(table, key);                                                                        \
        SDL_GenericHashItem *item = type##_internal_find_item(table, key, hash);                                                          \
        bool do_insert = true;                                                                                                            \
                                                                                                                                          \
        if (item) {                                                                                                                       \
            if (replace) {                                                                                                                \
                type##_internal_delete_item(table, item);                                                                                 \
            } else {                                                                                                                      \
                SDL_SetError("key already exists and replace is disabled");                                                               \
                do_insert = false;                                                                                                        \
            }                                                                                                                             \
        }                                                                                                                                 \
                                                                                                                                          \
        if (do_insert) {                                                                                                                  \
            SDL_GenericHashItem new_item;                                                                                                 \
            new_item.key = key;                                                                                                           \
            new_item.value = value;                                                                                                       \
            new_item.hash = hash;                                                                                                         \
            new_item.probe_len = 1;                                                                                                       \
                                                                                                                                          \
            table->num_occupied_slots++;                                                                                                  \
                                                                                                                                          \
            if (!type##_internal_maybe_resize(table)) {                                                                                   \
                table->num_occupied_slots--;                                                                                              \
            } else {                                                                                                                      \
                type##_internal_insert_item(&new_item, table->table, table->hash_mask);                                                   \
                result = true;                                                                                                            \
            }                                                                                                                             \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_UnlockRWLock(table->lock);                                                                                                \
        }                                                                                                                                 \
        return result;                                                                                                                    \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE bool type##_Find(const type *table, const void *key, const void **value)                                            \
    {                                                                                                                                     \
        if (!table) {                                                                                                                     \
            if (value) {                                                                                                                  \
                *value = NULL;                                                                                                            \
            }                                                                                                                             \
            return SDL_InvalidParamError("table");                                                                                        \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_LockRWLockForReading(table->lock);                                                                                        \
        }                                                                                                                                 \
                                                                                                                                          \
        bool result = false;                                                                                                              \
        const Uint32 hash = type##_internal_calc_hash(table, key);                                                                        \
        SDL_GenericHashItem *i = type##_internal_find_item(table, key, hash);                                                             \
        if (i) {                                                                                                                          \
            if (value) {                                                                                                                  \
                *value = i->value;                                                                                                        \
            }                                                                                                                             \
            result = true;                                                                                                                \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_UnlockRWLock(table->lock);                                                                                                \
        }                                                                                                                                 \
                                                                                                                                          \
        return result;                                                                                                                    \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE bool type##_Remove(type *table, const void *key)                                                                    \
    {                                                                                                                                     \
        if (!table) {                                                                                                                     \
            return SDL_InvalidParamError("table");                                                                                        \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_LockRWLockForWriting(table->lock);                                                                                        \
        }                                                                                                                                 \
                                                                                                                                          \
        bool result = false;                                                                                                              \
        const Uint32 hash = type##_internal_calc_hash(table, key);                                                                        \
        SDL_GenericHashItem *item = type##_internal_find_item(table, key, hash);                                                          \
        if (item) {                                                                                                                       \
            type##_internal_delete_item(table, item);                                                                                     \
            result = true;                                                                                                                \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_UnlockRWLock(table->lock);                                                                                                \
        }                                                                                                                                 \
        return result;                                                                                                                    \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE bool type##_Iterate(const type *table, type##_IterateCallback callback, void *userdata)                             \
    {                                                                                                                                     \
        if (!table) {                                                                                                                     \
            return SDL_InvalidParamError("table");                                                                                        \
        } else if (!callback) {                                                                                                           \
            return SDL_InvalidParamError("callback");                                                                                     \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_LockRWLockForReading(table->lock);                                                                                        \
        }                                                                                                                                 \
        SDL_GenericHashItem *end = table->table + (table->hash_mask + 1);                                                                 \
        Uint32 num_iterated = 0;                                                                                                          \
                                                                                                                                          \
        for (SDL_GenericHashItem *item = table->table; item < end; item++) {                                                              \
            if (item->probe_len != 0) {                                                                                                   \
                if (!callback(userdata, table, item->key, item->value)) {                                                                 \
                    break; /* callback requested iteration stop. */                                                                       \
                } else if (++num_iterated >= table->num_occupied_slots) {                                                                 \
                    break; /* we can drop out early because we've seen all the live items. */                                             \
                }                                                                                                                         \
            }                                                                                                                             \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_UnlockRWLock(table->lock);                                                                                                \
        }                                                                                                                                 \
        return true;                                                                                                                      \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE bool type##_Empty(type *table)                                                                                      \
    {                                                                                                                                     \
        if (!table) {                                                                                                                     \
            return SDL_InvalidParamError("table");                                                                                        \
        }                                                                                                                                 \
                                                                                                                                          \
        if (threadsafe) {                                                                                                                 \
            SDL_LockRWLockForReading(table->lock);                                                                                        \
        }                                                                                                                                 \
        const bool retval = (table->num_occupied_slots == 0);                                                                             \
        if (threadsafe) {                                                                                                                 \
            SDL_UnlockRWLock(table->lock);                                                                                                \
        }                                                                                                                                 \
        return retval;                                                                                                                    \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE void type##_internal_destroy_all(type *table)                                                                       \
    {                                                                                                                                     \
        void *userdata = table->userdata;                                                                                                 \
        SDL_GenericHashItem *end = table->table + (table->hash_mask + 1);                                                                 \
        for (SDL_GenericHashItem *i = table->table; i < end; ++i) {                                                                       \
            if (i->probe_len != 0) {                                                                                                      \
                i->probe_len = 0;                                                                                                         \
                destroy_callback(userdata, i->key, i->value);                                                                             \
            }                                                                                                                             \
        }                                                                                                                                 \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE void type##_Clear(type *table)                                                                                      \
    {                                                                                                                                     \
        if (table) {                                                                                                                      \
            if (threadsafe) {                                                                                                             \
                SDL_LockRWLockForWriting(table->lock);                                                                                    \
            }                                                                                                                             \
            {                                                                                                                             \
                type##_internal_destroy_all(table);                                                                                       \
                SDL_memset(table->table, 0, sizeof(*table->table) * (table->hash_mask + 1));                                              \
                table->num_occupied_slots = 0;                                                                                            \
            }                                                                                                                             \
            if (threadsafe) {                                                                                                             \
                SDL_UnlockRWLock(table->lock);                                                                                            \
            }                                                                                                                             \
        }                                                                                                                                 \
    }                                                                                                                                     \
                                                                                                                                          \
    static SDL_INLINE void type##_Destroy(type *table)                                                                                    \
    {                                                                                                                                     \
        if (table) {                                                                                                                      \
            type##_internal_destroy_all(table);                                                                                           \
            if (table->lock) {                                                                                                            \
                SDL_DestroyRWLock(table->lock);                                                                                           \
            }                                                                                                                             \
            SDL_free(table->table);                                                                                                       \
            SDL_free(table);                                                                                                              \
        }                                                                                                                                 \
    }

// this is djb's xor hashing function.
SDL_INLINE Uint32 SDL_internal_hash_string_djbxor(const char *str, size_t len)
{
    Uint32 hash = 5381;
    while (len--) {
        hash = ((hash << 5) + hash) ^ *(str++);
    }
    return hash;
}

SDL_INLINE Uint32 SDL_HashPointerInline(void *unused, const void *key)
{
    return SDL_murmur3_32(&key, sizeof(key), 0);
}

SDL_INLINE bool SDL_KeyMatchPointerInline(void *unused, const void *a, const void *b)
{
    return (a == b);
}

SDL_INLINE Uint32 SDL_HashStringInline(void *unused, const void *key)
{
    const char *str = (const char *)key;
    return SDL_internal_hash_string_djbxor(str, SDL_strlen(str));
}

SDL_INLINE bool SDL_KeyMatchStringInline(void *unused, const void *a, const void *b)
{
    const char *a_string = (const char *)a;
    const char *b_string = (const char *)b;

    (void)unused;
    if (a == b) {
        return true; // same pointer, must match.
    } else if (!a || !b) {
        return false; // one pointer is NULL (and first test shows they aren't the same pointer), must not match.
    } else if (a_string[0] != b_string[0]) {
        return false; // we know they don't match
    }
    return (SDL_strcmp(a_string, b_string) == 0); // Check against actual string contents.
}

// We assume we can fit the ID in the key directly
SDL_COMPILE_TIME_ASSERT(SDL_HashID_KeySize, sizeof(Uint32) <= sizeof(const void *));

SDL_INLINE Uint32 SDL_HashIDInline(void *unused, const void *key)
{
    return (Uint32)(uintptr_t)key;
}

SDL_INLINE bool SDL_KeyMatchIDInline(void *unused, const void *a, const void *b)
{
    return (a == b);
}

SDL_INLINE void SDL_DestroyEmptyInline(void *unused1, const void *unused2, const void *unused3)
{
}

SDL_INLINE void SDL_DestroyHashKeyAndValueInline(void *unused, const void *key, const void *value)
{
    (void)unused;
    SDL_free((void *)key);
    SDL_free((void *)value);
}

SDL_INLINE void SDL_DestroyHashKeyInline(void *unused, const void *key, const void *value)
{
    (void)value;
    (void)unused;
    SDL_free((void *)key);
}

SDL_INLINE void SDL_DestroyHashValueInline(void *unused, const void *key, const void *value)
{
    (void)key;
    (void)unused;
    SDL_free((void *)value);
}

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_generic_hashtable_h_ */
