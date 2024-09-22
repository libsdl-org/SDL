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
#ifndef SDL_hashtable_h_
#define SDL_hashtable_h_

// this is not (currently) a public API. But maybe it should be!

struct SDL_HashTable;
typedef struct SDL_HashTable SDL_HashTable;
typedef Uint32 (*SDL_HashTable_HashFn)(const void *key, void *data);
typedef bool (*SDL_HashTable_KeyMatchFn)(const void *a, const void *b, void *data);
typedef void (*SDL_HashTable_NukeFn)(const void *key, const void *value, void *data);

extern SDL_HashTable *SDL_CreateHashTable(void *data,
                                          const Uint32 num_buckets,
                                          const SDL_HashTable_HashFn hashfn,
                                          const SDL_HashTable_KeyMatchFn keymatchfn,
                                          const SDL_HashTable_NukeFn nukefn,
                                          const bool stackable);

extern void SDL_EmptyHashTable(SDL_HashTable *table);
extern void SDL_DestroyHashTable(SDL_HashTable *table);
extern bool SDL_InsertIntoHashTable(SDL_HashTable *table, const void *key, const void *value);
extern bool SDL_RemoveFromHashTable(SDL_HashTable *table, const void *key);
extern bool SDL_FindInHashTable(const SDL_HashTable *table, const void *key, const void **_value);
extern bool SDL_HashTableEmpty(SDL_HashTable *table);

// iterate all values for a specific key. This only makes sense if the hash is stackable. If not-stackable, just use SDL_FindInHashTable().
extern bool SDL_IterateHashTableKey(const SDL_HashTable *table, const void *key, const void **_value, void **iter);

// iterate all key/value pairs in a hash (stackable hashes can have duplicate keys with multiple values).
extern bool SDL_IterateHashTable(const SDL_HashTable *table, const void **_key, const void **_value, void **iter);

extern Uint32 SDL_HashString(const void *key, void *unused);
extern bool SDL_KeyMatchString(const void *a, const void *b, void *unused);

extern Uint32 SDL_HashID(const void *key, void *unused);
extern bool SDL_KeyMatchID(const void *a, const void *b, void *unused);

extern void SDL_NukeFreeKey(const void *key, const void *value, void *unused);
extern void SDL_NukeFreeValue(const void *key, const void *value, void *unused);

#endif // SDL_hashtable_h_
