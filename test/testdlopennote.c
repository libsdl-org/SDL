#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef SDL_PLATFORM_WINDOWS
#define PNG_SHARED_LIBRARY "libpng16-16.dll"
#elif defined(SDL_PLATFORM_APPLE)
#define PNG_SHARED_LIBRARY "libpng16.16.dylib"
#else
#define PNG_SHARED_LIBRARY "libpng16.so.16"
#endif

SDL_ELF_NOTE_DLOPEN(
    "png",
    "Support for loading PNG images using libpng",
    SDL_ELF_NOTE_DLOPEN_PRIORITY_RECOMMENDED,
    PNG_SHARED_LIBRARY
)

typedef int png_sig_cmp_fn(const unsigned char *sig, size_t start, size_t num_to_check);

static struct {
    SDL_SharedObject *library;
    png_sig_cmp_fn *png_sig_cmp;
} libpng16;

static bool libpng_init(void)
{
    libpng16.library = SDL_LoadObject(PNG_SHARED_LIBRARY);
    if (!libpng16.library) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load libpng library \"" PNG_SHARED_LIBRARY "\"");
        return false;
    }
    libpng16.png_sig_cmp = (png_sig_cmp_fn *)SDL_LoadFunction(libpng16.library, "png_sig_cmp");
    return libpng16.png_sig_cmp != NULL;
}

static void libpng_quit(void)
{
    SDL_UnloadObject(libpng16.library);
    SDL_zero(libpng16);
}

static bool is_png(const char *path)
{
    unsigned char header[8];
    size_t count;
    bool result;
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (io == NULL) {
        return false;
    }
    count = SDL_ReadIO(io, header, sizeof(header));
    result = libpng16.png_sig_cmp(header, 0, count) == 0;
    SDL_CloseIO(io);
    return result;
}

int main(int argc, char *argv[])
{
    int i;

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (argc < 2) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Usage: %s IMAGE [IMAGE [IMAGE ... ]]", argv[0]);
        return 1;
    }
    if (!libpng_init()) {
        return 1;
    }
    for (i = 1; i < argc; i++) {
        SDL_Log("\"%s\" is a png: %s", argv[i], is_png(argv[i]) ? "YES" : "NO");
    }
    libpng_quit();
    return 0;
}
