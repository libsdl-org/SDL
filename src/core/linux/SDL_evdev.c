/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_INPUT_LINUXEV

/* This is based on the linux joystick driver */
/* References: https://www.kernel.org/doc/Documentation/input/input.txt 
 *             https://www.kernel.org/doc/Documentation/input/event-codes.txt
 *             /usr/include/linux/input.h
 *             The evtest application is also useful to debug the protocol
 */


#include "SDL_evdev.h"
#define _THIS SDL_EVDEV_PrivateData *_this
static _THIS = NULL;

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>             /* For the definition of PATH_MAX */
#include <linux/input.h>
#ifdef SDL_INPUT_LINUXKD
#include <linux/kd.h>
#include <linux/keyboard.h>
#endif


/* We need this to prevent keystrokes from appear in the console */
#ifndef KDSKBMUTE
#define KDSKBMUTE 0x4B51
#endif
#ifndef KDSKBMODE
#define KDSKBMODE 0x4B45
#endif
#ifndef K_OFF
#define K_OFF 0x04
#endif

#include "SDL.h"
#include "SDL_assert.h"
#include "SDL_endian.h"
#include "../../core/linux/SDL_udev.h"
#include "SDL_scancode.h"
#include "../../events/SDL_events_c.h"

/* This isn't defined in older Linux kernel headers */
#ifndef SYN_DROPPED
#define SYN_DROPPED 3
#endif

static SDL_Scancode SDL_EVDEV_translate_keycode(int keycode);
static void SDL_EVDEV_sync_device(SDL_evdevlist_item *item);
static int SDL_EVDEV_device_removed(const char *devpath);

#if SDL_USE_LIBUDEV
static int SDL_EVDEV_device_added(const char *devpath);
void SDL_EVDEV_udev_callback(SDL_UDEV_deviceevent udev_type, int udev_class, const char *devpath);
#endif /* SDL_USE_LIBUDEV */

