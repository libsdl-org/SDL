/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>             /* For the definition of PATH_MAX */
#include <linux/input.h>
#ifdef SDL_INPUT_LINUXKD
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <linux/vt.h>
#include <linux/tiocl.h> /* for TIOCL_GETSHIFTSTATE */
#endif

#include "SDL.h"
#include "SDL_assert.h"
#include "SDL_endian.h"
#include "../../core/linux/SDL_udev.h"
#include "SDL_scancode.h"
#include "../../events/SDL_events_c.h"
#include "../../events/scancodes_linux.h" /* adds linux_scancode_table */

/* This isn't defined in older Linux kernel headers */
#ifndef SYN_DROPPED
#define SYN_DROPPED 3
#endif

typedef struct SDL_evdevlist_item
{
    char *path;
    int fd;
    
    /* TODO: use this for every device, not just touchscreen */
    int out_of_sync;
    
    /* TODO: expand on this to have data for every possible class (mouse,
       keyboard, touchpad, etc.). Also there's probably some things in here we
       can pull out to the SDL_evdevlist_item i.e. name */
    int is_touchscreen;
    struct {
        char* name;
        
        int min_x, max_x, range_x;
        int min_y, max_y, range_y;
        
        int max_slots;
        int current_slot;
        struct {
            enum {
                EVDEV_TOUCH_SLOTDELTA_NONE = 0,
                EVDEV_TOUCH_SLOTDELTA_DOWN,
                EVDEV_TOUCH_SLOTDELTA_UP,
                EVDEV_TOUCH_SLOTDELTA_MOVE
            } delta;
            int tracking_id;
            int x, y;
        } * slots;
    } * touchscreen_data;
    
    struct SDL_evdevlist_item *next;
} SDL_evdevlist_item;

typedef struct SDL_EVDEV_PrivateData
{
    SDL_evdevlist_item *first;
    SDL_evdevlist_item *last;
    int num_devices;
    int ref_count;
    int console_fd;
    int kb_mode;
} SDL_EVDEV_PrivateData;

#define _THIS SDL_EVDEV_PrivateData *_this
static _THIS = NULL;

static SDL_Scancode SDL_EVDEV_translate_keycode(int keycode);
static void SDL_EVDEV_sync_device(SDL_evdevlist_item *item);
static int SDL_EVDEV_device_removed(const char *dev_path);

#if SDL_USE_LIBUDEV
static int SDL_EVDEV_device_added(const char *dev_path, int udev_class);
void SDL_EVDEV_udev_callback(SDL_UDEV_deviceevent udev_type, int udev_class,
    const char *dev_path);
#endif /* SDL_USE_LIBUDEV */

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

static const char* EVDEV_consoles[] = {
    /* "/proc/self/fd/0",
    "/dev/tty",
    "/dev/tty0", */ /* the tty ioctl's prohibit these */
    "/dev/tty1",
    "/dev/tty2",
    "/dev/tty3",
    "/dev/tty4",
    "/dev/tty5",
    "/dev/tty6",
    "/dev/tty7", /* usually X is spawned in tty7 */
    "/dev/vc/0",
    "/dev/console"
};

static int SDL_EVDEV_is_console(int fd) {
    int type;
    
    return isatty(fd) && ioctl(fd, KDGKBTYPE, &type) == 0 &&
        (type == KB_101 || type == KB_84);
}

/* Prevent keystrokes from reaching the tty */
static int SDL_EVDEV_mute_keyboard(int tty_fd, int* old_kb_mode)
{
    if (!SDL_EVDEV_is_console(tty_fd)) {
        return SDL_SetError("Tried to mute an invalid tty");
    }
    
    if (ioctl(tty_fd, KDGKBMODE, old_kb_mode) < 0) {
        return SDL_SetError("Failed to get keyboard mode during muting");
    }
    
    /* FIXME: atm this absolutely ruins the vt, and KDSKBMUTE isn't implemented
       in the kernel */
    /*
    if (ioctl(tty_fd, KDSKBMODE, K_OFF) < 0) {
        return SDL_SetError("Failed to set keyboard mode during muting");
    }
    */
    
    return 0;  
}

/* Restore the keyboard mode for given tty */
static void SDL_EVDEV_unmute_keyboard(int tty_fd, int kb_mode)
{
    /* read above */
    /*
    if (ioctl(tty_fd, KDSKBMODE, kb_mode) < 0) {
        SDL_Log("Failed to unmute keyboard");
    }
    */
}

