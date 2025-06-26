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
#ifdef __cplusplus
extern "C" {
#endif

#include "../../events/SDL_keyboard_c.h"
#include "../SDL_sysrender.h"
#include "SDL_internal.h"
#include "SDL_render_ngage_c.h"

#ifdef __cplusplus
}
#endif

#ifdef SDL_VIDEO_RENDER_NGAGE

#include "SDL_render_ngage_c.hpp"
#include "SDL_render_ops.hpp"

const TUint32 WindowClientHandle = 0x571D0A;

extern CRenderer *gRenderer;

#ifdef __cplusplus
extern "C" {
#endif

void NGAGE_Clear(const Uint32 color)
{
    gRenderer->Clear(color);
}

bool NGAGE_Copy(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *srcrect, SDL_Rect *dstrect)
{
    return gRenderer->Copy(renderer, texture, srcrect, dstrect);
}

bool NGAGE_CopyEx(SDL_Renderer *renderer, SDL_Texture *texture, NGAGE_CopyExData *copydata)
{
    return gRenderer->CopyEx(renderer, texture, copydata);
}

bool NGAGE_CreateTextureData(NGAGE_TextureData *data, const int width, const int height)
{
    return gRenderer->CreateTextureData(data, width, height);
}

void NGAGE_DestroyTextureData(NGAGE_TextureData *data)
{
    if (data) {
        delete data->bitmap;
        data->bitmap = NULL;
    }
}

void NGAGE_DrawLines(NGAGE_Vertex *verts, const int count)
{
    gRenderer->DrawLines(verts, count);
}

void NGAGE_DrawPoints(NGAGE_Vertex *verts, const int count)
{
    gRenderer->DrawPoints(verts, count);
}

void NGAGE_FillRects(NGAGE_Vertex *verts, const int count)
{
    gRenderer->FillRects(verts, count);
}

void NGAGE_Flip()
{
    gRenderer->Flip();
}

void NGAGE_SetClipRect(const SDL_Rect *rect)
{
    gRenderer->SetClipRect(rect->x, rect->y, rect->w, rect->h);
}

void NGAGE_SetDrawColor(const Uint32 color)
{
    if (gRenderer) {
        gRenderer->SetDrawColor(color);
    }
}

void NGAGE_PumpEventsInternal()
{
    gRenderer->PumpEvents();
}

void NGAGE_SuspendScreenSaverInternal(bool suspend)
{
    gRenderer->SuspendScreenSaver(suspend);
}

#ifdef __cplusplus
}
#endif

