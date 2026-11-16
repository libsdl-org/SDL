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

#include "../../SDL_internal.h"

#ifdef SDL_JOYSTICK_DSU

/* DSU Joystick Driver - SDL Driver Interface Implementation */

#include "SDL_joystick.h"
#include "SDL_endian.h"
#include "SDL_timer.h"
#include "SDL_hints.h"
#include "SDL_thread.h"
#include "SDL_atomic.h"
#include "SDL_mutex.h"
#include "SDL_events.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../../thread/SDL_systhread.h"
#include "SDL_dsujoystick_c.h"

/* Platform-specific socket includes for rumble support */
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

/* Global DSU context (defined in SDL_dsujoystick.c) */
DSU_Context *g_dsu_context = NULL;

/* Forward declarations */
extern int DSU_ReceiverThread(void *data);
extern void DSU_RequestControllerInfo(DSU_Context *ctx, Uint8 slot);
extern void DSU_RequestControllerData(DSU_Context *ctx, Uint8 slot);

/* Driver functions */
static int DSU_JoystickInit(void)
{
    const char *enabled;
    const char *server;
    const char *server_port;
    const char *client_port;
    
    /* Check if DSU is enabled */
    enabled = SDL_GetHint(SDL_HINT_JOYSTICK_DSU);
    if (enabled && SDL_atoi(enabled) == 0) {
        return 0;  /* DSU disabled */
    }
    
    /* Allocate context */
    g_dsu_context = (DSU_Context *)SDL_calloc(1, sizeof(DSU_Context));
    if (!g_dsu_context) {
        return SDL_OutOfMemory();
    }
    
    /* Get configuration from hints with fallbacks */
    server = SDL_GetHint(SDL_HINT_DSU_SERVER);
    if (!server || !*server) {
        server = DSU_SERVER_ADDRESS_DEFAULT;
    }
    SDL_strlcpy(g_dsu_context->server_address, server, 
                sizeof(g_dsu_context->server_address));
    
    server_port = SDL_GetHint(SDL_HINT_DSU_SERVER_PORT);
    if (server_port && *server_port) {
        g_dsu_context->server_port = SDL_atoi(server_port);
    } else {
        g_dsu_context->server_port = DSU_SERVER_PORT_DEFAULT;
    }
    
    client_port = SDL_GetHint(SDL_HINT_DSU_CLIENT_PORT);
    if (client_port && *client_port) {
        g_dsu_context->client_port = SDL_atoi(client_port);
    } else {
        g_dsu_context->client_port = DSU_CLIENT_PORT_DEFAULT;
    }
    
    g_dsu_context->client_id = SDL_GetTicks();
    
    /* Initialize sockets */
    if (DSU_InitSockets() != 0) {
        SDL_free(g_dsu_context);
        g_dsu_context = NULL;
        return -1;
    }
    
    /* Create UDP socket */
    g_dsu_context->socket = DSU_CreateSocket(g_dsu_context->client_port);
    if (g_dsu_context->socket == -1) {
        DSU_QuitSockets();
        SDL_free(g_dsu_context);
        g_dsu_context = NULL;
        return -1;
    }
    
    /* Create mutex */
    g_dsu_context->slots_mutex = SDL_CreateMutex();
    if (!g_dsu_context->slots_mutex) {
        DSU_CloseSocket(g_dsu_context->socket);
        DSU_QuitSockets();
        SDL_free(g_dsu_context);
        g_dsu_context = NULL;
        return SDL_OutOfMemory();
    }
    
    /* Start receiver thread */
    SDL_AtomicSet(&g_dsu_context->running, 1);
    g_dsu_context->receiver_thread = SDL_CreateThreadInternal(
        DSU_ReceiverThread, "DSU_Receiver", 0, g_dsu_context);
    if (!g_dsu_context->receiver_thread) {
        SDL_DestroyMutex(g_dsu_context->slots_mutex);
        DSU_CloseSocket(g_dsu_context->socket);
        DSU_QuitSockets();
        SDL_free(g_dsu_context);
        g_dsu_context = NULL;
        return SDL_SetError("Failed to create DSU receiver thread");
    }
    
    /* Request controller info from all slots */
    DSU_RequestControllerInfo(g_dsu_context, 0xFF);
    
    return 0;
}

static int DSU_JoystickGetCount(void)
{
    int count = 0;
    int i;
    
    if (!g_dsu_context) {
        return 0;
    }
    
    SDL_LockMutex(g_dsu_context->slots_mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (g_dsu_context->slots[i].connected) {
            count++;
        }
    }
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
    
    return count;
}

