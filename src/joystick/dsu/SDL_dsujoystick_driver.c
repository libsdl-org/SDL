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

/* Include socket headers before SDL to avoid macro conflicts */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
/* Unix-like systems including Haiku */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "SDL_internal.h"

/* Define this before including our header to get internal definitions */
#define IN_JOYSTICK_DSU_

/* Include protocol definitions and shared DSU header for types */
#include "SDL_dsuprotocol.h"
#include "SDL_dsujoystick_c.h"

#ifdef SDL_JOYSTICK_DSU

/* Forward declare the driver */
extern SDL_JoystickDriver SDL_DSU_JoystickDriver;

/* Then include system joystick headers */
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
/* Ensure timer prototypes are visible for SDL_GetTicks64 */
#include <SDL3/SDL_timer.h>

/* Additional Windows headers already included at the top */
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

/* Global DSU context pointer */
struct DSU_Context_t *s_dsu_ctx = NULL;

/* Forward declarations */
extern int SDLCALL DSU_ReceiverThread(void *data);
extern void DSU_RequestControllerInfo(struct DSU_Context_t *ctx, Uint8 slot);
extern void DSU_RequestControllerData(struct DSU_Context_t *ctx, Uint8 slot);

/* Socket helpers */
extern int DSU_InitSockets(void);
extern void DSU_QuitSockets(void);
extern dsu_socket_t DSU_CreateSocket(Uint16 port);
extern void DSU_CloseSocket(dsu_socket_t socket);
extern Uint32 DSU_CalculateCRC32(const Uint8 *data, size_t length);

/* Driver functions */
static bool DSU_JoystickInit(void)
{
    const char *enabled;
    const char *server;
    const char *server_port;
    const char *client_port;
    struct DSU_Context_t *ctx;

    /* Check if DSU is enabled */
    enabled = SDL_GetHint(SDL_HINT_JOYSTICK_DSU);
    if (enabled && SDL_atoi(enabled) == 0) {
        return true;  /* DSU disabled */
    }

    /* Allocate context */
    ctx = (struct DSU_Context_t *)SDL_calloc(1, sizeof(struct DSU_Context_t));
    if (!ctx) {
        SDL_OutOfMemory();
        return false;
    }

    /* Get configuration from hints with fallbacks */
    server = SDL_GetHint(SDL_HINT_DSU_SERVER);
    if (!server || !*server) {
        server = DSU_SERVER_ADDRESS_DEFAULT;
    }
    SDL_strlcpy(ctx->server_address, server,
                sizeof(ctx->server_address));

    server_port = SDL_GetHint(SDL_HINT_DSU_SERVER_PORT);
    if (server_port && *server_port) {
        ctx->server_port = (Uint16)SDL_atoi(server_port);
    } else {
        ctx->server_port = DSU_SERVER_PORT_DEFAULT;
    }

    client_port = SDL_GetHint(SDL_HINT_DSU_CLIENT_PORT);
    if (client_port && *client_port) {
        ctx->client_port = (Uint16)SDL_atoi(client_port);
    } else {
        ctx->client_port = DSU_CLIENT_PORT_DEFAULT;
    }

    ctx->client_id = (Uint32)SDL_GetTicks();

    /* Initialize sockets */
    if (DSU_InitSockets() != 0) {
        SDL_free(ctx);
        return false;
    }

    /* Create UDP socket */
    ctx->socket = DSU_CreateSocket(ctx->client_port);
    if (ctx->socket == DSU_INVALID_SOCKET) {
        DSU_QuitSockets();
        SDL_free(ctx);
        return false;
    }

    /* Create mutex */
    ctx->slots_mutex = SDL_CreateMutex();
    if (!ctx->slots_mutex) {
        DSU_CloseSocket(ctx->socket);
        DSU_QuitSockets();
        SDL_free(ctx);
        SDL_OutOfMemory();
        return false;
    }

    /* Start receiver thread */
    SDL_SetAtomicInt(&ctx->running, 1);
    ctx->receiver_thread = SDL_CreateThread(
        DSU_ReceiverThread, "DSU_Receiver", ctx);
    if (!ctx->receiver_thread) {
        SDL_DestroyMutex(ctx->slots_mutex);
        DSU_CloseSocket(ctx->socket);
        DSU_QuitSockets();
        SDL_free(ctx);
        SDL_SetError("Failed to create DSU receiver thread");
        return false;
    }

    /* Store context globally */
    s_dsu_ctx = ctx;

    /* Request controller info from all slots */
    DSU_RequestControllerInfo(ctx, 0xFF);

    return true;
}

