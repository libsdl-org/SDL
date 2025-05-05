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

#ifndef ngage_video_render_ngage_c_hpp
#define ngage_video_render_ngage_c_hpp

#include "SDL_render_ngage_c.h"
#include <NRenderer.h>
#include <e32std.h>
#include <w32std.h>

class CRenderer : public MDirectScreenAccess
{
  public:
    static CRenderer *NewL();
    virtual ~CRenderer();

    // Rendering functions.
    void Clear(TUint32 iColor);
    bool Copy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect);
    bool CopyEx(SDL_Renderer *renderer, SDL_Texture *texture, const NGAGE_CopyExData *copydata);
    bool CreateTextureData(NGAGE_TextureData *aTextureData, const TInt aWidth, const TInt aHeight);
    void DrawLines(NGAGE_Vertex *aVerts, const TInt aCount);
    void DrawPoints(NGAGE_Vertex *aVerts, const TInt aCount);
    void FillRects(NGAGE_Vertex *aVerts, const TInt aCount);
    void Flip();
    void SetDrawColor(TUint32 iColor);
    void SetClipRect(TInt aX, TInt aY, TInt aWidth, TInt aHeight);
    void UpdateFPS();
    void SuspendScreenSaver(TBool aSuspend);

    // Event handling.
    void DisableKeyBlocking();
    void HandleEvent(const TWsEvent &aWsEvent);
    void PumpEvents();

  private:
    CRenderer();
    void ConstructL(void);

    // BackBuffer.
    CNRenderer *iRenderer;

    // Direct screen access.
    CDirectScreenAccess *iDirectScreen;
    CFbsBitGc *iScreenGc;
    TBool iIsFocused;

    // Window server session.
    RWsSession iWsSession;
    RWindowGroup iWsWindowGroup;
    TInt iWsWindowGroupID;
    RWindow iWsWindow;
    CWsScreenDevice *iWsScreen;

    // Event handling.
    TRequestStatus iWsEventStatus;
    TWsEvent iWsEvent;

    // MDirectScreenAccess functions.
    void Restart(RDirectScreenAccess::TTerminationReasons aReason);
    void AbortNow(RDirectScreenAccess::TTerminationReasons aReason);

    // Frame per second.
    TBool iShowFPS;
    TUint iFPS;
    const CFont *iFont;

    // Screen saver.
    TBool iSuspendScreenSaver;
};

#endif // ngage_video_render_ngage_c_hpp
