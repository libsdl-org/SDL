/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Simple test of the SDL MessageBox API */
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

/* This enables themed Windows dialogs when building with Visual Studio */
#if defined(SDL_PLATFORM_WINDOWS) && defined(_MSC_VER)
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0'  processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc)
{
    SDL_Quit();
    /* Let 'main()' return normally */
    if (rc != 0) {
        exit(rc);
    }
}

static int SDLCALL button_messagebox(void *eventNumber)
{
    int i;
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
          0,
          "OK" },
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT,
          1,
          "Cancel" },
        { 0,
          2,
          "Retry" }
    };
    SDL_MessageBoxData data = {
        SDL_MESSAGEBOX_INFORMATION,
        NULL, /* no parent window */
        "Custom MessageBox",
        "This is a custom messagebox",
        SDL_arraysize(buttons),
        NULL, /* buttons */
        NULL  /* Default color scheme */
    };

    for (i = 0; ; ++i) {
        SDL_MessageBoxColorScheme colorScheme;
        if (i != 0) {
            int j;
            for (j = 0; j < SDL_MESSAGEBOX_COLOR_COUNT; ++j) {
                colorScheme.colors[j].r = SDL_rand(256);
                colorScheme.colors[j].g = SDL_rand(256);
                colorScheme.colors[j].b = SDL_rand(256);
            }
            data.colorScheme = &colorScheme;
        } else {
            data.colorScheme = NULL;
        }

        int button = -1;
        data.buttons = buttons;
        if (eventNumber) {
            data.message = "This is a custom messagebox from a background thread.";
        }

        if (!SDL_ShowMessageBox(&data, &button)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
            if (eventNumber) {
                SDL_Event event;
                event.type = (Uint32)(intptr_t)eventNumber;
                SDL_PushEvent(&event);
                return 1;
            } else {
                quit(2);
            }
        }

        const char* text;
        if (button == 1) {
            text = "Cancel";
        } else if (button == 2) {
            text = "Retry";
        } else {
            text = "OK";
        }
        SDL_Log("Pressed button: %d, %s", button, button == -1 ? "[closed]" : text);

        if (button == 2) {
            continue;
        }

        if (eventNumber) {
            SDL_Event event;
            event.type = (Uint32)(intptr_t)eventNumber;
            SDL_PushEvent(&event);
        }

        return 0;
    }
}

int main(int argc, char *argv[])
{
    bool success;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Simple MessageBox",
                                       "This is a simple error MessageBox",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }

    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING,
                                       "Simple MessageBox",
                                       "This is a simple MessageBox with a newline:\r\nHello world!",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }

    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       NULL,
                                       "NULL Title",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }

    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "NULL Message",
                                       NULL,
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }

    success = SDL_ShowSimpleMessageBox(0,
                                       "No icon",
                                       "This is a MessageBox with no icon!",
                                       NULL);

    /* Google says this is Traditional Chinese for "beef with broccoli" */
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "UTF-8 Simple MessageBox",
                                       "Unicode text: '牛肉西蘭花' ...",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }

    /* Google says this is Traditional Chinese for "beef with broccoli" */
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "UTF-8 Simple MessageBox",
                                       "Unicode text and newline:\r\n'牛肉西蘭花'\n'牛肉西蘭花'",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
    
    /* Google says this is Traditional Chinese for "beef with broccoli" */
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "牛肉西蘭花",
                                       "Unicode text in the title.",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
    
	/* Various other Unicode tests */
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Latin",
                                       "Türkçe özellikle çok güzel bir dil.\nBjörn/Bjørn\nÆgypt",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
    
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Arabic",
                                       "يتم استخدام أنظمة الكتابة لتسجيل اللغة البشرية.",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
    
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Arabic (multi line)",
                                       "سطر طويل جدًا من النص\nخط قصير\nسطر طويل للغاية من النص مذهل بشكل لا يصدق في اللغة العربية التي يتم التحدث بها في منطقة الشرق الأوسط وشمال أفريقيا",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
    
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Cyrillic (Ukranian)",
                                       "Для запису людської мови використовуються системи письма.",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
    
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Japanese",
                                       "文字体系は人間の言語を記録するために使用されます。",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
 
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Indian",
                                       "लेखन प्रणालियों का उपयोग मानव भाषा को रिकॉर्ड करने के लिए किया जाता है।",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
  
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Korean",
                                       "문자 체계는 인간의 언어를 기록하는 데 사용됩니다.",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
      
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Thai",
                                       "ระบบการเขียนใช้เพื่อบันทึกภาษาของมนุษย์",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
    
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Greek",
                                       "Τα συστήματα γραφής χρησιμοποιούνται για την καταγραφή της ανθρώπινης γλώσσας.",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }
          
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Hebrew",
                                       "מערכות כתיבה משמשות לרישום שפה אנושית.",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }          
    
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Armenian",
                                       "Գրային համակարգերը օգտագործվում են մարդկային լեզուն գրանցելու համար։",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }         
     
    success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Georgian",
                                       "წერის სისტემები გამოიყენება ადამიანის ენის ჩასაწერად.",
                                       NULL);
    if (!success) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
        quit(1);
    }    
          
    button_messagebox(NULL);

    /* Test showing a message box from a background thread.

       On macOS, the video subsystem needs to be initialized for this
       to work, since the message box events are dispatched by the Cocoa
       subsystem on the main thread.
     */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL video subsystem: %s", SDL_GetError());
        return 1;
    }
    {
        int status = 0;
        SDL_Event event;
        Uint32 eventNumber = SDL_RegisterEvents(1);
        SDL_Thread *thread = SDL_CreateThread(&button_messagebox, "MessageBox", (void *)(uintptr_t)eventNumber);

        while (SDL_WaitEvent(&event)) {
            if (event.type == eventNumber) {
                break;
            }
        }

        SDL_WaitThread(thread, &status);

        SDL_Log("Message box thread return %i", status);
    }

    /* Test showing a message box with a parent window */
    {
        SDL_Event event;
        SDL_Window *window = SDL_CreateWindow("Test", 640, 480, 0);

        /* On wayland, no window will actually show until something has
           actually been displayed.
        */
        SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
        SDL_RenderPresent(renderer);

        success = SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                           "Simple MessageBox",
                                           "This is a simple error MessageBox with a parent window. Press a key or close the window after dismissing this messagebox.",
                                           window);
        if (!success) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error Presenting MessageBox: %s", SDL_GetError());
            quit(1);
        }

        while (SDL_WaitEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_KEY_UP ||
                event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                break;
            }
        }
    }

    SDL_Quit();
    SDLTest_CommonDestroyState(state);
    return 0;
}
