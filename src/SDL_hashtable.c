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
#include "SDL_hashtable.h"

typedef struct SDL_HashItem
{
    const void *key;
    const void *value;
    struct SDL_HashItem *next;
} SDL_HashItem;

struct SDL_HashTable
{
    SDL_HashItem **table;
    Uint32 table_len;
    SDL_bool stackable;
    void *data;
    SDL_HashTable_HashFn hash;
    SDL_HashTable_KeyMatchFn keymatch;
    SDL_HashTable_NukeFn nuke;
};

SDL_HashTable *SDL_CreateHashTable(void *data, const Uint32 num_buckets, const SDL_HashTable_HashFn hashfn,
                                   const SDL_HashTable_KeyMatchFn keymatchfn,
                                   const SDL_HashTable_NukeFn nukefn,
                                   const SDL_bool stackable)
{
    SDL_HashTable *table;

    // num_buckets must be a power of two so we get a solid block of bits to mask hash values against.
    if ((num_buckets == 0) || ((num_buckets & (num_buckets - 1)) != 0)) {
        SDL_SetError("num_buckets must be a power of two");
        return NULL;
    }

    table = (SDL_HashTable *) SDL_calloc(1, sizeof (SDL_HashTable));
    if (!table) {
        return NULL;
    }

    table->table = (SDL_HashItem **) SDL_calloc(num_buckets, sizeof (SDL_HashItem *));
    if (!table->table) {
        SDL_free(table);
        return NULL;
    }

    table->table_len = num_buckets;
    table->stackable = stackable;
    table->data = data;
    table->hash = hashfn;
    table->keymatch = keymatchfn;
    table->nuke = nukefn;
    return table;
}

static SDL_INLINE Uint32 calc_hash(const SDL_HashTable *table, const void *key)
{
    return table->hash(key, table->data) & (table->table_len - 1);
}


SDL_bool SDL_InsertIntoHashTable(SDL_HashTable *table, const void *key, const void *value)
{
    SDL_HashItem *item;
    const Uint32 hash = calc_hash(table, key);

    if ( (!table->stackable) && (SDL_FindInHashTable(table, key, NULL)) ) {
        return SDL_FALSE;
    }

    // !!! FIXME: grow and rehash table if it gets too saturated.
    item = (SDL_HashItem *) SDL_malloc(sizeof (SDL_HashItem));
    if (!item) {
        return SDL_FALSE;
    }

    item->key = key;
    item->value = value;
    item->next = table->table[hash];
    table->table[hash] = item;

    return SDL_TRUE;
}

SDL_bool SDL_FindInHashTable(const SDL_HashTable *table, const void *key, const void **_value)
{
    const Uint32 hash = calc_hash(table, key);
    void *data = table->data;
    SDL_HashItem *i;

    for (i = table->table[hash]; i; i = i->next) {
        if (table->keymatch(key, i->key, data)) {
            if (_value) {
                *_value = i->value;
            }
            return SDL_TRUE;
        }
    }

    return SDL_FALSE;
}

SDL_bool SDL_RemoveFromHashTable(SDL_HashTable *table, const void *key)
{
    const Uint32 hash = calc_hash(table, key);
    SDL_HashItem *item = NULL;
    SDL_HashItem *prev = NULL;
    void *data = table->data;

    for (item = table->table[hash]; item; item = item->next) {
        if (table->keymatch(key, item->key, data)) {
            if (prev) {
                prev->next = item->next;
            } else {
                table->table[hash] = item->next;
            }

            table->nuke(item->key, item->value, data);
            SDL_free(item);
            return SDL_TRUE;
        }

        prev = item;
    }

    return SDL_FALSE;
}

SDL_bool SDL_IterateHashTableKey(const SDL_HashTable *table, const void *key, const void **_value, void **iter)
{
    SDL_HashItem *item = *iter ? ((SDL_HashItem *) *iter)->next : table->table[calc_hash(table, key)];

    while (item) {
        if (table->keymatch(key, item->key, table->data)) {
            *_value = item->value;
            *iter = item;
            return SDL_TRUE;
        }
        item = item->next;
    }

    // no more matches.
    *_value = NULL;
    *iter = NULL;
    return SDL_FALSE;
}

SDL_bool SDL_IterateHashTable(const SDL_HashTable *table, const void **_key, const void **_value, void **iter)
{
    SDL_HashItem *item = (SDL_HashItem *) *iter;
    Uint32 idx = 0;

    if (item) {
        const SDL_HashItem *orig = item;
        item = item->next;
        if (!item) {
            idx = calc_hash(table, orig->key) + 1;  // !!! FIXME: we probably shouldn't rehash each time.
        }
    }

    while (!item && (idx < table->table_len)) {
        item = table->table[idx++];  // skip empty buckets...
    }

    if (!item) {  // no more matches?
        *_key = NULL;
        *iter = NULL;
        return SDL_FALSE;
    }

    *_key = item->key;
    *_value = item->value;
    *iter = item;

    return SDL_TRUE;
}

SDL_bool SDL_HashTableEmpty(SDL_HashTable *table)
{
    if (table) {
        Uint32 i;

        for (i = 0; i < table->table_len; i++) {
            SDL_HashItem *item = table->table[i];
            if (item) {
                return SDL_FALSE;
            }
        }
    }
    return SDL_TRUE;
}

void SDL_DestroyHashTable(SDL_HashTable *table)
{
    if (table) {
        void *data = table->data;
        Uint32 i;

        for (i = 0; i < table->table_len; i++) {
            SDL_HashItem *item = table->table[i];
            while (item) {
                SDL_HashItem *next = item->next;
                table->nuke(item->key, item->value, data);
                SDL_free(item);
                item = next;
            }
        }

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

Uint32 SDL_HashString(const void *key, void *data)
{
    const char *str = (const char *)key;
    return hash_string_djbxor(str, SDL_strlen(str));
}

SDL_bool SDL_KeyMatchString(const void *a, const void *b, void *data)
{
    if (a == b) {
        return SDL_TRUE;  // same pointer, must match.
    } else if (!a || !b) {
        return SDL_FALSE;  // one pointer is NULL (and first test shows they aren't the same pointer), must not match.
    }
    return (SDL_strcmp((const char *)a, (const char *)b) == 0);  // Check against actual string contents.
}

// We assume we can fit the ID in the key directly
SDL_COMPILE_TIME_ASSERT(SDL_HashID_KeySize, sizeof(Uint32) <= sizeof(const void *));

Uint32 SDL_HashID(const void *key, void *unused)
{
    return (Uint32)(uintptr_t)key;
}

SDL_bool SDL_KeyMatchID(const void *a, const void *b, void *unused)
{
    if (a == b) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}