static SDL_Scancode EVDEV_Keycodes[] = {
    SDL_SCANCODE_UNKNOWN,       /*  KEY_RESERVED        0 */
    SDL_SCANCODE_ESCAPE,        /*  KEY_ESC         1 */
    SDL_SCANCODE_1,             /*  KEY_1           2 */
    SDL_SCANCODE_2,             /*  KEY_2           3 */
    SDL_SCANCODE_3,             /*  KEY_3           4 */
    SDL_SCANCODE_4,             /*  KEY_4           5 */
    SDL_SCANCODE_5,             /*  KEY_5           6 */
    SDL_SCANCODE_6,             /*  KEY_6           7 */
    SDL_SCANCODE_7,             /*  KEY_7           8 */
    SDL_SCANCODE_8,             /*  KEY_8           9 */
    SDL_SCANCODE_9,             /*  KEY_9           10 */
    SDL_SCANCODE_0,             /*  KEY_0           11 */
    SDL_SCANCODE_MINUS,         /*  KEY_MINUS       12 */
    SDL_SCANCODE_EQUALS,        /*  KEY_EQUAL       13 */
    SDL_SCANCODE_BACKSPACE,     /*  KEY_BACKSPACE       14 */
    SDL_SCANCODE_TAB,           /*  KEY_TAB         15 */
    SDL_SCANCODE_Q,             /*  KEY_Q           16 */
    SDL_SCANCODE_W,             /*  KEY_W           17 */
    SDL_SCANCODE_E,             /*  KEY_E           18 */
    SDL_SCANCODE_R,             /*  KEY_R           19 */
    SDL_SCANCODE_T,             /*  KEY_T           20 */
    SDL_SCANCODE_Y,             /*  KEY_Y           21 */
    SDL_SCANCODE_U,             /*  KEY_U           22 */
    SDL_SCANCODE_I,             /*  KEY_I           23 */
    SDL_SCANCODE_O,             /*  KEY_O           24 */
    SDL_SCANCODE_P,             /*  KEY_P           25 */
    SDL_SCANCODE_LEFTBRACKET,   /*  KEY_LEFTBRACE       26 */
    SDL_SCANCODE_RIGHTBRACKET,  /*  KEY_RIGHTBRACE      27 */
    SDL_SCANCODE_RETURN,        /*  KEY_ENTER       28 */
    SDL_SCANCODE_LCTRL,         /*  KEY_LEFTCTRL        29 */
    SDL_SCANCODE_A,             /*  KEY_A           30 */
    SDL_SCANCODE_S,             /*  KEY_S           31 */
    SDL_SCANCODE_D,             /*  KEY_D           32 */
    SDL_SCANCODE_F,             /*  KEY_F           33 */
    SDL_SCANCODE_G,             /*  KEY_G           34 */
    SDL_SCANCODE_H,             /*  KEY_H           35 */
    SDL_SCANCODE_J,             /*  KEY_J           36 */
    SDL_SCANCODE_K,             /*  KEY_K           37 */
    SDL_SCANCODE_L,             /*  KEY_L           38 */
    SDL_SCANCODE_SEMICOLON,     /*  KEY_SEMICOLON       39 */
    SDL_SCANCODE_APOSTROPHE,    /*  KEY_APOSTROPHE      40 */
    SDL_SCANCODE_GRAVE,         /*  KEY_GRAVE       41 */
    SDL_SCANCODE_LSHIFT,        /*  KEY_LEFTSHIFT       42 */
    SDL_SCANCODE_BACKSLASH,     /*  KEY_BACKSLASH       43 */
    SDL_SCANCODE_Z,             /*  KEY_Z           44 */
    SDL_SCANCODE_X,             /*  KEY_X           45 */
    SDL_SCANCODE_C,             /*  KEY_C           46 */
    SDL_SCANCODE_V,             /*  KEY_V           47 */
    SDL_SCANCODE_B,             /*  KEY_B           48 */
    SDL_SCANCODE_N,             /*  KEY_N           49 */
    SDL_SCANCODE_M,             /*  KEY_M           50 */
    SDL_SCANCODE_COMMA,         /*  KEY_COMMA       51 */
    SDL_SCANCODE_PERIOD,        /*  KEY_DOT         52 */
    SDL_SCANCODE_SLASH,         /*  KEY_SLASH       53 */
    SDL_SCANCODE_RSHIFT,        /*  KEY_RIGHTSHIFT      54 */
    SDL_SCANCODE_KP_MULTIPLY,   /*  KEY_KPASTERISK      55 */
    SDL_SCANCODE_LALT,          /*  KEY_LEFTALT     56 */
    SDL_SCANCODE_SPACE,         /*  KEY_SPACE       57 */
    SDL_SCANCODE_CAPSLOCK,      /*  KEY_CAPSLOCK        58 */
    SDL_SCANCODE_F1,            /*  KEY_F1          59 */
    SDL_SCANCODE_F2,            /*  KEY_F2          60 */
    SDL_SCANCODE_F3,            /*  KEY_F3          61 */
    SDL_SCANCODE_F4,            /*  KEY_F4          62 */
    SDL_SCANCODE_F5,            /*  KEY_F5          63 */
    SDL_SCANCODE_F6,            /*  KEY_F6          64 */
    SDL_SCANCODE_F7,            /*  KEY_F7          65 */
    SDL_SCANCODE_F8,            /*  KEY_F8          66 */
    SDL_SCANCODE_F9,            /*  KEY_F9          67 */
    SDL_SCANCODE_F10,           /*  KEY_F10         68 */
    SDL_SCANCODE_NUMLOCKCLEAR,  /*  KEY_NUMLOCK     69 */
    SDL_SCANCODE_SCROLLLOCK,    /*  KEY_SCROLLLOCK      70 */
    SDL_SCANCODE_KP_7,          /*  KEY_KP7         71 */
    SDL_SCANCODE_KP_8,          /*  KEY_KP8         72 */
    SDL_SCANCODE_KP_9,          /*  KEY_KP9         73 */
    SDL_SCANCODE_KP_MINUS,      /*  KEY_KPMINUS     74 */
    SDL_SCANCODE_KP_4,          /*  KEY_KP4         75 */
    SDL_SCANCODE_KP_5,          /*  KEY_KP5         76 */
    SDL_SCANCODE_KP_6,          /*  KEY_KP6         77 */
    SDL_SCANCODE_KP_PLUS,       /*  KEY_KPPLUS      78 */
    SDL_SCANCODE_KP_1,          /*  KEY_KP1         79 */
    SDL_SCANCODE_KP_2,          /*  KEY_KP2         80 */
    SDL_SCANCODE_KP_3,          /*  KEY_KP3         81 */
    SDL_SCANCODE_KP_0,          /*  KEY_KP0         82 */
    SDL_SCANCODE_KP_PERIOD,     /*  KEY_KPDOT       83 */
    SDL_SCANCODE_UNKNOWN,       /*  84 */
    SDL_SCANCODE_LANG5,         /*  KEY_ZENKAKUHANKAKU  85 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_102ND       86 */
    SDL_SCANCODE_F11,           /*  KEY_F11         87 */
    SDL_SCANCODE_F12,           /*  KEY_F12         88 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_RO          89 */
    SDL_SCANCODE_LANG3,         /*  KEY_KATAKANA        90 */
    SDL_SCANCODE_LANG4,         /*  KEY_HIRAGANA        91 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_HENKAN      92 */
    SDL_SCANCODE_LANG3,         /*  KEY_KATAKANAHIRAGANA    93 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_MUHENKAN        94 */
    SDL_SCANCODE_KP_COMMA,      /*  KEY_KPJPCOMMA       95 */
    SDL_SCANCODE_KP_ENTER,      /*  KEY_KPENTER     96 */
    SDL_SCANCODE_RCTRL,         /*  KEY_RIGHTCTRL       97 */
    SDL_SCANCODE_KP_DIVIDE,     /*  KEY_KPSLASH     98 */
    SDL_SCANCODE_SYSREQ,        /*  KEY_SYSRQ       99 */
    SDL_SCANCODE_RALT,          /*  KEY_RIGHTALT        100 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_LINEFEED        101 */
    SDL_SCANCODE_HOME,          /*  KEY_HOME        102 */
    SDL_SCANCODE_UP,            /*  KEY_UP          103 */
    SDL_SCANCODE_PAGEUP,        /*  KEY_PAGEUP      104 */
    SDL_SCANCODE_LEFT,          /*  KEY_LEFT        105 */
    SDL_SCANCODE_RIGHT,         /*  KEY_RIGHT       106 */
    SDL_SCANCODE_END,           /*  KEY_END         107 */
    SDL_SCANCODE_DOWN,          /*  KEY_DOWN        108 */
    SDL_SCANCODE_PAGEDOWN,      /*  KEY_PAGEDOWN        109 */
    SDL_SCANCODE_INSERT,        /*  KEY_INSERT      110 */
    SDL_SCANCODE_DELETE,        /*  KEY_DELETE      111 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_MACRO       112 */
    SDL_SCANCODE_MUTE,          /*  KEY_MUTE        113 */
    SDL_SCANCODE_VOLUMEDOWN,    /*  KEY_VOLUMEDOWN      114 */
    SDL_SCANCODE_VOLUMEUP,      /*  KEY_VOLUMEUP        115 */
    SDL_SCANCODE_POWER,         /*  KEY_POWER       116 SC System Power Down */
    SDL_SCANCODE_KP_EQUALS,     /*  KEY_KPEQUAL     117 */
    SDL_SCANCODE_KP_MINUS,      /*  KEY_KPPLUSMINUS     118 */
    SDL_SCANCODE_PAUSE,         /*  KEY_PAUSE       119 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SCALE       120 AL Compiz Scale (Expose) */
    SDL_SCANCODE_KP_COMMA,      /*  KEY_KPCOMMA     121 */
    SDL_SCANCODE_LANG1,         /*  KEY_HANGEUL,KEY_HANGUEL 122 */
    SDL_SCANCODE_LANG2,         /*  KEY_HANJA       123 */
    SDL_SCANCODE_INTERNATIONAL3,/*  KEY_YEN         124 */
    SDL_SCANCODE_LGUI,          /*  KEY_LEFTMETA        125 */
    SDL_SCANCODE_RGUI,          /*  KEY_RIGHTMETA       126 */
    SDL_SCANCODE_APPLICATION,   /*  KEY_COMPOSE     127 */
    SDL_SCANCODE_STOP,          /*  KEY_STOP        128 AC Stop */
    SDL_SCANCODE_AGAIN,         /*  KEY_AGAIN       129 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PROPS       130 AC Properties */
    SDL_SCANCODE_UNDO,          /*  KEY_UNDO        131 AC Undo */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_FRONT       132 */
    SDL_SCANCODE_COPY,          /*  KEY_COPY        133 AC Copy */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_OPEN        134 AC Open */
    SDL_SCANCODE_PASTE,         /*  KEY_PASTE       135 AC Paste */
    SDL_SCANCODE_FIND,          /*  KEY_FIND        136 AC Search */
    SDL_SCANCODE_CUT,           /*  KEY_CUT         137 AC Cut */
    SDL_SCANCODE_HELP,          /*  KEY_HELP        138 AL Integrated Help Center */
    SDL_SCANCODE_MENU,          /*  KEY_MENU        139 Menu (show menu) */
    SDL_SCANCODE_CALCULATOR,    /*  KEY_CALC        140 AL Calculator */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SETUP       141 */
    SDL_SCANCODE_SLEEP,         /*  KEY_SLEEP       142 SC System Sleep */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_WAKEUP      143 System Wake Up */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_FILE        144 AL Local Machine Browser */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SENDFILE        145 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_DELETEFILE      146 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_XFER        147 */
    SDL_SCANCODE_APP1,          /*  KEY_PROG1       148 */
    SDL_SCANCODE_APP1,          /*  KEY_PROG2       149 */
    SDL_SCANCODE_WWW,           /*  KEY_WWW         150 AL Internet Browser */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_MSDOS       151 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_COFFEE,KEY_SCREENLOCK      152 AL Terminal Lock/Screensaver */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_DIRECTION       153 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CYCLEWINDOWS    154 */
    SDL_SCANCODE_MAIL,          /*  KEY_MAIL        155 */
    SDL_SCANCODE_AC_BOOKMARKS,  /*  KEY_BOOKMARKS       156 AC Bookmarks */
    SDL_SCANCODE_COMPUTER,      /*  KEY_COMPUTER        157 */
    SDL_SCANCODE_AC_BACK,       /*  KEY_BACK        158 AC Back */
    SDL_SCANCODE_AC_FORWARD,    /*  KEY_FORWARD     159 AC Forward */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CLOSECD     160 */
    SDL_SCANCODE_EJECT,         /*  KEY_EJECTCD     161 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_EJECTCLOSECD    162 */
    SDL_SCANCODE_AUDIONEXT,     /*  KEY_NEXTSONG        163 */
    SDL_SCANCODE_AUDIOPLAY,     /*  KEY_PLAYPAUSE       164 */
    SDL_SCANCODE_AUDIOPREV,     /*  KEY_PREVIOUSSONG    165 */
    SDL_SCANCODE_AUDIOSTOP,     /*  KEY_STOPCD      166 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_RECORD      167 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_REWIND      168 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PHONE       169 Media Select Telephone */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_ISO         170 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CONFIG      171 AL Consumer Control Configuration */
    SDL_SCANCODE_AC_HOME,       /*  KEY_HOMEPAGE        172 AC Home */
    SDL_SCANCODE_AC_REFRESH,    /*  KEY_REFRESH     173 AC Refresh */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_EXIT        174 AC Exit */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_MOVE        175 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_EDIT        176 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SCROLLUP        177 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SCROLLDOWN      178 */
    SDL_SCANCODE_KP_LEFTPAREN,  /*  KEY_KPLEFTPAREN     179 */
    SDL_SCANCODE_KP_RIGHTPAREN, /*  KEY_KPRIGHTPAREN    180 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_NEW         181 AC New */
    SDL_SCANCODE_AGAIN,         /*  KEY_REDO        182 AC Redo/Repeat */
    SDL_SCANCODE_F13,           /*  KEY_F13         183 */
    SDL_SCANCODE_F14,           /*  KEY_F14         184 */
    SDL_SCANCODE_F15,           /*  KEY_F15         185 */
    SDL_SCANCODE_F16,           /*  KEY_F16         186 */
    SDL_SCANCODE_F17,           /*  KEY_F17         187 */
    SDL_SCANCODE_F18,           /*  KEY_F18         188 */
    SDL_SCANCODE_F19,           /*  KEY_F19         189 */
    SDL_SCANCODE_F20,           /*  KEY_F20         190 */
    SDL_SCANCODE_F21,           /*  KEY_F21         191 */
    SDL_SCANCODE_F22,           /*  KEY_F22         192 */
    SDL_SCANCODE_F23,           /*  KEY_F23         193 */
    SDL_SCANCODE_F24,           /*  KEY_F24         194 */
    SDL_SCANCODE_UNKNOWN,       /*  195 */
    SDL_SCANCODE_UNKNOWN,       /*  196 */
    SDL_SCANCODE_UNKNOWN,       /*  197 */
    SDL_SCANCODE_UNKNOWN,       /*  198 */
    SDL_SCANCODE_UNKNOWN,       /*  199 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PLAYCD      200 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PAUSECD     201 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PROG3       202 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PROG4       203 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_DASHBOARD       204 AL Dashboard */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SUSPEND     205 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CLOSE       206 AC Close */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PLAY        207 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_FASTFORWARD     208 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_BASSBOOST       209 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_PRINT       210 AC Print */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_HP          211 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CAMERA      212 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SOUND       213 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_QUESTION        214 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_EMAIL       215 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CHAT        216 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SEARCH      217 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CONNECT     218 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_FINANCE     219 AL Checkbook/Finance */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SPORT       220 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SHOP        221 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_ALTERASE        222 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_CANCEL      223 AC Cancel */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_BRIGHTNESSDOWN  224 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_BRIGHTNESSUP    225 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_MEDIA       226 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SWITCHVIDEOMODE 227 Cycle between available video outputs (Monitor/LCD/TV-out/etc) */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_KBDILLUMTOGGLE  228 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_KBDILLUMDOWN    229 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_KBDILLUMUP      230 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SEND        231 AC Send */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_REPLY       232 AC Reply */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_FORWARDMAIL     233 AC Forward Msg */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_SAVE        234 AC Save */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_DOCUMENTS       235 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_BATTERY     236  */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_BLUETOOTH       237 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_WLAN        238 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_UWB         239 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_UNKNOWN     240 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_VIDEO_NEXT      241 drive next video source */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_VIDEO_PREV      242 drive previous video source */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_BRIGHTNESS_CYCLE    243 brightness up, after max is min */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_BRIGHTNESS_ZERO 244 brightness off, use ambient */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_DISPLAY_OFF     245 display device to off state */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_WIMAX       246 */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_RFKILL      247 Key that controls all radios */
    SDL_SCANCODE_UNKNOWN,       /*  KEY_MICMUTE     248 Mute / unmute the microphone */
};

