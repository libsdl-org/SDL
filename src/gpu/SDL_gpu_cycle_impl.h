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

/* this file is included multiple times with different defines. */

struct SDL_GPUCYCLETYPE
{
    const char *label;
    Uint32 num_items;
    Uint32 next_item;
    SDL_GPUCYCLEITEMTYPE *items[SDL_VARIABLE_LENGTH_ARRAY];
};

SDL_GPUCYCLETYPE *
SDL_GPUCYCLECREATEFNSIG
{
    /* allocate the whole thing as one block: `items` as a variable length array at the end, the label data after that. */
    const size_t labellen = label ? (SDL_strlen(label) + 1) : 0;
    const size_t thislabellen = label ? 0 : labellen + 32;
    const size_t alloclen = sizeof (SDL_GPUCYCLETYPE) + (sizeof (SDL_GPUCYCLEITEMTYPE *) * numitems) + labellen;
    SDL_GPUCYCLETYPE *retval = (SDL_GPUCYCLETYPE *) SDL_calloc(1, alloclen);
    char *thislabel = NULL;
    SDL_bool isstack = SDL_FALSE;
    Uint32 i;

    if (!retval) {
        SDL_OutOfMemory();
        return NULL;
    }

    thislabel = label ? SDL_small_alloc(char, thislabellen, &isstack) : NULL;
    for (i = 0; i < numitems; i++) {
        SDL_bool failed = SDL_TRUE;
        if (thislabel) {
            SDL_snprintf(thislabel, thislabellen, "%s (cycle %u/%u)", label, (unsigned int) i, (unsigned int) numitems);
        }
        SDL_GPUCYCLECREATE(thislabel, failed, retval->items[i]);
        if (failed) {
            Uint32 j;
            for (j = 0; j < i; j++) {
                SDL_GPUCYCLEDESTROY(retval->items[j]);
            }
            SDL_free(retval);
            if (thislabel) {
                SDL_small_free(thislabel, isstack);
            }
            return NULL;
        }
    }

    if (label) {
        char *ptr = ((char *) retval) + (sizeof (SDL_GPUCYCLETYPE) + (sizeof (SDL_GPUCYCLEITEMTYPE *) * numitems));
        SDL_strlcpy(ptr, label, labellen);
        retval->label = ptr;
    }

    retval->num_items = numitems;
    return retval;
}

SDL_GPUCYCLEITEMTYPE **
SDL_GPUCYCLENEXTPTRFNNAME(SDL_GPUCYCLETYPE *cycle)

{
    SDL_GPUCYCLEITEMTYPE **retval = NULL;
    if (!cycle) {
        SDL_InvalidParamError("cycle");
    } else {
        retval = &cycle->items[cycle->next_item++];
        if (cycle->next_item >= cycle->num_items) {
            cycle->next_item = 0;
        }
    }
    return retval;
}

SDL_GPUCYCLEITEMTYPE *
SDL_GPUCYCLENEXTFNNAME(SDL_GPUCYCLETYPE *cycle)
{
    SDL_GPUCYCLEITEMTYPE **itemptr = SDL_GPUCYCLENEXTPTRFNNAME(cycle);
    return itemptr ? *itemptr : NULL;
}

void
SDL_GPUCYCLEDESTROYFNNAME(SDL_GPUCYCLETYPE *cycle)
{
    if (cycle) {
        Uint32 i;
        for (i = 0; i < cycle->num_items; i++) {
            SDL_GPUCYCLEDESTROY(cycle->items[i]);
        }
        SDL_free(cycle);  /* this frees everything, including the variable length array and the string data that label points to. */
    }
}

#undef SDL_GPUCYCLETYPE
#undef SDL_GPUCYCLEITEMTYPE
#undef SDL_GPUCYCLECREATEFNSIG
#undef SDL_GPUCYCLENEXTFNNAME
#undef SDL_GPUCYCLENEXTPTRFNNAME
#undef SDL_GPUCYCLEDESTROYFNNAME
#undef SDL_GPUCYCLECREATE
#undef SDL_GPUCYCLEDESTROY

/* vi: set ts=4 sw=4 expandtab: */

