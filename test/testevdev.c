/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2020-2022 Collabora Ltd.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

/* Hack to avoid dynapi renaming */
#include "../src/dynapi/SDL_dynapi.h"
#ifdef SDL_DYNAMIC_API
#undef SDL_DYNAMIC_API
#endif
#define SDL_DYNAMIC_API 0

#include "../src/SDL_internal.h"

#include <stdio.h>
#include <string.h>

static int run_test(void);

/* FIXME: Need CMake tests for this */
#if (defined(HAVE_LIBUDEV_H) || defined(SDL_JOYSTICK_LINUX)) && defined(HAVE_LINUX_INPUT_H)

#include <stdint.h>

#include "../src/core/linux/SDL_evdev_capabilities.h"
#include "../src/core/linux/SDL_evdev_capabilities.c"

static const struct
{
    int code;
    const char *name;
} device_classes[] = {
#define CLS(x)                  \
    {                           \
        SDL_UDEV_DEVICE_##x, #x \
    }
    CLS(MOUSE),
    CLS(KEYBOARD),
    CLS(HAS_KEYS),
    CLS(JOYSTICK),
    CLS(SOUND),
    CLS(TOUCHSCREEN),
    CLS(ACCELEROMETER),
    CLS(TOUCHPAD),
#undef CLS
    { 0, NULL }
};

typedef struct
{
    const char *name;
    const char *eviocgname;
    const char *usb_vendor_name;
    const char *usb_product_name;
    uint16_t bus_type;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t version;
    uint8_t ev[(EV_MAX + 1) / 8];
    uint8_t keys[(KEY_MAX + 1) / 8];
    uint8_t abs[(ABS_MAX + 1) / 8];
    uint8_t rel[(REL_MAX + 1) / 8];
    uint8_t ff[(FF_MAX + 1) / 8];
    uint8_t props[(INPUT_PROP_MAX + 1) / 8];
    int expected;
    const char *todo;
    size_t hid_report_descriptor_length;
    const unsigned char *hid_report_descriptor;
} GuessTest;

/*
 * Test-cases for guessing a device type from its capabilities.
 *
 * The bytes in ev, etc. are in little-endian byte order
 * Trailing zeroes can be omitted.
 *
 * The evemu-describe tool is a convenient way to add a test-case for
 * a physically available device. To contribute new test-cases, see:
 * https://github.com/libsdl-org/SDL/issues/7801#issuecomment-1589114910
 */
#define ZEROx4 0, 0, 0, 0
#define ZEROx8 ZEROx4, ZEROx4
#define FFx4   0xff, 0xff, 0xff, 0xff
#define FFx8   FFx4, FFx4

static unsigned char xbox_one_elite_2_hid_report_descriptor[] =
{
    /* Generic Desktop / Game Pad, Generic Desktop / Keyboard */
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01, 0x85, 0x01,
    0x09, 0x01, 0xa1, 0x00, 0x09, 0x30, 0x09, 0x31,
    0x15, 0x00, 0x27, 0xff, 0xff, 0x00, 0x00, 0x95,
    0x02, 0x75, 0x10, 0x81, 0x02, 0xc0, 0x09, 0x01,
    0xa1, 0x00, 0x09, 0x32, 0x09, 0x35, 0x15, 0x00,
    0x27, 0xff, 0xff, 0x00, 0x00, 0x95, 0x02, 0x75,
    0x10, 0x81, 0x02, 0xc0, 0x05, 0x02, 0x09, 0xc5,
    0x15, 0x00, 0x26, 0xff, 0x03, 0x95, 0x01, 0x75,
    0x0a, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00, 0x75,
    0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x02, 0x09,
    0xc4, 0x15, 0x00, 0x26, 0xff, 0x03, 0x95, 0x01,
    0x75, 0x0a, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00,
    0x75, 0x06, 0x95, 0x01, 0x81, 0x03, 0x05, 0x01,
    0x09, 0x39, 0x15, 0x01, 0x25, 0x08, 0x35, 0x00,
    0x46, 0x3b, 0x01, 0x66, 0x14, 0x00, 0x75, 0x04,
    0x95, 0x01, 0x81, 0x42, 0x75, 0x04, 0x95, 0x01,
    0x15, 0x00, 0x25, 0x00, 0x35, 0x00, 0x45, 0x00,
    0x65, 0x00, 0x81, 0x03, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x0f, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01,
    0x95, 0x0f, 0x81, 0x02, 0x15, 0x00, 0x25, 0x00,
    0x75, 0x01, 0x95, 0x01, 0x81, 0x03, 0x05, 0x0c,
    0x0a, 0xb2, 0x00, 0x15, 0x00, 0x25, 0x01, 0x95,
    0x01, 0x75, 0x01, 0x81, 0x02, 0x15, 0x00, 0x25,
    0x00, 0x75, 0x07, 0x95, 0x01, 0x81, 0x03, 0x05,
    0x0c, 0x09, 0x01, 0xa1, 0x01, 0x0a, 0x85, 0x00,
    0x15, 0x00, 0x26, 0xff, 0x00, 0x95, 0x01, 0x75,
    0x08, 0x81, 0x02, 0x0a, 0x99, 0x00, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x95, 0x01, 0x75, 0x04, 0x81,
    0x02, 0x15, 0x00, 0x25, 0x00, 0x95, 0x01, 0x75,
    0x04, 0x81, 0x03, 0x0a, 0x81, 0x00, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x95, 0x01, 0x75, 0x04, 0x81,
    0x02, 0x15, 0x00, 0x25, 0x00, 0x95, 0x01, 0x75,
    0x04, 0x81, 0x03, 0xc0, 0x05, 0x0f, 0x09, 0x21,
    0x85, 0x03, 0xa1, 0x02, 0x09, 0x97, 0x15, 0x00,
    0x25, 0x01, 0x75, 0x04, 0x95, 0x01, 0x91, 0x02,
    0x15, 0x00, 0x25, 0x00, 0x75, 0x04, 0x95, 0x01,
    0x91, 0x03, 0x09, 0x70, 0x15, 0x00, 0x25, 0x64,
    0x75, 0x08, 0x95, 0x04, 0x91, 0x02, 0x09, 0x50,
    0x66, 0x01, 0x10, 0x55, 0x0e, 0x15, 0x00, 0x26,
    0xff, 0x00, 0x75, 0x08, 0x95, 0x01, 0x91, 0x02,
    0x09, 0xa7, 0x15, 0x00, 0x26, 0xff, 0x00, 0x75,
    0x08, 0x95, 0x01, 0x91, 0x02, 0x65, 0x00, 0x55,
    0x00, 0x09, 0x7c, 0x15, 0x00, 0x26, 0xff, 0x00,
    0x75, 0x08, 0x95, 0x01, 0x91, 0x02, 0xc0, 0x05,
    0x0c, 0x09, 0x01, 0x85, 0x0c, 0xa1, 0x01, 0x0a,
    0x9e, 0x00, 0x15, 0x00, 0x26, 0xff, 0x00, 0x95,
    0x01, 0x75, 0x08, 0x81, 0x02, 0x0a, 0xa1, 0x00,
    0x15, 0x00, 0x26, 0xff, 0x00, 0x95, 0x01, 0x75,
    0x08, 0x81, 0x02, 0x0a, 0xa2, 0x00, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x95, 0x01, 0x75, 0x08, 0x81,
    0x02, 0x0a, 0xa3, 0x00, 0x15, 0x00, 0x26, 0xff,
    0x00, 0x95, 0x01, 0x75, 0x08, 0x81, 0x02, 0xc0,
    0xc0, 0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85,
    0x05, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15,
    0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81,
    0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95,
    0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05,
    0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xc0,
};
SDL_COMPILE_TIME_ASSERT (xbox_one_elite_2, sizeof (xbox_one_elite_2_hid_report_descriptor) == 0720);

static unsigned char ps3_hid_report_descriptor[] =
{
    /* Generic Desktop / Joystick */
    0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0xa1, 0x02,
    0x85, 0x01, 0x75, 0x08, 0x95, 0x01, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x81, 0x03, 0x75, 0x01, 0x95,
    0x13, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45,
    0x01, 0x05, 0x09, 0x19, 0x01, 0x29, 0x13, 0x81,
    0x02, 0x75, 0x01, 0x95, 0x0d, 0x06, 0x00, 0xff,
    0x81, 0x03, 0x15, 0x00, 0x26, 0xff, 0x00, 0x05,
    0x01, 0x09, 0x01, 0xa1, 0x00, 0x75, 0x08, 0x95,
    0x04, 0x35, 0x00, 0x46, 0xff, 0x00, 0x09, 0x30,
    0x09, 0x31, 0x09, 0x32, 0x09, 0x35, 0x81, 0x02,
    0xc0, 0x05, 0x01, 0x75, 0x08, 0x95, 0x27, 0x09,
    0x01, 0x81, 0x02, 0x75, 0x08, 0x95, 0x30, 0x09,
    0x01, 0x91, 0x02, 0x75, 0x08, 0x95, 0x30, 0x09,
    0x01, 0xb1, 0x02, 0xc0, 0xa1, 0x02, 0x85, 0x02,
    0x75, 0x08, 0x95, 0x30, 0x09, 0x01, 0xb1, 0x02,
    0xc0, 0xa1, 0x02, 0x85, 0xee, 0x75, 0x08, 0x95,
    0x30, 0x09, 0x01, 0xb1, 0x02, 0xc0, 0xa1, 0x02,
    0x85, 0xef, 0x75, 0x08, 0x95, 0x30, 0x09, 0x01,
    0xb1, 0x02, 0xc0, 0xc0, 0x00,
};
SDL_COMPILE_TIME_ASSERT (ps3, sizeof (ps3_hid_report_descriptor) == 149);

