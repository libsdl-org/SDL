/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#include "../../SDL_hints_c.h"
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

bool NGAGE_CreateTextureData(NGAGE_TextureData *data, const int width, const int height, const int access)
{
    return gRenderer->CreateTextureData(data, width, height, access);
}

void NGAGE_DestroyTextureData(NGAGE_TextureData *data)
{
    if (data) {
        if (data->gc) {
            delete data->gc;
            data->gc = NULL;
        }
        if (data->device) {
            delete data->device;
            data->device = NULL;
        }
        if (data->mask_bitmap) {
            delete data->mask_bitmap;
            data->mask_bitmap = NULL;
        }
        delete data->bitmap;
        data->bitmap = NULL;
    }
}

void *NGAGE_GetBitmapDataAddress(NGAGE_TextureData *data)
{
    if (!data || !data->bitmap) {
        return NULL;
    }
    return data->bitmap->DataAddress();
}

int NGAGE_GetBitmapScanLineLength(NGAGE_TextureData *data)
{
    if (!data || !data->bitmap) {
        return 0;
    }
    return (int)CFbsBitmap::ScanLineLength(data->bitmap->SizeInPixels().iWidth, EColor4K);
}

void NGAGE_DrawLines(NGAGE_Vertex *verts, const int count)
{
    gRenderer->DrawLines(verts, count);
}

void NGAGE_DrawPoints(NGAGE_Vertex *verts, const int count)
{
    gRenderer->DrawPoints(verts, count);
}

