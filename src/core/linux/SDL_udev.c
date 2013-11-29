/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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

/* 
 * To list the properties of a device, try something like:
 * udevadm info -a -n snd/hwC0D0 (for a sound card)
 * udevadm info --query=all -n input/event3 (for a keyboard, mouse, etc)
 * udevadm info --query=property -n input/event2
 */

#include "SDL_udev.h"

#ifdef SDL_USE_LIBUDEV

static char* SDL_UDEV_LIBS[] = { "libudev.so.1", "libudev.so.0" };

#define _THIS SDL_UDEV_PrivateData *_this
static _THIS = NULL;

#include "SDL.h"

static SDL_bool SDL_UDEV_load_sym(const char *fn, void **addr);
static int SDL_UDEV_load_syms(void);
static SDL_bool SDL_UDEV_hotplug_update_available(void);
static void device_event(SDL_UDEV_deviceevent type, struct udev_device *dev);

static SDL_bool
SDL_UDEV_load_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(_this->udev_handle, fn);
    if (*addr == NULL) {
        /* Don't call SDL_SetError(): SDL_LoadFunction already did. */
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

static int
SDL_UDEV_load_syms(void)
{
    /* cast funcs to char* first, to please GCC's strict aliasing rules. */
    #define SDL_UDEV_SYM(x) \
        if (!SDL_UDEV_load_sym(#x, (void **) (char *) & _this->x)) return -1

    SDL_UDEV_SYM(udev_device_get_action);
    SDL_UDEV_SYM(udev_device_get_devnode);
    SDL_UDEV_SYM(udev_device_get_subsystem);
    SDL_UDEV_SYM(udev_device_get_property_value);
    SDL_UDEV_SYM(udev_device_new_from_syspath);
    SDL_UDEV_SYM(udev_device_unref);
    SDL_UDEV_SYM(udev_enumerate_add_match_property);
    SDL_UDEV_SYM(udev_enumerate_add_match_subsystem);
    SDL_UDEV_SYM(udev_enumerate_get_list_entry);
    SDL_UDEV_SYM(udev_enumerate_new);
    SDL_UDEV_SYM(udev_enumerate_scan_devices);
    SDL_UDEV_SYM(udev_enumerate_unref);
    SDL_UDEV_SYM(udev_list_entry_get_name);
    SDL_UDEV_SYM(udev_list_entry_get_next);
    SDL_UDEV_SYM(udev_monitor_enable_receiving);
    SDL_UDEV_SYM(udev_monitor_filter_add_match_subsystem_devtype);
    SDL_UDEV_SYM(udev_monitor_get_fd);
    SDL_UDEV_SYM(udev_monitor_new_from_netlink);
    SDL_UDEV_SYM(udev_monitor_receive_device);
    SDL_UDEV_SYM(udev_monitor_unref);
    SDL_UDEV_SYM(udev_new);
    SDL_UDEV_SYM(udev_unref);
    SDL_UDEV_SYM(udev_device_new_from_devnum);
    SDL_UDEV_SYM(udev_device_get_devnum);
    #undef SDL_UDEV_SYM

    return 0;
}

static SDL_bool
SDL_UDEV_hotplug_update_available(void)
{
    if (_this->udev_mon != NULL) {
        const int fd = _this->udev_monitor_get_fd(_this->udev_mon);
        fd_set fds;
        struct timeval tv;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        if ((select(fd+1, &fds, NULL, NULL, &tv) > 0) && (FD_ISSET(fd, &fds))) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}


int
SDL_UDEV_Init(void)
{
    int retval = 0;
    
    if (_this == NULL) {
        _this = (SDL_UDEV_PrivateData *) SDL_calloc(1, sizeof(*_this));
        if(_this == NULL) {
            return SDL_OutOfMemory();
        }
        
        retval = SDL_UDEV_LoadLibrary();
        if (retval < 0) {
            SDL_UDEV_Quit();
            return retval;
        }
        
        /* Set up udev monitoring 
         * Listen for input devices (mouse, keyboard, joystick, etc) and sound devices
         */
        
        _this->udev = _this->udev_new();
        if (_this->udev == NULL) {
            SDL_UDEV_Quit();
            return SDL_SetError("udev_new() failed");
        }

        _this->udev_mon = _this->udev_monitor_new_from_netlink(_this->udev, "udev");
        if (_this->udev_mon == NULL) {
            SDL_UDEV_Quit();
            return SDL_SetError("udev_monitor_new_from_netlink() failed");
        }
        
        _this->udev_monitor_filter_add_match_subsystem_devtype(_this->udev_mon, "input", NULL);
        _this->udev_monitor_filter_add_match_subsystem_devtype(_this->udev_mon, "sound", NULL);
        _this->udev_monitor_enable_receiving(_this->udev_mon);
        
        /* Do an initial scan of existing devices */
        SDL_UDEV_Scan();

    }
    
    _this->ref_count += 1;
    
    return retval;
}

void
SDL_UDEV_Quit(void)
{
    SDL_UDEV_CallbackList *item;
    
    if (_this == NULL) {
        return;
    }
    
    _this->ref_count -= 1;
    
    if (_this->ref_count < 1) {
        
        if (_this->udev_mon != NULL) {
            _this->udev_monitor_unref(_this->udev_mon);
            _this->udev_mon = NULL;
        }
        if (_this->udev != NULL) {
            _this->udev_unref(_this->udev);
            _this->udev = NULL;
        }
        
        /* Remove existing devices */
        while (_this->first != NULL) {
            item = _this->first;
            _this->first = _this->first->next;
            SDL_free(item);
        }
        
        SDL_UDEV_UnloadLibrary();
        SDL_free(_this);
        _this = NULL;
    }
}

void
SDL_UDEV_Scan(void)
{
    struct udev_enumerate *enumerate = NULL;
    struct udev_list_entry *devs = NULL;
    struct udev_list_entry *item = NULL;  
    
    if (_this == NULL) {
        return;
    }
   
    enumerate = _this->udev_enumerate_new(_this->udev);
    if (enumerate == NULL) {
        SDL_UDEV_Quit();
        SDL_SetError("udev_monitor_new_from_netlink() failed");
        return;
    }
    
    _this->udev_enumerate_add_match_subsystem(enumerate, "input");
    _this->udev_enumerate_add_match_subsystem(enumerate, "sound");
    
    _this->udev_enumerate_scan_devices(enumerate);
    devs = _this->udev_enumerate_get_list_entry(enumerate);
    for (item = devs; item; item = _this->udev_list_entry_get_next(item)) {
        const char *path = _this->udev_list_entry_get_name(item);
        struct udev_device *dev = _this->udev_device_new_from_syspath(_this->udev, path);
        if (dev != NULL) {
            device_event(SDL_UDEV_DEVICEADDED, dev);
            _this->udev_device_unref(dev);
        }
    }

    _this->udev_enumerate_unref(enumerate);
}


void
SDL_UDEV_UnloadLibrary(void)
{
    if (_this == NULL) {
        return;
    }
    
    if (_this->udev_handle != NULL) {
        SDL_UnloadObject(_this->udev_handle);
        _this->udev_handle = NULL;
    }
}

int
SDL_UDEV_LoadLibrary(void)
{
    int retval = 0, i;
    
    if (_this == NULL) {
        return SDL_SetError("UDEV not initialized");
    }
    
   
    if (_this->udev_handle == NULL) {
        for( i = 0 ; i < SDL_arraysize(SDL_UDEV_LIBS); i++) {
            _this->udev_handle = SDL_LoadObject(SDL_UDEV_LIBS[i]);
            if (_this->udev_handle != NULL) {
                retval = SDL_UDEV_load_syms();
                if (retval < 0) {
                    SDL_UDEV_UnloadLibrary();
                }
                else {
                    break;
                }
            }
        }
        
        if (_this->udev_handle == NULL) {
            retval = -1;
            /* Don't call SDL_SetError(): SDL_LoadObject already did. */
        }
    }

    return retval;
}

static void 
device_event(SDL_UDEV_deviceevent type, struct udev_device *dev) 
{
    const char *subsystem;
    const char *val = NULL;
    int devclass = 0;
    const char *path;
    SDL_UDEV_CallbackList *item;
    
    path = _this->udev_device_get_devnode(dev);
    if (path == NULL) {
        return;
    }
    
    subsystem = _this->udev_device_get_subsystem(dev);
    if (SDL_strcmp(subsystem, "sound") == 0) {
        devclass = SDL_UDEV_DEVICE_SOUND;
    } else if (SDL_strcmp(subsystem, "input") == 0) {
        val = _this->udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK");
        if (val != NULL && SDL_strcmp(val, "1") == 0 ) {
            devclass |= SDL_UDEV_DEVICE_JOYSTICK;
        }
        
        val = _this->udev_device_get_property_value(dev, "ID_INPUT_MOUSE");
        if (val != NULL && SDL_strcmp(val, "1") == 0 ) {
            devclass |= SDL_UDEV_DEVICE_MOUSE;
        }

        val = _this->udev_device_get_property_value(dev, "ID_INPUT_KEYBOARD");
        if (val != NULL && SDL_strcmp(val, "1") == 0 ) {
            devclass |= SDL_UDEV_DEVICE_KEYBOARD;
        }

        if (devclass == 0) {
            return;
        }
    } else {
        return;
    }
    
    /* Process callbacks */
    for (item = _this->first; item != NULL; item = item->next) {
        item->callback(type, devclass, path);
    }
}

void 
SDL_UDEV_Poll(void)
{
    struct udev_device *dev = NULL;
    const char *action = NULL;

    if (_this == NULL) {
        return;
    }

    while (SDL_UDEV_hotplug_update_available()) {
        dev = _this->udev_monitor_receive_device(_this->udev_mon);
        if (dev == NULL) {
            break;
        }
        action = _this->udev_device_get_action(dev);

        if (SDL_strcmp(action, "add") == 0) {
            device_event(SDL_UDEV_DEVICEADDED, dev);
        } else if (SDL_strcmp(action, "remove") == 0) {
            device_event(SDL_UDEV_DEVICEREMOVED, dev);
        }
        
        _this->udev_device_unref(dev);
    }
}

int 
SDL_UDEV_AddCallback(SDL_UDEV_Callback cb)
{
    SDL_UDEV_CallbackList *item;
    item = (SDL_UDEV_CallbackList *) SDL_calloc(1, sizeof (SDL_UDEV_CallbackList));
    if (item == NULL) {
        return SDL_OutOfMemory();
    }
    
    item->callback = cb;

    if (_this->last == NULL) {
        _this->first = _this->last = item;
    } else {
        _this->last->next = item;
        _this->last = item;
    }
    
    return 1;
}

void 
SDL_UDEV_DelCallback(SDL_UDEV_Callback cb)
{
    SDL_UDEV_CallbackList *item;
    SDL_UDEV_CallbackList *prev = NULL;

    for (item = _this->first; item != NULL; item = item->next) {
        /* found it, remove it. */
        if (item->callback == cb) {
            if (prev != NULL) {
                prev->next = item->next;
            } else {
                SDL_assert(_this->first == item);
                _this->first = item->next;
            }
            if (item == _this->last) {
                _this->last = prev;
            }
            SDL_free(item);
            return;
        }
        prev = item;
    }
    
}


#endif /* SDL_USE_LIBUDEV */