#if 0 /* unused for now */
static unsigned char vrs_pedals_hid_report_descriptor[] =
{
    /* Generic Desktop / Joystick */
    0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0x05, 0x01,
    0xa1, 0x02, 0x85, 0x01, 0x09, 0x30, 0x09, 0x31,
    0x09, 0x32, 0x15, 0x00, 0x27, 0xff, 0xff, 0x00,
    0x00, 0x35, 0x00, 0x47, 0xff, 0xff, 0x00, 0x00,
    0x75, 0x10, 0x95, 0x03, 0x81, 0x02, 0x06, 0x00,
    0xff, 0x09, 0x01, 0x95, 0x39, 0x75, 0x08, 0x26,
    0xff, 0x00, 0x15, 0x00, 0x81, 0x02, 0xc0, 0x06,
    0x00, 0xff, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x64,
    0x95, 0x3f, 0x75, 0x08, 0x26, 0xff, 0x00, 0x15,
    0x00, 0x09, 0x01, 0x91, 0x02, 0x85, 0x65, 0x95,
    0x3f, 0x75, 0x08, 0x26, 0xff, 0x00, 0x15, 0x00,
    0x09, 0x01, 0x81, 0x02, 0xc0, 0xc0,
};
SDL_COMPILE_TIME_ASSERT (vrs_pedals, sizeof (vrs_pedals_hid_report_descriptor) == 0136);
#endif

static unsigned char thinkpad_usb_keyboard_hid_report_descriptor[] =
{
    /* Generic Desktop / Keyboard */
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x05, 0x07,
    0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01,
    0x95, 0x08, 0x75, 0x01, 0x81, 0x02, 0x95, 0x08,
    0x75, 0x01, 0x81, 0x01, 0x05, 0x08, 0x19, 0x01,
    0x29, 0x03, 0x95, 0x03, 0x75, 0x01, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x05, 0x91, 0x01, 0x05, 0x07,
    0x19, 0x00, 0x2a, 0xff, 0x00, 0x15, 0x00, 0x26,
    0xff, 0x00, 0x95, 0x06, 0x75, 0x08, 0x81, 0x00,
    0xc0,
};
SDL_COMPILE_TIME_ASSERT (thinkpad_usb_keyboard, sizeof (thinkpad_usb_keyboard_hid_report_descriptor) == 65);

static unsigned char thinkpad_usb_trackpoint_hid_report_descriptor[] =
{
    /* Generic Desktop / Mouse, Generic Desktop / System Control,
     * Consumer Devices / Consumer Control */
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x01,
    0x09, 0x01, 0xa1, 0x00, 0x05, 0x09, 0x19, 0x01,
    0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03,
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05,
    0x81, 0x01, 0x05, 0x01, 0x09, 0x30, 0x09, 0x31,
    0x15, 0x81, 0x25, 0x7f, 0x95, 0x02, 0x75, 0x08,
    0x81, 0x06, 0xc0, 0xc0, 0x05, 0x01, 0x09, 0x80,
    0xa1, 0x01, 0x85, 0x02, 0x05, 0x01, 0x15, 0x00,
    0x25, 0x01, 0x95, 0x08, 0x75, 0x01, 0x19, 0x81,
    0x29, 0x88, 0x81, 0x02, 0xc0, 0x05, 0x0c, 0x09,
    0x01, 0xa1, 0x01, 0x85, 0x03, 0x95, 0x08, 0x75,
    0x01, 0x15, 0x00, 0x25, 0x01, 0x09, 0xe9, 0x09,
    0xea, 0x09, 0xe2, 0x09, 0xb7, 0x09, 0xcd, 0x09,
    0xb5, 0x09, 0xb6, 0x0a, 0x94, 0x01, 0x81, 0x02,
    0x09, 0x03, 0xa1, 0x02, 0x05, 0x09, 0x19, 0x10,
    0x29, 0x17, 0x81, 0x02, 0x05, 0x09, 0x19, 0x18,
    0x29, 0x1f, 0x81, 0x02, 0xc0, 0x05, 0x08, 0x95,
    0x02, 0x09, 0x09, 0x09, 0x21, 0x91, 0x02, 0x95,
    0x01, 0x75, 0x06, 0x91, 0x03, 0xc0, 0x06, 0x01,
    0xff, 0x09, 0x01, 0xa1, 0x01, 0x85, 0x04, 0x95,
    0x01, 0x75, 0x08, 0x15, 0x00, 0x26, 0xff, 0x00,
    0x09, 0x20, 0xb1, 0x03, 0x09, 0x21, 0xb1, 0x03,
    0x09, 0x22, 0xb1, 0x03, 0x09, 0x23, 0xb1, 0x03,
    0xc0,
};
SDL_COMPILE_TIME_ASSERT (thinkpad_usb_trackpoint, sizeof (thinkpad_usb_trackpoint_hid_report_descriptor) == 185);

#if 0 /* unused for now */
static unsigned char heusinkveld_pedals_hid_report_descriptor[] =
{
    /* Generic Desktop / Joystick */
    0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0x09, 0x33,
    0x09, 0x34, 0x09, 0x35, 0x15, 0x00, 0x26, 0xff,
    0x0f, 0x85, 0x01, 0x75, 0x10, 0x95, 0x03, 0x81,
    0x02, 0x09, 0x00, 0x75, 0x10, 0x95, 0x06, 0x82,
    0x01, 0x01, 0x85, 0x02, 0x75, 0x10, 0x95, 0x03,
    0x09, 0x00, 0x09, 0x00, 0xb1, 0x02, 0x85, 0x03,
    0x75, 0x08, 0x95, 0x03, 0x09, 0x00, 0x82, 0x01,
    0x01, 0xc0,
};
SDL_COMPILE_TIME_ASSERT (heusinkveld_pedals, sizeof (heusinkveld_pedals_hid_report_descriptor) == 072);
#endif

static unsigned char fanatec_handbrake_hid_report_descriptor[] =
{
    /* Generic Desktop / Joystick */
    0x05, 0x01, 0x09, 0x04, 0xa1, 0x01, 0x15, 0x00,
    0x26, 0xff, 0x00, 0x95, 0x01, 0x75, 0x08, 0x09,
    0x30, 0x81, 0x02, 0x06, 0x00, 0xff, 0x09, 0x01,
    0x95, 0x03, 0x81, 0x02, 0x06, 0x00, 0xff, 0x09,
    0x01, 0x95, 0x02, 0x91, 0x02, 0xc0,
};
SDL_COMPILE_TIME_ASSERT (fanatec_handbrake, sizeof (fanatec_handbrake_hid_report_descriptor) == 046);

