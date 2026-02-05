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
    /* iOS/tvOS/watchOS/visionOS - Same as macOS */
    #if defined(__APPLE__)
        #include <sys/socket.h>
        #include <netinet/in.h>
        #include <arpa/inet.h>
        #include <fcntl.h>
        #include <unistd.h>
        #include <errno.h>
        #include <sys/ioctl.h>
    /* QNX */
    #elif defined(__QNXNTO__)
        #include <sys/socket.h>
        #include <netinet/in.h>
        #include <arpa/inet.h>
        #include <fcntl.h>
        #include <unistd.h>
        #include <errno.h>
        #include <sys/ioctl.h>
    /* RISC OS */
    #elif defined(__riscos__)
        #include <sys/socket.h>
        #include <netinet/in.h>
        #include <arpa/inet.h>
        #include <fcntl.h>
        #include <unistd.h>
        #include <errno.h>
    /* Standard Unix/Linux */
    #else
        #include <sys/socket.h>
        #include <netinet/in.h>
        #include <arpa/inet.h>
        #include <fcntl.h>
        #include <unistd.h>
        #include <errno.h>
        #include <sys/time.h>
        #include <sys/select.h>
        #ifdef HAVE_SYS_IOCTL_H
            #include <sys/ioctl.h>
        #endif
        #ifdef __sun
            #include <sys/filio.h>
        #endif
        #define closesocket close
    #endif

    /* Default closesocket if not defined */
    #ifndef closesocket
        #define closesocket close
    #endif
#endif

#include <math.h>

/* Constants */
#define SERVER_REREGISTER_INTERVAL 1000  /* ms */
#define SERVER_TIMEOUT_INTERVAL 2000     /* ms */
#define GRAVITY_ACCELERATION 9.80665f    /* m/s² */

/* Internal DSU helper macros and functions */
#ifdef _WIN32
    #define DSU_htons(x) htons(x)
    #define DSU_htonl(x) htonl(x)
    #define DSU_SOCKET_ERROR SOCKET_ERROR
    #define DSU_INVALID_SOCKET INVALID_SOCKET
#else
    #define DSU_htons(x) htons(x)
    #define DSU_htonl(x) htonl(x)
    #define DSU_SOCKET_ERROR (-1)
    #define DSU_INVALID_SOCKET (-1)
#endif

/* Helper function to convert IP address string to network byte order */
static unsigned long DSU_ipv4_addr(const char *str)
{
    struct sockaddr_in addr;
    SDL_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

#ifdef _WIN32
    /* Use getaddrinfo on Windows keeping compatibility with older systems (e.g. Windows XP). */
    {
        struct addrinfo hints;
        struct addrinfo *result = NULL;

        SDL_memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;

        if (getaddrinfo(str, NULL, &hints, &result) == 0 && result && result->ai_addr) {
            const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)result->ai_addr;
            addr.sin_addr = ipv4->sin_addr;
            freeaddrinfo(result);
            return addr.sin_addr.s_addr;
        }

        if (result) {
            freeaddrinfo(result);
        }
    }
#else
    /* Use inet_pton on Unix-like systems */
    if (inet_pton(AF_INET, str, &addr.sin_addr) == 1) {
        return addr.sin_addr.s_addr;
    }
#endif

    /* Return INADDR_NONE on error (same as inet_addr would) */
    return (unsigned long)(-1);
}

/* Global DSU context pointer */
struct DSU_Context_t *s_dsu_ctx = NULL;

/* Use the DSU_Context type from the shared header */

/* Forward declarations */
void DSU_RequestControllerInfo(DSU_ServerConnection *server, Uint8 slot);
void DSU_RequestControllerData(DSU_ServerConnection *server, Uint8 slot);

/* Platform-specific network function wrappers */
/* Standard sendto/recvfrom for all supported platforms */
#define DSU_sendto sendto
#define DSU_recvfrom recvfrom
#define DSU_GetLastError() errno

/* Socket helpers implementation */
int DSU_InitSockets(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    /* Unix/Linux - no initialization needed */
    return 0;
#endif
}

