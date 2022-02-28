/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_windowsvideo_h_
#define SDL_windowsvideo_h_

#include "../../core/windows/SDL_windows.h"

#include "../SDL_sysvideo.h"

#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#include <msctf.h>
#else
#include "SDL_msctf.h"
#endif

#include <imm.h>

#define MAX_CANDLIST    10
#define MAX_CANDLENGTH  256
#define MAX_CANDSIZE    (sizeof(WCHAR) * MAX_CANDLIST * MAX_CANDLENGTH)

#include "SDL_windowsclipboard.h"
#include "SDL_windowsevents.h"
#include "SDL_windowskeyboard.h"
#include "SDL_windowsmodes.h"
#include "SDL_windowsmouse.h"
#include "SDL_windowsopengl.h"
#include "SDL_windowsopengles.h"
#include "SDL_windowswindow.h"
#include "SDL_events.h"
#include "SDL_loadso.h"


#if WINVER < 0x0601
/* Touch input definitions */
#define TWF_FINETOUCH   1
#define TWF_WANTPALM    2

#define TOUCHEVENTF_MOVE 0x0001
#define TOUCHEVENTF_DOWN 0x0002
#define TOUCHEVENTF_UP   0x0004

DECLARE_HANDLE(HTOUCHINPUT);

typedef struct _TOUCHINPUT {
    LONG      x;
    LONG      y;
    HANDLE    hSource;
    DWORD     dwID;
    DWORD     dwFlags;
    DWORD     dwMask;
    DWORD     dwTime;
    ULONG_PTR dwExtraInfo;
    DWORD     cxContact;
    DWORD     cyContact;
} TOUCHINPUT, *PTOUCHINPUT;

#endif /* WINVER < 0x0601 */

#if WINVER < 0x0603

typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

typedef enum PROCESS_DPI_AWARENESS {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

#else
#include <shellscalingapi.h>
#endif /* WINVER < 0x0603 */

#ifndef _DPI_AWARENESS_CONTEXTS_

typedef enum DPI_AWARENESS {
    DPI_AWARENESS_INVALID = -1,
    DPI_AWARENESS_UNAWARE = 0,
    DPI_AWARENESS_SYSTEM_AWARE = 1,
    DPI_AWARENESS_PER_MONITOR_AWARE = 2
} DPI_AWARENESS;

DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);

#define DPI_AWARENESS_CONTEXT_UNAWARE           ((DPI_AWARENESS_CONTEXT)-1)
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE      ((DPI_AWARENESS_CONTEXT)-2)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ((DPI_AWARENESS_CONTEXT)-3)

#endif /* _DPI_AWARENESS_CONTEXTS_ */

/* Windows 10 Creators Update */
#if NTDDI_VERSION < 0x0A000003

#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)

#endif /* NTDDI_VERSION < 0x0A000003 */

typedef BOOL  (*PFNSHFullScreen)(HWND, DWORD);
typedef void  (*PFCoordTransform)(SDL_Window*, POINT*);

typedef struct
{
    void **lpVtbl;
    int refcount;
    void *data;
} TSFSink;

/* Definition from Win98DDK version of IMM.H */
typedef struct tagINPUTCONTEXT2 {
    HWND hWnd;
    BOOL fOpen;
    POINT ptStatusWndPos;
    POINT ptSoftKbdPos;
    DWORD fdwConversion;
    DWORD fdwSentence;
    union {
        LOGFONTA A;
        LOGFONTW W;
    } lfFont;
    COMPOSITIONFORM cfCompForm;
    CANDIDATEFORM cfCandForm[4];
    HIMCC hCompStr;
    HIMCC hCandInfo;
    HIMCC hGuideLine;
    HIMCC hPrivate;
    DWORD dwNumMsgBuf;
    HIMCC hMsgBuf;
    DWORD fdwInit;
    DWORD dwReserve[3];
} INPUTCONTEXT2, *PINPUTCONTEXT2, NEAR *NPINPUTCONTEXT2, FAR *LPINPUTCONTEXT2;

/* Private display data */