static void DSU_JoystickDetect(void)
{
    Uint64 now;
    int i;
    
    if (!g_dsu_context) {
        return;
    }
    
    /* Periodically request controller info and re-subscribe to data */
    now = SDL_GetTicks64();
    if (now - g_dsu_context->last_request_time >= 500) {  /* Request more frequently */
        DSU_RequestControllerInfo(g_dsu_context, 0xFF);
        
        /* Re-subscribe to data for connected controllers */
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if (g_dsu_context->slots[i].connected) {
                DSU_RequestControllerData(g_dsu_context, i);
            }
        }
        
        g_dsu_context->last_request_time = now;
    }
    
    /* Check for timeouts */
    SDL_LockMutex(g_dsu_context->slots_mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (g_dsu_context->slots[i].connected &&
            now - g_dsu_context->slots[i].last_packet_time > 5000) {  /* Increased timeout */
            /* Controller timed out */
            g_dsu_context->slots[i].connected = SDL_FALSE;
            SDL_PrivateJoystickRemoved(g_dsu_context->slots[i].instance_id);
            g_dsu_context->slots[i].instance_id = 0;
        }
    }
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
}

static const char *DSU_JoystickGetDeviceName(int device_index)
{
    int i, count = 0;
    
    if (!g_dsu_context) {
        return NULL;
    }
    
    SDL_LockMutex(g_dsu_context->slots_mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (g_dsu_context->slots[i].connected) {
            if (count == device_index) {
                SDL_UnlockMutex(g_dsu_context->slots_mutex);
                return g_dsu_context->slots[i].name;
            }
            count++;
        }
    }
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
    
    return NULL;
}

static const char *DSU_JoystickGetDevicePath(int device_index)
{
    return NULL;  /* No path for network devices */
}

static int DSU_JoystickGetDevicePlayerIndex(int device_index)
{
    int i, count = 0;
    
    if (!g_dsu_context) {
        return -1;
    }
    
    SDL_LockMutex(g_dsu_context->slots_mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (g_dsu_context->slots[i].connected) {
            if (count == device_index) {
                SDL_UnlockMutex(g_dsu_context->slots_mutex);
                return i;  /* Return slot ID as player index */
            }
            count++;
        }
    }
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
    
    return -1;
}

static void DSU_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
    /* DSU controllers have fixed slots, can't change */
}

static SDL_JoystickGUID DSU_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    int i, count = 0;
    
    SDL_zero(guid);
    
    if (!g_dsu_context) {
        return guid;
    }
    
    SDL_LockMutex(g_dsu_context->slots_mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (g_dsu_context->slots[i].connected) {
            if (count == device_index) {
                guid = g_dsu_context->slots[i].guid;
                SDL_UnlockMutex(g_dsu_context->slots_mutex);
                return guid;
            }
            count++;
        }
    }
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
    
    return guid;
}

static SDL_JoystickID DSU_JoystickGetDeviceInstanceID(int device_index)
{
    int i, count = 0;
    
    if (!g_dsu_context) {
        return -1;
    }
    
    SDL_LockMutex(g_dsu_context->slots_mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (g_dsu_context->slots[i].connected) {
            if (count == device_index) {
                SDL_JoystickID id = g_dsu_context->slots[i].instance_id;
                SDL_UnlockMutex(g_dsu_context->slots_mutex);
                return id;
            }
            count++;
        }
    }
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
    
    return -1;
}

static int DSU_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    DSU_ControllerSlot *slot = NULL;
    int i, count = 0;
    
    if (!g_dsu_context) {
        return SDL_SetError("DSU not initialized");
    }
    
    /* Find the slot for this device */
    SDL_LockMutex(g_dsu_context->slots_mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (g_dsu_context->slots[i].connected) {
            if (count == device_index) {
                slot = &g_dsu_context->slots[i];
                break;
            }
            count++;
        }
    }
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
    
    if (!slot) {
        return SDL_SetError("DSU device not found");
    }
    
    joystick->instance_id = slot->instance_id;
    joystick->hwdata = (struct joystick_hwdata *)slot;
    joystick->nbuttons = 12;  /* Standard PS4 buttons */
    joystick->naxes = 6;      /* LX, LY, RX, RY, L2, R2 */
    joystick->nhats = 1;      /* D-Pad */
    
    /* Set up touchpad if available */
    if (slot->has_touchpad) {
        joystick->ntouchpads = 1;
        joystick->touchpads = (SDL_JoystickTouchpadInfo *)SDL_calloc(1, sizeof(SDL_JoystickTouchpadInfo));
        if (joystick->touchpads) {
            joystick->touchpads[0].nfingers = 2;  /* DSU supports 2 fingers */
        } else {
            joystick->ntouchpads = 0;  /* Failed to allocate, disable touchpad */
        }
    }
    
    /* Register sensors if available */
    if (slot->has_gyro || (slot->model == DSU_MODEL_FULL_GYRO) || (slot->model == DSU_MODEL_PARTIAL_GYRO)) {
        /* DSU reports gyro at varying rates, but typically 250-1000Hz for DS4/DS5 */
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 250.0f);
        slot->has_gyro = SDL_TRUE;
    }
    if (slot->has_accel || (slot->model == DSU_MODEL_FULL_GYRO)) {
        /* DSU reports accelerometer at same rate as gyro */
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 250.0f);
        slot->has_accel = SDL_TRUE;
    }
    
    return 0;
}