static Uint8 EVDEV_MouseButtons[] = {
    SDL_BUTTON_LEFT,            /*  BTN_LEFT        0x110 */
    SDL_BUTTON_RIGHT,           /*  BTN_RIGHT       0x111 */
    SDL_BUTTON_MIDDLE,          /*  BTN_MIDDLE      0x112 */
    SDL_BUTTON_X1,              /*  BTN_SIDE        0x113 */
    SDL_BUTTON_X2,              /*  BTN_EXTRA       0x114 */
    SDL_BUTTON_X2 + 1,          /*  BTN_FORWARD     0x115 */
    SDL_BUTTON_X2 + 2,          /*  BTN_BACK        0x116 */
    SDL_BUTTON_X2 + 3           /*  BTN_TASK        0x117 */
};

static char* EVDEV_consoles[] = {
    "/proc/self/fd/0",
    "/dev/tty",
    "/dev/tty0",
    "/dev/tty1",
    "/dev/tty2",
    "/dev/tty3",
    "/dev/tty4",
    "/dev/tty5",
    "/dev/tty6",
    "/dev/vc/0",
    "/dev/console"
};

#define IS_CONSOLE(fd) isatty (fd) && ioctl(fd, KDGKBTYPE, &arg) == 0 && ((arg == KB_101) || (arg == KB_84))