static int DSU_JoystickGetCount(void)
{
    int count = 0;
    int i;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return 0;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (ctx->slots[i].connected) {
            count++;
        }
    }
    SDL_UnlockMutex(mutex);

    return count;
}

static void DSU_JoystickDetect(void)
{
    Uint64 now;
    int i;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return;
    }

    /* Periodically request controller info and re-subscribe to data */
    now = SDL_GetTicks();
    if (now - ctx->last_request_time >= 500) {  /* Request more frequently */
        DSU_RequestControllerInfo(ctx, (Uint8)0xFF);

        /* Re-subscribe to data for detected controllers */
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if (ctx->slots[i].detected || ctx->slots[i].connected) {
                DSU_RequestControllerData(ctx, (Uint8)i);
            }
        }

        ctx->last_request_time = now;
    }

    /* Process pending joystick additions (SDL holds joystick lock during Detect) */
    /* First, collect which controllers need to be added while holding the mutex */
    struct {
        SDL_JoystickID instance_id;
        int slot;
    } pending_adds[DSU_MAX_SLOTS];
    int num_pending = 0;

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (ctx->slots[i].pending_add && ctx->slots[i].detected) {
            SDL_Log("DSU: Found pending add for slot %d, instance %d\n",
                    i, (int)ctx->slots[i].instance_id);
            ctx->slots[i].pending_add = false;
            /* DON'T mark as connected yet - wait until SDL accepts it */
            pending_adds[num_pending].instance_id = ctx->slots[i].instance_id;
            pending_adds[num_pending].slot = i;
            num_pending++;
        }
    }
    SDL_UnlockMutex(mutex);

    /* Now notify SDL about the new controllers without holding the mutex */
    for (i = 0; i < num_pending; i++) {
        SDL_Log("DSU: About to call SDL_PrivateJoystickAdded for instance %d\n", (int)pending_adds[i].instance_id);
        SDL_Log("DSU: Current joystick count = %d\n", DSU_JoystickGetCount());
        SDL_PrivateJoystickAdded(pending_adds[i].instance_id);
        SDL_Log("DSU: SDL_PrivateJoystickAdded returned for instance %d\n", (int)pending_adds[i].instance_id);

        /* NOW mark it as connected since SDL accepted it */
        SDL_LockMutex(mutex);
        ctx->slots[pending_adds[i].slot].connected = true;
        SDL_UnlockMutex(mutex);

        SDL_Log("DSU: New joystick count = %d\n", DSU_JoystickGetCount());
    }

    /* Check for timeouts */
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if ((ctx->slots[i].detected || ctx->slots[i].connected) &&
            now - ctx->slots[i].last_packet_time > 5000) {  /* Increased timeout */
            /* Controller timed out */
            SDL_Log("DSU: Controller timeout on slot %d (instance %d)\n", i, (int)ctx->slots[i].instance_id);

            /* Notify SDL if it was connected */
            if (ctx->slots[i].connected && ctx->slots[i].instance_id != 0) {
                SDL_PrivateJoystickRemoved(ctx->slots[i].instance_id);
            }

            /* Clear all state flags */
            ctx->slots[i].detected = false;
            ctx->slots[i].connected = false;
            ctx->slots[i].pending_add = false;
            ctx->slots[i].instance_id = 0;
        }
    }
    SDL_UnlockMutex(mutex);
}

static const char *DSU_JoystickGetDeviceName(int device_index)
{
    int i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return NULL;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (ctx->slots[i].connected) {
            if (count == device_index) {
                SDL_UnlockMutex(mutex);
                return ctx->slots[i].name;
            }
            count++;
        }
    }
    SDL_UnlockMutex(mutex);

    return NULL;
}

static bool DSU_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    /* DSU devices are network-based, not USB, so we don't match by VID/PID */
    return false;
}

static const char *DSU_JoystickGetDevicePath(int device_index)
{
    return NULL;  /* No path for network devices */
}

static int DSU_JoystickGetDevicePlayerIndex(int device_index)
{
    int i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return -1;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (ctx->slots[i].connected) {
            if (count == device_index) {
                SDL_UnlockMutex(mutex);
                return i;  /* Return slot ID as player index */
            }
            count++;
        }
    }
    SDL_UnlockMutex(mutex);

    return -1;
}

static void DSU_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
    /* DSU controllers have fixed slots, can't change */
}

static SDL_GUID DSU_JoystickGetDeviceGUID(int device_index)
{
    SDL_GUID guid;
    int i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    SDL_zero(guid);

    ctx = s_dsu_ctx;
    if (!ctx) {
        return guid;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (ctx->slots[i].connected) {
            if (count == device_index) {
                guid = ctx->slots[i].guid;
                SDL_UnlockMutex(mutex);
                return guid;
            }
            count++;
        }
    }
    SDL_UnlockMutex(mutex);

    return guid;
}