static int DSU_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;
    DSU_RumblePacket packet;
    struct sockaddr_in server;
    
    if (!g_dsu_context || !slot || !slot->connected) {
        return SDL_SetError("DSU controller not available");
    }
    
    /* Build rumble packet */
    SDL_memset(&packet, 0, sizeof(packet));
    SDL_memcpy(packet.header.magic, DSU_MAGIC_CLIENT, 4);
    packet.header.version = SDL_SwapLE16(DSU_PROTOCOL_VERSION);
    packet.header.length = SDL_SwapLE16((Uint16)(sizeof(packet) - sizeof(DSU_Header)));
    packet.header.client_id = SDL_SwapLE32(g_dsu_context->client_id);
    packet.header.message_type = SDL_SwapLE32(DSU_MSG_RUMBLE);
    
    /* Set rumble values */
    packet.slot = slot->slot_id;
    packet.motor_left = (Uint8)(low_frequency_rumble >> 8);   /* Convert from 16-bit to 8-bit */
    packet.motor_right = (Uint8)(high_frequency_rumble >> 8);
    
    /* Calculate CRC32 */
    packet.header.crc32 = 0;
    packet.header.crc32 = SDL_SwapLE32(DSU_CalculateCRC32((Uint8*)&packet, sizeof(packet)));
    
    /* Send to server */
    SDL_memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(g_dsu_context->server_port);
    server.sin_addr.s_addr = inet_addr(g_dsu_context->server_address);
    
    if (sendto(g_dsu_context->socket, (const char*)&packet, sizeof(packet), 0,
               (struct sockaddr *)&server, sizeof(server)) < 0) {
        return SDL_SetError("Failed to send rumble packet");
    }
    
    return 0;
}

static int DSU_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static Uint32 DSU_JoystickGetCapabilities(SDL_Joystick *joystick)
{
    Uint32 caps = 0;
    
    /* DSU protocol supports rumble through unofficial extensions */
    caps |= SDL_JOYCAP_RUMBLE;
    
    /* Note: SDL doesn't have a capability flag for motion sensors yet,
     * but they're supported through SDL_JoystickGetSensor* APIs */
    
    return caps;
}

static int DSU_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static int DSU_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static int DSU_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;
    
    /* Sensors are always enabled if available */
    return (slot->has_gyro || slot->has_accel) ? 0 : SDL_Unsupported();
}

