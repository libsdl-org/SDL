/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_report_descriptor_h_
#define SDL_report_descriptor_h_

typedef struct
{
	Uint8 report_id;
	Uint32 usage;
	int bit_offset;
	int bit_size;
} DescriptorInputField;

typedef struct
{
	Uint32 usage;
	Uint8  type;         /* collection type byte: 0=Physical, 1=Application,
	                        2=Logical, 3=Report, 4=NamedArray, ... */
} DescriptorCollection;

typedef struct
{
	int field_count;
	DescriptorInputField *fields;
	int collection_count;
	DescriptorCollection *collections;
} SDL_ReportDescriptor;

extern SDL_ReportDescriptor *SDL_ParseReportDescriptor(const Uint8 *descriptor, size_t descriptor_size);
extern bool SDL_DescriptorHasUsage(SDL_ReportDescriptor *descriptor, Uint16 usage_page, Uint16 usage);
extern bool SDL_DescriptorHasCollectionUsage(SDL_ReportDescriptor *descriptor, Uint16 usage_page, Uint16 usage, Uint8 type);
extern void SDL_DestroyDescriptor(SDL_ReportDescriptor *descriptor);
extern bool SDL_ReadReportData(const Uint8 *data, size_t size, int bit_offset, int bit_size, Uint32 *value);

#endif // SDL_report_descriptor_h_