/* Test-cases derived from real devices or from Linux kernel source */
/* *INDENT-OFF* */ /* clang-format off */
static const GuessTest guess_tests[] =
{
    {
      .name = "Xbox 360 wired USB controller",
      .eviocgname = "Microsoft X-Box 360 pad",
      .usb_vendor_name = "©Microsoft Corporation",
      .usb_product_name = "Controller",
      /* 8BitDo N30 Pro 2 v0114 via USB-C (with the xpad driver) is
       * reported as 0003:045e:028e v0114, and is functionally equivalent */
      .bus_type = 0x0003,
      .vendor_id = 0x045e,
      .product_id = 0x028e,
      .version = 0x0114,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, FF */
      .ev = { 0x0b, 0x00, 0x20 },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    {
      .name = "X-Box One Elite",
      .bus_type = 0x0003,
      .vendor_id = 0x045e,
      .product_id = 0x02e3,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    { /* Reference: https://github.com/libsdl-org/SDL/issues/7814 */
      .name = "X-Box One Elite 2 via USB",
      /* The same physical device via Bluetooth, 0005:045e:0b22 v0517,
       * is reported differently (below). */
      /* Version 0407 is functionally equivalent. */
      .bus_type = 0x0003,
      .vendor_id = 0x045e,
      .product_id = 0x0b00,
      .version = 0x0511,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, FF */
      .ev = { 0x0b, 0x00, 0x20 },
      /* XY (left stick), RX/RY (right stick), Z/RZ (triggers), HAT0 (dpad) */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ ZEROx8,
          /* 0x280 */ ZEROx8,
          /* BTN_TRIGGER_HAPPY5 up to BTN_TRIGGER_HAPPY8 inclusive are the
           * back buttons (paddles) */
          /* 0x2c0 */ 0xf0,
      },
    },
    { /* Reference: https://github.com/libsdl-org/SDL/issues/7814 */
      .name = "X-Box One Elite 2 via Bluetooth",
      /* The same physical device via USB, 0003:045e:0b00 v0511,
       * is reported differently (above). */
      .bus_type = 0x0003,
      .vendor_id = 0x045e,
      .product_id = 0x0b22,
      .version = 0x0517,
      .expected = SDL_UDEV_DEVICE_JOYSTICK | SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY, ABS, FF */
      .ev = { 0x0b, 0x00, 0x20 },
      /* Android-style mapping:
       * XY (left stick), Z/RZ (right stick), GAS/BRAKE (triggers), HAT0 (dpad) */
      .abs = { 0x27, 0x06, 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx8,
          /* KEY_RECORD is advertised but isn't generated in practice */
          /* 0x80 */ ZEROx4, 0x80, 0x00, 0x00, 0x00,
          /* KEY_UNKNOWN (240) is reported for the profile selector and all
           * four back buttons (paddles) */
          /* 0xc0 */ ZEROx4, 0x00, 0x00, 0x01, 0x00,
          /* ABXY, TL, TR, TL2, TR2, SELECT, START, MODE, THUMBL,
           * THUMBR have their obvious meanings; C and Z are also
           * advertised, but are not generated in practice. */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x7f,
      },
      .hid_report_descriptor_length = sizeof (xbox_one_elite_2_hid_report_descriptor),
      .hid_report_descriptor = &xbox_one_elite_2_hid_report_descriptor[0],
    },
    {
      .name = "X-Box One S via Bluetooth",
      .bus_type = 0x0005,
      .vendor_id = 0x045e,
      .product_id = 0x02e0,
      .version = 0x1130,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    {
      .name = "X-Box One S wired",
      .bus_type = 0x0003,
      .vendor_id = 0x045e,
      .product_id = 0x02ea,
      .version = 0x0301,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    {
      .name = "DualSense (PS5) - gamepad",
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x0ce6,
      .version = 0x111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, select, start, mode, thumbl,
           * thumbr; note that C and Z don't physically exist */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x7f,
      },
    },
    {
      .name = "DualSense (PS5) v8111 - gamepad",
      .eviocgname = "Sony Interactive Entertainment Wireless Controller",
      .usb_vendor_name = "Sony Interactive Entertainment",
      .usb_product_name = "Wireless Controller",
      /* Same physical device via Bluetooth is 0005:054c:0ce6 v8100
       * and EVIOCGNAME is just "Wireless Controller", otherwise equivalent */
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x0ce6,
      .version = 0x8111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7f,
      },
    },
    {
      .name = "DualShock 4 - gamepad",
      /* EVIOCGNAME is just "Wireless Controller" when seen via Bluetooth */
      .eviocgname = "Sony Interactive Entertainment Wireless Controller",
      .usb_vendor_name = "Sony Interactive Entertainment",
      .usb_product_name = "Wireless Controller",
      /* Same physical device via Bluetooth is 0005:054c:09cc v8100,
       * but otherwise equivalent */
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x09cc,
      .version = 0x8111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC, FF */
      /* Some versions only have 0x0b, SYN, KEY, ABS, like the
       * Bluetooth example below */
      .ev = { 0x1b, 0x00, 0x20 },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7f,
      },
    },
    {
      .name = "DualShock 4 - gamepad via Bluetooth (unknown version)",
      .bus_type = 0x0005,
      .vendor_id = 0x054c,
      .product_id = 0x09cc,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7f,
      },
    },
    {
      .name = "DualShock 4 - touchpad",
      /* EVIOCGNAME is just "Wireless Controller Touchpad" when seen via Bluetooth */
      .eviocgname = "Sony Interactive Entertainment Wireless Controller Touchpad",
      .usb_vendor_name = "Sony Interactive Entertainment",
      .usb_product_name = "Wireless Controller",
      /* Same physical device via Bluetooth is 0005:054c:09cc v8100 and is
       * functionally equivalent. */
      /* DualSense (PS5), 0003:054c:0ce6 v8111, is functionally equivalent.
       * Same physical device via Bluetooth is 0005:054c:0ce6 v8100 and also
       * functionally equivalent. */
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x09cc,
      .version = 0x8111,
      .expected = SDL_UDEV_DEVICE_TOUCHPAD,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, multitouch */
      .abs = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x80, 0x60, 0x02 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* Left mouse button */
          /* 0x100 */ 0x00, 0x00, 0x01, 0x00, ZEROx4,
          /* BTN_TOOL_FINGER and some multitouch stuff */
          /* 0x140 */ 0x20, 0x24, 0x00, 0x00
      },
      /* POINTER, BUTTONPAD */
      .props = { 0x05 },
    },
    {
      .name = "DualShock 4 - accelerometer",
      /* EVIOCGNAME is just "Wireless Controller Motion Sensors" when seen via Bluetooth */
      .eviocgname = "Sony Interactive Entertainment Wireless Controller Motion Sensors",
      .usb_vendor_name = "Sony Interactive Entertainment",
      .usb_product_name = "Wireless Controller",
      /* Same physical device via Bluetooth is 0005:054c:09cc v8100 and is
       * functionally equivalent. */
      /* DualSense (PS5), 0003:054c:0ce6 v8111, is functionally equivalent.
       * Same physical device via Bluetooth is 0005:054c:0ce6 v8100 and also
       * functionally equivalent. */
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x09cc,
      .version = 0x8111,
      .expected = SDL_UDEV_DEVICE_ACCELEROMETER,
      /* SYN, ABS, MSC */
      .ev = { 0x19 },
      /* X, Y, Z, RX, RY, RZ */
      .abs = { 0x3f },
      /* ACCELEROMETER */
      .props = { 0x40 },
    },
    {
      .name = "DualShock 4 via USB dongle",
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x0ba0,
      .version = 0x8111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, ABS, KEY */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7f,
      },
    },
    {
      .name = "DualShock 3 - gamepad",
      .eviocgname = "Sony PLAYSTATION(R)3 Controller",
      .usb_vendor_name = "Sony",
      .usb_product_name = "PLAYSTATION(R)3 Controller",
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x0268,
      .version = 0x8111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC, FF */
      .ev = { 0x1b, 0x00, 0x20 },
      /* X, Y, Z, RX, RY, RZ */
      .abs = { 0x3f },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7f,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* Digital dpad */
          /* 0x200 */ ZEROx4, 0x0f, 0x00, 0x00, 0x00,
      },
      .hid_report_descriptor_length = sizeof (ps3_hid_report_descriptor),
      .hid_report_descriptor = &ps3_hid_report_descriptor[0],
    },
    {
      .name = "DualShock 3 - accelerometer",
      .eviocgname = "Sony PLAYSTATION(R)3 Controller Motion Sensors",
      .usb_vendor_name = "Sony",
      .usb_product_name = "PLAYSTATION(R)3 Controller",
      .bus_type = 0x0003,
      .vendor_id = 0x054c,
      .product_id = 0x0268,
      .expected = SDL_UDEV_DEVICE_ACCELEROMETER,
      /* SYN, ABS */
      .ev = { 0x09 },
      /* X, Y, Z */
      .abs = { 0x07 },
      /* ACCELEROMETER */
      .props = { 0x40 },
      .hid_report_descriptor_length = sizeof (ps3_hid_report_descriptor),
      .hid_report_descriptor = &ps3_hid_report_descriptor[0],
    },
    {
      .name = "Steam Controller - gamepad",
      .bus_type = 0x0003,
      .vendor_id = 0x28de,
      .product_id = 0x1142,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, RX, RY, HAT0X, HAT0Y, HAT2X, HAT2Y */
      .abs = { 0x1b, 0x00, 0x33 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL, THUMBR, joystick THUMB, joystick THUMB2  */
          /* 0x100 */ ZEROx4, 0x06, 0x00, 0xdb, 0x7f,
          /* GEAR_DOWN, GEAR_UP */
          /* 0x140 */ 0x00, 0x00, 0x03, 0x00, ZEROx4,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* Digital dpad */
          /* 0x200 */ ZEROx4, 0x0f, 0x00, 0x00, 0x00,
      },
    },
    {
      /* Present to support lizard mode, even if no Steam Controller
       * is connected */
      .name = "Steam Controller - dongle",
      .bus_type = 0x0003,
      .vendor_id = 0x28de,
      .product_id = 0x1142,
      .expected = (SDL_UDEV_DEVICE_HAS_KEYS
                   | SDL_UDEV_DEVICE_KEYBOARD
                   | SDL_UDEV_DEVICE_MOUSE),
      /* SYN, KEY, REL, MSC, LED, REP */
      .ev = { 0x17, 0x00, 0x12 },
      /* X, Y, mouse wheel, high-res mouse wheel */
      .rel = { 0x03, 0x09 },
      .keys = {
          /* 0x00 */ 0xfe, 0xff, 0xff, 0xff, FFx4,
          /* 0x40 */ 0xff, 0xff, 0xcf, 0x01, 0xdf, 0xff, 0x80, 0xe0,
          /* 0x80 */ ZEROx8,
          /* 0xc0 */ ZEROx8,
          /* 0x100 */ 0x00, 0x00, 0x1f, 0x00, ZEROx4,
      },
    },
    {
      .name = "Guitar Hero for PS3",
      /* SWITCH CO.,LTD. Controller (Dinput) off-brand N64-style USB controller
       * 0003:2563:0575 v0111 is functionally equivalent.
       * https://linux-hardware.org/?id=usb:2563-0575 reports the same IDs as
       * ShenZhen ShanWan Technology ZD-V+ Wired Gaming Controller */
      .bus_type = 0x0003,
      .vendor_id = 0x12ba,
      .product_id = 0x0100,
      .version = 0x0110,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RZ, HAT0X, HAT0Y */
      .abs = { 0x27, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, SELECT, START, MODE */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x1f,
      },
    },
    {
      .name = "G27 Racing Wheel, 0003:046d:c29b v0111",
      .bus_type = 0x0003,
      .vendor_id = 0x046d,
      .product_id = 0xc29b,
      .version = 0x0111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RZ, HAT0X, HAT0Y */
      .abs = { 0x27, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* 16 buttons: TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE, BASE,
           * BASE2..BASE6, unregistered event codes 0x12c-0x12e, DEAD */
          /* 0x100 */ ZEROx4, 0xff, 0xff, 0x00, 0x00,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ ZEROx8,
          /* 0x280 */ ZEROx8,
          /* TRIGGER_HAPPY1..TRIGGER_HAPPY7 */
          /* 0x2c0 */ 0x7f,
      },
    },
    {
      .name = "Logitech Driving Force, 0003:046d:c294 v0100",
      .bus_type = 0x0003,
      .vendor_id = 0x046d,
      .product_id = 0xc294,
      .version = 0x0100,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, RZ, HAT0X, HAT0Y */
      .abs = { 0x23, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* 12 buttons: TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE, BASE,
           * BASE2..BASE6 */
          /* 0x100 */ ZEROx4, 0xff, 0x0f, 0x00, 0x00,
      },
    },
    {
      .name = "Logitech Dual Action",
      .bus_type = 0x0003,
      .vendor_id = 0x046d,
      .product_id = 0xc216,
      .version = 0x0110,
      /* Logitech RumblePad 2 USB, 0003:046d:c218 v0110, is the same
       * except for having force feedback, which we don't use in our
       * heuristic */
      /* Jess Tech GGE909 PC Recoil Pad, 0003:0f30:010b v0110, is the same */
      /* 8BitDo SNES30 via USB, 0003:2dc8:ab20 v0110, is the same;
       * see below for the same physical device via Bluetooth,
       * 0005:2dc8:2840 v0100 */
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RZ, HAT0X, HAT0Y */
      .abs = { 0x27, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* 12 buttons: TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE, BASE,
           * BASE2..BASE6 */
          /* 0x100 */ ZEROx4, 0xff, 0x0f, 0x00, 0x00,
      },
    },
    {
      .name = "8BitDo SNES30 v0100 via Bluetooth",
      .eviocgname = "8Bitdo SNES30 GamePad",
      /* The same physical device via USB, 0003:2dc8:ab20 v0110,
       * is reported differently (above). */
      /* 8BitDo NES30 Pro (aka N30 Pro) via Bluetooth, 0005:2dc8:3820 v0100,
       * is functionally equivalent; but the same physical device via USB,
       * 0003:2dc8:9001 v0111, matches N30 Pro 2 v0111. */
      .bus_type = 0x0005,
      .vendor_id = 0x2dc8,
      .product_id = 0x2840,
      .version = 0x0100,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC */
      .ev = { 0x1b },
      /* XYZ, RZ, GAS, BRAKE, HAT0X, HAT0Y */
      .abs = { 0x27, 0x06, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, SELECT, START, MODE, THUMBL, THUMBR,
           * and an unassigned button code */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0xff,
      },
    },
    {
      .name = "Saitek ST290 Pro flight stick",
      .bus_type = 0x0003,
      .vendor_id = 0x06a3,
      .product_id = 0x0160,   /* 0x0460 seems to be the same */
      .version = 0x0100,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC */
      .ev = { 0x1b },
      /* X, Y, Z, RZ, HAT0X, HAT0Y */
      .abs = { 0x27, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE */
          /* 0x100 */ ZEROx4, 0x3f, 0x00, 0x00, 0x00,
      },
    },
    {
      .name = "Saitek X52 Pro Flight Control System",
      .bus_type = 0x0003,
      .vendor_id = 0x06a3,
      .product_id = 0x0762,
      .version = 0x0111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .ev = { 0x0b },
      /* XYZ, RXYZ, throttle, hat0, MISC, unregistered event code 0x29 */
      .abs = { 0x7f, 0x00, 0x03, 0x00, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* 16 buttons: TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE, BASE,
           * BASE2..BASE6, unregistered event codes 0x12c-0x12e, DEAD */
          /* 0x100 */ ZEROx4, 0xff, 0xff, 0x00, 0x00,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ ZEROx8,
          /* 0x280 */ ZEROx8,
          /* TRIGGER_HAPPY1..TRIGGER_HAPPY23 */
          /* 0x2c0 */ 0xff, 0xff, 0x7f,
      },
    },
    {
      .name = "Logitech Extreme 3D",
      .bus_type = 0x0003,
      .vendor_id = 0x046d,
      .product_id = 0xc215,
      .version = 0x0110,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC */
      .ev = { 0x0b },
      /* X, Y, RZ, throttle, hat 0 */
      .abs = { 0x63, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* 12 buttons: TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE, BASE,
           * BASE2..BASE6 */
          /* 0x100 */ ZEROx4, 0xff, 0x0f, 0x00, 0x00,
      },
    },
    {
      .name = "Hori Real Arcade Pro VX-SA",
      .bus_type = 0x0003,
      .vendor_id = 0x24c6,
      .product_id = 0x5501,
      .version = 0x0533,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, hat 0 */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    {
      /* https://github.com/ValveSoftware/steam-devices/pull/42
       * PS4 mode is functionally equivalent, but with product ID 0x011c
       * and version 0x1101. */
      .name = "Hori Fighting Stick Alpha - PS5 mode",
      .bus_type = 0x0003,   /* USB */
      .vendor_id = 0x0f0d,  /* Hori Co., Ltd. */
      .product_id = 0x0184, /* HORI FIGHTING STICK α (PS5 mode) */
      .version = 0x0111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC */
      .ev = { 0x1b },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x3f,
      },
    },
    {  /* https://github.com/ValveSoftware/steam-devices/pull/42 */
      .name = "Hori Fighting Stick Alpha - PC mode",
      .bus_type = 0x0003,   /* USB */
      .vendor_id = 0x0f0d,  /* Hori Co., Ltd. */
      .product_id = 0x011e, /* HORI FIGHTING STICK α (PC mode) */
      .version = 0x0116,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, FF */
      .ev = { 0x0b, 0x00, 0x20 },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    {  /* https://github.com/ValveSoftware/steam-devices/issues/29 */
      .name = "HORIPAD S for Nintendo",
      .bus_type = 0x0003,   /* USB */
      .vendor_id = 0x0f0d,  /* Hori Co., Ltd. */
      .product_id = 0x00dc, /* HORIPAD S */
      .version = 0x0112,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, FF */
      .ev = { 0x0b, 0x00, 0x20 },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    {
      .name = "Switch Pro Controller via Bluetooth",
      .bus_type = 0x0005,
      .vendor_id = 0x057e,
      .product_id = 0x2009,
      .version = 0x0001,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, RX, RY, hat 0 */
      .abs = { 0x1b, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* 16 buttons: TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE, BASE,
           * BASE2..BASE6, unregistered event codes 0x12c-0x12e, DEAD */
          /* 0x100 */ ZEROx4, 0xff, 0xff, 0x00, 0x00,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ ZEROx8,
          /* 0x280 */ ZEROx8,
          /* TRIGGER_HAPPY1..TRIGGER_HAPPY2 */
          /* 0x2c0 */ 0x03,
      },
    },
    {
      .name = "Switch Pro Controller via Bluetooth (Linux 6.2.11)",
      .eviocgname = "Pro Controller",
      .bus_type = 0x0005,
      .vendor_id = 0x057e,
      .product_id = 0x2009,
      .version = 0x0001,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, RX, RY, hat 0 */
      .abs = { 0x1b, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, SELECT, START, MODE, THUMBL, THUMBR,
           * and an unassigned button code */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0xff,
      },
    },
    {
      .name = "Switch Pro Controller via USB",
      .eviocgname = "Nintendo Co., Ltd. Pro Controller",
      .usb_vendor_name = "Nintendo Co., Ltd.",
      .usb_product_name = "Pro Controller",
      .bus_type = 0x0003,
      .vendor_id = 0x057e,
      .product_id = 0x2009,
      .version = 0x0111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, Z, RZ, HAT0X, HAT0Y */
      .abs = { 0x27, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* 16 buttons: TRIGGER, THUMB, THUMB2, TOP, TOP2, PINKIE, BASE,
           * BASE2..BASE6, unregistered event codes 0x12c-0x12e, DEAD */
          /* 0x100 */ ZEROx4, 0xff, 0xff, 0x00, 0x00,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ ZEROx8,
          /* 0x280 */ ZEROx8,
          /* TRIGGER_HAPPY1..TRIGGER_HAPPY2 */
          /* 0x2c0 */ 0x03,
      },
    },
    { /* https://github.com/ValveSoftware/steam-devices/pull/40 */
      .name = "PDP wired Pro Controller for Switch",
      /* 0003:0e6f:0184 "Performance Designed Products" /
       * "Faceoff Deluxe+ Audio Wired Controller for Nintendo Switch" appears
       * to be functionally equivalent */
      .eviocgname = "PDP CO.,LTD. Faceoff Wired Pro Controller for Nintendo Switch",
      .usb_vendor_name = "PDP CO.,LTD.",
      .usb_product_name = "Faceoff Wired Pro Controller for Nintendo Switch",
      .bus_type = 0x0003,
      .vendor_id = 0x0e6f,
      .product_id = 0x0180,
      .version = 0x0111,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC */
      .ev = { 0x1b },
      /* X, Y, Z, RZ, HAT0X, HAT0Y */
      .abs = { 0x27, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x3f,
      },
    },
    {
      .name = "NES Controller (R) NES-style Joycon from Nintendo Online",
      .eviocgname = "NES Controller (R)",
      /* Joy-Con (L), 0005:057e:2006 v0001, is functionally equivalent.
       * Ordinary Joy-Con (R) and NES-style Joy-Con (L) are assumed to be
       * functionally equivalent as well. */
      .bus_type = 0x0005,   /* Bluetooth-only */
      .vendor_id = 0x057e,
      .product_id = 0x2007,
      .version = 0x0001,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, RX, RY, hat 0 */
      .abs = { 0x1b, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, SELECT, START, MODE, THUMBL, THUMBR,
           * and an unassigned button code */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0xff,
      },
    },
    {
      .name = "Thrustmaster Racing Wheel FFB",
      /* Several devices intended for PS4 are functionally equivalent to this:
       * https://github.com/ValveSoftware/steam-devices/pull/34
       * Mad Catz FightStick TE S+ PS4, 0003:0738:8384:0111 v0111
       * (aka Street Fighter V Arcade FightStick TES+)
       * Mad Catz FightStick TE2+ PS4, 0003:0738:8481 v0100
       * (aka Street Fighter V Arcade FightStick TE2+)
       * Bigben Interactive DAIJA Arcade Stick, 0003:146b:0d09 v0111
       * (aka Nacon Daija PS4 Arcade Stick)
       * Razer Razer Raiju Ultimate Wired, 0003:1532:1009 v0000
       * Razer Razer Raiju Ultimate (Bluetooth), 0005:1532:1009 v0001
       */
      .bus_type = 0x0003,
      .vendor_id = 0x044f,
      .product_id = 0xb66d,
      .version = 0x0110,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .ev = { 0x0b },
      /* XYZ, RXYZ, hat 0 */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x3f,
      },
    },
    {
      .name = "Thrustmaster T.Flight Hotas X",
      .bus_type = 0x0003,
      .vendor_id = 0x044f,
      .product_id = 0xb108,
      .version = 0x0100,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .ev = { 0x0b },
      /* XYZ, RZ, throttle, hat 0 */
      .abs = { 0x67, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* trigger, thumb, thumb2, top, top2, pinkie, base,
           * base2..base6 */
          /* 0x100 */ ZEROx4, 0xff, 0x0f
      },
    },
    {
      .name = "8BitDo N30 Pro via Bluetooth",
      /* This device has also been seen reported with an additional
       * unassigned button code, the same as the SNES30 v0100 via Bluetooth */
      .bus_type = 0x0005,
      .vendor_id = 0x2dc8,
      .product_id = 0x3820,
      .version = 0x0100,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS, MSC */
      .ev = { 0x1b },
      /* XYZ, RZ, gas, brake, hat0 */
      .abs = { 0x27, 0x06, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, select, start, mode, thumbl,
           * thumbr */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x7f,
      },
    },
    {
      .name = "8BitDo N30 Pro 2 v0111",
      .bus_type = 0x0003,
      .vendor_id = 0x2dc8,
      .product_id = 0x9015,
      .version = 0x0111,
      /* 8BitDo NES30 Pro via USB, 0003:2dc8:9001 v0111, is the same;
       * but the same physical device via Bluetooth, 0005:2dc8:3820 v0100,
       * matches 8BitDo SNES30 v0100 via Bluetooth instead (see above). */
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .ev = { 0x0b },
      /* XYZ, RZ, gas, brake, hat0 */
      .abs = { 0x27, 0x06, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, select, start, mode, thumbl,
           * thumbr */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x7f,
      },
    },
    {
      .name = "8BitDo N30 Pro 2 via Bluetooth",
      /* Physically the same device as the one that mimics an Xbox 360
       * USB controller when wired */
      .bus_type = 0x0005,
      .vendor_id = 0x045e,
      .product_id = 0x02e0,
      .version = 0x0903,
      .expected = SDL_UDEV_DEVICE_JOYSTICK | SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY, ABS, MSC, FF */
      .ev = { 0x1b, 0x00, 0x20 },
      /* X, Y, Z, RX, RY, RZ, HAT0X, HAT0Y */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx8,
          /* KEY_MENU */
          /* 0x80 */ 0x00, 0x08, 0x00, 0x00, ZEROx4,
          /* 0xc0 */ ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2 */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x03,
      },
    },
    {
      .name = "Retro Power SNES-style controller, 0003:0079:0011 v0110",
      .bus_type = 0x0003,
      .vendor_id = 0x0079,
      .product_id = 0x0011,
      .version = 0x0110,
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .ev = { 0x0b },
      /* X, Y */
      .abs = { 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* trigger, thumb, thumb2, top, top2, pinkie, base,
           * base2..base4 */
          /* 0x100 */ ZEROx4, 0xff, 0x03, 0x00, 0x00,
      },
    },
    {
      .name = "Google Stadia Controller rev.A",
      .eviocgname = "Google LLC Stadia Controller rev. A",
      .usb_vendor_name = "Google LLC",
      .usb_product_name = "Stadia Controller rev. A",
      /* This data is with USB-C, but the same physical device via Bluetooth,
       * 0005:18d1:9400 v0000, is functionally equivalent other than having
       * EVIOCGNAME = StadiaXXXX-YYYY where XXXX is the last 4 digits of
       * the serial number and YYYY is some other mystery number */
      .bus_type = 0x0003,
      .vendor_id = 0x18d1,
      .product_id = 0x9400,
      .version = 0x0100,
      .expected = SDL_UDEV_DEVICE_JOYSTICK | SDL_UDEV_DEVICE_HAS_KEYS,
      .ev = { 0x0b },
      /* XYZ, RZ, gas, brake, hat0 */
      .abs = { 0x27, 0x06, 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* Volume up/down */
          /* 0x40 */ ZEROx4, 0x00, 0x00, 0x0c, 0x00,
          /* Media play/pause */
          /* 0x80 */ ZEROx4, 0x10, 0x00, 0x00, 0x00,
          /* 0xc0 */ ZEROx8,
          /* ABXY, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ ZEROx8,
          /* 0x280 */ ZEROx8,
          /* TRIGGER_HAPPY 1-4 */
          /* 0x2c0 */ 0x0f, 0x00, 0x00, 0x00, ZEROx4,
      },
    },
    {
      .name = "Microsoft Xbox Series S|X Controller (model 1914) via USB",
      .eviocgname = "Microsoft Xbox Series S|X Controller",
      .usb_vendor_name = "Microsoft",
      .usb_product_name = "Controller",
      /* Physically the same device as 0003:045e:0b13 v0515 below,
       * but some functionality is mapped differently */
      .bus_type = 0x0003,
      .vendor_id = 0x045e,
      .product_id = 0x0b12,
      .version = 0x050f,
      .expected = SDL_UDEV_DEVICE_JOYSTICK | SDL_UDEV_DEVICE_HAS_KEYS,
      .ev = { 0x0b },
      /* X, Y, Z, RX, RY, RZ, hat 0 */
      .abs = { 0x3f, 0x00, 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx8,
          /* Record */
          /* 0x80 */ ZEROx4, 0x80, 0x00, 0x00, 0x00,
          /* 0xc0 */ ZEROx8,
          /* ABXY, TL, TR, SELECT, START, MODE, THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7c,
      },
    },
    {
      .name = "Microsoft Xbox Series S|X Controller (model 1914) via Bluetooth",
      .eviocgname = "Xbox Wireless Controller",
      /* Physically the same device as 0003:045e:0b12 v050f above,
       * but some functionality is mapped differently */
      .bus_type = 0x0005,
      .vendor_id = 0x045e,
      .product_id = 0x0b13,
      .version = 0x0515,
      .expected = SDL_UDEV_DEVICE_JOYSTICK | SDL_UDEV_DEVICE_HAS_KEYS,
      .ev = { 0x0b },
      /* XYZ, RZ, gas, brake, hat0 */
      .abs = { 0x27, 0x06, 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx8,
          /* Record */
          /* 0x80 */ ZEROx4, 0x80, 0x00, 0x00, 0x00,
          /* 0xc0 */ ZEROx8,
          /* ABC, XYZ, TL, TR, TL2, TR2, select, start, mode, thumbl,
           * thumbr */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xff, 0x7f,
      },
    },
    {
      .name = "Wiimote - buttons",
      .eviocgname = "Nintendo Wii Remote",
      .bus_type = 0x0005,
      .vendor_id = 0x057e,
      .product_id = 0x0306,
      .version = 0x0600,
      /* This one is a bit weird because some of the buttons are mapped
       * to the arrow, page up and page down keys, so it's a joystick
       * with a subset of a keyboard attached. */
      /* TODO: Should this be JOYSTICK, or even JOYSTICK|HAS_KEYS? */
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY, FF */
      .ev = { 0x03, 0x00, 0x20 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* left, right, up down */
          /* 0x40 */ ZEROx4, 0x80, 0x16, 0x00, 0x00,
          /* 0x80 */ ZEROx8,
          /* 0xc0 */ ZEROx8,
          /* BTN_1, BTN_2, BTN_A, BTN_B, BTN_MODE */
          /* 0x100 */ 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x10,
          /* 0x140 */ ZEROx8,
          /* next (keyboard page down), previous (keyboard page up) */
          /* 0x180 */ 0x00, 0x00, 0x80, 0x10, ZEROx4,
      },
    },
    {
      .name = "Wiimote - accelerometer",
      .eviocgname = "Nintendo Wii Remote Accelerometer",
      .bus_type = 0x0005,
      .vendor_id = 0x057e,
      .product_id = 0x0306,
      .version = 0x0600,
      .expected = SDL_UDEV_DEVICE_ACCELEROMETER,
      /* SYN, ABS */
      .ev = { 0x09 },
      /* RX, RY, RZ - even though it would more conventionally be X, Y, Z */
      .abs = { 0x38 },
    },
    {
      .name = "Wiimote - Motion Plus gyroscope",
      .eviocgname = "Nintendo Wii Remote Motion Plus",
      /* Note that if we only look at the bus type, vendor, product, version
       * and axes, this is indistinguishable from the accelerometer */
      .bus_type = 0x0005,
      .vendor_id = 0x057e,
      .product_id = 0x0306,
      .version = 0x0600,
      .expected = SDL_UDEV_DEVICE_ACCELEROMETER,
      /* SYN, ABS */
      .ev = { 0x09 },
      /* RX, RY, RZ */
      .abs = { 0x38 },
    },
    {
      .name = "Wiimote - IR positioning",
      .eviocgname = "Nintendo Wii Remote IR",
      .bus_type = 0x0005,
      .vendor_id = 0x057e,
      .product_id = 0x0306,
      .version = 0x0600,
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
      /* SYN, ABS */
      .ev = { 0x09 },
      /* HAT0X, Y to HAT3X, Y */
      .abs = { 0x00, 0x00, 0xff },
    },
    {
      .name = "Wiimote - Nunchuck",
      .eviocgname = "Nintendo Wii Remote Nunchuk",
      .bus_type = 0x0005,
      .vendor_id = 0x057e,
      .product_id = 0x0306,
      .version = 0x0600,
      /* TODO: Should this be JOYSTICK? It has one stick and two buttons */
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* RX, RY, RZ, hat 0 - even though this is an accelerometer, which
       * would more conventionally be X, Y, Z, and a left joystick, which
       * would more conventionally be X, Y */
      .abs = { 0x38, 0x00, 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
         /* C and Z buttons */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0x24, 0x00,
      },
    },
    {
      .name = "Wiimote - Classic Controller",
      .eviocgname = "Nintendo Wii Remote Classic Controller",
      /* TODO: Should this be JOYSTICK, or maybe JOYSTICK|HAS_KEYS?
       * It's unusual in the same ways as the Wiimote */
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* Hat 1-3 X and Y */
      .abs = { 0x00, 0x00, 0xfc },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* left, right, up down */
          /* 0x40 */ ZEROx4, 0x80, 0x16, 0x00, 0x00,
          /* 0x80 */ ZEROx8,
          /* 0xc0 */ ZEROx8,
          /* A, B, X, Y, MODE, TL, TL2, TR, TR2 */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x13,
          /* 0x140 */ ZEROx8,
          /* next (keyboard page down), previous (keyboard page up) */
          /* 0x180 */ 0x00, 0x00, 0x80, 0x10, ZEROx4,
      },
    },
    {
      /* Flags guessed from kernel source code, not confirmed with real
       * hardware */
      .name = "Wiimote - Balance Board",
      /* TODO: Should this be JOYSTICK? */
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* Hat 0-1 */
      .abs = { 0x00, 0x00, 0x0f },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* BTN_A */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0x01, 0x00,
      },
    },
    {
      /* Flags guessed from kernel source code, not confirmed with real
       * hardware */
      .name = "Wiimote - Wii U Pro Controller",
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, RX, RY */
      .abs = { 0x1b },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* A, B, X, Y, TL, TR, TL2, TR2, SELECT, START, MODE,
           * THUMBL, THUMBR */
          /* 0x100 */ ZEROx4, 0x00, 0x00, 0xdb, 0x7f,
          /* 0x140 */ ZEROx8,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ ZEROx8,
          /* Digital dpad */
          /* 0x200 */ ZEROx4, 0x0f, 0x00, 0x00, 0x00,
      },
    },
    {
      .name = "Synaptics TM3381-002 (Thinkpad X280 trackpad)",
      .eviocgname = "Synaptics TM3381-002",
      .bus_type = 0x001d,   /* BUS_RMI */
      .vendor_id = 0x06cb,
      .product_id = 0x0000,
      .version = 0x0000,
      .expected = SDL_UDEV_DEVICE_TOUCHPAD,
      /* SYN, KEY, ABS */
      .ev = { 0x0b },
      /* X, Y, pressure, multitouch */
      .abs = { 0x03, 0x00, 0x00, 0x01, 0x00, 0x80, 0xf3, 0x06 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* Left mouse button */
          /* 0x100 */ 0x00, 0x00, 0x01, 0x00, ZEROx4,
          /* BTN_TOOL_FINGER and some multitouch gestures */
          /* 0x140 */ 0x20, 0xe5
      },
      /* POINTER, BUTTONPAD */
      .props = { 0x05 },
    },
    {
      .name = "DELL08AF:00 (Dell XPS laptop touchpad)",
      .bus_type = 0x18,
      .vendor_id = 0x6cb,
      .product_id = 0x76af,
      .version = 0x100,
      .ev = { 0x0b },
      .expected = SDL_UDEV_DEVICE_TOUCHPAD,
      /* X, Y, multitouch */
      .abs = { 0x03, 0x00, 0x00, 0x00, 0x00, 0x80, 0xe0, 0x02 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* Left mouse button */
          /* 0x100 */ 0x00, 0x00, 0x01, 0x00, ZEROx4,
          /* BTN_TOOL_FINGER and some multitouch gestures */
          /* 0x140 */ 0x20, 0xe5
      },
      /* POINTER, BUTTONPAD */
      .props = { 0x05 },
    },
    {
      .name = "TPPS/2 Elan TrackPoint (Thinkpad X280)",
      .eviocgname = "TPPS/2 Elan TrackPoint",
      .bus_type = 0x0011,   /* BUS_I8042 */
      .vendor_id = 0x0002,
      .product_id = 0x000a,
      .version = 0x0000,
      .expected = SDL_UDEV_DEVICE_MOUSE,
      /* SYN, KEY, REL */
      .ev = { 0x07 },
      /* X, Y */
      .rel = { 0x03 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* Left, middle, right mouse buttons */
          /* 0x100 */ 0x00, 0x00, 0x07,
      },
      /* POINTER, POINTING_STICK */
      .props = { 0x21 },
    },
    {
      .name = "Thinkpad ACPI buttons",
      .eviocgname = "ThinkPad Extra Buttons",
      .bus_type = 0x0019,
      .vendor_id = 0x17aa,
      .product_id = 0x5054,
      .version = 0x4101,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY, MSC, SW */
      .ev = { 0x33 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx4, 0x00, 0x00, 0x0e, 0x01,
          /* 0x80 */ 0x00, 0x50, 0x11, 0x51, 0x00, 0x28, 0x00, 0xc0,
          /* 0xc0 */ 0x04, 0x20, 0x10, 0x02, 0x1b, 0x70, 0x01, 0x00,
          /* 0x100 */ ZEROx8,
          /* 0x140 */ ZEROx4, 0x00, 0x00, 0x50, 0x00,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ 0x00, 0x00, 0x04, 0x18, ZEROx4,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ 0x40, 0x00, 0x01, 0x00, ZEROx4,
      },
    },
    {
      .name = "Thinkpad ACPI buttons (Linux 6.1)",
      .eviocgname = "ThinkPad Extra Buttons",
      .bus_type = 0x0019,
      .vendor_id = 0x17aa,
      .product_id = 0x5054,
      .version = 0x4101,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY, MSC, SW */
      .ev = { 0x33 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx4, 0x00, 0x00, 0x0e, 0x01,
          /* 0x80 */ 0x00, 0x50, 0x11, 0x51, 0x00, 0x28, 0x00, 0xc0,
          /* 0xc0 */ 0x04, 0x20, 0x10, 0x02, 0x1b, 0x70, 0x01, 0x00,
          /* 0x100 */ ZEROx8,
          /* 0x140 */ ZEROx4, 0x00, 0x00, 0x50, 0x00,
          /* 0x180 */ ZEROx4, 0x00, 0x00, 0x00, 0x70,
          /* 0x1c0 */ 0x00, 0x00, 0x04, 0x18, 0x20, 0x00, 0x00, 0x00,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ ZEROx8,
      },
    },
    {
      .name = "PC speaker",
      .eviocgname = "PC Speaker",
      .bus_type = 0x0010,   /* BUS_ISA */
      .vendor_id = 0x001f,
      .product_id = 0x0001,
      .version = 0x0100,
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
      /* SYN, SND */
      .ev = { 0x01, 0x00, 0x04 },
    },
    {
      .name = "HDA Digital PCBeep",
      .eviocgname = "HDA Digital PCBeep",
      .bus_type = 0x0001,
      .vendor_id = 0x10ec,
      .product_id = 0x0257,
      .version = 0x0001,
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
      /* SYN, SND */
      .ev = { 0x01, 0x00, 0x04 },
    },
    {
      .name = "ALSA headphone detection, etc.",
      .eviocgname = "HDA Intel PCH Mic",
      /* HDA Intel PCH Headphone is functionally equivalent */
      /* HDA Intel PCH HDMI/DP,pcm=3 is functionally equivalent */
      /* HDA Intel PCH HDMI/DP,pcm=7 is functionally equivalent */
      /* HDA Intel PCH HDMI/DP,pcm=8 is functionally equivalent */
      .bus_type = 0x0000,
      .vendor_id = 0x0000,
      .product_id = 0x0000,
      .version = 0x0000,
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
      /* SYN, SW */
      .ev = { 0x21 },
    },
    {
      /* Assumed to be a reasonably typical i8042 (PC AT) keyboard */
      .name = "Thinkpad T520 and X280 keyboards",
      .eviocgname = "AT Translated Set 2 keyboard",
      .bus_type = 0x0011,   /* BUS_I8042 */
      .vendor_id = 0x0001,
      .product_id = 0x0001,
      .version = 0xab54,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS | SDL_UDEV_DEVICE_KEYBOARD,
      /* SYN, KEY, MSC, LED, REP */
      .ev = { 0x13, 0x00, 0x12 },
      .keys = {
          /* 0x00 */ 0xfe, 0xff, 0xff, 0xff, FFx4,
          /* 0x40 */ 0xff, 0xff, 0xef, 0xff, 0xdf, 0xff, 0xff, 0xfe,
          /* 0x80 */ 0x01, 0xd0, 0x00, 0xf8, 0x78, 0x30, 0x80, 0x03,
          /* 0xc0 */ 0x00, 0x00, 0x00, 0x02, 0x04, 0x00, 0x00, 0x00,
      },
    },
    {
      .name = "Thinkpad X280 sleep button",
      .eviocgname = "Sleep Button",
      .bus_type = 0x0019,   /* BUS_HOST */
      .vendor_id = 0x0000,
      .product_id = 0x0003,
      .version = 0x0000,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY */
      .ev = { 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx8,
          /* KEY_SLEEP */
          /* 0x80 */ 0x00, 0x40,
      },
    },
    {
      .name = "Thinkpad X280 lid switch",
      .eviocgname = "Lid Switch",
      .bus_type = 0x0019,   /* BUS_HOST */
      .vendor_id = 0x0000,
      .product_id = 0x0005,
      .version = 0x0000,
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
      /* SYN, SW */
      .ev = { 0x21 },
    },
    {
      .name = "Thinkpad X280 power button",
      .eviocgname = "Power Button",
      .bus_type = 0x0019,   /* BUS_HOST */
      .vendor_id = 0x0000,
      .product_id = 0x0001,
      .version = 0x0000,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY */
      .ev = { 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* KEY_POWER */
          /* 0x40 */ ZEROx4, 0x00, 0x00, 0x10, 0x00,
      },
    },
    {
      .name = "Thinkpad X280 video bus",
      .eviocgname = "Video Bus",
      .bus_type = 0x0019,   /* BUS_HOST */
      .vendor_id = 0x0000,
      .product_id = 0x0006,
      .version = 0x0000,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY */
      .ev = { 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx8,
          /* 0x80 */ ZEROx8,
          /* brightness control, video mode, display off */
          /* 0xc0 */ ZEROx4, 0x0b, 0x00, 0x3e, 0x00,
      },
    },
    {
      .name = "Thinkpad X280 webcam",
      .eviocgname = "Integrated Camera: Integrated C",
      .usb_vendor_name = "Chicony Electronics Co.,Ltd.",
      .usb_product_name = "Integrated Camera",
      .bus_type = 0x0003,
      .vendor_id = 0x04f2,
      .product_id = 0xb604,
      .version = 0x0027,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY */
      .ev = { 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx8,
          /* 0x80 */ ZEROx8,
          /* KEY_CAMERA */
          /* 0xc0 */ 0x00, 0x00, 0x10, 0x00, ZEROx4,
      },
    },
    {
      .name = "Thinkpad X280 extra buttons",
      .bus_type = 0x0019,   /* BUS_HOST */
      .vendor_id = 0x17aa,
      .product_id = 0x5054,
      .version = 0x4101,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY */
      .ev = { 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx4, 0x00, 0x00, 0x0e, 0x01,
          /* 0x80 */ 0x00, 0x50, 0x11, 0x51, 0x00, 0x28, 0x00, 0xc0,
          /* 0xc0 */ 0x04, 0x20, 0x10, 0x02, 0x1b, 0x70, 0x01, 0x00,
          /* 0x100 */ ZEROx8,
          /* 0x140 */ ZEROx4, 0x00, 0x00, 0x50, 0x00,
          /* 0x180 */ ZEROx8,
          /* 0x1c0 */ 0x00, 0x00, 0x04, 0x18, ZEROx4,
          /* 0x200 */ ZEROx8,
          /* 0x240 */ 0x40, 0x00, 0x01, 0x00, ZEROx4,
      },
    },
    {
      .name = "Thinkpad USB keyboard with Trackpoint - keyboard",
      .eviocgname = "Lite-On Technology Corp. ThinkPad USB Keyboard with TrackPoint",
      .usb_vendor_name = "Lite-On Technology Corp.",
      .usb_product_name = "ThinkPad USB Keyboard with TrackPoint",
      .bus_type = 0x0003,
      .vendor_id = 0x17ef,
      .product_id = 0x6009,
      .expected = SDL_UDEV_DEVICE_HAS_KEYS | SDL_UDEV_DEVICE_KEYBOARD,
      /* SYN, KEY, MSC, LED, REP */
      .ev = { 0x13, 0x00, 0x12 },
      .keys = {
          /* 0x00 */ 0xfe, 0xff, 0xff, 0xff, FFx4,
          /* 0x40 */ 0xff, 0xff, 0xef, 0xff, 0xdf, 0xff, 0xbe, 0xfe,
          /* 0x80 */ 0xff, 0x57, 0x40, 0xc1, 0x7a, 0x20, 0x9f, 0xff,
          /* 0xc0 */ 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
      },
      .hid_report_descriptor_length = sizeof (thinkpad_usb_keyboard_hid_report_descriptor),
      .hid_report_descriptor = &thinkpad_usb_keyboard_hid_report_descriptor[0],
    },
    {
      .name = "Thinkpad USB keyboard with Trackpoint - Trackpoint",
      .eviocgname = "Lite-On Technology Corp. ThinkPad USB Keyboard with TrackPoint",
      .usb_vendor_name = "Lite-On Technology Corp.",
      .usb_product_name = "ThinkPad USB Keyboard with TrackPoint",
      .bus_type = 0x0003,
      .vendor_id = 0x17ef,
      .product_id = 0x6009,
      .version = 0x0110,
      /* For some reason the special keys like mute and wlan toggle
       * show up here instead of, or in addition to, as part of
       * the keyboard - so both udev and SDL report this as having keys too. */
      .expected = SDL_UDEV_DEVICE_MOUSE | SDL_UDEV_DEVICE_HAS_KEYS,
      /* SYN, KEY, REL, MSC, LED */
      .ev = { 0x17, 0x00, 0x02 },
      /* X, Y */
      .rel = { 0x03 },
      .keys = {
          /* 0x00 */ ZEROx8,
          /* 0x40 */ ZEROx4, 0x00, 0x00, 0x1e, 0x00,
          /* 0x80 */ 0x00, 0xcc, 0x11, 0x01, 0x78, 0x40, 0x00, 0xc0,
          /* 0xc0 */ 0x00, 0x20, 0x10, 0x00, 0x0b, 0x50, 0x00, 0x00,
          /* Mouse buttons: left, right, middle, "task" */
          /* 0x100 */ 0x00, 0x00, 0x87, 0x68, ZEROx4,
          /* 0x140 */ ZEROx4, 0x00, 0x00, 0x10, 0x00,
          /* 0x180 */ ZEROx4, 0x00, 0x00, 0x40, 0x00,
      },
      .hid_report_descriptor_length = sizeof (thinkpad_usb_trackpoint_hid_report_descriptor),
      .hid_report_descriptor = &thinkpad_usb_trackpoint_hid_report_descriptor[0],
    },
    { /* https://github.com/ValveSoftware/Proton/issues/5126 */
      .name = "Smarty Co. VRS DirectForce Pro Pedals",
      .bus_type = 0x0003,
      .vendor_id = 0x0483,  /* STMicroelectronics */
      .product_id = 0xa3be, /* VRS DirectForce Pro Pedals */
      .version = 0x0111,
      /* TODO: Ideally we would identify this as a joystick, but there
       * isn't currently enough information to do that without a table
       * of known devices. */
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .todo = "https://github.com/ValveSoftware/Proton/issues/5126",
      /* SYN, ABS */
      .ev = { 0x09 },
      /* X, Y, Z */
      .abs = { 0x07 },
    },
    { /* https://github.com/ValveSoftware/Proton/issues/5126 */
      .name = "Heusinkveld Heusinkveld Sim Pedals Ultimate",
      .bus_type = 0x0003,
      .vendor_id = 0x30b7,  /* Heusinkveld Engineering */
      .product_id = 0x1003, /* Heusinkveld Sim Pedals Ultimate */
      .version = 0x0000,
      /* TODO: Ideally we would identify this as a joystick, but there
       * isn't currently enough information to do that without a table
       * of known devices. */
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .todo = "https://github.com/ValveSoftware/Proton/issues/5126",
      /* SYN, ABS */
      .ev = { 0x09 },
      /* RX, RY, RZ */
      .abs = { 0x38 },
    },
    { /* https://github.com/ValveSoftware/Proton/issues/5126 */
      .name = "Vitaly [mega_mozg] Naidentsev ODDOR-handbrake",
      .bus_type = 0x0003,
      .vendor_id = 0x0000,
      .product_id = 0x0000,
      .version = 0x0001,
      /* TODO: Ideally we would identify this as a joystick by it having
       * the joystick-specific THROTTLE axis and TRIGGER/THUMB buttons */
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .todo = "https://github.com/ValveSoftware/Proton/issues/5126",
      /* SYN, KEY, ABS, MSC */
      .ev = { 0x1b },
      /* THROTTLE only */
      .abs = { 0x40 },
      .keys = {
          /* 0x00-0xff */ ZEROx8, ZEROx8, ZEROx8, ZEROx8,
          /* TRIGGER = 0x120, THUMB = 0x121 */
          /* 0x100 */ ZEROx4, 0x03, 0x00, 0x00, 0x00,
      },
    },
    { /* https://github.com/ValveSoftware/Proton/issues/5126 */
      .name = "Leo Bodnar Logitech® G25 Pedals",
      .bus_type = 0x0003,
      .vendor_id = 0x1dd2,  /* Leo Bodnar Electronics Ltd */
      .product_id = 0x100c,
      .version = 0x0110,
      /* TODO: Ideally we would identify this as a joystick, but there
       * isn't currently enough information to do that without a table
       * of known devices. */
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .todo = "https://github.com/ValveSoftware/Proton/issues/5126",
      /* SYN, ABS */
      .ev = { 0x09 },
      /* RX, RY, RZ */
      .abs = { 0x38 },
    },
    { /* https://github.com/ValveSoftware/Proton/issues/5126 */
      .name = "FANATEC ClubSport USB Handbrake",
      .bus_type = 0x0003,
      .vendor_id = 0x0eb7,
      .product_id = 0x1a93,
      .version = 0x0111,
      /* TODO: Ideally we would identify this as a joystick, but there
       * isn't currently enough information to do that without a table
       * of known devices. */
      .expected = SDL_UDEV_DEVICE_JOYSTICK,
      .todo = "https://github.com/ValveSoftware/Proton/issues/5126",
      /* SYN, ABS */
      .ev = { 0x09 },
      /* X only */
      .abs = { 0x01 },
      .hid_report_descriptor_length = sizeof (fanatec_handbrake_hid_report_descriptor),
      .hid_report_descriptor = &fanatec_handbrake_hid_report_descriptor[0],
    },
    { /* Artificial test data, not a real device */
      .name = "Fake accelerometer with fewer than usual axes reported",
      .expected = SDL_UDEV_DEVICE_ACCELEROMETER,
      /* SYN, ABS */
      .ev = { 0x09 },
      /* X only */
      .abs = { 0x01 },
      /* ACCELEROMETER */
      .props = { 0x40 },
    },
    { /* Artificial test data, not a real device */
      .name = "Fake pointing stick with no buttons",
      .expected = SDL_UDEV_DEVICE_MOUSE,
      /* SYN, REL */
      .ev = { 0x05 },
      /* X,Y */
      .rel = { 0x03 },
      /* POINTER, POINTING_STICK */
      .props = { 0x21 },
    },
    { /* Artificial test data, not a real device */
      .name = "Fake buttonpad",
      .expected = SDL_UDEV_DEVICE_TOUCHPAD,
      /* SYN, ABS */
      .ev = { 0x09 },
      /* X,Y */
      .abs = { 0x03 },
      /* POINTER, BUTTONPAD */
      .props = { 0x05 },
    },
    {
      .name = "No information",
      .expected = SDL_UDEV_DEVICE_UNKNOWN,
    }
};
/* *INDENT-ON* */ /* clang-format on */

