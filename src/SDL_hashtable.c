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
#include "SDL_internal.h"
#include "SDL_hashtable.h"

// XXX: We can't use SDL_assert here because it's going to call into hashtable code
#ifdef NDEBUG
#define HT_ASSERT(x) (void)(0)
#else
#if (defined(_WIN32) || defined(SDL_PLATFORM_CYGWIN)) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
#include <windows.h>
#endif
/* This is not declared in any header, although it is shared between some
   parts of SDL, because we don't want anything calling it without an
   extremely good reason. */
extern SDL_NORETURN void SDL_ExitProcess(int exitcode);
static void HT_ASSERT_FAIL(const char *msg)
{
    const char *caption = "SDL_HashTable Assertion Failure!";
    (void)caption;
#if (defined(_WIN32) || defined(SDL_PLATFORM_CYGWIN)) && !defined(SDL_PLATFORM_XBOXONE) && !defined(SDL_PLATFORM_XBOXSERIES)
    MessageBoxA(NULL, msg, caption, MB_OK | MB_ICONERROR);
#elif defined(HAVE_STDIO_H)
    fprintf(stderr, "\n\n%s\n%s\n\n", caption, msg);
    fflush(stderr);
#endif
    SDL_TriggerBreakpoint();
    SDL_ExitProcess(-1);
}
#define HT_ASSERT(x) if (!(x)) HT_ASSERT_FAIL("SDL_HashTable Assertion Failure: " #x)
#endif

typedef struct SDL_HashItem
{
    // TODO: Splitting off values into a separate array might be more cache-friendly
    const void *key;
    const void *value;
    Uint32 hash;
    Uint32 probe_len : 31;
    Uint32 live : 1;
} SDL_HashItem;

// Must be a power of 2 >= sizeof(SDL_HashItem)
#define MAX_HASHITEM_SIZEOF 32u
SDL_COMPILE_TIME_ASSERT(sizeof_SDL_HashItem, sizeof(SDL_HashItem) <= MAX_HASHITEM_SIZEOF);

// Anything larger than this will cause integer overflows
#define MAX_HASHTABLE_SIZE (0x80000000u / (MAX_HASHITEM_SIZEOF))

struct SDL_HashTable
{
    SDL_RWLock *lock;
    SDL_HashItem *table;
    SDL_HashTable_HashFn hash;
    SDL_HashTable_KeyMatchFn keymatch;
    SDL_HashTable_NukeFn nuke;
    void *data;
    Uint32 hash_mask;
    Uint32 max_probe_len;
    Uint32 num_occupied_slots;
    bool stackable;
};

SDL_HashTable *SDL_CreateHashTable(void *data,
                                   Uint32 num_buckets,
                                   SDL_HashTable_HashFn hashfn,
                                   SDL_HashTable_KeyMatchFn keymatchfn,
                                   SDL_HashTable_NukeFn nukefn,
                                   bool threadsafe,
                                   bool stackable)
{
    SDL_HashTable *table;

    // num_buckets must be a power of two so we can derive the bucket index with just a bit-and.
    if ((num_buckets < 1) || !SDL_HasExactlyOneBitSet32(num_buckets)) {
        SDL_SetError("num_buckets must be a power of two");
        return NULL;
    }

    if (num_buckets > MAX_HASHTABLE_SIZE) {
        SDL_SetError("num_buckets is too large");
        return NULL;
    }

    table = (SDL_HashTable *)SDL_calloc(1, sizeof(SDL_HashTable));
    if (!table) {
        return NULL;
    }

    if (threadsafe) {
        // Don't fail if we can't create a lock (single threaded environment?)
        table->lock = SDL_CreateRWLock();
    }

    table->table = (SDL_HashItem *)SDL_calloc(num_buckets, sizeof(SDL_HashItem));
    if (!table->table) {
        SDL_DestroyHashTable(table);
        return NULL;
    }

    table->hash_mask = num_buckets - 1;
    table->stackable = stackable;
    table->data = data;
    table->hash = hashfn;
    table->keymatch = keymatchfn;
    table->nuke = nukefn;
    return table;
}