static int SDL_EVDEV_get_console_fd(void)
{
    int fd, i;
    char arg = 0;
    
    /* Try a few consoles to see which one we have read access to */
    
    for( i = 0; i < SDL_arraysize(EVDEV_consoles); i++) {
        fd = open(EVDEV_consoles[i], O_RDONLY);
        if (fd >= 0) {
            if (IS_CONSOLE(fd)) return fd;
            close(fd);
        }
    }
    
    /* Try stdin, stdout, stderr */
    
    for( fd = 0; fd < 3; fd++) {
        if (IS_CONSOLE(fd)) return fd;
    }
    
    /* We won't be able to send SDL_TEXTINPUT events */
    return -1;
}

/* Prevent keystrokes from reaching the tty */
static int SDL_EVDEV_mute_keyboard(int tty, int *kb_mode)
{
    char arg;
    
    *kb_mode = 0; /* FIXME: Is this a sane default in case KDGKBMODE fails? */
    if (!IS_CONSOLE(tty)) {
        return SDL_SetError("Tried to mute an invalid tty");
    }
    ioctl(tty, KDGKBMODE, kb_mode); /* It's not fatal if this fails */
    if (ioctl(tty, KDSKBMUTE, 1) && ioctl(tty, KDSKBMODE, K_OFF)) {
        return SDL_SetError("EVDEV: Failed muting keyboard");
    }
    
    return 0;  
}

