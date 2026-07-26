#include "SDL_internal.h"
#include "../../core/ohos/SDL_ohos.h"

#include "../SDL_sysurl.h"

bool SDL_SYS_OpenURL(const char *url)
{
    OHOS_OpenLink(url);
    return true;
}