typedef struct SDL_VideoData
{
    int render;

    DWORD clipboard_count;

    /* Touch input functions */
    void* userDLL;
    BOOL (WINAPI *CloseTouchInputHandle)( HTOUCHINPUT );
    BOOL (WINAPI *GetTouchInputInfo)( HTOUCHINPUT, UINT, PTOUCHINPUT, int );
    BOOL (WINAPI *RegisterTouchWindow)( HWND, ULONG );
    BOOL (WINAPI *SetProcessDPIAware)( void );
    BOOL (WINAPI *SetProcessDpiAwarenessContext)( DPI_AWARENESS_CONTEXT );
    DPI_AWARENESS_CONTEXT (WINAPI *SetThreadDpiAwarenessContext)( DPI_AWARENESS_CONTEXT );
    DPI_AWARENESS_CONTEXT (WINAPI *GetThreadDpiAwarenessContext)( void );
    DPI_AWARENESS (WINAPI *GetAwarenessFromDpiAwarenessContext)( DPI_AWARENESS_CONTEXT );
    BOOL (WINAPI *EnableNonClientDpiScaling)( HWND );
    BOOL (WINAPI *AdjustWindowRectExForDpi)( LPRECT, DWORD, BOOL, DWORD, UINT );
    UINT (WINAPI *GetDpiForWindow)( HWND );
    BOOL (WINAPI *AreDpiAwarenessContextsEqual)(DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT);
    BOOL (WINAPI *IsValidDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);

    void* shcoreDLL;
    HRESULT (WINAPI *GetDpiForMonitor)( HMONITOR         hmonitor,
                                        MONITOR_DPI_TYPE dpiType,
                                        UINT             *dpiX,
                                        UINT             *dpiY );
    HRESULT (WINAPI *SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS dpiAwareness);

    SDL_bool ime_com_initialized;
    struct ITfThreadMgr *ime_threadmgr;
    SDL_bool ime_initialized;
    SDL_bool ime_enabled;
    SDL_bool ime_available;
    HWND ime_hwnd_main;
    HWND ime_hwnd_current;
    HIMC ime_himc;

    WCHAR ime_composition[SDL_TEXTEDITINGEVENT_TEXT_SIZE];
    WCHAR ime_readingstring[16];
    int ime_cursor;

    SDL_bool ime_candlist;
    WCHAR* ime_candidates;
    DWORD ime_candcount;
    DWORD ime_candref;
    DWORD ime_candsel;
    UINT ime_candpgsize;
    int ime_candlistindexbase;
    SDL_bool ime_candvertical;

    SDL_bool ime_dirty;
    SDL_Rect ime_rect;
    SDL_Rect ime_candlistrect;
    int ime_winwidth;
    int ime_winheight;

    HKL ime_hkl;
    void* ime_himm32;
    UINT (WINAPI *GetReadingString)(HIMC himc, UINT uReadingBufLen, LPWSTR lpwReadingBuf, PINT pnErrorIndex, BOOL *pfIsVertical, PUINT puMaxReadingLen);
    BOOL (WINAPI *ShowReadingWindow)(HIMC himc, BOOL bShow);
    LPINPUTCONTEXT2 (WINAPI *ImmLockIMC)(HIMC himc);
    BOOL (WINAPI *ImmUnlockIMC)(HIMC himc);
    LPVOID (WINAPI *ImmLockIMCC)(HIMCC himcc);
    BOOL (WINAPI *ImmUnlockIMCC)(HIMCC himcc);

    SDL_bool ime_uiless;
    struct ITfThreadMgrEx *ime_threadmgrex;
    DWORD ime_uielemsinkcookie;
    DWORD ime_alpnsinkcookie;
    DWORD ime_openmodesinkcookie;
    DWORD ime_convmodesinkcookie;
    TSFSink *ime_uielemsink;
    TSFSink *ime_ippasink;

    BYTE pre_hook_key_state[256];
    UINT _SDL_WAKEUP;
} SDL_VideoData;

extern SDL_bool g_WindowsEnableMessageLoop;
extern SDL_bool g_WindowFrameUsableWhileCursorHidden;

typedef struct IDirect3D9 IDirect3D9;
extern SDL_bool D3D_LoadDLL( void **pD3DDLL, IDirect3D9 **pDirect3D9Interface );

extern SDL_bool WIN_IsPerMonitorV2DPIAware(_THIS);

#endif /* SDL_windowsvideo_h_ */

/* vi: set ts=4 sw=4 expandtab: */