static int SDL_EVDEV_get_active_tty()
{  
    int i, fd, ret, tty = 0;
    char tiocl;
    struct vt_stat vt_state;
    char path[PATH_MAX + 1];
    
    for(i = 0; i < SDL_arraysize(EVDEV_consoles); i++) {
        fd = open(EVDEV_consoles[i], O_RDONLY);
        
        if (fd < 0 && !SDL_EVDEV_is_console(fd))
            break;
        
        tiocl = TIOCL_GETFGCONSOLE;
        if ((ret = ioctl(fd, TIOCLINUX, &tiocl)) >= 0)
            tty = ret + 1;
        else if (ioctl(fd, VT_GETSTATE, &vt_state) == 0)
            tty = vt_state.v_active;
        
        close(fd);
        
        if (tty) {
            sprintf(path, "/dev/tty%u", tty);
            fd = open(path, O_RDONLY);
            if (fd >= 0 && SDL_EVDEV_is_console(fd))
                return fd;
        }
    }
    
    return SDL_SetError("Failed to determine active tty");
}

int
SDL_EVDEV_Init(void)
{
    if (_this == NULL) {
        _this = (SDL_EVDEV_PrivateData*)SDL_calloc(1, sizeof(*_this));
        if (_this == NULL) {
            return SDL_OutOfMemory();
        }

#if SDL_USE_LIBUDEV
        if (SDL_UDEV_Init() < 0) {
            SDL_free(_this);
            _this = NULL;
            return -1;
        }

        /* Set up the udev callback */
        if (SDL_UDEV_AddCallback(SDL_EVDEV_udev_callback) < 0) {
            SDL_UDEV_Quit();
            SDL_free(_this);
            _this = NULL;
            return -1;
        }
        
        /* Force a scan to build the initial device list */
        SDL_UDEV_Scan();
#else
        /* TODO: Scan the devices manually, like a caveman */
#endif /* SDL_USE_LIBUDEV */
        
        /* We need a physical terminal (not PTS) to be able to translate key
           code to symbols via the kernel tables */
        _this->console_fd = SDL_EVDEV_get_active_tty();
        
        /* Mute the keyboard so keystrokes only generate evdev events and do not
           leak through to the console */
        SDL_EVDEV_mute_keyboard(_this->console_fd, &_this->kb_mode);
    }
    
    _this->ref_count += 1;
    
    return 0;
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
            SDL_EVDEV_unmute_keyboard(_this->console_fd, _this->kb_mode);
            close(_this->console_fd);
        }
        
        /* Remove existing devices */
        while(_this->first != NULL) {
            SDL_EVDEV_device_removed(_this->first->path);
        }
        
        SDL_assert(_this->first == NULL);
        SDL_assert(_this->last == NULL);
        SDL_assert(_this->num_devices == 0);
        
        SDL_free(_this);
        _this = NULL;
    }
}

#if SDL_USE_LIBUDEV
void SDL_EVDEV_udev_callback(SDL_UDEV_deviceevent udev_event, int udev_class,
    const char* dev_path)
{
    if (dev_path == NULL) {
        return;
    }
    
    switch(udev_event) {
    case SDL_UDEV_DEVICEADDED:
        if (!(udev_class & (SDL_UDEV_DEVICE_MOUSE | SDL_UDEV_DEVICE_KEYBOARD |
            SDL_UDEV_DEVICE_TOUCHSCREEN)))
            return;

        SDL_EVDEV_device_added(dev_path, udev_class);
        break;  
    case SDL_UDEV_DEVICEREMOVED:
        SDL_EVDEV_device_removed(dev_path);
        break;
    default:
        break;
    }
}
#endif /* SDL_USE_LIBUDEV */

#ifdef SDL_INPUT_LINUXKD
/* this logic is pulled from kbd_keycode() in drivers/tty/vt/keyboard.c in the
   Linux kernel source */