void DSU_CleanupSockets(void)
{
#ifdef _WIN32
    WSACleanup();
#else
    /* Unix/Linux - no cleanup needed */
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

/* Send a packet to the DSU server */
static int DSU_SendPacket(DSU_ServerConnection *conn, void *packet, size_t size)
{
    DSU_Header *header;
    struct sockaddr_in server_addr;
    int result;

    header = (DSU_Header *)packet;

    /* Fill header */
    SDL_memcpy(header->magic, DSU_MAGIC_CLIENT, 4);
    header->version = SDL_Swap16LE(DSU_PROTOCOL_VERSION);
    header->length = SDL_Swap16LE((Uint16)(size - sizeof(DSU_Header)));
    header->client_id = SDL_Swap32LE(conn->client_id);
    header->crc32 = 0;

    /* Calculate and store CRC32 */
    header->crc32 = SDL_Swap32LE(SDL_crc32(0, packet, size));

    /* Send to server */
    SDL_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = DSU_htons(conn->server_port);
    server_addr.sin_addr.s_addr = DSU_ipv4_addr(conn->server_address);

    result = DSU_sendto(conn->socket, (const char*)packet, (int)size, 0,
                        (struct sockaddr *)&server_addr, (int)sizeof(server_addr));

    if (result < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: sendto failed with error %d", err);
#else
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: sendto failed with errno %d", DSU_GetLastError());
#endif
    }

    return result;
}

/* Request controller information */
void DSU_RequestControllerInfo(DSU_ServerConnection *conn, Uint8 slot)
{
    DSU_PortRequest request;
    int result;

    SDL_memset(&request, 0, sizeof(request));
    request.header.message_type = SDL_Swap32LE(DSU_MSG_PORTS_INFO);
    request.flags = 0;
    request.slot_id = slot;  /* 0xFF for all slots */
    /* MAC is zeros for all controllers */

    result = DSU_SendPacket(conn, &request, sizeof(request));
    (void)result;
}

/* Request controller data */
void DSU_RequestControllerData(DSU_ServerConnection *conn, Uint8 slot)
{
    DSU_PortRequest request;

    SDL_memset(&request, 0, sizeof(request));
    request.header.message_type = SDL_Swap32LE(DSU_MSG_DATA);
    request.flags = 0;  /* Subscribe to data */
    request.slot_id = slot;

    DSU_SendPacket(conn, &request, sizeof(request));
}

/* Process incoming controller data */
static void DSU_ProcessControllerData(DSU_ServerConnection *conn, DSU_ControllerData *data)
{
    DSU_ControllerSlot *slot;
    DSU_Context *ctx;
    int slot_id;
    bool was_connected;

    /* Validate connection and parent context */
    if (!conn || !conn->parent || !conn->parent->slots_mutex) {
        return;
    }
    ctx = conn->parent;

    /* Get slot ID */
    slot_id = data->info.slot;
    if (slot_id >= DSU_MAX_SLOTS) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: Invalid slot_id %d in data packet", slot_id);
        return;
    }

    SDL_LockMutex(ctx->slots_mutex);
    slot = &conn->slots[slot_id];


    /* If already connected to SDL, just update data without changing state */
    if (slot->connected) {
        was_connected = true;
    } else {
        /* Update connection state */
        was_connected = slot->detected;
        slot->detected = (data->info.slot_state == DSU_STATE_CONNECTED);
        /* Debug logging removed */
    }

    if (slot->detected || slot->connected) {
        /* Update controller info */
        SDL_memcpy(slot->mac, data->info.mac, 6);
        slot->battery = data->info.battery;
        slot->model = data->info.device_model;
        slot->connection = data->info.connection_type;
        slot->slot_id = (Uint8)slot_id;
        slot->parent_conn = conn;  /* Back-reference for rumble */

        /* Generate name with server index for multi-server support */
        SDL_snprintf(slot->name, sizeof(slot->name), "DSUClient/%d/%d", conn->server_index, slot_id);

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

        /* New controller detected */

        /* New controller connected */
        slot->instance_id = SDL_GetNextObjectID();

        /* Update controller ID for SDL */
        vendor = 0x054C;  /* Sony vendor ID */
        product = 0x05C4; /* DS4 product ID by default */
        if (slot->model == DSU_MODEL_FULL_GYRO) {
            product = 0x09CC;
        }
        slot->guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_BLUETOOTH, vendor, product, 0,
                                           NULL, slot->name, 'd', 0);

        /* Mark slot as ready for detection - DSU_JoystickDetect will add it to SDL */
        /* Do NOT call SDL_PrivateJoystickAdded here - it would deadlock with main thread */
        /* Controller ready for JoystickDetect */
    }

    SDL_UnlockMutex(ctx->slots_mutex);

    /* Subscribe to controller data updates if just detected */
    if (!was_connected && slot->detected) {
        DSU_RequestControllerData(conn, (Uint8)slot_id);
    }
}

