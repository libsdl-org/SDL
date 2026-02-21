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

#ifndef SDL_dsuprotocol_h_
#define SDL_dsuprotocol_h_

#include "SDL_internal.h"

/* DSU (DualShock UDP) Protocol Constants - Based on CemuHook */

#define DSU_PROTOCOL_VERSION 1001
#define DSU_SERVER_PORT_DEFAULT 26760
#define DSU_CLIENT_PORT_DEFAULT 26761
#define DSU_SERVER_ADDRESS_DEFAULT "127.0.0.1"

/* Magic strings */
#define DSU_MAGIC_CLIENT "DSUC"
#define DSU_MAGIC_SERVER "DSUS"

/* Message types */
typedef enum {
    DSU_MSG_VERSION = 0x100000,
    DSU_MSG_PORTS_INFO = 0x100001,
    DSU_MSG_DATA = 0x100002,
    DSU_MSG_RUMBLE_INFO = 0x110001,  /* Unofficial */
    DSU_MSG_RUMBLE = 0x110002        /* Unofficial */
} DSU_MessageType;

/* Controller states */
typedef enum {
    DSU_STATE_DISCONNECTED = 0,
    DSU_STATE_RESERVED = 1,
    DSU_STATE_CONNECTED = 2
} DSU_SlotState;

/* Device models */
typedef enum {
    DSU_MODEL_NONE = 0,
    DSU_MODEL_PARTIAL_GYRO = 1,
    DSU_MODEL_FULL_GYRO = 2,  /* DS4, DS5 */
    DSU_MODEL_NO_GYRO = 3
} DSU_DeviceModel;

/* Connection types */
typedef enum {
    DSU_CONN_NONE = 0,
    DSU_CONN_USB = 1,
    DSU_CONN_BLUETOOTH = 2
} DSU_ConnectionType;

/* Battery states */
typedef enum {
    DSU_BATTERY_NONE = 0x00,
    DSU_BATTERY_DYING = 0x01,     /* 0-10% */
    DSU_BATTERY_LOW = 0x02,        /* 10-40% */
    DSU_BATTERY_MEDIUM = 0x03,     /* 40-70% */
    DSU_BATTERY_HIGH = 0x04,       /* 70-100% */
    DSU_BATTERY_FULL = 0x05,       /* 100% */
    DSU_BATTERY_CHARGING = 0xEE,
    DSU_BATTERY_CHARGED = 0xEF
} DSU_BatteryState;

/* Packet structures */
#pragma pack(push, 1)

typedef struct {
    char magic[4];           /* DSUC or DSUS */
    Uint16 version;          /* Protocol version (1001) */
    Uint16 length;           /* Packet length after header */
    Uint32 crc32;            /* CRC32 of packet (with this field zeroed) */
    Uint32 client_id;        /* Random client ID */
    Uint32 message_type;     /* Message type enum */
} DSU_Header;

typedef struct {
    DSU_Header header;
    Uint8 flags;             /* Slot registration flags */
    Uint8 slot_id;           /* 0-3 for specific slot, 0xFF for all */
    Uint8 mac[6];            /* MAC address filter (zeros for all) */
} DSU_PortRequest;

typedef struct {
    Uint8 slot;              /* Controller slot 0-3 */
    Uint8 slot_state;        /* DSU_SlotState */
    Uint8 device_model;      /* DSU_DeviceModel */
    Uint8 connection_type;   /* DSU_ConnectionType */
    Uint8 mac[6];            /* Controller MAC address */
    Uint8 battery;           /* DSU_BatteryState */
    Uint8 is_active;         /* 0 or 1 */
} DSU_ControllerInfo;

typedef struct {
    DSU_Header header;
    Uint8 slot;              /* Controller slot 0-3 */
    Uint8 motor_left;        /* Left/Low frequency motor intensity (0-255) */
    Uint8 motor_right;       /* Right/High frequency motor intensity (0-255) */
} DSU_RumblePacket;

typedef struct {
    DSU_Header header;
    DSU_ControllerInfo info;
    
    /* Controller data */
    Uint32 packet_number;    /* Incremental counter */
    
    /* Digital buttons */
    Uint8 button_states_1;   /* Share, L3, R3, Options, DPad */
    Uint8 button_states_2;   /* L2, R2, L1, R1, Triangle, Circle, Cross, Square */
    Uint8 button_ps;         /* PS/Home button */
    Uint8 button_touch;      /* Touchpad button */
    
    /* Analog sticks (0-255, 128=center) */
    Uint8 left_stick_x;
    Uint8 left_stick_y;
    Uint8 right_stick_x;
    Uint8 right_stick_y;
    
    /* Analog buttons (0-255, pressure sensitive) */
    Uint8 analog_dpad_left;
    Uint8 analog_dpad_down;
    Uint8 analog_dpad_right;
    Uint8 analog_dpad_up;
    Uint8 analog_button_square;
    Uint8 analog_button_cross;
    Uint8 analog_button_circle;
    Uint8 analog_button_triangle;
    Uint8 analog_button_r1;
    Uint8 analog_button_l1;
    Uint8 analog_trigger_r2;
    Uint8 analog_trigger_l2;
    
    /* Touch data (2 points max) */
    Uint8 touch1_active;
    Uint8 touch1_id;
    Uint16 touch1_x;
    Uint16 touch1_y;
    
    Uint8 touch2_active;
    Uint8 touch2_id;
    Uint16 touch2_x;
    Uint16 touch2_y;
    
    /* Motion data (optional) */
    Uint64 motion_timestamp;  /* Microseconds */
    float accel_x;            /* In g units */
    float accel_y;
    float accel_z;
    float gyro_pitch;         /* In degrees/second */
    float gyro_yaw;
    float gyro_roll;
} DSU_ControllerData;

#pragma pack(pop)

/* Button masks for button_states_1 */
#define DSU_BUTTON_SHARE    0x01
#define DSU_BUTTON_L3       0x02
#define DSU_BUTTON_R3       0x04
#define DSU_BUTTON_OPTIONS  0x08
#define DSU_BUTTON_DPAD_UP  0x10
#define DSU_BUTTON_DPAD_RIGHT 0x20
#define DSU_BUTTON_DPAD_DOWN 0x40
#define DSU_BUTTON_DPAD_LEFT 0x80

/* Button masks for button_states_2 */
#define DSU_BUTTON_L2       0x01
#define DSU_BUTTON_R2       0x02
#define DSU_BUTTON_L1       0x04
#define DSU_BUTTON_R1       0x08
#define DSU_BUTTON_TRIANGLE 0x10
#define DSU_BUTTON_CIRCLE   0x20
#define DSU_BUTTON_CROSS    0x40
#define DSU_BUTTON_SQUARE   0x80

/* Maximum number of DSU slots per server */
#define DSU_MAX_SLOTS 4

/* We can support up to 8 controllers by using 2 server connections */
#define DSU_MAX_CONTROLLERS 8

#endif /* SDL_dsuprotocol_h_ */

/* vi: set ts=4 sw=4 expandtab: */