/* Restore the keyboard mode for given tty */
static void SDL_EVDEV_unmute_keyboard(int tty, int kb_mode)
{
    if (ioctl(tty, KDSKBMUTE, 0) && ioctl(tty, KDSKBMODE, kb_mode)) {
        SDL_Log("EVDEV: Failed restoring keyboard mode");
    }
}

/* Read /sys/class/tty/tty0/active and open the tty */
static int SDL_EVDEV_get_active_tty()
{
    int fd, len;
    char ttyname[NAME_MAX + 1];
    char ttypath[PATH_MAX+1] = "/dev/";
    char arg;
    
    fd = open("/sys/class/tty/tty0/active", O_RDONLY);
    if (fd < 0) {
        return SDL_SetError("Could not determine which tty is active");
    }
    
    len = read(fd, ttyname, NAME_MAX);
    close(fd);
    
    if (len <= 0) {
        return SDL_SetError("Could not read which tty is active");
    }
    
    if (ttyname[len-1] == '\n') {
        ttyname[len-1] = '\0';
    }
    else {
        ttyname[len] = '\0';
    }
    
    SDL_strlcat(ttypath, ttyname, PATH_MAX);
    fd = open(ttypath, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return SDL_SetError("Could not open tty: %s", ttypath);
    }
    
    if (!IS_CONSOLE(fd)) {
        close(fd);
        return SDL_SetError("Invalid tty obtained: %s", ttypath);
    }

    return fd;  
}

