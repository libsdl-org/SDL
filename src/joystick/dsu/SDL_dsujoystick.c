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

#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_DSU

/* DSU (DualShock UDP) client joystick driver - Main Implementation */

/* Include system joystick headers */
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

/* Define this before including our header to get internal definitions */
#define IN_JOYSTICK_DSU_

/* Include our header to get structure definitions */
#include "SDL_dsujoystick_c.h"

/* Additional Windows headers */
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
/* Define socklen_t for MinGW/other Windows compilers if needed */
#ifndef _MSC_VER
#ifndef socklen_t
typedef int socklen_t;
#endif
#endif
#endif

/* Ensure timer prototypes are visible */
#include <SDL3/SDL_timer.h>

/* Platform-specific socket includes */
#ifndef _WIN32
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <errno.h>
    #ifdef HAVE_SYS_IOCTL_H
        #include <sys/ioctl.h>
    #endif
    #ifdef __sun
        #include <sys/filio.h>
    #endif
    #define closesocket close
#endif

#include <math.h>

/* Constants */
#define SERVER_REREGISTER_INTERVAL 1000  /* ms */
#define SERVER_TIMEOUT_INTERVAL 2000     /* ms */
#define GRAVITY_ACCELERATION 9.80665f    /* m/s² */

/* Internal DSU helper macros */
#ifdef _WIN32
    #define DSU_htons(x) htons(x)
    #define DSU_htonl(x) htonl(x)
    #define DSU_ipv4_addr(str) inet_addr(str)
    #define DSU_SOCKET_ERROR SOCKET_ERROR
    #define DSU_INVALID_SOCKET INVALID_SOCKET
#else
    #define DSU_htons(x) htons(x)
    #define DSU_htonl(x) htonl(x)
    #define DSU_ipv4_addr(str) inet_addr(str)
    #define DSU_SOCKET_ERROR (-1)
    #define DSU_INVALID_SOCKET (-1)
#endif

/* Global DSU context pointer */
struct DSU_Context_t *s_dsu_ctx = NULL;

/* Use the DSU_Context type from the shared header */

/* Forward declarations */
void DSU_RequestControllerInfo(DSU_Context *ctx, Uint8 slot);
void DSU_RequestControllerData(DSU_Context *ctx, Uint8 slot);

/* Socket helpers implementation */
int DSU_InitSockets(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

void DSU_QuitSockets(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

dsu_socket_t DSU_CreateSocket(Uint16 port)
{
    dsu_socket_t sock;
    struct sockaddr_in addr;
    int reuse = 1;
#ifdef _WIN32
    u_long mode = 1;
#else
    int flags;
#if defined(FIONBIO)
    int nonblock = 1;
#endif
#endif

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == DSU_INVALID_SOCKET) {
        return DSU_INVALID_SOCKET;
    }

    /* Allow address reuse */
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    /* Set socket to non-blocking */
#ifdef _WIN32
    ioctlsocket(sock, FIONBIO, &mode);
#else
#if defined(FIONBIO)
    if (ioctl(sock, FIONBIO, &nonblock) < 0) {
        flags = fcntl(sock, F_GETFL, 0);
        if (flags != -1) {
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        }
    }
#else
    flags = fcntl(sock, F_GETFL, 0);
    if (flags != -1) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
#endif
#endif

    /* Bind to client port if specified */
    if (port != 0) {
        SDL_memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            /* Bind failure is not fatal, continue anyway */
        }
    }

    return sock;
}

void DSU_CloseSocket(dsu_socket_t socket)
{
    if (socket != DSU_INVALID_SOCKET) {
        closesocket(socket);
    }
}
#if 0  /* Legacy CRC32 implementation replaced by SDL_crc32() */
/* Complete CRC32 table */
static const Uint32 crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

