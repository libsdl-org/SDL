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

/* Global DSU context is defined in SDL_dsujoystick_driver.c */
extern DSU_Context *s_dsu_ctx;

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
    header->crc32 = SDL_Swap32LE(DSU_CalculateCRC32((const Uint8 *)packet, size));

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
                calculated_crc = DSU_CalculateCRC32(buffer, received);

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

#endif /* SDL_JOYSTICK_DSU */

/* vi: set ts=4 sw=4 expandtab: */