void NGAGE_DrawGeometry(NGAGE_Vertex *verts, const int count)
{
    gRenderer->DrawGeometry(verts, count);
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

void NGAGE_SetRenderTargetInternal(NGAGE_TextureData *target)
{
    if (gRenderer) {
        gRenderer->SetRenderTarget(target);
    }
}

static void SDLCALL NGAGE_ShowFPSChanged(void *userdata, const char *name, const char *oldValue, const char *newValue)
{
    CRenderer *renderer = (CRenderer *)userdata;
    renderer->SetShowFPS(SDL_GetStringBoolean(newValue, false));
}

void *NGAGE_GetBackbufferAddress(void)
{
    return gRenderer->GetCurrentBitmap()->DataAddress();
}

int NGAGE_GetBackbufferPitch(void)
{
    return CFbsBitmap::ScanLineLength(NGAGE_SCREEN_WIDTH, EColor4K) / 2;
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

CRenderer::CRenderer() : iRenderer(0), iDirectScreen(0), iScreenGc(0), iWsSession(), iWsWindowGroup(), iWsWindowGroupID(0), iWsWindow(), iWsScreen(0), iWsEventStatus(), iWsEvent(), iShowFPS(EFalse), iFPS(0), iFont(0), iCurrentRenderTarget(0), iPixelBufferA(0), iPixelBufferB(0), iPixelBufferSize(0), iScratchBitmap(0), iMaskBitmap(0), iPointsBuffer(0), iPointsBufferSize(0) {}

CRenderer::~CRenderer()
{
    SDL_RemoveHintCallback(SDL_HINT_RENDER_NGAGE_SHOW_FPS, NGAGE_ShowFPSChanged, this);

    delete iRenderer;
    iRenderer = 0;

    SDL_free(iPixelBufferA);
    SDL_free(iPixelBufferB);
    delete iScratchBitmap;
    iScratchBitmap = 0;
    delete iMaskBitmap;
    iMaskBitmap = 0;
    delete[] iPointsBuffer;
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

    SDL_AddHintCallback(SDL_HINT_RENDER_NGAGE_SHOW_FPS, NGAGE_ShowFPSChanged, this);
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
    CFbsBitGc *gc = GetCurrentGc();
    if (gc) {
        gc->SetBrushColor(iColor);
        gc->Clear();
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
    if (!phdata || !phdata->bitmap) {
        return false;
    }

    SDL_FColor *c = &texture->color;
    const int bytes_per_pixel = 2;

    int sw = srcrect->w;
    int sh = srcrect->h;

    // Fast path: render target texture with no color mod.
    // BitBlt directly from its bitmap — DataAddress() is unreliable
    // for bitmaps that have been drawn into via a CFbsBitGc.
    bool no_color_mod = (c->a == 1.f && c->r == 1.f && c->g == 1.f && c->b == 1.f);
    float sx, sy;
    SDL_GetRenderScale(renderer, &sx, &sy);
    bool no_scale = (sx == 1.f && sy == 1.f);

    SDL_BlendMode blend;
    SDL_GetTextureBlendMode(texture, &blend);
    bool no_color_key = (blend != SDL_BLENDMODE_BLEND);

    if (phdata->gc && no_color_mod && no_scale && no_color_key) {
        CFbsBitGc *gc = GetCurrentGc();
        if (gc) {
            TRect aSource(TPoint(srcrect->x, srcrect->y), TSize(sw, sh));
            TPoint aDest(dstrect->x, dstrect->y);
            gc->BitBlt(aDest, phdata->bitmap, aSource);
        }
        return true;
    }

    // Fast path: color-key with no color mod and no scale.
    // Blit directly from the source bitmap into the destination, skipping transparent pixels.
    if (no_color_mod && no_scale && !no_color_key && phdata->has_color_key) {
        void *tex_data_ck = phdata->bitmap->DataAddress();
        CFbsBitmap *dst_bmp = GetCurrentBitmap();
        if (dst_bmp && tex_data_ck) {
            int tex_stride_ck = CFbsBitmap::ScanLineLength(phdata->bitmap->SizeInPixels().iWidth, EColor4K) / 2;
            TUint16 *src_base = static_cast<TUint16 *>(tex_data_ck) + srcrect->y * tex_stride_ck + srcrect->x;
            BlitWithAlphaKey(dst_bmp, dstrect->x, dstrect->y, src_base, sw, sh, tex_stride_ck);
        }
        return true;
    }

    int src_pitch = sw * bytes_per_pixel;
    int tex_pitch = CFbsBitmap::ScanLineLength(texture->w, EColor4K);

    void *tex_data = phdata->bitmap->DataAddress();
    if (!tex_data) {
        return false;
    }

    TInt required_size = src_pitch * sh;
    if (required_size > iPixelBufferSize) {
        void *new_buffer_a = SDL_realloc(iPixelBufferA, required_size);
        if (!new_buffer_a) {
            return false;
        }
        iPixelBufferA = new_buffer_a;

        void *new_buffer_b = SDL_realloc(iPixelBufferB, required_size);
        if (!new_buffer_b) {
            return false;
        }
        iPixelBufferB = new_buffer_b;

        iPixelBufferSize = required_size;
    }

    // Ensure scratch bitmap is allocated and large enough.
    if (!iScratchBitmap) {
        iScratchBitmap = new CFbsBitmap();
        if (!iScratchBitmap) {
            return false;
        }
    }

    TSize scratch_size = iScratchBitmap->SizeInPixels();
    if (scratch_size.iWidth < sw || scratch_size.iHeight < sh) {
        iScratchBitmap->Reset();
        TInt err = iScratchBitmap->Create(TSize(sw, sh), EColor4K);
        if (err != KErrNone) {
            return false;
        }
    }

    // Extract the srcrect region from the texture into buffer A.
    {
        TUint16 *tex_pixels = (TUint16 *)tex_data;
        TUint16 *buf_pixels = (TUint16 *)iPixelBufferA;
        int tex_pitch_u16 = tex_pitch / 2;
        for (int y = 0; y < sh; ++y) {
            TUint16 *src_row = tex_pixels + (srcrect->y + y) * tex_pitch_u16 + srcrect->x;
            TUint16 *dst_row = buf_pixels + y * sw;
            Mem::Copy(dst_row, src_row, src_pitch);
        }
    }

    void *source = iPixelBufferA;
    void *dest = iPixelBufferB;

    if (!no_color_mod) {
        ApplyColorMod(dest, source, src_pitch, sw, sh, texture->color);
        void *tmp = source;
        source = dest;
        dest = tmp;
    }

    if (!no_scale) {
        TFixed scale_x = Real2Fix(sx);
        TFixed scale_y = Real2Fix(sy);
        TFixed center_x = Int2Fix(sw / 2);
        TFixed center_y = Int2Fix(sh / 2);
        ApplyScale(dest, source, src_pitch, sw, sh, center_x, center_y, scale_x, scale_y);
        void *tmp = source;
        source = dest;
        dest = tmp;
    }

    // Copy result into scratch bitmap and blit from there.
    // The source texture is never modified.
    {
        TUint16 *scratch_pixels = (TUint16 *)iScratchBitmap->DataAddress();
        TUint16 *res_pixels = (TUint16 *)source;
        int scratch_pitch_u16 = CFbsBitmap::ScanLineLength(iScratchBitmap->SizeInPixels().iWidth, EColor4K) / 2;

        // Always copy all pixels into the scratch bitmap.
        for (int y = 0; y < sh; ++y) {
            TUint16 *dst_row = scratch_pixels + y * scratch_pitch_u16;
            TUint16 *src_row = res_pixels + y * sw;
            Mem::Copy(dst_row, src_row, src_pitch);
        }

        CFbsBitGc *gc = GetCurrentGc();
        if (gc) {
            TRect aSource(TPoint(0, 0), TSize(sw, sh));
            TPoint aDest(dstrect->x, dstrect->y);

            if (!no_color_key && phdata->has_color_key) {
                CFbsBitmap *dst_bmp = GetCurrentBitmap();
                if (dst_bmp) {
                    BlitWithAlphaKey(dst_bmp, dstrect->x, dstrect->y, res_pixels, sw, sh, sw);
                }
            } else {
                gc->BitBlt(aDest, iScratchBitmap, aSource);
            }
        }
    }

    return true;
}

bool CRenderer::CopyEx(SDL_Renderer *renderer, SDL_Texture *texture, const NGAGE_CopyExData *copydata)
{
    NGAGE_TextureData *phdata = (NGAGE_TextureData *)texture->internal;
    if (!phdata || !phdata->bitmap) {
        return false;
    }

    SDL_FColor *c = &texture->color;
    const int bytes_per_pixel = 2;

    int sw = copydata->srcrect.w;
    int sh = copydata->srcrect.h;
    int src_pitch = sw * bytes_per_pixel;
    int tex_pitch = CFbsBitmap::ScanLineLength(texture->w, EColor4K);

    void *tex_data = phdata->bitmap->DataAddress();
    if (!tex_data) {
        return false;
    }

    TInt required_size = src_pitch * sh;
    if (required_size > iPixelBufferSize) {
        void *new_buffer_a = SDL_realloc(iPixelBufferA, required_size);
        if (!new_buffer_a) {
            return false;
        }
        iPixelBufferA = new_buffer_a;

        void *new_buffer_b = SDL_realloc(iPixelBufferB, required_size);
        if (!new_buffer_b) {
            return false;
        }
        iPixelBufferB = new_buffer_b;

        iPixelBufferSize = required_size;
    }

    // Ensure scratch bitmap is allocated and large enough for the srcrect.
    if (!iScratchBitmap) {
        iScratchBitmap = new CFbsBitmap();
        if (!iScratchBitmap) {
            return false;
        }
    }
    TSize scratch_size = iScratchBitmap->SizeInPixels();
    if (scratch_size.iWidth < sw || scratch_size.iHeight < sh) {
        iScratchBitmap->Reset();
        TInt err = iScratchBitmap->Create(TSize(sw, sh), EColor4K);
        if (err != KErrNone) {
            return false;
        }
    }

    // Extract the srcrect region from the texture into buffer A.
    {
        TUint16 *tex_pixels = (TUint16 *)tex_data;
        TUint16 *buf_pixels = (TUint16 *)iPixelBufferA;
        int tex_pitch_u16 = tex_pitch / 2;
        for (int y = 0; y < sh; ++y) {
            TUint16 *src_row = tex_pixels + (copydata->srcrect.y + y) * tex_pitch_u16 + copydata->srcrect.x;
            TUint16 *dst_row = buf_pixels + y * sw;
            Mem::Copy(dst_row, src_row, src_pitch);
        }
    }

    void *source = iPixelBufferA;
    void *dest = iPixelBufferB;

    if (copydata->flip) {
        ApplyFlip(dest, source, src_pitch, sw, sh, copydata->flip);
        void *tmp = source;
        source = dest;
        dest = tmp;
    }

    if (copydata->scale_x != 1.f || copydata->scale_y != 1.f) {
        ApplyScale(dest, source, src_pitch, sw, sh, copydata->center.x, copydata->center.y, copydata->scale_x, copydata->scale_y);
        void *tmp = source;
        source = dest;
        dest = tmp;
    }

    if (copydata->angle) {
        ApplyRotation(dest, source, src_pitch, sw, sh, copydata->center.x, copydata->center.y, copydata->angle);
        void *tmp = source;
        source = dest;
        dest = tmp;
    }

    if (c->a != 1.f || c->r != 1.f || c->g != 1.f || c->b != 1.f) {
        ApplyColorMod(dest, source, src_pitch, sw, sh, texture->color);
        void *tmp = source;
        source = dest;
        dest = tmp;
    }

    // Copy the final result into the scratch bitmap and blit from there.
    // The source texture is never modified.
    {
        SDL_BlendMode blend;
        SDL_GetTextureBlendMode(texture, &blend);
        bool has_color_key = (blend == SDL_BLENDMODE_BLEND);

        TUint16 *scratch_pixels = (TUint16 *)iScratchBitmap->DataAddress();
        TUint16 *res_pixels = (TUint16 *)source;
        int scratch_pitch_u16 = CFbsBitmap::ScanLineLength(iScratchBitmap->SizeInPixels().iWidth, EColor4K) / 2;

        // Always copy all pixels into the scratch bitmap.
        for (int y = 0; y < sh; ++y) {
            TUint16 *dst_row = scratch_pixels + y * scratch_pitch_u16;
            TUint16 *src_row = res_pixels + y * sw;
            Mem::Copy(dst_row, src_row, src_pitch);
        }

        CFbsBitGc *gc = GetCurrentGc();
        if (gc) {
            TRect aSource(TPoint(0, 0), TSize(sw, sh));
            TPoint aDest(copydata->dstrect.x, copydata->dstrect.y);

            if (has_color_key && phdata->has_color_key) {
                CFbsBitmap *dst_bmp = GetCurrentBitmap();
                if (dst_bmp) {
                    BlitWithAlphaKey(dst_bmp, copydata->dstrect.x, copydata->dstrect.y, res_pixels, sw, sh, sw);
                }
            } else {
                gc->BitBlt(aDest, iScratchBitmap, aSource);
            }
        }
    }

    return true;
}

bool CRenderer::CreateTextureData(NGAGE_TextureData *aTextureData, const TInt aWidth, const TInt aHeight, const TInt aAccess)
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

    if (aAccess == SDL_TEXTUREACCESS_TARGET) {
        TRAPD(err1, aTextureData->device = CFbsBitmapDevice::NewL(aTextureData->bitmap));
        if (err1 != KErrNone || !aTextureData->device) {
            delete aTextureData->bitmap;
            aTextureData->bitmap = NULL;
            return false;
        }

        TRAPD(err2, aTextureData->gc = CFbsBitGc::NewL());
        if (err2 != KErrNone || !aTextureData->gc) {
            delete aTextureData->device;
            aTextureData->device = NULL;
            delete aTextureData->bitmap;
            aTextureData->bitmap = NULL;
            return false;
        }

        aTextureData->gc->Activate(aTextureData->device);
    } else {
        aTextureData->gc = NULL;
        aTextureData->device = NULL;
    }

    return true;
}

void CRenderer::DrawLines(NGAGE_Vertex *aVerts, const TInt aCount)
{
    CFbsBitGc *gc = GetCurrentGc();
    if (gc) {
        gc->SetPenStyle(CGraphicsContext::ESolidPen);

        // Draw lines as pairs of points (start, end)
        for (TInt i = 0; i < aCount - 1; i += 2) {
            TPoint start(aVerts[i].x, aVerts[i].y);
            TPoint end(aVerts[i + 1].x, aVerts[i + 1].y);

            TRgb color = TRgb(aVerts[i].color.r, aVerts[i].color.g, aVerts[i].color.b);

            gc->SetPenColor(color);
            gc->DrawLine(start, end);
        }
    }
}

void CRenderer::DrawPoints(NGAGE_Vertex *aVerts, const TInt aCount)
{
    CFbsBitGc *gc = GetCurrentGc();
    if (gc) {
        for (TInt i = 0; i < aCount; i++, aVerts++) {
            TUint32 aColor = (((TUint8)aVerts->color.a << 24) |
                              ((TUint8)aVerts->color.b << 16) |
                              ((TUint8)aVerts->color.g << 8) |
                              (TUint8)aVerts->color.r);

            gc->SetPenColor(aColor);
            gc->Plot(TPoint(aVerts->x, aVerts->y));
        }
    }
}

// Gouraud-shaded triangle scanline fill directly into an EColor4K (XRGB4444) framebuffer.
// Colors are interpolated per-scanline endpoint and per-pixel.
static void FillTriangle(TUint16 *aPixels, TInt aStride,
                         TInt aBmpW, TInt aBmpH,
                         TInt aX0, TInt aY0, TInt aR0, TInt aG0, TInt aB0,
                         TInt aX1, TInt aY1, TInt aR1, TInt aG1, TInt aB1,
                         TInt aX2, TInt aY2, TInt aR2, TInt aG2, TInt aB2)
{
    // Sort vertices by Y ascending (bubble sort on 3 elements).
    // Swap positions and colors together.
#define SWAP3(ax, ay, ar, ag, ab, bx, by, br, bg, bb) \
    do {                                              \
        TInt _t;                                      \
        _t = ax;                                      \
        ax = bx;                                      \
        bx = _t;                                      \
        _t = ay;                                      \
        ay = by;                                      \
        by = _t;                                      \
        _t = ar;                                      \
        ar = br;                                      \
        br = _t;                                      \
        _t = ag;                                      \
        ag = bg;                                      \
        bg = _t;                                      \
        _t = ab;                                      \
        ab = bb;                                      \
        bb = _t;                                      \
    } while (0)

    if (aY0 > aY1) {
        SWAP3(aX0, aY0, aR0, aG0, aB0, aX1, aY1, aR1, aG1, aB1);
    }
    if (aY1 > aY2) {
        SWAP3(aX1, aY1, aR1, aG1, aB1, aX2, aY2, aR2, aG2, aB2);
    }
    if (aY0 > aY1) {
        SWAP3(aX0, aY0, aR0, aG0, aB0, aX1, aY1, aR1, aG1, aB1);
    }
#undef SWAP3

    TInt totalHeight = aY2 - aY0;
    if (totalHeight == 0) {
        return;
    }

    // Walk upper half [y0..y1] and lower half [y1..y2].
    for (TInt part = 0; part < 2; ++part) {
        TInt segHeight = (part == 0) ? (aY1 - aY0) : (aY2 - aY1);
        if (segHeight == 0) {
            continue;
        }
        TInt yStart = (part == 0) ? aY0 : aY1;
        TInt yEnd = (part == 0) ? aY1 : aY2;

        for (TInt y = yStart; y <= yEnd; ++y) {
            if (y < 0 || y >= aBmpH) {
                continue;
            }

            // 16.16 interpolation factors for long edge (v0->v2) and short edge.
            TInt tLong = ((y - aY0) << 16) / totalHeight;
            TInt tShort = ((y - yStart) << 16) / segHeight;

            // Interpolate X along both edges.
            TInt xLong = aX0 + (((aX2 - aX0) * tLong) >> 16);
            TInt xShort = (part == 0)
                              ? aX0 + (((aX1 - aX0) * tShort) >> 16)
                              : aX1 + (((aX2 - aX1) * tShort) >> 16);

            // Interpolate color along both edges (values 0-255).
            TInt rLong = aR0 + (((aR2 - aR0) * tLong) >> 16);
            TInt gLong = aG0 + (((aG2 - aG0) * tLong) >> 16);
            TInt bLong = aB0 + (((aB2 - aB0) * tLong) >> 16);
            TInt rShort, gShort, bShort;
            if (part == 0) {
                rShort = aR0 + (((aR1 - aR0) * tShort) >> 16);
                gShort = aG0 + (((aG1 - aG0) * tShort) >> 16);
                bShort = aB0 + (((aB1 - aB0) * tShort) >> 16);
            } else {
                rShort = aR1 + (((aR2 - aR1) * tShort) >> 16);
                gShort = aG1 + (((aG2 - aG1) * tShort) >> 16);
                bShort = aB1 + (((aB2 - aB1) * tShort) >> 16);
            }

            // Determine left/right endpoints and their colors.
            TInt xLeft, xRight;
            TInt rLeft, gLeft, bLeft;
            TInt rRight, gRight, bRight;
            if (xLong < xShort) {
                xLeft = xLong;
                xRight = xShort;
                rLeft = rLong;
                gLeft = gLong;
                bLeft = bLong;
                rRight = rShort;
                gRight = gShort;
                bRight = bShort;
            } else {
                xLeft = xShort;
                xRight = xLong;
                rLeft = rShort;
                gLeft = gShort;
                bLeft = bShort;
                rRight = rLong;
                gRight = gLong;
                bRight = bLong;
            }

            // Clamp X to bitmap width.
            if (xLeft < 0) {
                xLeft = 0;
            }
            if (xRight >= aBmpW) {
                xRight = aBmpW - 1;
            }

            TInt spanWidth = xRight - xLeft;
            TUint16 *row = aPixels + y * aStride;

            // Compute per-pixel color deltas once per span (one division each)
            // then step incrementally; avoids a division per pixel.
            if (spanWidth > 0) {
                TInt dr = ((rRight - rLeft) << 16) / spanWidth;
                TInt dg = ((gRight - gLeft) << 16) / spanWidth;
                TInt db = ((bRight - bLeft) << 16) / spanWidth;
                TInt r = rLeft << 16;
                TInt g = gLeft << 16;
                TInt b = bLeft << 16;
                for (TInt x = xLeft; x <= xRight; ++x) {
                    // Pack to XRGB4444.
                    row[x] = (TUint16)((((r >> 16) >> 4) << 8) | (((g >> 16) >> 4) << 4) | ((b >> 16) >> 4));
                    r += dr;
                    g += dg;
                    b += db;
                }
            } else {
                row[xLeft] = (TUint16)(((rLeft >> 4) << 8) | ((gLeft >> 4) << 4) | (bLeft >> 4));
            }
        }
    }
}

void CRenderer::DrawGeometry(NGAGE_Vertex *aVerts, const TInt aCount)
{
    if (aCount < 3) {
        return;
    }

    CFbsBitmap *bmp = GetCurrentBitmap();
    if (bmp) {
        TUint16 *pixels = reinterpret_cast<TUint16 *>(bmp->DataAddress());
        if (pixels) {
            TSize bmpSize = bmp->SizeInPixels();
            TInt stride = CFbsBitmap::ScanLineLength(bmpSize.iWidth, EColor4K) / 2;

            for (TInt i = 0; i + 2 < aCount; i += 3) {
                FillTriangle(pixels, stride,
                             bmpSize.iWidth, bmpSize.iHeight,
                             aVerts[i].x, aVerts[i].y,
                             aVerts[i].color.r, aVerts[i].color.g, aVerts[i].color.b,
                             aVerts[i + 1].x, aVerts[i + 1].y,
                             aVerts[i + 1].color.r, aVerts[i + 1].color.g, aVerts[i + 1].color.b,
                             aVerts[i + 2].x, aVerts[i + 2].y,
                             aVerts[i + 2].color.r, aVerts[i + 2].color.g, aVerts[i + 2].color.b);
            }
            return;
        }
    }
}

void CRenderer::FillRects(NGAGE_Vertex *aVerts, const TInt aCount)
{
    CFbsBitGc *gc = GetCurrentGc();
    if (gc) {
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

            gc->SetPenColor(aColor);
            gc->SetBrushColor(aColor);
            gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
            gc->SetPenStyle(CGraphicsContext::ENullPen);
            gc->DrawRect(rect);
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
    CFbsBitGc *gc = GetCurrentGc();
    if (gc) {
        gc->SetPenColor(iColor);
        gc->SetBrushColor(iColor);
        gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
    }

    if (iRenderer) {
        TRAPD(err, iRenderer->SetCurrentColor(iColor));
        if (err != KErrNone) {
            return;
        }
    }
}

void CRenderer::SetClipRect(TInt aX, TInt aY, TInt aWidth, TInt aHeight)
{
    CFbsBitGc *gc = GetCurrentGc();
    if (gc) {
        TRect viewportRect(aX, aY, aX + aWidth, aY + aHeight);
        gc->SetClippingRect(viewportRect);
    }
}

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

void CRenderer::SuspendScreenSaver(TBool aSuspend)
{
    iSuspendScreenSaver = aSuspend;
}

void CRenderer::SetRenderTarget(NGAGE_TextureData *aTarget)
{
    iCurrentRenderTarget = aTarget;
}

CFbsBitGc *CRenderer::GetCurrentGc()
{
    if (iCurrentRenderTarget && iCurrentRenderTarget->gc) {
        return iCurrentRenderTarget->gc;
    }
    return iRenderer ? iRenderer->Gc() : NULL;
}

CFbsBitmap *CRenderer::GetCurrentBitmap()
{
    if (iCurrentRenderTarget && iCurrentRenderTarget->bitmap) {
        return iCurrentRenderTarget->bitmap;
    }
    return iRenderer ? iRenderer->Bitmap() : NULL;
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

        break;
    case EEventKeyUp: /* Key events */
        timestamp = SDL_GetPerformanceCounter();
        SDL_SendKeyboardKey(timestamp, 1, aWsEvent.Key()->iCode, ConvertScancode(aWsEvent.Key()->iScanCode), false);

        break;
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