static void DSU_JoystickUpdate(SDL_Joystick *joystick)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;
    int i;
    
    if (!slot || !slot->connected) {
        return;
    }
    
    SDL_LockMutex(g_dsu_context->slots_mutex);
    
    /* Update buttons */
    for (i = 0; i < 12; i++) {
        SDL_PrivateJoystickButton(joystick, i, (slot->buttons & (1 << i)) ? 1 : 0);
    }
    
    /* Update axes */
    for (i = 0; i < 6; i++) {
        SDL_PrivateJoystickAxis(joystick, i, slot->axes[i]);
    }
    
    /* Update hat */
    SDL_PrivateJoystickHat(joystick, 0, slot->hat);
    
    /* Update touchpad if available */
    if (slot->has_touchpad && joystick->ntouchpads > 0) {
        /* DS4/DS5 touchpad resolution is typically 1920x943 */
        const float TOUCHPAD_WIDTH = 1920.0f;
        const float TOUCHPAD_HEIGHT = 943.0f;
        
        /* First touch point */
        Uint8 touchpad_state = slot->touch1_active ? SDL_PRESSED : SDL_RELEASED;
        float touchpad_x = (float)slot->touch1_x / TOUCHPAD_WIDTH;
        float touchpad_y = (float)slot->touch1_y / TOUCHPAD_HEIGHT;
        
        /* Clamp to valid range */
        if (touchpad_x < 0.0f) touchpad_x = 0.0f;
        if (touchpad_x > 1.0f) touchpad_x = 1.0f;
        if (touchpad_y < 0.0f) touchpad_y = 0.0f;
        if (touchpad_y > 1.0f) touchpad_y = 1.0f;
        
        SDL_PrivateJoystickTouchpad(joystick, 0, 0, touchpad_state,
                                    touchpad_x, touchpad_y,
                                    touchpad_state ? 1.0f : 0.0f);
        
        /* Second touch point */
        touchpad_state = slot->touch2_active ? SDL_PRESSED : SDL_RELEASED;
        touchpad_x = (float)slot->touch2_x / TOUCHPAD_WIDTH;
        touchpad_y = (float)slot->touch2_y / TOUCHPAD_HEIGHT;
        
        /* Clamp to valid range */
        if (touchpad_x < 0.0f) touchpad_x = 0.0f;
        if (touchpad_x > 1.0f) touchpad_x = 1.0f;
        if (touchpad_y < 0.0f) touchpad_y = 0.0f;
        if (touchpad_y > 1.0f) touchpad_y = 1.0f;
        
        SDL_PrivateJoystickTouchpad(joystick, 0, 1, touchpad_state,
                                    touchpad_x, touchpad_y,
                                    touchpad_state ? 1.0f : 0.0f);
    }
    
    /* Update battery level */
    switch (slot->battery) {
    case DSU_BATTERY_DYING:
    case DSU_BATTERY_LOW:
        SDL_PrivateJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_LOW);
        break;
    case DSU_BATTERY_MEDIUM:
        SDL_PrivateJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_MEDIUM);
        break;
    case DSU_BATTERY_HIGH:
    case DSU_BATTERY_FULL:
        SDL_PrivateJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_FULL);
        break;
    case DSU_BATTERY_CHARGING:
    case DSU_BATTERY_CHARGED:
        SDL_PrivateJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_WIRED);
        break;
    default:
        SDL_PrivateJoystickBatteryLevel(joystick, SDL_JOYSTICK_POWER_UNKNOWN);
        break;
    }
    
    /* Update sensors if available */
    if (slot->has_gyro) {
        SDL_PrivateJoystickSensor(joystick, SDL_SENSOR_GYRO, 
                                  slot->motion_timestamp, slot->gyro, 3);
    }
    if (slot->has_accel) {
        SDL_PrivateJoystickSensor(joystick, SDL_SENSOR_ACCEL,
                                  slot->motion_timestamp, slot->accel, 3);
    }
    
    SDL_UnlockMutex(g_dsu_context->slots_mutex);
}

static void DSU_JoystickClose(SDL_Joystick *joystick)
{
    /* Free touchpad info if allocated */
    if (joystick->touchpads) {
        SDL_free(joystick->touchpads);
        joystick->touchpads = NULL;
        joystick->ntouchpads = 0;
    }
    
    joystick->hwdata = NULL;
}

static void DSU_JoystickQuit(void)
{
    if (!g_dsu_context) {
        return;
    }
    
    /* Stop receiver thread */
    SDL_AtomicSet(&g_dsu_context->running, 0);
    if (g_dsu_context->receiver_thread) {
        SDL_WaitThread(g_dsu_context->receiver_thread, NULL);
    }
    
    /* Close socket */
    DSU_CloseSocket(g_dsu_context->socket);
    DSU_QuitSockets();
    
    /* Clean up mutex */
    if (g_dsu_context->slots_mutex) {
        SDL_DestroyMutex(g_dsu_context->slots_mutex);
    }
    
    /* Free context */
    SDL_free(g_dsu_context);
    g_dsu_context = NULL;
}

static SDL_bool DSU_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    /* DSU controllers map well to standard gamepad layout */
    return SDL_FALSE;  /* Use default mapping */
}

/* Export the driver */
SDL_JoystickDriver SDL_DSU_JoystickDriver = {
    DSU_JoystickInit,
    DSU_JoystickGetCount,
    DSU_JoystickDetect,
    DSU_JoystickGetDeviceName,
    DSU_JoystickGetDevicePath,
    NULL, /* GetDeviceSteamVirtualGamepadSlot */
    DSU_JoystickGetDevicePlayerIndex,
    DSU_JoystickSetDevicePlayerIndex,
    DSU_JoystickGetDeviceGUID,
    DSU_JoystickGetDeviceInstanceID,
    DSU_JoystickOpen,
    DSU_JoystickRumble,
    DSU_JoystickRumbleTriggers,
    DSU_JoystickGetCapabilities,
    DSU_JoystickSetLED,
    DSU_JoystickSendEffect,
    DSU_JoystickSetSensorsEnabled,
    DSU_JoystickUpdate,
    DSU_JoystickClose,
    DSU_JoystickQuit,
    DSU_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_DSU */

/* vi: set ts=4 sw=4 expandtab: */
