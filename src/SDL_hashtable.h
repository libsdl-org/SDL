/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

/* this is not (currently) a public API. But maybe it should be! */

struct SDL_HashTable;
typedef struct SDL_HashTable SDL_HashTable;
typedef Uint32 (*SDL_HashTable_HashFn)(const void *key, void *data);
typedef SDL_bool (*SDL_HashTable_KeyMatchFn)(const void *a, const void *b, void *data);
typedef void (*SDL_HashTable_NukeFn)(const void *key, const void *value, void *data);

SDL_HashTable *SDL_NewHashTable(void *data,
                                const Uint32 num_buckets,
                                const SDL_HashTable_HashFn hashfn,
                                const SDL_HashTable_KeyMatchFn keymatchfn,
                                const SDL_HashTable_NukeFn nukefn,
                                const SDL_bool stackable);

void SDL_FreeHashTable(SDL_HashTable *table);
SDL_bool SDL_InsertIntoHashTable(SDL_HashTable *table, const void *key, const void *value);
SDL_bool SDL_RemoveFromHashTable(SDL_HashTable *table, const void *key);
SDL_bool SDL_FindInHashTable(const SDL_HashTable *table, const void *key, const void **_value);

SDL_bool SDL_IterateHashTable(const SDL_HashTable *table, const void *key, const void **_value, void **iter);
SDL_bool SDL_IterateHashTableKeys(const SDL_HashTable *table, const void **_key, void **iter);

Uint32 SDL_HashString(const void *sym, void *unused);
SDL_bool SDL_KeyMatchString(const void *a, const void *b, void *unused);

#endif /* SDL_hashtable_h_ */

/* vi: set ts=4 sw=4 expandtab: */