/* Receiver thread implementation - one per server connection */
int SDLCALL DSU_ReceiverThread(void *data)
{
    DSU_ServerConnection *conn = (DSU_ServerConnection *)data;
    Uint8 buffer[1024];
    struct sockaddr_in sender;
    socklen_t sender_len = sizeof(sender);
    DSU_Header *header;
    int received;
    fd_set readfds;
    struct timeval tv;
    int select_result;

    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);

    /* Main receive loop */
    while (SDL_GetAtomicInt(&conn->running)) {
        /* Double-check parent context is still valid */
        if (!conn->parent || !conn->parent->slots_mutex) {
            break;
        }

        /* Use select() with timeout instead of spinning */
        FD_ZERO(&readfds);
        FD_SET(conn->socket, &readfds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;  /* 10ms timeout */

#ifdef _WIN32
        select_result = select(0, &readfds, NULL, NULL, &tv);
#else
        select_result = select((int)conn->socket + 1, &readfds, NULL, NULL, &tv);
#endif

        if (select_result < 0) {
            /* Select error - check if socket was closed */
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAENOTSOCK || error == WSAEBADF) {
                break;
            }
            if (error != WSAEINTR) {
                SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: select error %d", error);
                SDL_Delay(100);
            }
#else
            int err = DSU_GetLastError();
            if (err == EBADF) {
                break;
            }
            if (err != EINTR) {
                SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: select errno %d", err);
                SDL_Delay(100);
            }
#endif
            continue;
        }

        if (select_result == 0) {
            /* Timeout - no data available, loop continues */
            continue;
        }

        /* Data available - receive it */
        sender_len = sizeof(sender);
        received = DSU_recvfrom(conn->socket, (char*)buffer, sizeof(buffer), 0,
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

                            /* If controller is connected in this slot, request data */
                            if (slot_state == DSU_STATE_CONNECTED && slot_id < DSU_MAX_SLOTS) {
                                DSU_RequestControllerData(conn, slot_id);
                            }
                        }
                        break;
                    }

                    case DSU_MSG_DATA:
                        /* Controller data */
                        if (received >= (int)sizeof(DSU_ControllerData)) {
                            DSU_ProcessControllerData(conn, (DSU_ControllerData *)buffer);
                        }
                        break;

                    default:
                        /* Unknown message type */
                        break;
                    }
                }
            }
        } else if (received < 0) {
            /* Check for real errors (not just EWOULDBLOCK) */
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAENOTSOCK || error == WSAEBADF) {
                break;
            }
#else
            int err = DSU_GetLastError();
            if (err == EBADF) {
                break;
            }
#endif
        }
    }

    return 0;
}

/* Helper to parse server string (format: "address" or "address:port") */
static void DSU_ParseServerString(const char *str, char *address, size_t addr_size, Uint16 *port)
{
    const char *colon;
    size_t addr_len;

    colon = SDL_strchr(str, ':');
    if (colon) {
        addr_len = (size_t)(colon - str);
        if (addr_len >= addr_size) {
            addr_len = addr_size - 1;
        }
        SDL_memcpy(address, str, addr_len);
        address[addr_len] = '\0';
        *port = (Uint16)SDL_atoi(colon + 1);
    } else {
        SDL_strlcpy(address, str, addr_size);
    }
}