CRenderer *CRenderer::NewL()
{
    CRenderer *self = new (ELeave) CRenderer();
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

CRenderer::CRenderer() : iRenderer(0), iDirectScreen(0), iScreenGc(0), iWsSession(), iWsWindowGroup(), iWsWindowGroupID(0), iWsWindow(), iWsScreen(0), iWsEventStatus(), iWsEvent(), iShowFPS(EFalse), iFPS(0), iFont(0) {}

CRenderer::~CRenderer()
{
    delete iRenderer;
    iRenderer = 0;
}

void CRenderer::ConstructL()
{
    TInt error = KErrNone;

    error = iWsSession.Connect();
    if (error != KErrNone) {
        SDL_Log("Failed to connect to window server: %d", error);
        User::Leave(error);
    }

    iWsScreen = new (ELeave) CWsScreenDevice(iWsSession);
    error = iWsScreen->Construct();
    if (error != KErrNone) {
        SDL_Log("Failed to construct screen device: %d", error);
        User::Leave(error);
    }

    iWsWindowGroup = RWindowGroup(iWsSession);
    error = iWsWindowGroup.Construct(WindowClientHandle);
    if (error != KErrNone) {
        SDL_Log("Failed to construct window group: %d", error);
        User::Leave(error);
    }
    iWsWindowGroup.SetOrdinalPosition(0);

    RProcess thisProcess;
    TParse exeName;
    exeName.Set(thisProcess.FileName(), NULL, NULL);
    TBuf<32> winGroupName;
    winGroupName.Append(0);
    winGroupName.Append(0);
    winGroupName.Append(0); // UID
    winGroupName.Append(0);
    winGroupName.Append(exeName.Name()); // Caption
    winGroupName.Append(0);
    winGroupName.Append(0); // DOC name
    iWsWindowGroup.SetName(winGroupName);

    iWsWindow = RWindow(iWsSession);
    error = iWsWindow.Construct(iWsWindowGroup, WindowClientHandle - 1);
    if (error != KErrNone) {
        SDL_Log("Failed to construct window: %d", error);
        User::Leave(error);
    }
    iWsWindow.SetBackgroundColor(KRgbWhite);
    iWsWindow.SetRequiredDisplayMode(EColor4K);
    iWsWindow.Activate();
    iWsWindow.SetSize(iWsScreen->SizeInPixels());
    iWsWindow.SetVisible(ETrue);

    iWsWindowGroupID = iWsWindowGroup.Identifier();

    TRAPD(errc, iRenderer = iRenderer->NewL());
    if (errc != KErrNone) {
        SDL_Log("Failed to create renderer: %d", errc);
        return;
    }

    iDirectScreen = CDirectScreenAccess::NewL(
        iWsSession,
        *(iWsScreen),
        iWsWindow, *this);

    // Select font.
    TFontSpec fontSpec(_L("LatinBold12"), 12);
    TInt errd = iWsScreen->GetNearestFontInTwips((CFont *&)iFont, fontSpec);
    if (errd != KErrNone) {
        SDL_Log("Failed to get font: %d", errd);
        return;
    }

    // Activate events.
    iWsEventStatus = KRequestPending;
    iWsSession.EventReady(&iWsEventStatus);

    DisableKeyBlocking();

    iIsFocused = ETrue;
    iShowFPS = EFalse;
    iSuspendScreenSaver = EFalse;

    if (!iDirectScreen->IsActive()) {
        TRAPD(err, iDirectScreen->StartL());
        if (KErrNone != err) {
            return;
        }
        iDirectScreen->ScreenDevice()->SetAutoUpdate(ETrue);
    }
}

void CRenderer::Restart(RDirectScreenAccess::TTerminationReasons aReason)
{
    if (!iDirectScreen->IsActive()) {
        TRAPD(err, iDirectScreen->StartL());
        if (KErrNone != err) {
            return;
        }
        iDirectScreen->ScreenDevice()->SetAutoUpdate(ETrue);
    }
}

void CRenderer::AbortNow(RDirectScreenAccess::TTerminationReasons aReason)
{
    if (iDirectScreen->IsActive()) {
        iDirectScreen->Cancel();
    }
}

void CRenderer::Clear(TUint32 iColor)
{
    if (iRenderer && iRenderer->Gc()) {
        iRenderer->Gc()->SetBrushColor(iColor);
        iRenderer->Gc()->Clear();
    }
}

#ifdef __cplusplus
extern "C" {
#endif

Uint32 NGAGE_ConvertColor(float r, float g, float b, float a, float color_scale)
{
    TFixed ff = 255 << 16; // 255.f

    TFixed scalef = Real2Fix(color_scale);
    TFixed rf = Real2Fix(r);
    TFixed gf = Real2Fix(g);
    TFixed bf = Real2Fix(b);
    TFixed af = Real2Fix(a);

    rf = FixMul(rf, scalef);
    gf = FixMul(gf, scalef);
    bf = FixMul(bf, scalef);

    rf = SDL_clamp(rf, 0, ff);
    gf = SDL_clamp(gf, 0, ff);
    bf = SDL_clamp(bf, 0, ff);
    af = SDL_clamp(af, 0, ff);

    rf = FixMul(rf, ff) >> 16;
    gf = FixMul(gf, ff) >> 16;
    bf = FixMul(bf, ff) >> 16;
    af = FixMul(af, ff) >> 16;

    return (af << 24) | (bf << 16) | (gf << 8) | rf;
}

#ifdef __cplusplus
}
#endif

bool CRenderer::Copy(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect)
{
    if (!texture) {
        return false;
    }

    NGAGE_TextureData *phdata = (NGAGE_TextureData *)texture->internal;
    if (!phdata) {
        return false;
    }

    SDL_FColor *c = &texture->color;
    int w = phdata->surface->w;
    int h = phdata->surface->h;
    int pitch = phdata->surface->pitch;
    void *source = phdata->surface->pixels;
    void *dest;

    if (!source) {
        return false;
    }

    void *pixel_buffer_a = SDL_calloc(1, pitch * h);
    if (!pixel_buffer_a) {
        return false;
    }
    dest = pixel_buffer_a;

    void *pixel_buffer_b = SDL_calloc(1, pitch * h);
    if (!pixel_buffer_b) {
        SDL_free(pixel_buffer_a);
        return false;
    }

    if (c->a != 1.f || c->r != 1.f || c->g != 1.f || c->b != 1.f) {
        ApplyColorMod(dest, source, pitch, w, h, texture->color);

        source = dest;
    }

    float sx;
    float sy;
    SDL_GetRenderScale(renderer, &sx, &sy);

    if (sx != 1.f || sy != 1.f) {
        TFixed scale_x = Real2Fix(sx);
        TFixed scale_y = Real2Fix(sy);
        TFixed center_x = Int2Fix(w / 2);
        TFixed center_y = Int2Fix(h / 2);

        dest == pixel_buffer_a ? dest = pixel_buffer_b : dest = pixel_buffer_a;

        ApplyScale(dest, source, pitch, w, h, center_x, center_y, scale_x, scale_y);

        source = dest;
    }

    Mem::Copy(phdata->bitmap->DataAddress(), source, pitch * h);
    SDL_free(pixel_buffer_a);
    SDL_free(pixel_buffer_b);

    if (phdata->bitmap) {
        TRect aSource(TPoint(srcrect->x, srcrect->y), TSize(srcrect->w, srcrect->h));
        TPoint aDest(dstrect->x, dstrect->y);
        iRenderer->Gc()->BitBlt(aDest, phdata->bitmap, aSource);
    }

    return true;
}

bool CRenderer::CopyEx(SDL_Renderer *renderer, SDL_Texture *texture, const NGAGE_CopyExData *copydata)
{
    NGAGE_TextureData *phdata = (NGAGE_TextureData *)texture->internal;
    if (!phdata) {
        return false;
    }

    SDL_FColor *c = &texture->color;
    int w = phdata->surface->w;
    int h = phdata->surface->h;
    int pitch = phdata->surface->pitch;
    void *source = phdata->surface->pixels;
    void *dest;

    if (!source) {
        return false;
    }

    void *pixel_buffer_a = SDL_calloc(1, pitch * h);
    if (!pixel_buffer_a) {
        return false;
    }
    dest = pixel_buffer_a;

    void *pixel_buffer_b = SDL_calloc(1, pitch * h);
    if (!pixel_buffer_a) {
        SDL_free(pixel_buffer_a);
        return false;
    }

    if (copydata->flip) {
        ApplyFlip(dest, source, pitch, w, h, copydata->flip);
        source = dest;
    }

    if (copydata->scale_x != 1.f || copydata->scale_y != 1.f) {
        dest == pixel_buffer_a ? dest = pixel_buffer_b : dest = pixel_buffer_a;
        ApplyScale(dest, source, pitch, w, h, copydata->center.x, copydata->center.y, copydata->scale_x, copydata->scale_y);
        source = dest;
    }

    if (copydata->angle) {
        dest == pixel_buffer_a ? dest = pixel_buffer_b : dest = pixel_buffer_a;
        ApplyRotation(dest, source, pitch, w, h, copydata->center.x, copydata->center.y, copydata->angle);
        source = dest;
    }

    if (c->a != 1.f || c->r != 1.f || c->g != 1.f || c->b != 1.f) {
        dest == pixel_buffer_a ? dest = pixel_buffer_b : dest = pixel_buffer_a;
        ApplyColorMod(dest, source, pitch, w, h, texture->color);
        source = dest;
    }

    Mem::Copy(phdata->bitmap->DataAddress(), source, pitch * h);
    SDL_free(pixel_buffer_a);
    SDL_free(pixel_buffer_b);

    if (phdata->bitmap) {
        TRect aSource(TPoint(copydata->srcrect.x, copydata->srcrect.y), TSize(copydata->srcrect.w, copydata->srcrect.h));
        TPoint aDest(copydata->dstrect.x, copydata->dstrect.y);
        iRenderer->Gc()->BitBlt(aDest, phdata->bitmap, aSource);
    }

    return true;
}

bool CRenderer::CreateTextureData(NGAGE_TextureData *aTextureData, const TInt aWidth, const TInt aHeight)
{
    if (!aTextureData) {
        return false;
    }

    aTextureData->bitmap = new CFbsBitmap();
    if (!aTextureData->bitmap) {
        return false;
    }

    TInt error = aTextureData->bitmap->Create(TSize(aWidth, aHeight), EColor4K);
    if (error != KErrNone) {
        delete aTextureData->bitmap;
        aTextureData->bitmap = NULL;
        return false;
    }

    return true;
}

void CRenderer::DrawLines(NGAGE_Vertex *aVerts, const TInt aCount)
{
    if (iRenderer && iRenderer->Gc()) {
        TPoint *aPoints = new TPoint[aCount];

        for (TInt i = 0; i < aCount; i++) {
            aPoints[i] = TPoint(aVerts[i].x, aVerts[i].y);
        }

        TUint32 aColor = (((TUint8)aVerts->color.a << 24) |
                          ((TUint8)aVerts->color.b << 16) |
                          ((TUint8)aVerts->color.g << 8) |
                          (TUint8)aVerts->color.r);

        iRenderer->Gc()->SetPenColor(aColor);
        iRenderer->Gc()->DrawPolyLineNoEndPoint(aPoints, aCount);

        delete[] aPoints;
    }
}

void CRenderer::DrawPoints(NGAGE_Vertex *aVerts, const TInt aCount)
{
    if (iRenderer && iRenderer->Gc()) {
        for (TInt i = 0; i < aCount; i++, aVerts++) {
            TUint32 aColor = (((TUint8)aVerts->color.a << 24) |
                              ((TUint8)aVerts->color.b << 16) |
                              ((TUint8)aVerts->color.g << 8) |
                              (TUint8)aVerts->color.r);

            iRenderer->Gc()->SetPenColor(aColor);
            iRenderer->Gc()->Plot(TPoint(aVerts->x, aVerts->y));
        }
    }
}

void CRenderer::FillRects(NGAGE_Vertex *aVerts, const TInt aCount)
{
    if (iRenderer && iRenderer->Gc()) {
        for (TInt i = 0; i < aCount; i++, aVerts++) {
            TPoint pos(aVerts[i].x, aVerts[i].y);
            TSize size(
                aVerts[i + 1].x,
                aVerts[i + 1].y);
            TRect rect(pos, size);

            TUint32 aColor = (((TUint8)aVerts->color.a << 24) |
                              ((TUint8)aVerts->color.b << 16) |
                              ((TUint8)aVerts->color.g << 8) |
                              (TUint8)aVerts->color.r);

            iRenderer->Gc()->SetPenColor(aColor);
            iRenderer->Gc()->SetBrushColor(aColor);
            iRenderer->Gc()->DrawRect(rect);
        }
    }
}

void CRenderer::Flip()
{
    if (!iRenderer) {
        SDL_Log("iRenderer is NULL.");
        return;
    }

    if (!iIsFocused) {
        return;
    }

#ifdef SDL_VIDEO_RENDER_NGAGE_FPS
    iRenderer->Gc()->UseFont(iFont);

    if (iShowFPS && iRenderer->Gc()) {
        UpdateFPS();

        TBuf<64> info;

        iRenderer->Gc()->SetPenStyle(CGraphicsContext::ESolidPen);
        iRenderer->Gc()->SetBrushStyle(CGraphicsContext::ENullBrush);
        iRenderer->Gc()->SetPenColor(KRgbCyan);

        TRect aTextRect(TPoint(3, 203 - iFont->HeightInPixels()), TSize(45, iFont->HeightInPixels() + 2));
        iRenderer->Gc()->SetBrushStyle(CGraphicsContext::ESolidBrush);
        iRenderer->Gc()->SetBrushColor(KRgbBlack);
        iRenderer->Gc()->DrawRect(aTextRect);

        // Draw messages.
        info.Format(_L("FPS: %d"), iFPS);
        iRenderer->Gc()->DrawText(info, TPoint(5, 203));
    } else {
        // This is a workaround that helps regulating the FPS.
        iRenderer->Gc()->DrawText(_L(""), TPoint(0, 0));
    }
    iRenderer->Gc()->DiscardFont();
#endif // SDL_VIDEO_RENDER_NGAGE_FPS
    iRenderer->Flip(iDirectScreen);

    // Keep the backlight on.
    if (iSuspendScreenSaver) {
        User::ResetInactivityTime();
    }
    // Suspend the current thread for a short while.
    // Give some time to other threads and active objects.
    User::After(0);
}

void CRenderer::SetDrawColor(TUint32 iColor)
{
    if (iRenderer && iRenderer->Gc()) {
        iRenderer->Gc()->SetPenColor(iColor);
        iRenderer->Gc()->SetBrushColor(iColor);
        iRenderer->Gc()->SetBrushStyle(CGraphicsContext::ESolidBrush);

        TRAPD(err, iRenderer->SetCurrentColor(iColor));
        if (err != KErrNone) {
            return;
        }
    }
}

void CRenderer::SetClipRect(TInt aX, TInt aY, TInt aWidth, TInt aHeight)
{
    if (iRenderer && iRenderer->Gc()) {
        TRect viewportRect(aX, aY, aX + aWidth, aY + aHeight);
        iRenderer->Gc()->SetClippingRect(viewportRect);
    }
}

#ifdef SDL_VIDEO_RENDER_NGAGE_FPS
void CRenderer::UpdateFPS()
{
    static TTime lastTime;
    static TInt frameCount = 0;
    TTime currentTime;
    const TUint KOneSecond = 1000000; // 1s in ms.

    currentTime.HomeTime();
    ++frameCount;

    TTimeIntervalMicroSeconds timeDiff = currentTime.MicroSecondsFrom(lastTime);

    if (timeDiff.Int64() >= KOneSecond) {
        // Calculate FPS.
        iFPS = frameCount;

        // Reset frame count and last time.
        frameCount = 0;
        lastTime = currentTime;
    }
}
#endif

void CRenderer::SuspendScreenSaver(TBool aSuspend)
{
    iSuspendScreenSaver = aSuspend;
}

static SDL_Scancode ConvertScancode(int key)
{
    SDL_Keycode keycode;

    switch (key) {
    case EStdKeyBackspace: // Clear key
        keycode = SDLK_BACKSPACE;
        break;
    case 0x31: // 1
        keycode = SDLK_1;
        break;
    case 0x32: // 2
        keycode = SDLK_2;
        break;
    case 0x33: // 3
        keycode = SDLK_3;
        break;
    case 0x34: // 4
        keycode = SDLK_4;
        break;
    case 0x35: // 5
        keycode = SDLK_5;
        break;
    case 0x36: // 6
        keycode = SDLK_6;
        break;
    case 0x37: // 7
        keycode = SDLK_7;
        break;
    case 0x38: // 8
        keycode = SDLK_8;
        break;
    case 0x39: // 9
        keycode = SDLK_9;
        break;
    case 0x30: // 0
        keycode = SDLK_0;
        break;
    case 0x2a: // Asterisk
        keycode = SDLK_ASTERISK;
        break;
    case EStdKeyHash: // Hash
        keycode = SDLK_HASH;
        break;
    case EStdKeyDevice0: // Left softkey
        keycode = SDLK_SOFTLEFT;
        break;
    case EStdKeyDevice1: // Right softkey
        keycode = SDLK_SOFTRIGHT;
        break;
    case EStdKeyApplication0: // Call softkey
        keycode = SDLK_CALL;
        break;
    case EStdKeyApplication1: // End call softkey
        keycode = SDLK_ENDCALL;
        break;
    case EStdKeyDevice3: // Middle softkey
        keycode = SDLK_SELECT;
        break;
    case EStdKeyUpArrow: // Up arrow
        keycode = SDLK_UP;
        break;
    case EStdKeyDownArrow: // Down arrow
        keycode = SDLK_DOWN;
        break;
    case EStdKeyLeftArrow: // Left arrow
        keycode = SDLK_LEFT;
        break;
    case EStdKeyRightArrow: // Right arrow
        keycode = SDLK_RIGHT;
        break;
    default:
        keycode = SDLK_UNKNOWN;
        break;
    }

    return SDL_GetScancodeFromKey(keycode, NULL);
}

void CRenderer::HandleEvent(const TWsEvent &aWsEvent)
{
    Uint64 timestamp;

    switch (aWsEvent.Type()) {
    case EEventKeyDown: /* Key events */
        timestamp = SDL_GetPerformanceCounter();
        SDL_SendKeyboardKey(timestamp, 1, aWsEvent.Key()->iCode, ConvertScancode(aWsEvent.Key()->iScanCode), true);

#ifdef SDL_VIDEO_RENDER_NGAGE_FPS
        if (aWsEvent.Key()->iScanCode == EStdKeyHash) {
            if (iShowFPS) {
                iShowFPS = EFalse;
            } else {
                iShowFPS = ETrue;
            }
        }
#endif

        break;
    case EEventKeyUp: /* Key events */
        timestamp = SDL_GetPerformanceCounter();
        SDL_SendKeyboardKey(timestamp, 1, aWsEvent.Key()->iCode, ConvertScancode(aWsEvent.Key()->iScanCode), false);

    case EEventFocusGained:
        DisableKeyBlocking();
        if (!iDirectScreen->IsActive()) {
            TRAPD(err, iDirectScreen->StartL());
            if (KErrNone != err) {
                return;
            }
            iDirectScreen->ScreenDevice()->SetAutoUpdate(ETrue);
            iIsFocused = ETrue;
        }
        Flip();
        break;
    case EEventFocusLost:
    {
        if (iDirectScreen->IsActive()) {
            iDirectScreen->Cancel();
        }

        iIsFocused = EFalse;
        break;
    }
    default:
        break;
    }
}

void CRenderer::DisableKeyBlocking()
{
    TRawEvent aEvent;

    aEvent.Set((TRawEvent::TType) /*EDisableKeyBlock*/ 51);
    iWsSession.SimulateRawEvent(aEvent);
}

void CRenderer::PumpEvents()
{
    while (iWsEventStatus != KRequestPending) {
        iWsSession.GetEvent(iWsEvent);
        HandleEvent(iWsEvent);
        iWsEventStatus = KRequestPending;
        iWsSession.EventReady(&iWsEventStatus);
    }
}

#endif // SDL_VIDEO_RENDER_NGAGE
