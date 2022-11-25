# Migrating to SDL 3.0

This guide provides useful information for migrating applications from SDL 2.0 to SDL 3.0.

We have provided a handy Python script to automate some of this work for you [link to script], and details on the changes are organized by SDL 2.0 header below.

## SDL_rwops.h

SDL_RWFromFP has been removed from the API, due to issues when the SDL library uses a different C runtime from the application.

You can implement this in your own code easily:
```c
#include <stdio.h>


static Sint64 SDLCALL
stdio_size(SDL_RWops * context)
{
    Sint64 pos, size;

    pos = SDL_RWseek(context, 0, RW_SEEK_CUR);
    if (pos < 0) {
        return -1;
    }
    size = SDL_RWseek(context, 0, RW_SEEK_END);

    SDL_RWseek(context, pos, RW_SEEK_SET);
    return size;
}

static Sint64 SDLCALL
stdio_seek(SDL_RWops * context, Sint64 offset, int whence)
{
    int stdiowhence;

    switch (whence) {
    case RW_SEEK_SET:
        stdiowhence = SEEK_SET;
        break;
    case RW_SEEK_CUR:
        stdiowhence = SEEK_CUR;
        break;
    case RW_SEEK_END:
        stdiowhence = SEEK_END;
        break;
    default:
        return SDL_SetError("Unknown value for 'whence'");
    }

    if (fseek((FILE *)context->hidden.stdio.fp, (fseek_off_t)offset, stdiowhence) == 0) {
        Sint64 pos = ftell((FILE *)context->hidden.stdio.fp);
        if (pos < 0) {
            return SDL_SetError("Couldn't get stream offset");
        }
        return pos;
    }
    return SDL_Error(SDL_EFSEEK);
}

static size_t SDLCALL
stdio_read(SDL_RWops * context, void *ptr, size_t size, size_t maxnum)
{
    size_t nread;

    nread = fread(ptr, size, maxnum, (FILE *)context->hidden.stdio.fp);
    if (nread == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        SDL_Error(SDL_EFREAD);
    }
    return nread;
}

static size_t SDLCALL
stdio_write(SDL_RWops * context, const void *ptr, size_t size, size_t num)
{
    size_t nwrote;

    nwrote = fwrite(ptr, size, num, (FILE *)context->hidden.stdio.fp);
    if (nwrote == 0 && ferror((FILE *)context->hidden.stdio.fp)) {
        SDL_Error(SDL_EFWRITE);
    }
    return nwrote;
}

static int SDLCALL
stdio_close(SDL_RWops * context)
{
    int status = 0;
    if (context) {
        if (context->hidden.stdio.autoclose) {
            /* WARNING:  Check the return value here! */
            if (fclose((FILE *)context->hidden.stdio.fp) != 0) {
                status = SDL_Error(SDL_EFWRITE);
            }
        }
        SDL_FreeRW(context);
    }
    return status;
}

SDL_RWops *
SDL_RWFromFP(void *fp, SDL_bool autoclose)
{
    SDL_RWops *rwops = NULL;

    rwops = SDL_AllocRW();
    if (rwops != NULL) {
        rwops->size = stdio_size;
        rwops->seek = stdio_seek;
        rwops->read = stdio_read;
        rwops->write = stdio_write;
        rwops->close = stdio_close;
        rwops->hidden.stdio.fp = fp;
        rwops->hidden.stdio.autoclose = autoclose;
        rwops->type = SDL_RWOPS_STDFILE;
    }
    return rwops;
}
```

## SDL_stdinc.h

M_PI is no longer defined in SDL_stdinc.h, you can use the new symbols SDL_PI_D (double) and SDL_PI_F (float) instead.

## SDL_syswm.h

This header no longer includes platform specific headers and type definitions, instead allowing you to include the ones appropriate for your use case. You should define one or more of the following to enable the relevant platform-specific support:
* SDL_ENABLE_SYSWM_ANDROID
* SDL_ENABLE_SYSWM_COCOA
* SDL_ENABLE_SYSWM_KMSDRM
* SDL_ENABLE_SYSWM_UIKIT
* SDL_ENABLE_SYSWM_VIVANTE
* SDL_ENABLE_SYSWM_WAYLAND
* SDL_ENABLE_SYSWM_WINDOWS
* SDL_ENABLE_SYSWM_X11

The structures in this file are versioned separately from the rest of SDL, allowing better backwards compatibility and limited forwards compatibility with your application. Instead of calling `SDL_VERSION(&info.version)` before calling SDL_GetWindowWMInfo(), you pass the version in explicitly as `SDL_SYSWM_CURRENT_VERSION` so SDL knows what fields you expect to be filled out.

### SDL_GetWindowWMInfo

This function now returns a standard int result instead of SDL_bool, returning 0 if the function succeeds or a negative error code if there was an error. You should also pass `SDL_SYSWM_CURRENT_VERSION` as the new third version parameter. The version member of the info structure will be filled in with the version of data that is returned, the minimum of the version you requested and the version supported by the runtime SDL library.

## SDL_video.h

SDL_SetWindowBrightness and SDL_SetWindowGammaRamp have been removed from the API, because they interact poorly with modern operating systems and aren't able to limit their effects to the SDL window.

Programs which have access to shaders can implement more robust versions of those functions using custom shader code rendered as a post-process effect.
