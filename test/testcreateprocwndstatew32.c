/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* Sample program: When starting a process, the parent can influence the initial window shown (minimized / maximized). We Test SDL's ability to defuse this. */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>
#include <Windows.h>

enum
{
    TEST_CHILD_SUCCEEDED = 0,
    TEST_CHILD_WINDOW_MAXIMIZED = 1,
    TEST_CHILD_WINDOW_MINIMIZED = 2,

    TEST_CHILD_NO_WINDOW = 10,
};
static const char test_window_title[] = "sdl_maximize_test_wnd";
static HWND test_window_handle = NULL;
static int process_exit_code = TEST_CHILD_SUCCEEDED;
static HHOOK hook_handle = NULL;

static void create_dlg()
{
    SDL_MessageBoxButtonData buttons[1] = { 0 };
    SDL_MessageBoxData mbdata = { 0 };
    int button = -1;

    buttons[0].buttonID = 0;
    buttons[0].text = "Quit";

    mbdata.flags = SDL_MESSAGEBOX_INFORMATION;
    mbdata.message = "Maximize testcase";
    mbdata.title = test_window_title;
    mbdata.numbuttons = 1;
    mbdata.buttons = buttons;

    SDL_ShowMessageBox(&mbdata, &button);
}

VOID CALLBACK TimerProc(HWND hWnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    if (!IsWindowVisible(test_window_handle)) {
        /* Our timing might need to be relaxed a bit */
        process_exit_code = TEST_CHILD_NO_WINDOW;
    }
    PostMessageA(test_window_handle, WM_CLOSE, 0, 0);
    KillTimer(NULL, idEvent);
}

static LRESULT CALLBACK CallWndProc(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        CWPSTRUCT *cwp = (CWPSTRUCT *)lParam;
        CREATESTRUCT *cs;
        switch (cwp->message) {
        case WM_CREATE:
            cs = (CREATESTRUCT *)cwp->lParam;
            if (cs && cs->lpszName && !strcmp(cs->lpszName, test_window_title)) {
                if (test_window_handle) {
                    SDL_Log("WARNING: test_window_handle already set!\n");
                }
                test_window_handle = cwp->hwnd;
                SDL_Log("WM_CREATE: %s\n", test_window_title);
                /* Give the window max 3 seconds to show */
                SetTimer(NULL, 0, 3000, TimerProc);
            }
            break;
        case WM_DESTROY:
            if (test_window_handle == cwp->hwnd) {
                test_window_handle = NULL;
                SDL_Log("WM_DESTROY: %s\n", test_window_title);
            }
            break;
        case WM_SIZE:
            if (test_window_handle == cwp->hwnd) {
                /* Inspect our initial size */
                if (cwp->wParam == SIZE_MAXIMIZED) {
                    SDL_Log("WM_SIZE: SIZE_MAXIMIZED\n");
                    process_exit_code = TEST_CHILD_WINDOW_MAXIMIZED;
                } else if (cwp->wParam == SIZE_MINIMIZED) {
                    SDL_Log("WM_SIZE: SIZE_MINIMIZED\n");
                    process_exit_code = TEST_CHILD_WINDOW_MINIMIZED;
                }
                /* Now that we know the initial size, close the window asap */
                PostMessageA(test_window_handle, WM_CLOSE, 0, 0);
            }
            break;
        }
    }
    return CallNextHookEx(hook_handle, nCode, wParam, lParam);
}

static void spawn_child_process(void)
{
    char cmdline[MAX_PATH + 30] = { 0 };
    char path[MAX_PATH] = { 0 };
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    /* Retrieve our executable path */
    GetModuleFileNameA(NULL, path, SDL_arraysize(path));

    /* Build the full command line for our child process */
    SDL_snprintf(cmdline, SDL_arraysize(cmdline), "\"%s\" --child-spawn-dialog", path);

    /* We set flags here asking Windows to maximize the initial window */
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_MAXIMIZE;
    if (CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, 4000)) {
            SDLTest_AssertCheck(false, "Child process did not quit!");
            /* Force it */
            TerminateProcess(pi.hProcess, 1);
        } else {
            /* The child process exits with a non-0 code when the window is minimized / maximized */
            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            SDLTest_AssertCheck(exit_code == 0, "Child process failed with code %lu!", exit_code);
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int main(int argc, char *argv[])
{
    SDLTest_CommonState *state;
    int i;
    int spawn_test_dialog = 0;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (state == NULL) {
        return 1;
    }

    /* Parse commandline */
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);

        if (!consumed) {
            if (SDL_strcmp(argv[i], "--child-spawn-dialog") == 0) {
                consumed = 1;
                spawn_test_dialog = 1;
            }
        }

        if (consumed <= 0) {
            static const char *options[] = { "[--child-spawn-dialog]", NULL};
            SDLTest_CommonLogUsage(state, argv[0], options);
            return 1;
        }

        i += consumed;
    }

    if (spawn_test_dialog) {
        /* Install a hook to watch for events */
        hook_handle = SetWindowsHookExA(WH_CALLWNDPROC, CallWndProc, NULL, GetCurrentThreadId());
        if (hook_handle) {
            /* The hook was installed succesfully, now spawn the dialog */
            create_dlg();
            /* We are done, remove the hook */
            UnhookWindowsHookEx(hook_handle);
            hook_handle = NULL;
        }
    } else {
        /* Spawn a child process containing the checks */
        spawn_child_process();
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return process_exit_code;
}