/* Driver functions - merged from SDL_dsujoystick_driver.c */
static bool DSU_JoystickInit(void)
{
    const char *enabled;
    const char *servers_hint;
    const char *client_port_hint;
    struct DSU_Context_t *ctx;
    char server_list[1024];
    char *token;
    char *saveptr;
    int server_idx;
    DSU_ServerConnection *conn;

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

    /* Get client port */
    client_port_hint = SDL_GetHint(SDL_HINT_DSU_CLIENT_PORT);
    if (client_port_hint && *client_port_hint) {
        ctx->client_port = (Uint16)SDL_atoi(client_port_hint);
    } else {
        ctx->client_port = DSU_CLIENT_PORT_DEFAULT;
    }

    /* Initialize sockets */
    if (DSU_InitSockets() != 0) {
        SDL_free(ctx);
        return false;
    }

    /* Create mutex */
    ctx->slots_mutex = SDL_CreateMutex();
    if (!ctx->slots_mutex) {
        DSU_CleanupSockets();
        SDL_free(ctx);
        SDL_OutOfMemory();
        return false;
    }

    /* Parse server list (comma-separated: "127.0.0.1:26760,192.168.1.50:26761") */
    servers_hint = SDL_GetHint(SDL_HINT_DSU_SERVER);
    if (!servers_hint || !*servers_hint) {
        /* Default to single localhost server */
        SDL_snprintf(server_list, sizeof(server_list), "%s:%d",
                     DSU_SERVER_ADDRESS_DEFAULT, DSU_SERVER_PORT_DEFAULT);
    } else {
        SDL_strlcpy(server_list, servers_hint, sizeof(server_list));
    }

    /* Parse each server */
    server_idx = 0;
    token = SDL_strtok_r(server_list, ",", &saveptr);
    while (token && server_idx < DSU_MAX_SERVERS) {
        /* Skip whitespace */
        while (*token == ' ') token++;

        conn = &ctx->servers[server_idx];
        conn->parent = ctx;
        conn->server_index = server_idx;
        conn->server_port = DSU_SERVER_PORT_DEFAULT;

        /* Parse address:port */
        DSU_ParseServerString(token, conn->server_address,
                              sizeof(conn->server_address), &conn->server_port);

        /* Create socket for this server */
        conn->socket = DSU_CreateSocket(ctx->client_port + (Uint16)server_idx);
        if (conn->socket == DSU_INVALID_SOCKET) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: Failed to create socket for server %d", server_idx);
            token = SDL_strtok_r(NULL, ",", &saveptr);
            continue;
        }

        /* Generate unique client ID */
        conn->client_id = (Uint32)SDL_GetTicks() + (Uint32)server_idx;

        /* Start receiver thread for this server */
        SDL_SetAtomicInt(&conn->running, 1);
        {
            char thread_name[32];
            SDL_snprintf(thread_name, sizeof(thread_name), "DSU_Recv_%d", server_idx);
            conn->receiver_thread = SDL_CreateThread(DSU_ReceiverThread, thread_name, conn);
        }

        if (!conn->receiver_thread) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: Failed to create thread for server %d", server_idx);
            DSU_CloseSocket(conn->socket);
            conn->socket = DSU_INVALID_SOCKET;
            token = SDL_strtok_r(NULL, ",", &saveptr);
            continue;
        }

        server_idx++;
        token = SDL_strtok_r(NULL, ",", &saveptr);
    }

    ctx->server_count = server_idx;

    if (ctx->server_count == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DSU: No servers configured");
        SDL_DestroyMutex(ctx->slots_mutex);
        DSU_CleanupSockets();
        SDL_free(ctx);
        return true;  /* Not an error, just no servers */
    }

    /* Store context globally */
    s_dsu_ctx = ctx;

    /* Request controller info from all servers */
    for (server_idx = 0; server_idx < ctx->server_count; server_idx++) {
        DSU_RequestControllerInfo(&ctx->servers[server_idx], 0xFF);
    }

    return true;
}

static int DSU_JoystickGetCount(void)
{
    int s, i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return 0;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (s = 0; s < ctx->server_count; s++) {
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if (ctx->servers[s].slots[i].connected) {
                count++;
            }
        }
    }
    SDL_UnlockMutex(mutex);

    return count;
}