/* The Linux kernel provides capability info in EVIOCGBIT and in /sys
 * as an array of unsigned long in native byte order, rather than an array
 * of bytes, an array of native-endian 32-bit words or an array of
 * native-endian 64-bit words like you might have reasonably expected.
 * The order of words in the array is always lowest-valued first: for
 * instance, the first unsigned long in abs[] contains the bit representing
 * absolute axis 0 (ABS_X).
 *
 * The constant arrays above provide test data in little-endian, because
 * that's the easiest representation for hard-coding into a test like this.
 * On a big-endian platform we need to byteswap it, one unsigned long at a
 * time, to match what the kernel would produce. This requires us to choose
 * an appropriate byteswapping function for the architecture's word size. */
SDL_COMPILE_TIME_ASSERT(sizeof_long, sizeof(unsigned long) == 4 || sizeof(unsigned long) == 8);
#define SwapLongLE(X) \
    ((sizeof(unsigned long) == 4) ? SDL_SwapLE32(X) : SDL_SwapLE64(X))

static int
run_test(void)
{
    int success = 1;
    size_t i;

    for (i = 0; i < SDL_arraysize(guess_tests); i++) {
        const GuessTest *t = &guess_tests[i];
        size_t j;
        int actual;
        struct
        {
            unsigned long props[NBITS(INPUT_PROP_MAX)];
            unsigned long ev[NBITS(EV_MAX)];
            unsigned long abs[NBITS(ABS_MAX)];
            unsigned long keys[NBITS(KEY_MAX)];
            unsigned long rel[NBITS(REL_MAX)];
        } caps;

        printf("%s...\n", t->name);

        SDL_memset(&caps, '\0', sizeof(caps));
        SDL_memcpy(caps.props, t->props, sizeof(t->props));
        SDL_memcpy(caps.ev, t->ev, sizeof(t->ev));
        SDL_memcpy(caps.keys, t->keys, sizeof(t->keys));
        SDL_memcpy(caps.abs, t->abs, sizeof(t->abs));
        SDL_memcpy(caps.rel, t->rel, sizeof(t->rel));

        for (j = 0; j < SDL_arraysize(caps.props); j++) {
            caps.props[j] = SwapLongLE(caps.props[j]);
        }

        for (j = 0; j < SDL_arraysize(caps.ev); j++) {
            caps.ev[j] = SwapLongLE(caps.ev[j]);
        }

        for (j = 0; j < SDL_arraysize(caps.keys); j++) {
            caps.keys[j] = SwapLongLE(caps.keys[j]);
        }

        for (j = 0; j < SDL_arraysize(caps.abs); j++) {
            caps.abs[j] = SwapLongLE(caps.abs[j]);
        }

        for (j = 0; j < SDL_arraysize(caps.rel); j++) {
            caps.rel[j] = SwapLongLE(caps.rel[j]);
        }

        actual = SDL_EVDEV_GuessDeviceClass(caps.props, caps.ev, caps.abs,
                                            caps.keys, caps.rel);

        if (actual == t->expected) {
            printf("\tOK\n");
        } else {
            printf("\tExpected 0x%08x\n", t->expected);

            for (j = 0; device_classes[j].code != 0; j++) {
                if (t->expected & device_classes[j].code) {
                    printf("\t\t%s\n", device_classes[j].name);
                }
            }

            printf("\tGot      0x%08x\n", actual);

            for (j = 0; device_classes[j].code != 0; j++) {
                if (actual & device_classes[j].code) {
                    printf("\t\t%s\n", device_classes[j].name);
                }
            }

            if (t->todo) {
                printf("\tKnown issue, ignoring: %s\n", t->todo);
            } else {
                printf("\tFailed\n");
                success = 0;
            }
        }
    }

    return success;
}

#else /* !(HAVE_LIBUDEV_H || defined(SDL_JOYSTICK_LINUX)) */

static int
run_test(void)
{
    printf("SDL compiled without evdev capability check.\n");
    return 1;
}

#endif

int main(int argc, char *argv[])
{
    int result;
    SDLTest_CommonState *state;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, 0);
    if (!state) {
        return 1;
    }

    /* Parse commandline */
    if (!SDLTest_CommonDefaultArgs(state, argc, argv)) {
        return 1;
    }

    result = run_test() ? 0 : 1;

    SDLTest_CommonDestroyState(state);
    return result;
}