static void SDL_EVDEV_do_text_input(unsigned short keycode) {
    char shift_state;
    int locks_state;
    struct kbentry kbe;
    unsigned char type;
    char text[2] = { 0 };
    
    if (_this->console_fd < 0)
        return;
    
    shift_state = TIOCL_GETSHIFTSTATE;
    if (ioctl(_this->console_fd, TIOCLINUX, &shift_state) < 0) {
        /* TODO: error */
        return;
    }

    kbe.kb_table = shift_state;
    kbe.kb_index = keycode;
    
    if (ioctl(_this->console_fd, KDGKBENT, &kbe) < 0) {
        /* TODO: error */
        return;
    }
    
    type = KTYP(kbe.kb_value);
    
    if (type < 0xf0) {
        /* 
         * FIXME: keysyms with a type below 0xf0 represent a unicode character
         * which requires special handling due to dead characters, diacritics,
         * etc. For perfect input a proper way to deal with such characters
         * should be implemented.
         *
         * For reference, the only place I was able to find out about this
         * special 0xf0 value was in an unused? couple of patches listed below.
         *
         * http://ftp.tc.edu.tw/pub/docs/Unicode/utf8/linux-2.3.12-keyboard.diff
         * http://ftp.tc.edu.tw/pub/docs/Unicode/utf8/linux-2.3.12-console.diff
         */
        
        return;
    }
    
    type -= 0xf0;
    
    /* if type is KT_LETTER then it can be affected by Caps Lock */
    if (type == KT_LETTER) {
        type = KT_LATIN;
    
        if (ioctl(_this->console_fd, KDGKBLED, &locks_state) < 0) {
            /* TODO: error */
            return;
        }
        
        if (locks_state & K_CAPSLOCK) {
            kbe.kb_table = shift_state ^ (1 << KG_SHIFT);
            
            if (ioctl(_this->console_fd, KDGKBENT, &kbe) < 0) {
                /* TODO: error */
                return;
            }
        }
    }
    
    /* TODO: convert values >= 0x80 from ISO-8859-1? to UTF-8 */
    if (type != KT_LATIN || KVAL(kbe.kb_value) >= 0x80)
        return;
    
    *text = KVAL(kbe.kb_value);
    SDL_SendKeyboardText(text);
}
#endif /* SDL_INPUT_LINUXKD */