static void DSU_JoystickDetect(void)
{
    Uint64 now;
    int s, i;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;
    DSU_ServerConnection *conn;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return;
    }

    now = SDL_GetTicks();

    /* For each server, periodically request controller info */
    for (s = 0; s < ctx->server_count; s++) {
        conn = &ctx->servers[s];

        if (now >= (conn->last_request_time + 500)) {  /* Request every 500ms */
            DSU_RequestControllerInfo(conn, (Uint8)0xFF);

            /* Re-subscribe to data for detected controllers */
            for (i = 0; i < DSU_MAX_SLOTS; i++) {
                if (conn->slots[i].detected || conn->slots[i].connected) {
                    DSU_RequestControllerData(conn, (Uint8)i);
                }
            }

            conn->last_request_time = now;
        }
    }

    /* Check for newly detected controllers - notify SDL about them */
    /* Collect IDs to add, then notify SDL outside our mutex to avoid deadlock */
    {
        SDL_JoystickID ids_to_add[DSU_MAX_SERVERS * DSU_MAX_SLOTS];
        int add_count = 0;

        mutex = ctx->slots_mutex;
        SDL_LockMutex(mutex);
        for (s = 0; s < ctx->server_count; s++) {
            conn = &ctx->servers[s];
            for (i = 0; i < DSU_MAX_SLOTS; i++) {
                if (conn->slots[i].detected && !conn->slots[i].connected && conn->slots[i].instance_id != 0) {
                    /* Mark connected BEFORE notifying so SDL can find it */
                    conn->slots[i].connected = true;
                    ids_to_add[add_count++] = conn->slots[i].instance_id;
                }
            }
        }
        SDL_UnlockMutex(mutex);

        /* Now notify SDL - must be outside our mutex because SDL will call back into us */
        for (i = 0; i < add_count; i++) {
            SDL_PrivateJoystickAdded(ids_to_add[i]);
        }
    }

    /* Check for timeouts on all servers */
    SDL_LockMutex(mutex);
    for (s = 0; s < ctx->server_count; s++) {
        conn = &ctx->servers[s];
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if ((conn->slots[i].detected || conn->slots[i].connected) &&
                now > (conn->slots[i].last_packet_time + 5000)) {  /* 5 second timeout */
                /* Controller timed out - notify SDL if it was connected */
                if (conn->slots[i].connected && conn->slots[i].instance_id != 0) {
                    SDL_JoystickID removed_id = conn->slots[i].instance_id;

                    /* Clear state before notifying to avoid race conditions */
                    conn->slots[i].detected = false;
                    conn->slots[i].connected = false;
                    conn->slots[i].instance_id = 0;

                    /* Unlock our mutex before taking the joystick lock to avoid deadlock */
                    SDL_UnlockMutex(mutex);

                    SDL_LockJoysticks();
                    SDL_PrivateJoystickRemoved(removed_id);
                    SDL_UnlockJoysticks();

                    /* Re-lock our mutex to continue the loop */
                    SDL_LockMutex(mutex);
                } else {
                    /* Clear all state flags */
                    conn->slots[i].detected = false;
                    conn->slots[i].connected = false;
                    conn->slots[i].instance_id = 0;
                }
            }
        }
    }
    SDL_UnlockMutex(mutex);
}

