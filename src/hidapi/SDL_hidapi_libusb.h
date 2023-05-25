/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifndef __FreeBSD__
/* this is awkwardly inlined, so we need to re-implement it here
 * so we can override the libusb_control_transfer call */
static int SDL_libusb_get_string_descriptor(libusb_device_handle *dev,
                                 uint8_t descriptor_index, uint16_t lang_id,
                                 unsigned char *data, int length)
{
    return libusb_control_transfer(dev, LIBUSB_ENDPOINT_IN | 0x0, LIBUSB_REQUEST_GET_DESCRIPTOR, (LIBUSB_DT_STRING << 8) | descriptor_index, lang_id,
                                   data, (uint16_t)length, 1000); /* Endpoint 0 IN */
}
#define libusb_get_string_descriptor SDL_libusb_get_string_descriptor
#endif /* __FreeBSD__ */

#undef HIDAPI_H__
#include "libusb/hid.c"