static SDL_JoystickID DSU_JoystickGetDeviceInstanceID(int device_index)
{
    int i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return 0;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        if (ctx->slots[i].connected) {
            if (count == device_index) {
                SDL_JoystickID id = ctx->slots[i].instance_id;
                SDL_UnlockMutex(mutex);
                return id;
            }
            count++;
        }
    }
    SDL_UnlockMutex(mutex);

    return 0;
}

static bool DSU_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    DSU_ControllerSlot *slot = NULL;
    int i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    SDL_Log("DSU: JoystickOpen called for device_index %d\n", device_index);

    ctx = s_dsu_ctx;
    if (!ctx) {
        SDL_SetError("DSU not initialized");
        SDL_Log("DSU: JoystickOpen failed - not initialized\n");
        return false;
    }
    SDL_Log("DSU: JoystickOpen - context valid\n");

    if (!joystick) {
        SDL_SetError("DSU: NULL joystick pointer");
        SDL_Log("DSU: JoystickOpen failed - NULL joystick\n");
        return false;
    }

    /* Find the slot for this device - check detected controllers */
    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (i = 0; i < DSU_MAX_SLOTS; i++) {
        /* Look for detected controllers that are about to be connected */
        if (ctx->slots[i].detected && ctx->slots[i].instance_id != 0) {
            if (count == device_index) {
                slot = &ctx->slots[i];
                SDL_Log("DSU: JoystickOpen found slot %d for device_index %d\n", i, device_index);
                break;
            }
            count++;
        }
    }
    SDL_UnlockMutex(mutex);

    if (!slot) {
        SDL_SetError("Invalid DSU device index");
        return false;
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
    SDL_Log("DSU: JoystickOpen - About to register sensors\n");
    if (slot->has_gyro || (slot->model == DSU_MODEL_FULL_GYRO) || (slot->model == DSU_MODEL_PARTIAL_GYRO)) {
        /* DSU reports gyro at varying rates, but typically 250-1000Hz for DS4/DS5 */
        SDL_Log("DSU: Adding GYRO sensor\n");
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 250.0f);
        slot->has_gyro = true;
    }
    if (slot->has_accel || (slot->model == DSU_MODEL_FULL_GYRO)) {
        /* DSU reports accelerometer at same rate as gyro */
        SDL_Log("DSU: Adding ACCEL sensor\n");
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 250.0f);
        slot->has_accel = true;
    }

    SDL_Log("DSU: JoystickOpen completed successfully for slot %d, hwdata=%p\n", slot->slot_id, slot);
    return true;
}

static bool DSU_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;
    DSU_RumblePacket packet;
    struct sockaddr_in server;
    struct DSU_Context_t *ctx;

    ctx = s_dsu_ctx;
    if (!ctx || !slot || !slot->connected) {
        SDL_SetError("DSU controller not available");
        return false;
    }

    /* Build rumble packet */
    SDL_memset(&packet, 0, sizeof(packet));
    SDL_memcpy(packet.header.magic, DSU_MAGIC_CLIENT, 4);
    packet.header.version = SDL_Swap16LE(DSU_PROTOCOL_VERSION);
    packet.header.length = SDL_Swap16LE((Uint16)(sizeof(packet) - sizeof(DSU_Header)));
    packet.header.client_id = SDL_Swap32LE(ctx->client_id);
    packet.header.message_type = SDL_Swap32LE(DSU_MSG_RUMBLE);

    /* Set rumble values */
    packet.slot = slot->slot_id;
    packet.motor_left = (Uint8)(low_frequency_rumble >> 8);   /* Convert from 16-bit to 8-bit */
    packet.motor_right = (Uint8)(high_frequency_rumble >> 8);

    /* Calculate CRC32 */
    packet.header.crc32 = 0;
    packet.header.crc32 = SDL_Swap32LE(DSU_CalculateCRC32((Uint8*)&packet, sizeof(packet)));

    /* Send to server */
    SDL_memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = DSU_htons(ctx->server_port);
    server.sin_addr.s_addr = DSU_ipv4_addr(ctx->server_address);
    if ((sendto)(ctx->socket, (const char*)&packet, (int)sizeof(packet), 0,
               (struct sockaddr *)&server, (int)sizeof(server)) < 0) {
        SDL_SetError("Failed to send rumble packet");
        return false;
    }

    return true;
}

static bool DSU_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    SDL_Unsupported();
    return false;
}

static bool DSU_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    SDL_Unsupported();
    return false;
}

