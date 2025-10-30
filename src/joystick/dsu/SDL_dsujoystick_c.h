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

#ifndef SDL_dsujoystick_c_h_
#define SDL_dsujoystick_c_h_

#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_DSU

#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_atomic.h>
#include "../SDL_sysjoystick.h"
#include "SDL_dsuprotocol.h"

/* DSU Joystick driver */
extern SDL_JoystickDriver SDL_DSU_JoystickDriver;

/* Socket type definitions - move these out to ensure visibility */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <winsock2.h>
typedef SOCKET dsu_socket_t;
#else  
typedef int dsu_socket_t;
#endif

/* Internal structures - always visible */
typedef struct DSU_ControllerSlot {
    /* Connection state */
    bool detected;     /* Controller detected by DSU but not yet added to SDL */
    bool connected;    /* Controller added to SDL and visible */
    SDL_JoystickID instance_id;
    SDL_GUID guid;
    char name[128];
    
    /* DSU protocol data */
    Uint8 slot_id;
    Uint8 mac[6];
    Uint8 battery;
    DSU_DeviceModel model;
    DSU_ConnectionType connection;
    
    /* Controller state */
    Uint16 buttons;
    Sint16 axes[6];  /* LX, LY, RX, RY, L2, R2 */
    Uint8 hat;
    
    /* Motion data */
    bool has_gyro;
    bool has_accel;
    float gyro[3];   /* Pitch, Yaw, Roll in rad/s */
    float accel[3];  /* X, Y, Z in m/s² */
    Uint64 motion_timestamp;
    
    /* Touch data */
    bool has_touchpad;
    bool touch1_active;
    bool touch2_active;
    Uint8 touch1_id;
    Uint8 touch2_id;
    Uint16 touch1_x, touch1_y;
    Uint16 touch2_x, touch2_y;
    
    /* Timing */
    Uint64 last_packet_time;
    Uint32 packet_number;
    
    /* State change flags for deferred notifications */
    bool pending_add;
} DSU_ControllerSlot;

typedef struct DSU_Context_t {
    /* Network */
    dsu_socket_t socket;
    SDL_Thread *receiver_thread;
    SDL_AtomicInt running;
    
    /* Server configuration */
    char server_address[256];
    Uint16 server_port;
    Uint16 client_port;
    Uint32 client_id;
    
    /* Controller slots (4 max per DSU protocol) */
    DSU_ControllerSlot slots[DSU_MAX_SLOTS];
    SDL_Mutex *slots_mutex;
    
    /* Timing for periodic updates */
    Uint64 last_request_time;
} DSU_Context;

/* Global DSU context */
extern DSU_Context *s_dsu_ctx;

/* Socket helpers - only available when implementing */
#ifdef IN_JOYSTICK_DSU_

#ifdef __cplusplus
extern "C" {
#endif

 SDL_FORCE_INLINE Uint16 DSU_htons(Uint16 x) { return SDL_Swap16BE(x); }
 SDL_FORCE_INLINE Uint32 DSU_htonl(Uint32 x) { return SDL_Swap32BE(x); }
 SDL_FORCE_INLINE Uint32 DSU_ipv4_addr(const char *ip)
{
    unsigned int a, b, c, d;
    if (SDL_sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        Uint32 v = ((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((c & 0xFF) << 8) | (d & 0xFF);
        return DSU_htonl(v);
    }
    /* Fallback to 127.0.0.1 */
    return DSU_htonl(0x7F000001u);
}

#ifdef __cplusplus
}
#endif

#ifdef _WIN32
#define DSU_SOCKET_ERROR SOCKET_ERROR
#define DSU_INVALID_SOCKET INVALID_SOCKET
#else
#define DSU_SOCKET_ERROR -1
#define DSU_INVALID_SOCKET -1
#endif

/* Socket helpers */
int DSU_InitSockets(void);
void DSU_QuitSockets(void);
dsu_socket_t DSU_CreateSocket(Uint16 port);
void DSU_CloseSocket(dsu_socket_t socket);

/* CRC32 calculation */
Uint32 DSU_CalculateCRC32(const Uint8 *data, size_t length);

#endif /* IN_JOYSTICK_DSU_ */

#endif /* SDL_JOYSTICK_DSU */

#endif /* SDL_dsujoystick_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
