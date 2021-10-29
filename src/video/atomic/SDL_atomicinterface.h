/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>
  Atomic KMSDRM backend by Manuel Alfayate Corchete <redwindwanderer@gmail.com>

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
#include "../../SDL_internal.h"

#ifndef SDL_atomicinterface_h_
#define SDL_atomicinterface_h_

#if SDL_VIDEO_DRIVER_ATOMIC

/* ATOMIC interface helper functions */
extern int add_connector_property(drmModeAtomicReq *req, struct connector *connector,
                                     const char *name, uint64_t value);

extern int add_crtc_property(drmModeAtomicReq *req, struct crtc *crtc,
                         const char *name, uint64_t value);

extern int add_plane_property(drmModeAtomicReq *req, struct plane *plane,
                          const char *name, uint64_t value);

extern void print_plane_info(_THIS, drmModePlanePtr plane);

extern int get_plane_id(_THIS, unsigned int crtc_id, uint32_t plane_type);
extern int setup_plane(_THIS, struct plane **plane, uint32_t plane_type);
extern void free_plane(struct plane **plane);
extern void drm_atomic_set_plane_props(struct ATOMIC_PlaneInfo *info);

extern int drm_atomic_commit(_THIS, SDL_bool blocking, SDL_bool allow_modeset);

extern void drm_atomic_waitpending(_THIS);

#endif /* SDL_VIDEO_DRIVER_ATOMIC */

#endif /* SDL_atomicinterface_h_ */

/* vi: set ts=4 sw=4 expandtab: */