Uint32 DSU_CalculateCRC32(const Uint8 *data, size_t length)
{
    Uint32 crc = 0xFFFFFFFF;
    size_t i;

    for (i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}
#endif

/* Send a packet to the DSU server */
static int DSU_SendPacket(DSU_Context *ctx, void *packet, size_t size)
{
    DSU_Header *header;
    struct sockaddr_in server;
    int result;

    header = (DSU_Header *)packet;

    /* Fill header */
    SDL_memcpy(header->magic, DSU_MAGIC_CLIENT, 4);
    header->version = SDL_Swap16LE(DSU_PROTOCOL_VERSION);
    header->length = SDL_Swap16LE((Uint16)(size - sizeof(DSU_Header)));
    header->client_id = SDL_Swap32LE(ctx->client_id);
    header->crc32 = 0;

    /* Calculate and store CRC32 */
    header->crc32 = SDL_Swap32LE(SDL_crc32(0, packet, size));

    /* Send to server */
    SDL_memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = DSU_htons(ctx->server_port);
    server.sin_addr.s_addr = DSU_ipv4_addr(ctx->server_address);

    result = (sendto)(ctx->socket, (const char*)packet, (int)size, 0,
                 (struct sockaddr *)&server, (int)sizeof(server));

    if (result < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        SDL_Log("DSU: sendto failed with error %d\n", err);
#else
        SDL_Log("DSU: sendto failed with errno %d\n", errno);
#endif
    }

    return result;
}

/* Request controller information */
void DSU_RequestControllerInfo(DSU_Context *ctx, Uint8 slot)
{
    DSU_PortRequest request;

    SDL_memset(&request, 0, sizeof(request));
    request.header.message_type = SDL_Swap32LE(DSU_MSG_PORTS_INFO);
    request.flags = 0;
    request.slot_id = slot;  /* 0xFF for all slots */
    /* MAC is zeros for all controllers */

    SDL_Log("DSU: Requesting controller info for slot %d\n", slot);
    DSU_SendPacket(ctx, &request, sizeof(request));
}

/* Request controller data */
void DSU_RequestControllerData(DSU_Context *ctx, Uint8 slot)
{
    DSU_PortRequest request;

    SDL_memset(&request, 0, sizeof(request));
    request.header.message_type = SDL_Swap32LE(DSU_MSG_DATA);
    request.flags = 0;  /* Subscribe to data */
    request.slot_id = slot;

    SDL_Log("DSU: Subscribing to data for slot %d\n", slot);
    DSU_SendPacket(ctx, &request, sizeof(request));
}

/* Process incoming controller data */
static void DSU_ProcessControllerData(DSU_Context *ctx, DSU_ControllerData *data)
{
    DSU_ControllerSlot *slot;
    int slot_id;
    bool was_connected;

    /* Validate context */
    if (!ctx || !ctx->slots_mutex) {
        SDL_Log("DSU: Invalid context or mutex in ProcessControllerData\n");
        return;
    }

    /* Get slot ID */
    slot_id = data->info.slot;
    SDL_Log("DSU: Raw slot_id from packet: %d (max=%d)\n", slot_id, DSU_MAX_SLOTS);
    if (slot_id >= DSU_MAX_SLOTS) {
        SDL_Log("DSU: Invalid slot_id %d in data packet\n", slot_id);
        return;
    }

    SDL_Log("DSU: Processing data for slot %d\n", slot_id);

    if (!ctx->slots_mutex) {
        SDL_Log("DSU: ERROR - slots_mutex is NULL!\n");
        return;
    }

    SDL_LockMutex(ctx->slots_mutex);
    SDL_Log("DSU: Mutex locked, accessing slot %d\n", slot_id);
    slot = &ctx->slots[slot_id];
    SDL_Log("DSU: Got slot pointer for slot %d\n", slot_id);

    SDL_Log("DSU: Slot %d state: detected=%d connected=%d instance_id=%d\n",
            slot_id, slot->detected, slot->connected, (int)slot->instance_id);

    /* If already connected to SDL, just update data without changing state */
    if (slot->connected) {
        SDL_Log("DSU: Slot %d already connected, updating data only\n", slot_id);
        was_connected = true;
    } else {
        /* Update connection state */
        was_connected = slot->detected;
        slot->detected = (data->info.slot_state == DSU_STATE_CONNECTED);
        SDL_Log("DSU: Slot %d state updated: was_connected=%d detected=%d\n",
                slot_id, was_connected, slot->detected);
    }

    if (slot->detected || slot->connected) {
        /* Update controller info */
        SDL_memcpy(slot->mac, data->info.mac, 6);
        slot->battery = data->info.battery;
        slot->model = data->info.device_model;
        slot->connection = data->info.connection_type;
        slot->slot_id = (Uint8)slot_id;

        /* Generate name */
        SDL_snprintf(slot->name, sizeof(slot->name), "DSUClient/%d", slot_id);

        /* Update button states */
        slot->buttons = 0;

        /* Map DSU buttons to SDL buttons */
        if (data->button_states_2 & DSU_BUTTON_CROSS) slot->buttons |= (1 << 0);
        if (data->button_states_2 & DSU_BUTTON_CIRCLE) slot->buttons |= (1 << 1);
        if (data->button_states_2 & DSU_BUTTON_SQUARE) slot->buttons |= (1 << 2);
        if (data->button_states_2 & DSU_BUTTON_TRIANGLE) slot->buttons |= (1 << 3);
        if (data->button_states_2 & DSU_BUTTON_L1) slot->buttons |= (1 << 4);
        if (data->button_states_2 & DSU_BUTTON_R1) slot->buttons |= (1 << 5);
        if (data->button_states_1 & DSU_BUTTON_SHARE) slot->buttons |= (1 << 6);
        if (data->button_states_1 & DSU_BUTTON_OPTIONS) slot->buttons |= (1 << 7);
        if (data->button_states_1 & DSU_BUTTON_L3) slot->buttons |= (1 << 8);
        if (data->button_states_1 & DSU_BUTTON_R3) slot->buttons |= (1 << 9);
        if (data->button_ps) slot->buttons |= (1 << 10);
        if (data->button_touch) slot->buttons |= (1 << 11);

        /* Update analog sticks */
        slot->axes[0] = ((Sint16)data->left_stick_x - 128) * 257;
        slot->axes[1] = ((Sint16)data->left_stick_y - 128) * -257;
        slot->axes[2] = ((Sint16)data->right_stick_x - 128) * 257;
        slot->axes[3] = ((Sint16)data->right_stick_y - 128) * -257;

        /* Triggers */
        slot->axes[4] = ((Sint16)data->analog_trigger_l2) * 128;
        slot->axes[5] = ((Sint16)data->analog_trigger_r2) * 128;

        /* D-Pad as hat */
        slot->hat = SDL_HAT_CENTERED;
        if (data->button_states_1 & DSU_BUTTON_DPAD_UP) slot->hat |= SDL_HAT_UP;
        if (data->button_states_1 & DSU_BUTTON_DPAD_DOWN) slot->hat |= SDL_HAT_DOWN;
        if (data->button_states_1 & DSU_BUTTON_DPAD_LEFT) slot->hat |= SDL_HAT_LEFT;
        if (data->button_states_1 & DSU_BUTTON_DPAD_RIGHT) slot->hat |= SDL_HAT_RIGHT;

        /* Motion data */
        if (data->motion_timestamp != 0) {
            slot->has_gyro = true;
            slot->has_accel = true;
            slot->motion_timestamp = SDL_Swap64LE(data->motion_timestamp);

            /* Convert gyro from deg/s to rad/s (handling endianness) */
            slot->gyro[0] = SDL_SwapFloatLE(data->gyro_pitch) * (SDL_PI_F / 180.0f);
            slot->gyro[1] = SDL_SwapFloatLE(data->gyro_yaw) * (SDL_PI_F / 180.0f);
            slot->gyro[2] = SDL_SwapFloatLE(data->gyro_roll) * (SDL_PI_F / 180.0f);

            /* Convert accel from g to m/s² (handling endianness) */
            slot->accel[0] = SDL_SwapFloatLE(data->accel_x) * GRAVITY_ACCELERATION;
            slot->accel[1] = SDL_SwapFloatLE(data->accel_y) * GRAVITY_ACCELERATION;
            slot->accel[2] = SDL_SwapFloatLE(data->accel_z) * GRAVITY_ACCELERATION;
        }

        /* Update last packet time */
        slot->last_packet_time = SDL_GetTicks();

        /* Touch data */
        slot->has_touchpad = true;
        slot->touch1_active = data->touch1_active;
        slot->touch2_active = data->touch2_active;
        slot->touch1_id = data->touch1_id;
        slot->touch2_id = data->touch2_id;
        slot->touch1_x = SDL_Swap16LE(data->touch1_x);
        slot->touch1_y = SDL_Swap16LE(data->touch1_y);
        slot->touch2_x = SDL_Swap16LE(data->touch2_x);
        slot->touch2_y = SDL_Swap16LE(data->touch2_y);

        /* Update timing */
        slot->last_packet_time = SDL_GetTicks();
        slot->packet_number = SDL_Swap32LE(data->packet_number);
    }

    /* Handle connection state changes (before unlock) */
    if (!was_connected && slot->detected) {
        Uint16 vendor;
        Uint16 product;

        /* New controller connected */
        slot->instance_id = SDL_GetNextObjectID();
        SDL_Log("DSU: Generated new instance_id %d for slot %d\n", (int)slot->instance_id, slot_id);

        /* Update controller ID for SDL */
        vendor = 0x054C;  /* Sony vendor ID */
        product = 0x05C4; /* DS4 product ID by default */
        if (slot->model == DSU_MODEL_FULL_GYRO) {
            product = 0x09CC;
        }
        slot->guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_BLUETOOTH, vendor, product, 0,
                                           NULL, slot->name, 'd', 0);

        /* Mark for deferred add (SDL_PrivateJoystickAdded requires joystick lock) */
        slot->pending_add = true;

        SDL_Log("DSU: Controller connected on slot %d, instance %d, will notify SDL\n",
                slot_id, (int)slot->instance_id);
    }

    SDL_Log("DSU: About to unlock mutex for slot %d\n", slot_id);
    SDL_UnlockMutex(ctx->slots_mutex);
    SDL_Log("DSU: Unlocked mutex for slot %d\n", slot_id);

    /* Subscribe to controller data updates if just detected */
    if (!was_connected && slot->detected) {
        DSU_RequestControllerData(ctx, (Uint8)slot_id);
    }
    SDL_Log("DSU: Finished processing data for slot %d\n", slot_id);
}