static SDL_INLINE Uint32 calc_hash(const SDL_HashTable *table, const void *key)
{
    const Uint32 BitMixer = 0x9E3779B1u;
    return table->hash(key, table->data) * BitMixer;
}

static SDL_INLINE Uint32 get_probe_length(Uint32 zero_idx, Uint32 actual_idx, Uint32 num_buckets)
{
    // returns the probe sequence length from zero_idx to actual_idx

    if (actual_idx < zero_idx) {
        return num_buckets - zero_idx + actual_idx;
    }

    return actual_idx - zero_idx;
}

static SDL_HashItem *find_item(const SDL_HashTable *ht, const void *key, Uint32 hash, Uint32 *i, Uint32 *probe_len)
{
    Uint32 hash_mask = ht->hash_mask;
    Uint32 max_probe_len = ht->max_probe_len;

    SDL_HashItem *table = ht->table;

    for (;;) {
        SDL_HashItem *item = table + *i;
        Uint32 item_hash = item->hash;

        if (!item->live) {
            return NULL;
        }

        if (item_hash == hash && ht->keymatch(item->key, key, ht->data)) {
            return item;
        }

        Uint32 item_probe_len = item->probe_len;
        HT_ASSERT(item_probe_len == get_probe_length(item_hash & hash_mask, (Uint32)(item - table), hash_mask + 1));

        if (*probe_len > item_probe_len) {
            return NULL;
        }

        if (++*probe_len > max_probe_len) {
            return NULL;
        }

        *i = (*i + 1) & hash_mask;
    }
}

static SDL_HashItem *find_first_item(const SDL_HashTable *ht, const void *key, Uint32 hash)
{
    Uint32 i = hash & ht->hash_mask;
    Uint32 probe_len = 0;
    return find_item(ht, key, hash, &i, &probe_len);
}

static SDL_HashItem *insert_item(SDL_HashItem *item_to_insert, SDL_HashItem *table, Uint32 hash_mask, Uint32 *max_probe_len_ptr)
{
    Uint32 idx = item_to_insert->hash & hash_mask;
    SDL_HashItem temp_item, *target = NULL;
    Uint32 num_buckets = hash_mask + 1;

    for (;;) {
        SDL_HashItem *candidate = table + idx;

        if (!candidate->live) {
            // Found an empty slot. Put it here and we're done.

            *candidate = *item_to_insert;

            if (target == NULL) {
                target = candidate;
            }

            Uint32 probe_len = get_probe_length(candidate->hash & hash_mask, idx, num_buckets);
            candidate->probe_len = probe_len;

            if (*max_probe_len_ptr < probe_len) {
                *max_probe_len_ptr = probe_len;
            }

            break;
        }

        Uint32 candidate_probe_len = candidate->probe_len;
        HT_ASSERT(candidate_probe_len == get_probe_length(candidate->hash & hash_mask, idx, num_buckets));
        Uint32 new_probe_len = get_probe_length(item_to_insert->hash & hash_mask, idx, num_buckets);

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

            HT_ASSERT(new_probe_len == get_probe_length(candidate->hash & hash_mask, idx, num_buckets));
            candidate->probe_len = new_probe_len;

            if (*max_probe_len_ptr < new_probe_len) {
                *max_probe_len_ptr = new_probe_len;
            }
        }

        idx = (idx + 1) & hash_mask;
    }

    return target;
}

static void delete_item(SDL_HashTable *ht, SDL_HashItem *item)
{
    Uint32 hash_mask = ht->hash_mask;
    SDL_HashItem *table = ht->table;

    if (ht->nuke) {
        ht->nuke(item->key, item->value, ht->data);
    }
    ht->num_occupied_slots--;

    Uint32 idx = (Uint32)(item - ht->table);

    for (;;) {
        idx = (idx + 1) & hash_mask;
        SDL_HashItem *next_item = table + idx;

        if (next_item->probe_len < 1) {
            SDL_zerop(item);
            return;
        }

        *item = *next_item;
        item->probe_len -= 1;
        HT_ASSERT(item->probe_len < ht->max_probe_len);
        item = next_item;
    }
}

