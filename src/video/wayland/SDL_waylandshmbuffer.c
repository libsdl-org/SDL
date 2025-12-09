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

#ifdef SDL_VIDEO_DRIVER_WAYLAND

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

#include "SDL_waylandshmbuffer.h"
#include "SDL_waylandvideo.h"

static bool SetTempFileSize(int fd, off_t size)
{
#ifdef HAVE_POSIX_FALLOCATE
    sigset_t set, old_set;
    int ret;

    /* SIGALRM can potentially block a large posix_fallocate() operation
     * from succeeding, so block it.
     */
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, &old_set);

    do {
        ret = posix_fallocate(fd, 0, size);
    } while (ret == EINTR);

    sigprocmask(SIG_SETMASK, &old_set, NULL);

    if (ret == 0) {
        return true;
    } else if (ret != EINVAL && errno != EOPNOTSUPP) {
        return false;
    }
#endif

    if (ftruncate(fd, size) < 0) {
        return false;
    }
    return true;
}

static int CreateTempFD(off_t size)
{
    int fd;

#ifdef HAVE_MEMFD_CREATE
    fd = memfd_create("SDL", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd >= 0) {
        fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);
    } else
#endif
    {
        static const char template[] = "/sdl-shared-XXXXXX";
        const char *xdg_path;
        char tmp_path[PATH_MAX];

        xdg_path = SDL_getenv("XDG_RUNTIME_DIR");
        if (!xdg_path) {
            return -1;
        }

        SDL_strlcpy(tmp_path, xdg_path, PATH_MAX);
        SDL_strlcat(tmp_path, template, PATH_MAX);

        fd = mkostemp(tmp_path, O_CLOEXEC);
        if (fd < 0) {
            return -1;
        }

        // Need to manually unlink the temp files, or they can persist after close and fill up the temp storage.
        unlink(tmp_path);
    }

    if (!SetTempFileSize(fd, size)) {
        close(fd);
        return -1;
    }

    return fd;
}

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer)
{
    // NOP
}

static struct wl_buffer_listener buffer_listener = {
    buffer_handle_release
};

struct Wayland_SHMPool
{
    struct wl_shm_pool *shm_pool;
    void *shm_pool_memory;
    int shm_pool_size;
    int offset;
};

Wayland_SHMPool *Wayland_AllocSHMPool(int size)
{
    SDL_VideoDevice *vd = SDL_GetVideoDevice();
    SDL_VideoData *data = vd->internal;

    if (size <= 0) {
        return NULL;
    }

    Wayland_SHMPool *shmPool = SDL_calloc(1, sizeof(Wayland_SHMPool));
    if (!shmPool) {
        return NULL;
    }

    shmPool->shm_pool_size = (size + 15) & (~15);

    const int shm_fd = CreateTempFD(shmPool->shm_pool_size);
    if (shm_fd < 0) {
        SDL_free(shmPool);
        SDL_SetError("Creating SHM buffer failed.");
        return NULL;
    }

    shmPool->shm_pool_memory = mmap(NULL, shmPool->shm_pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shmPool->shm_pool_memory == MAP_FAILED) {
        shmPool->shm_pool_memory = NULL;
        close(shm_fd);
        SDL_free(shmPool);
        SDL_SetError("mmap() failed.");
        return NULL;
    }

    shmPool->shm_pool = wl_shm_create_pool(data->shm, shm_fd, shmPool->shm_pool_size);
    close(shm_fd);

    return shmPool;
}

struct wl_buffer *Wayland_AllocBufferFromPool(Wayland_SHMPool *shmPool, int width, int height, void **data)
{
    const Uint32 SHM_FMT = WL_SHM_FORMAT_ARGB8888;

    if (!shmPool || !width || !height || !data) {
        return NULL;
    }

    *data = (Uint8 *)shmPool->shm_pool_memory + shmPool->offset;
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(shmPool->shm_pool, shmPool->offset, width, height, width * 4, SHM_FMT);
    wl_buffer_add_listener(buffer, &buffer_listener, shmPool);

    shmPool->offset += width * height * 4;

    return buffer;
}

void Wayland_ReleaseSHMPool(Wayland_SHMPool *shmPool)
{
    if (shmPool) {
        wl_shm_pool_destroy(shmPool->shm_pool);
        munmap(shmPool->shm_pool_memory, shmPool->shm_pool_size);
        SDL_free(shmPool);
    }
}

#endif