static const char *DSU_JoystickGetDeviceName(int device_index)
{
    int s, i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return NULL;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (s = 0; s < ctx->server_count; s++) {
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if (ctx->servers[s].slots[i].connected) {
                if (count == device_index) {
                    const char *name = ctx->servers[s].slots[i].name;
                    SDL_UnlockMutex(mutex);
                    return name;
                }
                count++;
            }
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

static int DSU_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    return -1;  /* DSU controllers are not Steam virtual gamepads */
}

static int DSU_JoystickGetDevicePlayerIndex(int device_index)
{
    int s, i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return -1;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (s = 0; s < ctx->server_count; s++) {
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if (ctx->servers[s].slots[i].connected) {
                if (count == device_index) {
                    int result = (s * DSU_MAX_SLOTS) + i;
                    SDL_UnlockMutex(mutex);
                    return result;
                }
                count++;
            }
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
    int s, i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    SDL_zero(guid);

    ctx = s_dsu_ctx;
    if (!ctx) {
        return guid;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (s = 0; s < ctx->server_count; s++) {
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if (ctx->servers[s].slots[i].connected) {
                if (count == device_index) {
                    guid = ctx->servers[s].slots[i].guid;
                    SDL_UnlockMutex(mutex);
                    return guid;
                }
                count++;
            }
        }
    }
    SDL_UnlockMutex(mutex);
    return guid;
}

static SDL_JoystickID DSU_JoystickGetDeviceInstanceID(int device_index)
{
    int s, i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return 0;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (s = 0; s < ctx->server_count; s++) {
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            if (ctx->servers[s].slots[i].connected) {
                if (count == device_index) {
                    SDL_JoystickID id = ctx->servers[s].slots[i].instance_id;
                    SDL_UnlockMutex(mutex);
                    return id;
                }
                count++;
            }
        }
    }
    SDL_UnlockMutex(mutex);

    return 0;
}

static bool DSU_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    DSU_ControllerSlot *slot = NULL;
    int s, i, count = 0;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;
    bool found = false;

    ctx = s_dsu_ctx;
    if (!ctx) {
        SDL_SetError("DSU not initialized");
        return false;
    }

    if (!joystick) {
        SDL_SetError("DSU: NULL joystick pointer");
        return false;
    }

    /* Find the slot for this device - check detected controllers across all servers */
    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);
    for (s = 0; s < ctx->server_count && !found; s++) {
        for (i = 0; i < DSU_MAX_SLOTS; i++) {
            /* Look for detected controllers that are about to be connected */
            if (ctx->servers[s].slots[i].detected && ctx->servers[s].slots[i].instance_id != 0) {
                if (count == device_index) {
                    slot = &ctx->servers[s].slots[i];
                    found = true;
                    break;
                }
                count++;
            }
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

    /* Set up touchpad if available - use SDL's API to properly allocate */
    if (slot->has_touchpad) {
        SDL_PrivateJoystickAddTouchpad(joystick, 2);  /* DSU supports 2 fingers */
    }

    /* Register sensors if available */
    if (slot->has_gyro || (slot->model == DSU_MODEL_FULL_GYRO) || (slot->model == DSU_MODEL_PARTIAL_GYRO)) {
        /* DSU reports gyro at varying rates, typically 250-1000Hz for DS4/DS5 */
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 250.0f);
        /* Enable sensor directly - DSU always provides sensor data */
        if (joystick->nsensors > 0) {
            joystick->sensors[joystick->nsensors - 1].enabled = true;
        }
        slot->has_gyro = true;
        slot->sensors_enabled = true;
    }
    if (slot->has_accel || (slot->model == DSU_MODEL_FULL_GYRO)) {
        /* DSU reports accelerometer at same rate as gyro */
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 250.0f);
        /* Enable sensor directly - DSU always provides sensor data */
        if (joystick->nsensors > 0) {
            joystick->sensors[joystick->nsensors - 1].enabled = true;
        }
        slot->has_accel = true;
        slot->sensors_enabled = true;
    }

    return true;
}

static bool DSU_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;
    DSU_RumblePacket packet;
    struct sockaddr_in server_addr;
    DSU_ServerConnection *conn;

    if (!slot || !slot->connected || !slot->parent_conn) {
        return SDL_SetError("DSU controller not available");
    }
    conn = slot->parent_conn;

    /* Build rumble packet */
    SDL_memset(&packet, 0, sizeof(packet));
    SDL_memcpy(packet.header.magic, DSU_MAGIC_CLIENT, 4);
    packet.header.version = SDL_Swap16LE(DSU_PROTOCOL_VERSION);
    packet.header.length = SDL_Swap16LE((Uint16)(sizeof(packet) - sizeof(DSU_Header)));
    packet.header.client_id = SDL_Swap32LE(conn->client_id);
    packet.header.message_type = SDL_Swap32LE(DSU_MSG_RUMBLE);

    /* Set rumble values */
    packet.slot = slot->slot_id;
    packet.motor_left = (Uint8)(low_frequency_rumble >> 8);   /* Convert from 16-bit to 8-bit */
    packet.motor_right = (Uint8)(high_frequency_rumble >> 8);

    /* Calculate CRC32 */
    packet.header.crc32 = 0;
    packet.header.crc32 = SDL_Swap32LE(SDL_crc32(0, &packet, sizeof(packet)));

    /* Send to server */
    SDL_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = DSU_htons(conn->server_port);
    server_addr.sin_addr.s_addr = DSU_ipv4_addr(conn->server_address);
    if (DSU_sendto(conn->socket, (const char*)&packet, (int)sizeof(packet), 0,
                   (struct sockaddr *)&server_addr, (int)sizeof(server_addr)) < 0) {
        return SDL_SetError("Failed to send rumble packet");
    }

    return true;
}