int
SDL_EVDEV_Init(void)
{
    int retval = 0;
    
    if (_this == NULL) {
        
        _this = (SDL_EVDEV_PrivateData *) SDL_calloc(1, sizeof(*_this));
        if(_this == NULL) {
            return SDL_OutOfMemory();
        }

#if SDL_USE_LIBUDEV
        if (SDL_UDEV_Init() < 0) {
            SDL_free(_this);
            _this = NULL;
            return -1;
        }

        /* Set up the udev callback */
        if ( SDL_UDEV_AddCallback(SDL_EVDEV_udev_callback) < 0) {
            SDL_EVDEV_Quit();
            return -1;
        }
        
        /* Force a scan to build the initial device list */
        SDL_UDEV_Scan();
#else
        /* TODO: Scan the devices manually, like a caveman */
#endif /* SDL_USE_LIBUDEV */
        
        /* We need a physical terminal (not PTS) to be able to translate key code to symbols via the kernel tables */
        _this->console_fd = SDL_EVDEV_get_console_fd();
        
        /* Mute the keyboard so keystrokes only generate evdev events and do not leak through to the console */
        _this->tty = STDIN_FILENO;
        if (SDL_EVDEV_mute_keyboard(_this->tty, &_this->kb_mode) < 0) {
            /* stdin is not a tty, probably we were launched remotely, so we try to disable the active tty */
            _this->tty = SDL_EVDEV_get_active_tty();
            if (_this->tty >= 0) {
                if (SDL_EVDEV_mute_keyboard(_this->tty, &_this->kb_mode) < 0) {
                    close(_this->tty);
                    _this->tty = -1;
                }
            }
        }
    }
    
    _this->ref_count += 1;
    
    return retval;
}

void
SDL_EVDEV_Quit(void)
{
    if (_this == NULL) {
        return;
    }
    
    _this->ref_count -= 1;
    
    if (_this->ref_count < 1) {
        
#if SDL_USE_LIBUDEV
        SDL_UDEV_DelCallback(SDL_EVDEV_udev_callback);
        SDL_UDEV_Quit();
#endif /* SDL_USE_LIBUDEV */
       
        if (_this->console_fd >= 0) {
            close(_this->console_fd);
        }
        
        if (_this->tty >= 0) {
            SDL_EVDEV_unmute_keyboard(_this->tty, _this->kb_mode);
            close(_this->tty);
        }
        
        /* Remove existing devices */
        while(_this->first != NULL) {
            SDL_EVDEV_device_removed(_this->first->path);
        }
        
        SDL_assert(_this->first == NULL);
        SDL_assert(_this->last == NULL);
        SDL_assert(_this->numdevices == 0);
        
        SDL_free(_this);
        _this = NULL;
    }
}

#if SDL_USE_LIBUDEV
void SDL_EVDEV_udev_callback(SDL_UDEV_deviceevent udev_type, int udev_class, const char *devpath)
{
    if (devpath == NULL) {
        return;
    }
    
    if (!(udev_class & (SDL_UDEV_DEVICE_MOUSE|SDL_UDEV_DEVICE_KEYBOARD))) {
        return;
    }

    switch( udev_type ) {
    case SDL_UDEV_DEVICEADDED:
        SDL_EVDEV_device_added(devpath);
        break;
            
    case SDL_UDEV_DEVICEREMOVED:
        SDL_EVDEV_device_removed(devpath);
        break;
            
    default:
        break;
    }
}