static bool DSU_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    SDL_Unsupported();
    return false;
}

static bool DSU_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;

    /* Sensors are always enabled if available */
    if (!(slot->has_gyro || slot->has_accel)) {
        SDL_Unsupported();
        return false;
    }
    return true;
}

static void DSU_JoystickUpdate(SDL_Joystick *joystick)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;
    Uint64 timestamp;
    int i;

    if (!slot) {
        SDL_Log("DSU: JoystickUpdate called with NULL slot\n");
        return;
    }

    if (!slot->connected) {
        SDL_Log("DSU: JoystickUpdate called for disconnected slot %d\n", slot->slot_id);
        return;
    }

    ctx = s_dsu_ctx;
    if (!ctx || !ctx->slots_mutex) {
        SDL_Log("DSU: JoystickUpdate called but context invalid\n");
        return;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);

    /* Get current timestamp */
    timestamp = SDL_GetTicks();

    /* Log current input state */
    SDL_Log("DSU UPDATE: Slot %d buttons=0x%04x", slot->slot_id, slot->buttons);

    /* Update buttons with names */
    const char* button_names[] = {
        "Cross/A", "Circle/B", "Square/X", "Triangle/Y",
        "L1/LB", "R1/RB", "Share/Back", "Options/Start",
        "L3/LSClick", "R3/RSClick", "PS/Home", "Touchpad"
    };
    for (i = 0; i < 12; i++) {
        bool pressed = (slot->buttons & (1 << i)) ? true : false;
        if (pressed) {
            SDL_Log("  Button %d (%s): PRESSED", i, button_names[i]);
        }
        SDL_SendJoystickButton(timestamp, joystick, (Uint8)i, pressed);
    }

    /* Update axes with detailed logging */
    SDL_Log("DSU UPDATE: Axes - LX=%d LY=%d RX=%d RY=%d L2=%d R2=%d",
            slot->axes[0], slot->axes[1], slot->axes[2],
            slot->axes[3], slot->axes[4], slot->axes[5]);
    for (i = 0; i < 6; i++) {
        SDL_SendJoystickAxis(timestamp, joystick, (Uint8)i, slot->axes[i]);
    }

    /* Update hat (D-Pad) */
    const char* hat_str = "CENTERED";
    if (slot->hat & SDL_HAT_UP) {
        if (slot->hat & SDL_HAT_LEFT) hat_str = "UP-LEFT";
        else if (slot->hat & SDL_HAT_RIGHT) hat_str = "UP-RIGHT";
        else hat_str = "UP";
    } else if (slot->hat & SDL_HAT_DOWN) {
        if (slot->hat & SDL_HAT_LEFT) hat_str = "DOWN-LEFT";
        else if (slot->hat & SDL_HAT_RIGHT) hat_str = "DOWN-RIGHT";
        else hat_str = "DOWN";
    } else if (slot->hat & SDL_HAT_LEFT) {
        hat_str = "LEFT";
    } else if (slot->hat & SDL_HAT_RIGHT) {
        hat_str = "RIGHT";
    }
    if (slot->hat != SDL_HAT_CENTERED) {
        SDL_Log("  D-Pad: %s (0x%02x)", hat_str, slot->hat);
    }
    SDL_SendJoystickHat(timestamp, joystick, 0, slot->hat);

    /* Update touchpad if available */
    if (slot->has_touchpad && joystick->ntouchpads > 0) {
        /* DS4/DS5 touchpad resolution is typically 1920x943 */
        const float TOUCHPAD_WIDTH = 1920.0f;
        const float TOUCHPAD_HEIGHT = 943.0f;

        /* First touch point */
        bool touchpad_down = slot->touch1_active;
        float touchpad_x = (float)slot->touch1_x / TOUCHPAD_WIDTH;
        float touchpad_y = (float)slot->touch1_y / TOUCHPAD_HEIGHT;

        if (touchpad_down) {
            SDL_Log("  TOUCHPAD[0]: Active X=%d Y=%d (%.2f, %.2f)",
                    slot->touch1_x, slot->touch1_y, touchpad_x, touchpad_y);
        }

        /* Clamp to valid range */
        if (touchpad_x < 0.0f) touchpad_x = 0.0f;
        if (touchpad_x > 1.0f) touchpad_x = 1.0f;
        if (touchpad_y < 0.0f) touchpad_y = 0.0f;
        if (touchpad_y > 1.0f) touchpad_y = 1.0f;

        SDL_SendJoystickTouchpad(timestamp, joystick, 0, 0, touchpad_down,
                                    touchpad_x, touchpad_y,
                                    touchpad_down ? 1.0f : 0.0f);

        /* Second touch point */
        touchpad_down = slot->touch2_active;
        touchpad_x = (float)slot->touch2_x / TOUCHPAD_WIDTH;
        touchpad_y = (float)slot->touch2_y / TOUCHPAD_HEIGHT;

        if (touchpad_down) {
            SDL_Log("  TOUCHPAD[1]: Active X=%d Y=%d (%.2f, %.2f)",
                    slot->touch2_x, slot->touch2_y, touchpad_x, touchpad_y);
        }

        /* Clamp to valid range */
        if (touchpad_x < 0.0f) touchpad_x = 0.0f;
        if (touchpad_x > 1.0f) touchpad_x = 1.0f;
        if (touchpad_y < 0.0f) touchpad_y = 0.0f;
        if (touchpad_y > 1.0f) touchpad_y = 1.0f;

        SDL_SendJoystickTouchpad(timestamp, joystick, 0, 1, touchpad_down,
                                    touchpad_x, touchpad_y,
                                    touchpad_down ? 1.0f : 0.0f);
    }

    /* Update battery level */
    const char* battery_str = "Unknown";
    switch (slot->battery) {
    case DSU_BATTERY_DYING:
        battery_str = "Dying (0-10%)";
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 10);
        break;
    case DSU_BATTERY_LOW:
        battery_str = "Low (10-40%)";
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 25);
        break;
    case DSU_BATTERY_MEDIUM:
        battery_str = "Medium (40-70%)";
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 55);
        break;
    case DSU_BATTERY_HIGH:
        battery_str = "High (70-100%)";
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 85);
        break;
    case DSU_BATTERY_FULL:
        battery_str = "Full (100%)";
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 100);
        break;
    case DSU_BATTERY_CHARGING:
        battery_str = "Charging";
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_CHARGING, -1);
        break;
    case DSU_BATTERY_CHARGED:
        battery_str = "Charged";
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_CHARGING, 100);
        break;
    default:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_UNKNOWN, -1);
        break;
    }
    static Uint8 last_battery = 0xFF;
    if (slot->battery != last_battery) {
        SDL_Log("  BATTERY: %s (0x%02x)", battery_str, slot->battery);
        last_battery = slot->battery;
    }

    /* Update sensors if available */
    if (slot->has_gyro) {
        SDL_Log("  GYRO: X=%.3f Y=%.3f Z=%.3f rad/s",
                slot->gyro[0], slot->gyro[1], slot->gyro[2]);
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO,
                                  slot->motion_timestamp, slot->gyro, 3);
    }
    if (slot->has_accel) {
        SDL_Log("  ACCEL: X=%.3f Y=%.3f Z=%.3f m/s²",
                slot->accel[0], slot->accel[1], slot->accel[2]);
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL,
                                  slot->motion_timestamp, slot->accel, 3);
    }

    SDL_UnlockMutex(mutex);
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
    struct DSU_Context_t *ctx;

    SDL_Log("DSU: JoystickQuit called\n");

    ctx = s_dsu_ctx;
    if (!ctx) {
        return;
    }

    /* Clear the global pointer first to prevent access during shutdown */
    s_dsu_ctx = NULL;

    /* Stop receiver thread */
    if (SDL_GetAtomicInt(&ctx->running) != 0) {
        SDL_SetAtomicInt(&ctx->running, 0);
    }

    /* Close socket to interrupt any blocking recvfrom */
    if (ctx->socket != DSU_INVALID_SOCKET) {
        DSU_CloseSocket(ctx->socket);
        ctx->socket = DSU_INVALID_SOCKET;
    }

    /* Now wait for thread to finish */
    if (ctx->receiver_thread) {
        SDL_Log("DSU: Waiting for receiver thread to finish...\n");
        SDL_WaitThread(ctx->receiver_thread, NULL);
        ctx->receiver_thread = NULL;
        SDL_Log("DSU: Receiver thread finished\n");
    }

    /* Clean up sockets */
    DSU_QuitSockets();

    /* Clean up mutex */
    if (ctx->slots_mutex) {
        SDL_DestroyMutex(ctx->slots_mutex);
    }

    /* Free context */
    SDL_free(ctx);
    SDL_Log("DSU: Quit complete\n");
}

static bool DSU_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    /* DSU controllers map well to standard gamepad layout */
    return false;  /* Use default mapping */
}

/* Export the driver */
SDL_JoystickDriver SDL_DSU_JoystickDriver = {
    DSU_JoystickInit,
    DSU_JoystickGetCount,
    DSU_JoystickDetect,
    DSU_JoystickIsDevicePresent,
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
