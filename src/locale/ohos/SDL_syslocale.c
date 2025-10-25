#include "SDL_internal.h"
#include "../SDL_syslocale.h"
#include "../../core/ohos/SDL_ohos.h"

bool SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    const char* result = OHOS_Locale();
    SDL_memcpy(buf, result, buflen);
    SDL_Log("target %s", buf);
    return true;
}