static bool resize(SDL_HashTable *ht, Uint32 new_size)
{
    SDL_HashItem *old_table = ht->table;
    Uint32 old_size = ht->hash_mask + 1;
    Uint32 new_hash_mask = new_size - 1;
    SDL_HashItem *new_table = SDL_calloc(new_size, sizeof(*new_table));

    if (!new_table) {
        return false;
    }

    ht->max_probe_len = 0;
    ht->hash_mask = new_hash_mask;
    ht->table = new_table;

    for (Uint32 i = 0; i < old_size; ++i) {
        SDL_HashItem *item = old_table + i;
        if (item->live) {
            insert_item(item, new_table, new_hash_mask, &ht->max_probe_len);
        }
    }

    SDL_free(old_table);
    return true;
}

static bool maybe_resize(SDL_HashTable *ht)
{
    Uint32 capacity = ht->hash_mask + 1;

    if (capacity >= MAX_HASHTABLE_SIZE) {
        return false;
    }

    Uint32 max_load_factor = 217; // range: 0-255; 217 is roughly 85%
    Uint32 resize_threshold = (Uint32)((max_load_factor * (Uint64)capacity) >> 8);

    if (ht->num_occupied_slots > resize_threshold) {
        return resize(ht, capacity * 2);
    }

    return true;
}

bool SDL_InsertIntoHashTable(SDL_HashTable *table, const void *key, const void *value)
{
    SDL_HashItem *item;
    Uint32 hash;
    bool result = false;

    if (!table) {
        return false;
    }

    if (table->lock) {
        SDL_LockRWLockForWriting(table->lock);
    }

    hash = calc_hash(table, key);
    item = find_first_item(table, key, hash);

    if (item && !table->stackable) {
        // Allow overwrites, this might have been inserted on another thread
        delete_item(table, item);
    }

    SDL_HashItem new_item;
    new_item.key = key;
    new_item.value = value;
    new_item.hash = hash;
    new_item.live = true;
    new_item.probe_len = 0;

    table->num_occupied_slots++;

    if (!maybe_resize(table)) {
        table->num_occupied_slots--;
        goto done;
    }

    // This never returns NULL
    insert_item(&new_item, table->table, table->hash_mask, &table->max_probe_len);
    result = true;

done:
    if (table->lock) {
        SDL_UnlockRWLock(table->lock);
    }
    return result;
}

bool SDL_FindInHashTable(const SDL_HashTable *table, const void *key, const void **value)
{
    Uint32 hash;
    SDL_HashItem *i;
    bool result = false;

    if (!table) {
        if (value) {
            *value = NULL;
        }
        return false;
    }

    if (table->lock) {
        SDL_LockRWLockForReading(table->lock);
    }

    hash = calc_hash(table, key);
    i = find_first_item(table, key, hash);
    if (i) {
        if (value) {
            *value = i->value;
        }
        result = true;
    }

    if (table->lock) {
        SDL_UnlockRWLock(table->lock);
    }
    return result;
}

bool SDL_RemoveFromHashTable(SDL_HashTable *table, const void *key)
{
    Uint32 hash;
    SDL_HashItem *item;
    bool result = false;

    if (!table) {
        return false;
    }

    if (table->lock) {
        SDL_LockRWLockForWriting(table->lock);
    }

    // FIXME: what to do for stacking hashtables?
    // The original code removes just one item.
    // This hashtable happens to preserve the insertion order of multi-value keys,
    // so deleting the first one will always delete the least-recently inserted one.
    // But maybe it makes more sense to remove all matching items?

    hash = calc_hash(table, key);
    item = find_first_item(table, key, hash);
    if (!item) {
        goto done;
    }

    delete_item(table, item);
    result = true;

done:
    if (table->lock) {
        SDL_UnlockRWLock(table->lock);
    }
    return result;
}