/* Receiver thread implementation */
int SDLCALL DSU_ReceiverThread(void *data)
{
    DSU_Context *ctx = (DSU_Context *)data;
    Uint8 buffer[1024];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    DSU_Header *header;
    int received;

    SDL_Log("DSU: Receiver thread starting\n");
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);

    /* Main receive loop */
    while (SDL_GetAtomicInt(&ctx->running)) {
        /* Double-check context is still valid */
        if (!ctx->slots_mutex) {
            SDL_Log("DSU: Receiver thread exiting - mutex destroyed\n");
            break;
        }
        received = recvfrom(ctx->socket, (char*)buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&sender, &sender_len);

        if (received > (int)sizeof(DSU_Header)) {
            header = (DSU_Header *)buffer;

            /* Validate magic */
            if (SDL_memcmp(header->magic, DSU_MAGIC_SERVER, 4) == 0) {
                Uint32 received_crc;
                Uint32 calculated_crc;

                /* Validate CRC32 */
                received_crc = SDL_Swap32LE(header->crc32);
                header->crc32 = 0;
                calculated_crc = SDL_crc32(0, buffer, (size_t)received);

                if (received_crc == calculated_crc) {
                    Uint32 msg_type = SDL_Swap32LE(header->message_type);

                    switch (msg_type) {
                    case DSU_MSG_VERSION:
                        /* Version info received */
                        break;

                    case DSU_MSG_PORTS_INFO: {
                        /* Port info response - tells us which slots have controllers */
                        if (received >= (int)(sizeof(DSU_Header) + 4)) {
                            Uint8 *data_ptr;
                            Uint8 slot_id;
                            Uint8 slot_state;

                            /* Parse port info */
                            data_ptr = buffer + sizeof(DSU_Header);
                            slot_id = data_ptr[0];
                            slot_state = data_ptr[1];
                            /* Skip device_model = data_ptr[2] and connection_type = data_ptr[3] - not used */

                            SDL_Log("DSU: Port info - slot %d state %d\n", slot_id, slot_state);

                            /* If controller is connected in this slot, request data */
                            if (slot_state == DSU_STATE_CONNECTED && slot_id < DSU_MAX_SLOTS) {
                                DSU_RequestControllerData(ctx, slot_id);
                            }
                        }
                        break;
                    }

                    case DSU_MSG_DATA:
                        /* Controller data */
                        if (received >= (int)sizeof(DSU_ControllerData)) {
                            DSU_ControllerData *packet = (DSU_ControllerData *)buffer;
                            SDL_Log("DSU: Data packet received for slot %d\n", packet->info.slot);
                            DSU_ProcessControllerData(ctx, packet);
                        }
                        break;

                    default:
                        /* Unknown message type */
                        break;
                    }
                };
            }
        } else if (received < 0) {
            /* Check for real errors (not just EWOULDBLOCK) */
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAENOTSOCK || error == WSAEBADF) {
                /* Socket closed, exit gracefully */
                SDL_Log("DSU: Socket closed, receiver thread exiting\n");
                break;
            }
            if (error != WSAEWOULDBLOCK && error != WSAEINTR && error != WSAECONNRESET) {
                SDL_Log("DSU: recvfrom error %d\n", error);
                SDL_Delay(100);  /* Back off on errors */
            }
#else
            if (errno == EBADF) {
                /* Socket closed, exit gracefully */
                SDL_Log("DSU: Socket closed, receiver thread exiting\n");
                break;
            }
            if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
                SDL_Log("DSU: recvfrom errno %d\n", errno);
                SDL_Delay(100);
            }
#endif
        }

        /* Small delay to prevent CPU spinning */
        SDL_Delay(1);
    }

    SDL_Log("DSU: Receiver thread exiting cleanly\n");
    return 0;
}

/* Driver functions - merged from SDL_dsujoystick_driver.c */
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
    packet.header.crc32 = SDL_Swap32LE(SDL_crc32(0, &packet, sizeof(packet)));

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