void 
SDL_EVDEV_Poll(void)
{
    struct input_event events[32];
    int i, j, len;
    SDL_evdevlist_item *item;
    SDL_Scancode scan_code;
    int mouse_button;
    SDL_Mouse *mouse;
    float norm_x, norm_y;

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
                /* special handling for touchscreen, that should eventually be
                   used for all devices */
                if (item->out_of_sync && item->is_touchscreen &&
                    events[i].type == EV_SYN && events[i].code != SYN_REPORT) {
                    break;
                }
                
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
                        } else if (events[i].value == 1 || events[i].value == 2 /* key repeated */) {
                            SDL_SendKeyboardKey(SDL_PRESSED, scan_code);
#ifdef SDL_INPUT_LINUXKD
                            SDL_EVDEV_do_text_input(events[i].code);
#endif /* SDL_INPUT_LINUXKD */
                        }
                    }
                    break;
                case EV_ABS:
                    switch(events[i].code) {
                    case ABS_MT_SLOT:
                        if (!item->is_touchscreen) /* FIXME: temp hack */
                            break;
                        item->touchscreen_data->current_slot = events[i].value;
                        break;
                    case ABS_MT_TRACKING_ID:
                        if (!item->is_touchscreen) /* FIXME: temp hack */
                            break;
                        if (events[i].value >= 0) {
                            item->touchscreen_data->slots[item->touchscreen_data->current_slot].tracking_id = events[i].value;
                            item->touchscreen_data->slots[item->touchscreen_data->current_slot].delta = EVDEV_TOUCH_SLOTDELTA_DOWN;
                        } else {
                            item->touchscreen_data->slots[item->touchscreen_data->current_slot].delta = EVDEV_TOUCH_SLOTDELTA_UP;
                        }
                        break;
                    case ABS_MT_POSITION_X:
                        if (!item->is_touchscreen) /* FIXME: temp hack */
                            break;
                        item->touchscreen_data->slots[item->touchscreen_data->current_slot].x = events[i].value;
                        if (item->touchscreen_data->slots[item->touchscreen_data->current_slot].delta == EVDEV_TOUCH_SLOTDELTA_NONE) {
                            item->touchscreen_data->slots[item->touchscreen_data->current_slot].delta = EVDEV_TOUCH_SLOTDELTA_MOVE;
                        }
                        break;
                    case ABS_MT_POSITION_Y:
                        if (!item->is_touchscreen) /* FIXME: temp hack */
                            break;
                        item->touchscreen_data->slots[item->touchscreen_data->current_slot].y = events[i].value;
                        if (item->touchscreen_data->slots[item->touchscreen_data->current_slot].delta == EVDEV_TOUCH_SLOTDELTA_NONE) {
                            item->touchscreen_data->slots[item->touchscreen_data->current_slot].delta = EVDEV_TOUCH_SLOTDELTA_MOVE;
                        }
                        break;
                    case ABS_X:
                        if (item->is_touchscreen) /* FIXME: temp hack */
                            break;
                        SDL_SendMouseMotion(mouse->focus, mouse->mouseID, SDL_FALSE, events[i].value, mouse->y);
                        break;
                    case ABS_Y:
                        if (item->is_touchscreen) /* FIXME: temp hack */
                            break;
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
                    case SYN_REPORT:
                        if (!item->is_touchscreen) /* FIXME: temp hack */
                            break;
                            
                        for(j = 0; j < item->touchscreen_data->max_slots; j++) {
                            norm_x = (float)(item->touchscreen_data->slots[j].x - item->touchscreen_data->min_x) /
                                (float)item->touchscreen_data->range_x;
                            norm_y = (float)(item->touchscreen_data->slots[j].y - item->touchscreen_data->min_y) /
                                (float)item->touchscreen_data->range_y;
                        
                            switch(item->touchscreen_data->slots[j].delta) {
                            case EVDEV_TOUCH_SLOTDELTA_DOWN:
                                SDL_SendTouch(item->fd, item->touchscreen_data->slots[j].tracking_id, SDL_TRUE, norm_x, norm_y, 1.0f);
                                item->touchscreen_data->slots[j].delta = EVDEV_TOUCH_SLOTDELTA_NONE;
                                break;
                            case EVDEV_TOUCH_SLOTDELTA_UP:
                                SDL_SendTouch(item->fd, item->touchscreen_data->slots[j].tracking_id, SDL_FALSE, norm_x, norm_y, 1.0f);
                                item->touchscreen_data->slots[j].tracking_id = -1;
                                item->touchscreen_data->slots[j].delta = EVDEV_TOUCH_SLOTDELTA_NONE;
                                break;
                            case EVDEV_TOUCH_SLOTDELTA_MOVE:
                                SDL_SendTouchMotion(item->fd, item->touchscreen_data->slots[j].tracking_id, norm_x, norm_y, 1.0f);
                                item->touchscreen_data->slots[j].delta = EVDEV_TOUCH_SLOTDELTA_NONE;
                                break;
                            default:
                                break;
                            }
                        }
                        
                        if (item->out_of_sync)
                            item->out_of_sync = 0;
                        break;
                    case SYN_DROPPED:
                        if (item->is_touchscreen)
                            item->out_of_sync = 1;
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

    if (keycode < SDL_arraysize(linux_scancode_table))
        scancode = linux_scancode_table[keycode];

    if (scancode == SDL_SCANCODE_UNKNOWN) {
        SDL_Log("The key you just pressed is not recognized by SDL. To help "
            "get this fixed, please report this to the SDL mailing list "
            "<sdl@libsdl.org> EVDEV KeyCode %d\n", keycode);
    }

    return scancode;
}