bool SDL_IterateHashTableKey(const SDL_HashTable *table, const void *key, const void **_value, void **iter)
{
    SDL_HashItem *item = (SDL_HashItem *)*iter;

    if (!table) {
        return false;
    }

    Uint32 i, probe_len, hash;

    if (item) {
        HT_ASSERT(item >= table->table);
        HT_ASSERT(item < table->table + (table->hash_mask + 1));

        hash = item->hash;
        probe_len = item->probe_len + 1;
        i = ((Uint32)(item - table->table) + 1) & table->hash_mask;
        item = table->table + i;
    } else {
        hash = calc_hash(table, key);
        i = hash & table->hash_mask;
        probe_len = 0;
    }

    item = find_item(table, key, hash, &i, &probe_len);

    if (!item) {
        *_value = NULL;
        return false;
    }

    *_value = item->value;
    *iter = item;

    return true;
}

bool SDL_IterateHashTable(const SDL_HashTable *table, const void **_key, const void **_value, void **iter)
{
    SDL_HashItem *item = (SDL_HashItem *)*iter;

    if (!table) {
        return false;
    }

    if (!item) {
        item = table->table;
    } else {
        item++;
    }

    HT_ASSERT(item >= table->table);
    SDL_HashItem *end = table->table + (table->hash_mask + 1);

    while (item < end && !item->live) {
        ++item;
    }

    HT_ASSERT(item <= end);

    if (item == end) {
        if (_key) {
            *_key = NULL;
        }
        if (_value) {
            *_value = NULL;
        }
        return false;
    }

    if (_key) {
        *_key = item->key;
    }
    if (_value) {
        *_value = item->value;
    }
    *iter = item;

    return true;
}

bool SDL_HashTableEmpty(SDL_HashTable *table)
{
    return !(table && table->num_occupied_slots);
}

static void nuke_all(SDL_HashTable *table)
{
    void *data = table->data;
    SDL_HashItem *end = table->table + (table->hash_mask + 1);
    SDL_HashItem *i;

    for (i = table->table; i < end; ++i) {
        if (i->live) {
            table->nuke(i->key, i->value, data);
        }
    }
}

void SDL_EmptyHashTable(SDL_HashTable *table)
{
    if (table) {
        SDL_LockRWLockForWriting(table->lock);
        {
            if (table->nuke) {
                nuke_all(table);
            }

            SDL_memset(table->table, 0, sizeof(*table->table) * (table->hash_mask + 1));
            table->num_occupied_slots = 0;
        }
        SDL_UnlockRWLock(table->lock);
    }
}

void SDL_DestroyHashTable(SDL_HashTable *table)
{
    if (table) {
        SDL_EmptyHashTable(table);

        SDL_DestroyRWLock(table->lock);
        SDL_free(table->table);
        SDL_free(table);
    }
}

// this is djb's xor hashing function.
static SDL_INLINE Uint32 hash_string_djbxor(const char *str, size_t len)
{
    Uint32 hash = 5381;
    while (len--) {
        hash = ((hash << 5) + hash) ^ *(str++);
    }
    return hash;
}

Uint32 SDL_HashPointer(const void *key, void *unused)
{
    (void)unused;
    return SDL_murmur3_32(&key, sizeof(key), 0);
}

bool SDL_KeyMatchPointer(const void *a, const void *b, void *unused)
{
    (void)unused;
    return (a == b);
}

Uint32 SDL_HashString(const void *key, void *unused)
{
    (void)unused;
    const char *str = (const char *)key;
    return hash_string_djbxor(str, SDL_strlen(str));
}

bool SDL_KeyMatchString(const void *a, const void *b, void *unused)
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

Uint32 SDL_HashID(const void *key, void *unused)
{
    (void)unused;
    return (Uint32)(uintptr_t)key;
}

bool SDL_KeyMatchID(const void *a, const void *b, void *unused)
{
    (void)unused;
    return (a == b);
}

void SDL_NukeFreeKey(const void *key, const void *value, void *unused)
{
    (void)value;
    (void)unused;
    SDL_free((void *)key);
}

void SDL_NukeFreeValue(const void *key, const void *value, void *unused)
{
    (void)key;
    (void)unused;
    SDL_free((void *)value);
}
