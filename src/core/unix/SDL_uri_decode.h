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

#ifndef SDL_uri_decode_h_
#define SDL_uri_decode_h_

/* Decodes URI escape sequences in string src of len bytes
 * (excluding the terminating NULL byte) into the dst buffer.
 * Since URI-encoded characters take three times the space of
 * normal characters, src and dst can safely point to the same
 * buffer for in situ conversion.
 *
 * The buffer is guaranteed to be NULL-terminated, but
 * may contain embedded NULL bytes.
 *
 * Returns the number of decoded bytes that wound up in
 * the destination buffer, excluding the terminating NULL byte.
 *
 * On error, -1 is returned.
 */
int SDL_URIDecode(const char *src, char *dst, int len);

/* Convert URI to a local filename, stripping the "file://"
 * preamble and hostname if present, and writes the result
 * to the dst buffer. Since URI-encoded characters take
 * three times the space of normal characters, src and dst
 * can safely point to the same buffer for in situ conversion.
 *
 * Returns the number of decoded bytes that wound up in
 * the destination buffer, excluding the terminating NULL byte.
 *
 * On error, -1 is returned;
 */
int SDL_URIToLocal(const char *src, char *dst);

#endif /* SDL_uri_decode_h_ */