#ifdef SDL_USE_LIBUDEV
static int
SDL_EVDEV_init_touchscreen(SDL_evdevlist_item* item)
{
    int ret, i;
    char name[64];
    struct input_absinfo abs_info;
    
    if (!item->is_touchscreen)
        return 0;
    
    item->touchscreen_data = SDL_calloc(1, sizeof(*item->touchscreen_data));
    if (item->touchscreen_data == NULL)
        return SDL_OutOfMemory();
    
    ret = ioctl(item->fd, EVIOCGNAME(sizeof(name)), name);
    if (ret < 0) {
        SDL_free(item->touchscreen_data);
        return SDL_SetError("Failed to get evdev touchscreen name");
    }
    
    item->touchscreen_data->name = SDL_strdup(name);
    if (item->touchscreen_data->name == NULL) {
        SDL_free(item->touchscreen_data);
        return SDL_OutOfMemory();
    }
    
    ret = ioctl(item->fd, EVIOCGABS(ABS_MT_POSITION_X), &abs_info);
    if (ret < 0) {
        SDL_free(item->touchscreen_data->name);
        SDL_free(item->touchscreen_data);
        return SDL_SetError("Failed to get evdev touchscreen limits");
    }
    item->touchscreen_data->min_x = abs_info.minimum;
    item->touchscreen_data->max_x = abs_info.maximum;
    item->touchscreen_data->range_x = abs_info.maximum - abs_info.minimum;
        
    ret = ioctl(item->fd, EVIOCGABS(ABS_MT_POSITION_Y), &abs_info);
    if (ret < 0) {
        SDL_free(item->touchscreen_data->name);
        SDL_free(item->touchscreen_data);
        return SDL_SetError("Failed to get evdev touchscreen limits");
    }
    item->touchscreen_data->min_y = abs_info.minimum;
    item->touchscreen_data->max_y = abs_info.maximum;
    item->touchscreen_data->range_y = abs_info.maximum - abs_info.minimum;
    
    ret = ioctl(item->fd, EVIOCGABS(ABS_MT_SLOT), &abs_info);
    if (ret < 0) {
        SDL_free(item->touchscreen_data->name);
        SDL_free(item->touchscreen_data);
        return SDL_SetError("Failed to get evdev touchscreen limits");
    }
    item->touchscreen_data->max_slots = abs_info.maximum + 1;
    
    item->touchscreen_data->slots = SDL_calloc(
        item->touchscreen_data->max_slots,
        sizeof(*item->touchscreen_data->slots));
    if (item->touchscreen_data->slots == NULL) {
        SDL_free(item->touchscreen_data->name);
        SDL_free(item->touchscreen_data);
        return SDL_OutOfMemory();
    }
    
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        item->touchscreen_data->slots[i].tracking_id = -1;
    }
    
    ret = SDL_AddTouch(item->fd, /* I guess our fd is unique enough */
        item->touchscreen_data->name);
    if (ret < 0) {
        SDL_free(item->touchscreen_data->slots);
        SDL_free(item->touchscreen_data->name);
        SDL_free(item->touchscreen_data);
        return ret;
    }
    
    return 0;
}
#endif /* SDL_USE_LIBUDEV */

static void
SDL_EVDEV_destroy_touchscreen(SDL_evdevlist_item* item) {
    if (!item->is_touchscreen)
        return;
    
    SDL_DelTouch(item->fd);
    SDL_free(item->touchscreen_data->slots);
    SDL_free(item->touchscreen_data->name);
    SDL_free(item->touchscreen_data);
}

static void
SDL_EVDEV_sync_device(SDL_evdevlist_item *item) 
{
#ifdef EVIOCGMTSLOTS
    int i, ret;
    struct input_absinfo abs_info;
    /*
     * struct input_mt_request_layout {
     *     __u32 code;
     *     __s32 values[num_slots];
     * };
     *
     * this is the structure we're trying to emulate
     */
    __u32* mt_req_code;
    __s32* mt_req_values;
    size_t mt_req_size;
    
    /* TODO: sync devices other than touchscreen */
    if (!item->is_touchscreen)
        return;
    
    mt_req_size = sizeof(*mt_req_code) +
        sizeof(*mt_req_values) * item->touchscreen_data->max_slots;
    
    mt_req_code = SDL_calloc(1, mt_req_size);
    if (mt_req_code == NULL) {
        return;
    }
    
    mt_req_values = (__s32*)mt_req_code + 1;
    
    *mt_req_code = ABS_MT_TRACKING_ID;
    ret = ioctl(item->fd, EVIOCGMTSLOTS(mt_req_size), mt_req_code);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        /*
         * This doesn't account for the very edge case of the user removing their
         * finger and replacing it on the screen during the time we're out of sync,
         * which'll mean that we're not going from down -> up or up -> down, we're
         * going from down -> down but with a different tracking id, meaning we'd
         * have to tell SDL of the two events, but since we wait till SYN_REPORT in
         * SDL_EVDEV_Poll to tell SDL, the current structure of this code doesn't
         * allow it. Lets just pray to God it doesn't happen.
         */
        if (item->touchscreen_data->slots[i].tracking_id < 0 &&
            mt_req_values[i] >= 0) {
            item->touchscreen_data->slots[i].tracking_id = mt_req_values[i];
            item->touchscreen_data->slots[i].delta = EVDEV_TOUCH_SLOTDELTA_DOWN;
        } else if (item->touchscreen_data->slots[i].tracking_id >= 0 &&
            mt_req_values[i] < 0) {
            item->touchscreen_data->slots[i].tracking_id = -1;
            item->touchscreen_data->slots[i].delta = EVDEV_TOUCH_SLOTDELTA_UP;
        }
    }
    
    *mt_req_code = ABS_MT_POSITION_X;
    ret = ioctl(item->fd, EVIOCGMTSLOTS(mt_req_size), mt_req_code);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        if (item->touchscreen_data->slots[i].tracking_id >= 0 &&
            item->touchscreen_data->slots[i].x != mt_req_values[i]) {
            item->touchscreen_data->slots[i].x = mt_req_values[i];
            if (item->touchscreen_data->slots[i].delta ==
                EVDEV_TOUCH_SLOTDELTA_NONE) {
                item->touchscreen_data->slots[i].delta =
                    EVDEV_TOUCH_SLOTDELTA_MOVE;
            }
        }
    }
    
    *mt_req_code = ABS_MT_POSITION_Y;
    ret = ioctl(item->fd, EVIOCGMTSLOTS(mt_req_size), mt_req_code);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    for(i = 0; i < item->touchscreen_data->max_slots; i++) {
        if (item->touchscreen_data->slots[i].tracking_id >= 0 &&
            item->touchscreen_data->slots[i].y != mt_req_values[i]) {
            item->touchscreen_data->slots[i].y = mt_req_values[i];
            if (item->touchscreen_data->slots[i].delta ==
                EVDEV_TOUCH_SLOTDELTA_NONE) {
                item->touchscreen_data->slots[i].delta =
                    EVDEV_TOUCH_SLOTDELTA_MOVE;
            }
        }
    }
    
    ret = ioctl(item->fd, EVIOCGABS(ABS_MT_SLOT), &abs_info);
    if (ret < 0) {
        SDL_free(mt_req_code);
        return;
    }
    item->touchscreen_data->current_slot = abs_info.value;
    
    SDL_free(mt_req_code);