#endif /* SDL_USE_LIBUDEV */

void 
SDL_EVDEV_Poll(void)
{
    struct input_event events[32];
    int i, len;
    SDL_evdevlist_item *item;
    SDL_Scancode scan_code;
    int mouse_button;
    SDL_Mouse *mouse;
#ifdef SDL_INPUT_LINUXKD
    Uint16 modstate;
    struct kbentry kbe;
    static char keysym[8];
    char *end;
    Uint32 kval;
#endif

    if (!_this) {
        return;
    }

#if SDL_USE_LIBUDEV
    SDL_UDEV_Poll();
#endif

    mouse = SDL_GetMouse();

    for (item = _this->first; item != NULL; item = item->next) {
        while ((len = read(item->fd, events, (sizeof events))) > 0) {
            len /= sizeof(events[0]);
            for (i = 0; i < len; ++i) {
                switch (events[i].type) {
                case EV_KEY:
                    if (events[i].code >= BTN_MOUSE && events[i].code < BTN_MOUSE + SDL_arraysize(EVDEV_MouseButtons)) {
                        mouse_button = events[i].code - BTN_MOUSE;
                        if (events[i].value == 0) {
                            SDL_SendMouseButton(mouse->focus, mouse->mouseID, SDL_RELEASED, EVDEV_MouseButtons[mouse_button]);
                        } else if (events[i].value == 1) {
                            SDL_SendMouseButton(mouse->focus, mouse->mouseID, SDL_PRESSED, EVDEV_MouseButtons[mouse_button]);
                        }
                        break;
                    }

                    /* Probably keyboard */
                    scan_code = SDL_EVDEV_translate_keycode(events[i].code);
                    if (scan_code != SDL_SCANCODE_UNKNOWN) {
                        if (events[i].value == 0) {
                            SDL_SendKeyboardKey(SDL_RELEASED, scan_code);
                        } else if (events[i].value == 1 || events[i].value == 2 /* Key repeated */ ) {
                            SDL_SendKeyboardKey(SDL_PRESSED, scan_code);
#ifdef SDL_INPUT_LINUXKD
                            if (_this->console_fd >= 0) {
                                kbe.kb_index = events[i].code;
                                /* Convert the key to an UTF-8 char */
                                /* Ref: http://www.linuxjournal.com/article/2783 */
                                modstate = SDL_GetModState();
                                kbe.kb_table = 0;
                                
                                /* Ref: http://graphics.stanford.edu/~seander/bithacks.html#ConditionalSetOrClearBitsWithoutBranching */
                                kbe.kb_table |= -( (modstate & KMOD_LCTRL) != 0) & (1 << KG_CTRLL | 1 << KG_CTRL);
                                kbe.kb_table |= -( (modstate & KMOD_RCTRL) != 0) & (1 << KG_CTRLR | 1 << KG_CTRL);
                                kbe.kb_table |= -( (modstate & KMOD_LSHIFT) != 0) & (1 << KG_SHIFTL | 1 << KG_SHIFT);
                                kbe.kb_table |= -( (modstate & KMOD_RSHIFT) != 0) & (1 << KG_SHIFTR | 1 << KG_SHIFT);
                                kbe.kb_table |= -( (modstate & KMOD_LALT) != 0) & (1 << KG_ALT);
                                kbe.kb_table |= -( (modstate & KMOD_RALT) != 0) & (1 << KG_ALTGR);

                                if (ioctl(_this->console_fd, KDGKBENT, (unsigned long)&kbe) == 0 && 
                                    ((KTYP(kbe.kb_value) == KT_LATIN) || (KTYP(kbe.kb_value) == KT_ASCII) || (KTYP(kbe.kb_value) == KT_LETTER))) 
                                {
                                    kval = KVAL(kbe.kb_value);
                                    
                                    /* While there's a KG_CAPSSHIFT symbol, it's not useful to build the table index with it
                                     * because 1 << KG_CAPSSHIFT overflows the 8 bits of kb_table 
                                     * So, we do the CAPS LOCK logic here. Note that isalpha depends on the locale!
                                     */
                                    if ( modstate & KMOD_CAPS && isalpha(kval) ) {
                                        if ( isupper(kval) ) {
                                            kval = tolower(kval);
                                        } else {
                                            kval = toupper(kval);
                                        }
                                    }
                                     
                                    /* Convert to UTF-8 and send */
                                    end = SDL_UCS4ToUTF8( kval, keysym);
                                    *end = '\0';
                                    SDL_SendKeyboardText(keysym);
                                }
                            }
#endif /* SDL_INPUT_LINUXKD */
                        }
                    }
                    break;
                case EV_ABS:
                    switch(events[i].code) {
                    case ABS_X:
                        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, SDL_FALSE, events[i].value, mouse->y);
                        break;
                    case ABS_Y:
                        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, SDL_FALSE, mouse->x, events[i].value);
                        break;
                    default:
                        break;
                    }
                    break;
                case EV_REL:
                    switch(events[i].code) {
                    case REL_X:
                        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, SDL_TRUE, events[i].value, 0);
                        break;
                    case REL_Y:
                        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, SDL_TRUE, 0, events[i].value);
                        break;
                    case REL_WHEEL:
                        SDL_SendMouseWheel(mouse->focus, mouse->mouseID, 0, events[i].value, SDL_MOUSEWHEEL_NORMAL);
                        break;
                    case REL_HWHEEL:
                        SDL_SendMouseWheel(mouse->focus, mouse->mouseID, events[i].value, 0, SDL_MOUSEWHEEL_NORMAL);
                        break;
                    default:
                        break;
                    }
                    break;
                case EV_SYN:
                    switch (events[i].code) {
                    case SYN_DROPPED:
                        SDL_EVDEV_sync_device(item);
                        break;
                    default:
                        break;
                    }
                    break;
                }
            }
        }    
    }
}