static bool DSU_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static bool DSU_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool DSU_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool DSU_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;

    if (!(slot->has_gyro || slot->has_accel)) {
        return SDL_Unsupported();
    }
    slot->sensors_enabled = enabled;
    return true;
}

static void DSU_JoystickUpdate(SDL_Joystick *joystick)
{
    DSU_ControllerSlot *slot = (DSU_ControllerSlot *)joystick->hwdata;
    struct DSU_Context_t *ctx;
    SDL_Mutex *mutex;
    Uint64 timestamp;
    int i;

    if (!slot || !slot->connected) {
        return;
    }

    ctx = s_dsu_ctx;
    if (!ctx || !ctx->slots_mutex) {
        return;
    }

    mutex = ctx->slots_mutex;
    SDL_LockMutex(mutex);

    /* Get current timestamp */
    timestamp = SDL_GetTicks();

    /* Update buttons */
    for (i = 0; i < joystick->nbuttons && i < 12; i++) {
        bool pressed = (slot->buttons & (1 << i)) ? true : false;
        SDL_SendJoystickButton(timestamp, joystick, (Uint8)i, pressed);
    }

    /* Update axes */
    for (i = 0; i < joystick->naxes && i < 6; i++) {
        SDL_SendJoystickAxis(timestamp, joystick, (Uint8)i, slot->axes[i]);
    }

    /* Update hat (D-Pad) */
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
    switch (slot->battery) {
    case DSU_BATTERY_DYING:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 10);
        break;
    case DSU_BATTERY_LOW:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 25);
        break;
    case DSU_BATTERY_MEDIUM:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 55);
        break;
    case DSU_BATTERY_HIGH:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 85);
        break;
    case DSU_BATTERY_FULL:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_ON_BATTERY, 100);
        break;
    case DSU_BATTERY_CHARGING:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_CHARGING, -1);
        break;
    case DSU_BATTERY_CHARGED:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_CHARGING, 100);
        break;
    default:
        SDL_SendJoystickPowerInfo(joystick, SDL_POWERSTATE_UNKNOWN, -1);
        break;
    }

    /* Update sensors if enabled */
    if (slot->sensors_enabled) {
        if (slot->has_gyro) {
            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO,
                                      slot->motion_timestamp, slot->gyro, 3);
        }
        if (slot->has_accel) {
            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL,
                                      slot->motion_timestamp, slot->accel, 3);
        }
    }

    SDL_UnlockMutex(mutex);
}

static void DSU_JoystickClose(SDL_Joystick *joystick)
{
    /* SDL handles touchpad cleanup - we just clear hwdata */
    joystick->hwdata = NULL;
}

static void DSU_JoystickQuit(void)
{
    struct DSU_Context_t *ctx;
    int s;
    DSU_ServerConnection *conn;

    ctx = s_dsu_ctx;
    if (!ctx) {
        return;
    }

    /* Clear the global pointer first to prevent access during shutdown */
    s_dsu_ctx = NULL;

    /* Stop all receiver threads */
    for (s = 0; s < ctx->server_count; s++) {
        conn = &ctx->servers[s];
        if (SDL_GetAtomicInt(&conn->running) != 0) {
            SDL_SetAtomicInt(&conn->running, 0);
        }
    }

    /* Close all sockets to interrupt any blocking select/recvfrom */
    for (s = 0; s < ctx->server_count; s++) {
        conn = &ctx->servers[s];
        if (conn->socket != DSU_INVALID_SOCKET) {
            DSU_CloseSocket(conn->socket);
            conn->socket = DSU_INVALID_SOCKET;
        }
    }

    /* Wait for all threads to finish */
    for (s = 0; s < ctx->server_count; s++) {
        conn = &ctx->servers[s];
        if (conn->receiver_thread) {
            SDL_WaitThread(conn->receiver_thread, NULL);
            conn->receiver_thread = NULL;
        }
    }

    /* Clean up sockets */
    DSU_CleanupSockets();

    /* Clean up mutex */
    if (ctx->slots_mutex) {
        SDL_DestroyMutex(ctx->slots_mutex);
    }

    /* Free context */
    SDL_free(ctx);
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
    DSU_JoystickGetDeviceSteamVirtualGamepadSlot,
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