#endif /* EVIOCGMTSLOTS */
}

#if SDL_USE_LIBUDEV
static int
SDL_EVDEV_device_added(const char *dev_path, int udev_class)
{
    int ret;
    SDL_evdevlist_item *item;

    /* Check to make sure it's not already in list. */
    for (item = _this->first; item != NULL; item = item->next) {
        if (SDL_strcmp(dev_path, item->path) == 0) {
            return -1;  /* already have this one */
        }
    }
    
    item = (SDL_evdevlist_item *) SDL_calloc(1, sizeof (SDL_evdevlist_item));
    if (item == NULL) {
        return SDL_OutOfMemory();
    }

    item->fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (item->fd < 0) {
        SDL_free(item);
        return SDL_SetError("Unable to open %s", dev_path);
    }
    
    item->path = SDL_strdup(dev_path);
    if (item->path == NULL) {
        close(item->fd);
        SDL_free(item);
        return SDL_OutOfMemory();
    }
    
    if (udev_class & SDL_UDEV_DEVICE_TOUCHSCREEN) {
        item->is_touchscreen = 1;
        
        if ((ret = SDL_EVDEV_init_touchscreen(item)) < 0) {
            close(item->fd);
            SDL_free(item);
            return ret;
        }
    }
    
    if (_this->last == NULL) {
        _this->first = _this->last = item;
    } else {
        _this->last->next = item;
        _this->last = item;
    }
    
    SDL_EVDEV_sync_device(item);
    
    return _this->num_devices++;
}
#endif /* SDL_USE_LIBUDEV */

static int
SDL_EVDEV_device_removed(const char *dev_path)
{
    SDL_evdevlist_item *item;
    SDL_evdevlist_item *prev = NULL;

    for (item = _this->first; item != NULL; item = item->next) {
        /* found it, remove it. */
        if (SDL_strcmp(dev_path, item->path) == 0) {
            if (prev != NULL) {
                prev->next = item->next;
            } else {
                SDL_assert(_this->first == item);
                _this->first = item->next;
            }
            if (item == _this->last) {
                _this->last = prev;
            }
            if (item->is_touchscreen) {
                SDL_EVDEV_destroy_touchscreen(item);
            }
            close(item->fd);
            SDL_free(item->path);
            SDL_free(item);
            _this->num_devices--;
            return 0;
        }
        prev = item;
    }

    return -1;
}


#endif /* SDL_INPUT_LINUXEV */

/* vi: set ts=4 sw=4 expandtab: */