static SDL_Scancode
SDL_EVDEV_translate_keycode(int keycode)
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;

    if (keycode < SDL_arraysize(EVDEV_Keycodes)) {
        scancode = EVDEV_Keycodes[keycode];
    }
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        SDL_Log("The key you just pressed is not recognized by SDL. To help get this fixed, please report this to the SDL mailing list <sdl@libsdl.org> EVDEV KeyCode %d \n", keycode);
    }
    return scancode;
}

static void
SDL_EVDEV_sync_device(SDL_evdevlist_item *item) 
{
    /* TODO: get full state of device and report whatever is required */
}

#if SDL_USE_LIBUDEV
static int
SDL_EVDEV_device_added(const char *devpath)
{
    SDL_evdevlist_item *item;

    /* Check to make sure it's not already in list. */
    for (item = _this->first; item != NULL; item = item->next) {
        if (strcmp(devpath, item->path) == 0) {
            return -1;  /* already have this one */
        }
    }
    
    item = (SDL_evdevlist_item *) SDL_calloc(1, sizeof (SDL_evdevlist_item));
    if (item == NULL) {
        return SDL_OutOfMemory();
    }

    item->fd = open(devpath, O_RDONLY, 0);
    if (item->fd < 0) {
        SDL_free(item);
        return SDL_SetError("Unable to open %s", devpath);
    }
    
    item->path = SDL_strdup(devpath);
    if (item->path == NULL) {
        close(item->fd);
        SDL_free(item);
        return SDL_OutOfMemory();
    }
    
    /* Non blocking read mode */
    fcntl(item->fd, F_SETFL, O_NONBLOCK);
    
    if (_this->last == NULL) {
        _this->first = _this->last = item;
    } else {
        _this->last->next = item;
        _this->last = item;
    }
    
    SDL_EVDEV_sync_device(item);
    
    return _this->numdevices++;
}
#endif /* SDL_USE_LIBUDEV */

static int
SDL_EVDEV_device_removed(const char *devpath)
{
    SDL_evdevlist_item *item;
    SDL_evdevlist_item *prev = NULL;

    for (item = _this->first; item != NULL; item = item->next) {
        /* found it, remove it. */
        if ( strcmp(devpath, item->path) ==0 ) {
            if (prev != NULL) {
                prev->next = item->next;
            } else {
                SDL_assert(_this->first == item);
                _this->first = item->next;
            }
            if (item == _this->last) {
                _this->last = prev;
            }
            close(item->fd);
            SDL_free(item->path);
            SDL_free(item);
            _this->numdevices--;
            return 0;
        }
        prev = item;
    }

    return -1;
}


#endif /* SDL_INPUT_LINUXEV */

/* vi: set ts=4 sw=4 expandtab: */

