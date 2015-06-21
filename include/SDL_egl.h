/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

/**
 *  \file SDL_egl.h
 *
 *  This is a simple file to encapsulate the EGL API headers.
 */
#ifndef _MSC_VER

#include <EGL/egl.h>
#include <EGL/eglext.h>

#else /* _MSC_VER */

/* EGL headers for Visual Studio */

#ifndef __khrplatform_h_
#define __khrplatform_h_

/*
** Copyright (c) 2008-2009 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/* Khronos platform-specific types and definitions.
*
* $Revision: 23298 $ on $Date: 2013-09-30 17:07:13 -0700 (Mon, 30 Sep 2013) $
*
* Adopters may modify this file to suit their platform. Adopters are
* encouraged to submit platform specific modifications to the Khronos
* group so that they can be included in future versions of this file.
* Please submit changes by sending them to the public Khronos Bugzilla
* (http://khronos.org/bugzilla) by filing a bug against product
* "Khronos (general)" component "Registry".
*
* A predefined template which fills in some of the bug fields can be
* reached using http://tinyurl.com/khrplatform-h-bugreport, but you
* must create a Bugzilla login first.
*
*
* See the Implementer's Guidelines for information about where this file
* should be located on your system and for more details of its use:
*    http://www.khronos.org/registry/implementers_guide.pdf
*
* This file should be included as
*        #include <KHR/khrplatform.h>
* by Khronos client API header files that use its types and defines.
*
* The types in khrplatform.h should only be used to define API-specific types.
*
* Types defined in khrplatform.h:
*    khronos_int8_t              signed   8  bit
*    khronos_uint8_t             unsigned 8  bit
*    khronos_int16_t             signed   16 bit
*    khronos_uint16_t            unsigned 16 bit
*    khronos_int32_t             signed   32 bit
*    khronos_uint32_t            unsigned 32 bit
*    khronos_int64_t             signed   64 bit
*    khronos_uint64_t            unsigned 64 bit
*    khronos_intptr_t            signed   same number of bits as a pointer
*    khronos_uintptr_t           unsigned same number of bits as a pointer
*    khronos_ssize_t             signed   size
*    khronos_usize_t             unsigned size
*    khronos_float_t             signed   32 bit floating point
*    khronos_time_ns_t           unsigned 64 bit time in nanoseconds
*    khronos_utime_nanoseconds_t unsigned time interval or absolute time in
*                                         nanoseconds
*    khronos_stime_nanoseconds_t signed time interval in nanoseconds
*    khronos_boolean_enum_t      enumerated boolean type. This should
*      only be used as a base type when a client API's boolean type is
*      an enum. Client APIs which use an integer or other type for
*      booleans cannot use this as the base type for their boolean.
*
* Tokens defined in khrplatform.h:
*
*    KHRONOS_FALSE, KHRONOS_TRUE Enumerated boolean false/true values.
*
*    KHRONOS_SUPPORT_INT64 is 1 if 64 bit integers are supported; otherwise 0.
*    KHRONOS_SUPPORT_FLOAT is 1 if floats are supported; otherwise 0.
*
* Calling convention macros defined in this file:
*    KHRONOS_APICALL
*    KHRONOS_APIENTRY
*    KHRONOS_APIATTRIBUTES
*
* These may be used in function prototypes as:
*
*      KHRONOS_APICALL void KHRONOS_APIENTRY funcname(
*                                  int arg1,
*                                  int arg2) KHRONOS_APIATTRIBUTES;
*/

/*-------------------------------------------------------------------------
* Definition of KHRONOS_APICALL
*-------------------------------------------------------------------------
* This precedes the return type of the function in the function prototype.
*/
#if defined(_WIN32) && !defined(__SCITECH_SNAP__)
#   define KHRONOS_APICALL __declspec(dllimport)
#elif defined (__SYMBIAN32__)
#   define KHRONOS_APICALL IMPORT_C
#else
#   define KHRONOS_APICALL
#endif

/*-------------------------------------------------------------------------
* Definition of KHRONOS_APIENTRY
*-------------------------------------------------------------------------
* This follows the return type of the function  and precedes the function
* name in the function prototype.
*/
#if defined(_WIN32) && !defined(_WIN32_WCE) && !defined(__SCITECH_SNAP__)
/* Win32 but not WinCE */
#   define KHRONOS_APIENTRY __stdcall
#else
#   define KHRONOS_APIENTRY
#endif

/*-------------------------------------------------------------------------
* Definition of KHRONOS_APIATTRIBUTES
*-------------------------------------------------------------------------
* This follows the closing parenthesis of the function prototype arguments.
*/
#if defined (__ARMCC_2__)
#define KHRONOS_APIATTRIBUTES __softfp
#else
#define KHRONOS_APIATTRIBUTES
#endif

/*-------------------------------------------------------------------------
* basic type definitions
*-----------------------------------------------------------------------*/
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || defined(__GNUC__) || defined(__SCO__) || defined(__USLC__)


/*
* Using <stdint.h>
*/
#include <stdint.h>
typedef int32_t                 khronos_int32_t;
typedef uint32_t                khronos_uint32_t;
typedef int64_t                 khronos_int64_t;
typedef uint64_t                khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#elif defined(__VMS ) || defined(__sgi)

/*
* Using <inttypes.h>
*/
#include <inttypes.h>
typedef int32_t                 khronos_int32_t;
typedef uint32_t                khronos_uint32_t;
typedef int64_t                 khronos_int64_t;
typedef uint64_t                khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#elif defined(_WIN32) && !defined(__SCITECH_SNAP__)

/*
* Win32
*/
typedef __int32                 khronos_int32_t;
typedef unsigned __int32        khronos_uint32_t;
typedef __int64                 khronos_int64_t;
typedef unsigned __int64        khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#elif defined(__sun__) || defined(__digital__)

/*
* Sun or Digital
*/
typedef int                     khronos_int32_t;
typedef unsigned int            khronos_uint32_t;
#if defined(__arch64__) || defined(_LP64)
typedef long int                khronos_int64_t;
typedef unsigned long int       khronos_uint64_t;
#else
typedef long long int           khronos_int64_t;
typedef unsigned long long int  khronos_uint64_t;
#endif /* __arch64__ */
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#elif 0

/*
* Hypothetical platform with no float or int64 support
*/
typedef int                     khronos_int32_t;
typedef unsigned int            khronos_uint32_t;
#define KHRONOS_SUPPORT_INT64   0
#define KHRONOS_SUPPORT_FLOAT   0

#else

/*
* Generic fallback
*/
#include <stdint.h>
typedef int32_t                 khronos_int32_t;
typedef uint32_t                khronos_uint32_t;
typedef int64_t                 khronos_int64_t;
typedef uint64_t                khronos_uint64_t;
#define KHRONOS_SUPPORT_INT64   1
#define KHRONOS_SUPPORT_FLOAT   1

#endif


/*
* Types that are (so far) the same on all platforms
*/
typedef signed   char          khronos_int8_t;
typedef unsigned char          khronos_uint8_t;
typedef signed   short int     khronos_int16_t;
typedef unsigned short int     khronos_uint16_t;

/*
* Types that differ between LLP64 and LP64 architectures - in LLP64,
* pointers are 64 bits, but 'long' is still 32 bits. Win64 appears
* to be the only LLP64 architecture in current use.
*/
#ifdef _WIN64
typedef signed   long long int khronos_intptr_t;
typedef unsigned long long int khronos_uintptr_t;
typedef signed   long long int khronos_ssize_t;
typedef unsigned long long int khronos_usize_t;
#else
typedef signed   long  int     khronos_intptr_t;
typedef unsigned long  int     khronos_uintptr_t;
typedef signed   long  int     khronos_ssize_t;
typedef unsigned long  int     khronos_usize_t;
#endif

#if KHRONOS_SUPPORT_FLOAT
/*
* Float type
*/
typedef          float         khronos_float_t;
#endif

#if KHRONOS_SUPPORT_INT64
/* Time types
*
* These types can be used to represent a time interval in nanoseconds or
* an absolute Unadjusted System Time.  Unadjusted System Time is the number
* of nanoseconds since some arbitrary system event (e.g. since the last
* time the system booted).  The Unadjusted System Time is an unsigned
* 64 bit value that wraps back to 0 every 584 years.  Time intervals
* may be either signed or unsigned.
*/
typedef khronos_uint64_t       khronos_utime_nanoseconds_t;
typedef khronos_int64_t        khronos_stime_nanoseconds_t;
#endif

/*
* Dummy value used to pad enum types to 32 bits.
*/
#ifndef KHRONOS_MAX_ENUM
#define KHRONOS_MAX_ENUM 0x7FFFFFFF
#endif

/*
* Enumerated boolean type
*
* Values other than zero should be considered to be true.  Therefore
* comparisons should not be made against KHRONOS_TRUE.
*/
typedef enum {
    KHRONOS_FALSE = 0,
    KHRONOS_TRUE = 1,
    KHRONOS_BOOLEAN_ENUM_FORCE_SIZE = KHRONOS_MAX_ENUM
} khronos_boolean_enum_t;

#endif /* __khrplatform_h_ */


#ifndef __eglplatform_h_
#define __eglplatform_h_

/*
** Copyright (c) 2007-2009 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/* Platform-specific types and definitions for egl.h
* $Revision: 12306 $ on $Date: 2010-08-25 09:51:28 -0700 (Wed, 25 Aug 2010) $
*
* Adopters may modify khrplatform.h and this file to suit their platform.
* You are encouraged to submit all modifications to the Khronos group so that
* they can be included in future versions of this file.  Please submit changes
* by sending them to the public Khronos Bugzilla (http://khronos.org/bugzilla)
* by filing a bug against product "EGL" component "Registry".
*/

/*#include <KHR/khrplatform.h>*/

/* Macros used in EGL function prototype declarations.
*
* EGL functions should be prototyped as:
*
* EGLAPI return-type EGLAPIENTRY eglFunction(arguments);
* typedef return-type (EXPAPIENTRYP PFNEGLFUNCTIONPROC) (arguments);
*
* KHRONOS_APICALL and KHRONOS_APIENTRY are defined in KHR/khrplatform.h
*/

#ifndef EGLAPI
#define EGLAPI KHRONOS_APICALL
#endif

#ifndef EGLAPIENTRY
#define EGLAPIENTRY  KHRONOS_APIENTRY
#endif
#define EGLAPIENTRYP EGLAPIENTRY*

/* The types NativeDisplayType, NativeWindowType, and NativePixmapType
* are aliases of window-system-dependent types, such as X Display * or
* Windows Device Context. They must be defined in platform-specific
* code below. The EGL-prefixed versions of Native*Type are the same
* types, renamed in EGL 1.3 so all types in the API start with "EGL".
*
* Khronos STRONGLY RECOMMENDS that you use the default definitions
* provided below, since these changes affect both binary and source
* portability of applications using EGL running on different EGL
* implementations.
*/

#if defined(_WIN32) || defined(__VC32__) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__) /* Win32 and WinCE */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

#if __WINRT__
#include <Unknwn.h>
typedef IUnknown * EGLNativeWindowType;
typedef IUnknown * EGLNativePixmapType;
typedef IUnknown * EGLNativeDisplayType;
#else
typedef HDC     EGLNativeDisplayType;
typedef HBITMAP EGLNativePixmapType;
typedef HWND    EGLNativeWindowType;
#endif

#elif defined(__WINSCW__) || defined(__SYMBIAN32__)  /* Symbian */

typedef int   EGLNativeDisplayType;
typedef void *EGLNativeWindowType;
typedef void *EGLNativePixmapType;

#elif defined(WL_EGL_PLATFORM)

typedef struct wl_display     *EGLNativeDisplayType;
typedef struct wl_egl_pixmap  *EGLNativePixmapType;
typedef struct wl_egl_window  *EGLNativeWindowType;

#elif defined(__GBM__)

typedef struct gbm_device  *EGLNativeDisplayType;
typedef struct gbm_bo      *EGLNativePixmapType;
typedef void               *EGLNativeWindowType;

#elif defined(__ANDROID__) /* Android */

struct ANativeWindow;
struct egl_native_pixmap_t;

typedef struct ANativeWindow        *EGLNativeWindowType;
typedef struct egl_native_pixmap_t  *EGLNativePixmapType;
typedef void                        *EGLNativeDisplayType;

#elif defined(MIR_EGL_PLATFORM)

#include <mir_toolkit/mir_client_library.h>
typedef MirEGLNativeDisplayType EGLNativeDisplayType;
typedef void                   *EGLNativePixmapType;
typedef MirEGLNativeWindowType  EGLNativeWindowType;

#elif defined(__unix__)

#ifdef MESA_EGL_NO_X11_HEADERS

typedef void            *EGLNativeDisplayType;
typedef khronos_uintptr_t EGLNativePixmapType;
typedef khronos_uintptr_t EGLNativeWindowType;

#else

/* X11 (tentative)  */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef Display *EGLNativeDisplayType;
typedef Pixmap   EGLNativePixmapType;
typedef Window   EGLNativeWindowType;

#endif /* MESA_EGL_NO_X11_HEADERS */

#else
#error "Platform not recognized"
#endif

/* EGL 1.2 types, renamed for consistency in EGL 1.3 */
typedef EGLNativeDisplayType NativeDisplayType;
typedef EGLNativePixmapType  NativePixmapType;
typedef EGLNativeWindowType  NativeWindowType;


/* Define EGLint. This must be a signed integral type large enough to contain
* all legal attribute names and values passed into and out of EGL, whether
* their type is boolean, bitmask, enumerant (symbolic constant), integer,
* handle, or other.  While in general a 32-bit integer will suffice, if
* handles are 64 bit types, then EGLint should be defined as a signed 64-bit
* integer type.
*/
typedef khronos_int32_t EGLint;

#endif /* __eglplatform_h */

/* -*- mode: c; tab-width: 8; -*- */
/* vi: set sw=4 ts=8: */
/* Reference version of egl.h for EGL 1.4.
* $Revision: 9356 $ on $Date: 2009-10-21 02:52:25 -0700 (Wed, 21 Oct 2009) $
*/

/*
** Copyright (c) 2007-2009 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

#ifndef __egl_h_
#define __egl_h_

/* All platform-dependent types and macro boilerplate (such as EGLAPI
* and EGLAPIENTRY) should go in eglplatform.h.
*/
/*#include <EGL/eglplatform.h>*/

#ifdef __cplusplus
extern "C" {
#endif

    /* EGL Types */
    /* EGLint is defined in eglplatform.h */
    typedef unsigned int EGLBoolean;
    typedef unsigned int EGLenum;
    typedef void *EGLConfig;
    typedef void *EGLContext;
    typedef void *EGLDisplay;
    typedef void *EGLSurface;
    typedef void *EGLClientBuffer;

    /* EGL Versioning */
#define EGL_VERSION_1_0			1
#define EGL_VERSION_1_1			1
#define EGL_VERSION_1_2			1
#define EGL_VERSION_1_3			1
#define EGL_VERSION_1_4			1

    /* EGL Enumerants. Bitmasks and other exceptional cases aside, most
    * enums are assigned unique values starting at 0x3000.
    */

    /* EGL aliases */
#define EGL_FALSE			0
#define EGL_TRUE			1

    /* Out-of-band handle values */
#define EGL_DEFAULT_DISPLAY		((EGLNativeDisplayType)0)
#define EGL_NO_CONTEXT			((EGLContext)0)
#define EGL_NO_DISPLAY			((EGLDisplay)0)
#define EGL_NO_SURFACE			((EGLSurface)0)

    /* Out-of-band attribute value */
#define EGL_DONT_CARE			((EGLint)-1)

    /* Errors / GetError return values */
#define EGL_SUCCESS			0x3000
#define EGL_NOT_INITIALIZED		0x3001
#define EGL_BAD_ACCESS			0x3002
#define EGL_BAD_ALLOC			0x3003
#define EGL_BAD_ATTRIBUTE		0x3004
#define EGL_BAD_CONFIG			0x3005
#define EGL_BAD_CONTEXT			0x3006
#define EGL_BAD_CURRENT_SURFACE		0x3007
#define EGL_BAD_DISPLAY			0x3008
#define EGL_BAD_MATCH			0x3009
#define EGL_BAD_NATIVE_PIXMAP		0x300A
#define EGL_BAD_NATIVE_WINDOW		0x300B
#define EGL_BAD_PARAMETER		0x300C
#define EGL_BAD_SURFACE			0x300D
#define EGL_CONTEXT_LOST		0x300E	/* EGL 1.1 - IMG_power_management */

    /* Reserved 0x300F-0x301F for additional errors */

    /* Config attributes */
#define EGL_BUFFER_SIZE			0x3020
#define EGL_ALPHA_SIZE			0x3021
#define EGL_BLUE_SIZE			0x3022
#define EGL_GREEN_SIZE			0x3023
#define EGL_RED_SIZE			0x3024
#define EGL_DEPTH_SIZE			0x3025
#define EGL_STENCIL_SIZE		0x3026
#define EGL_CONFIG_CAVEAT		0x3027
#define EGL_CONFIG_ID			0x3028
#define EGL_LEVEL			0x3029
#define EGL_MAX_PBUFFER_HEIGHT		0x302A
#define EGL_MAX_PBUFFER_PIXELS		0x302B
#define EGL_MAX_PBUFFER_WIDTH		0x302C
#define EGL_NATIVE_RENDERABLE		0x302D
#define EGL_NATIVE_VISUAL_ID		0x302E
#define EGL_NATIVE_VISUAL_TYPE		0x302F
#define EGL_SAMPLES			0x3031
#define EGL_SAMPLE_BUFFERS		0x3032
#define EGL_SURFACE_TYPE		0x3033
#define EGL_TRANSPARENT_TYPE		0x3034
#define EGL_TRANSPARENT_BLUE_VALUE	0x3035
#define EGL_TRANSPARENT_GREEN_VALUE	0x3036
#define EGL_TRANSPARENT_RED_VALUE	0x3037
#define EGL_NONE			0x3038	/* Attrib list terminator */
#define EGL_BIND_TO_TEXTURE_RGB		0x3039
#define EGL_BIND_TO_TEXTURE_RGBA	0x303A
#define EGL_MIN_SWAP_INTERVAL		0x303B
#define EGL_MAX_SWAP_INTERVAL		0x303C
#define EGL_LUMINANCE_SIZE		0x303D
#define EGL_ALPHA_MASK_SIZE		0x303E
#define EGL_COLOR_BUFFER_TYPE		0x303F
#define EGL_RENDERABLE_TYPE		0x3040
#define EGL_MATCH_NATIVE_PIXMAP		0x3041	/* Pseudo-attribute (not queryable) */
#define EGL_CONFORMANT			0x3042

    /* Reserved 0x3041-0x304F for additional config attributes */

    /* Config attribute values */
#define EGL_SLOW_CONFIG			0x3050	/* EGL_CONFIG_CAVEAT value */
#define EGL_NON_CONFORMANT_CONFIG	0x3051	/* EGL_CONFIG_CAVEAT value */
#define EGL_TRANSPARENT_RGB		0x3052	/* EGL_TRANSPARENT_TYPE value */
#define EGL_RGB_BUFFER			0x308E	/* EGL_COLOR_BUFFER_TYPE value */
#define EGL_LUMINANCE_BUFFER		0x308F	/* EGL_COLOR_BUFFER_TYPE value */

    /* More config attribute values, for EGL_TEXTURE_FORMAT */
#define EGL_NO_TEXTURE			0x305C
#define EGL_TEXTURE_RGB			0x305D
#define EGL_TEXTURE_RGBA		0x305E
#define EGL_TEXTURE_2D			0x305F

    /* Config attribute mask bits */
#define EGL_PBUFFER_BIT			0x0001	/* EGL_SURFACE_TYPE mask bits */
#define EGL_PIXMAP_BIT			0x0002	/* EGL_SURFACE_TYPE mask bits */
#define EGL_WINDOW_BIT			0x0004	/* EGL_SURFACE_TYPE mask bits */
#define EGL_VG_COLORSPACE_LINEAR_BIT	0x0020	/* EGL_SURFACE_TYPE mask bits */
#define EGL_VG_ALPHA_FORMAT_PRE_BIT	0x0040	/* EGL_SURFACE_TYPE mask bits */
#define EGL_MULTISAMPLE_RESOLVE_BOX_BIT 0x0200	/* EGL_SURFACE_TYPE mask bits */
#define EGL_SWAP_BEHAVIOR_PRESERVED_BIT 0x0400	/* EGL_SURFACE_TYPE mask bits */

#define EGL_OPENGL_ES_BIT		0x0001	/* EGL_RENDERABLE_TYPE mask bits */
#define EGL_OPENVG_BIT			0x0002	/* EGL_RENDERABLE_TYPE mask bits */
#define EGL_OPENGL_ES2_BIT		0x0004	/* EGL_RENDERABLE_TYPE mask bits */
#define EGL_OPENGL_BIT			0x0008	/* EGL_RENDERABLE_TYPE mask bits */

    /* QueryString targets */
#define EGL_VENDOR			0x3053
#define EGL_VERSION			0x3054
#define EGL_EXTENSIONS			0x3055
#define EGL_CLIENT_APIS			0x308D

    /* QuerySurface / SurfaceAttrib / CreatePbufferSurface targets */
#define EGL_HEIGHT			0x3056
#define EGL_WIDTH			0x3057
#define EGL_LARGEST_PBUFFER		0x3058
#define EGL_TEXTURE_FORMAT		0x3080
#define EGL_TEXTURE_TARGET		0x3081
#define EGL_MIPMAP_TEXTURE		0x3082
#define EGL_MIPMAP_LEVEL		0x3083
#define EGL_RENDER_BUFFER		0x3086
#define EGL_VG_COLORSPACE		0x3087
#define EGL_VG_ALPHA_FORMAT		0x3088
#define EGL_HORIZONTAL_RESOLUTION	0x3090
#define EGL_VERTICAL_RESOLUTION		0x3091
#define EGL_PIXEL_ASPECT_RATIO		0x3092
#define EGL_SWAP_BEHAVIOR		0x3093
#define EGL_MULTISAMPLE_RESOLVE		0x3099

    /* EGL_RENDER_BUFFER values / BindTexImage / ReleaseTexImage buffer targets */
#define EGL_BACK_BUFFER			0x3084
#define EGL_SINGLE_BUFFER		0x3085

    /* OpenVG color spaces */
#define EGL_VG_COLORSPACE_sRGB		0x3089	/* EGL_VG_COLORSPACE value */
#define EGL_VG_COLORSPACE_LINEAR	0x308A	/* EGL_VG_COLORSPACE value */

    /* OpenVG alpha formats */
#define EGL_VG_ALPHA_FORMAT_NONPRE	0x308B	/* EGL_ALPHA_FORMAT value */
#define EGL_VG_ALPHA_FORMAT_PRE		0x308C	/* EGL_ALPHA_FORMAT value */

    /* Constant scale factor by which fractional display resolutions &
    * aspect ratio are scaled when queried as integer values.
    */
#define EGL_DISPLAY_SCALING		10000

    /* Unknown display resolution/aspect ratio */
#define EGL_UNKNOWN			((EGLint)-1)

    /* Back buffer swap behaviors */
#define EGL_BUFFER_PRESERVED		0x3094	/* EGL_SWAP_BEHAVIOR value */
#define EGL_BUFFER_DESTROYED		0x3095	/* EGL_SWAP_BEHAVIOR value */

    /* CreatePbufferFromClientBuffer buffer types */
#define EGL_OPENVG_IMAGE		0x3096

    /* QueryContext targets */
#define EGL_CONTEXT_CLIENT_TYPE		0x3097

    /* CreateContext attributes */
#define EGL_CONTEXT_CLIENT_VERSION	0x3098

    /* Multisample resolution behaviors */
#define EGL_MULTISAMPLE_RESOLVE_DEFAULT 0x309A	/* EGL_MULTISAMPLE_RESOLVE value */
#define EGL_MULTISAMPLE_RESOLVE_BOX	0x309B	/* EGL_MULTISAMPLE_RESOLVE value */

    /* BindAPI/QueryAPI targets */
#define EGL_OPENGL_ES_API		0x30A0
#define EGL_OPENVG_API			0x30A1
#define EGL_OPENGL_API			0x30A2

    /* GetCurrentSurface targets */
#define EGL_DRAW			0x3059
#define EGL_READ			0x305A

    /* WaitNative engines */
#define EGL_CORE_NATIVE_ENGINE		0x305B

    /* EGL 1.2 tokens renamed for consistency in EGL 1.3 */
#define EGL_COLORSPACE			EGL_VG_COLORSPACE
#define EGL_ALPHA_FORMAT		EGL_VG_ALPHA_FORMAT
#define EGL_COLORSPACE_sRGB		EGL_VG_COLORSPACE_sRGB
#define EGL_COLORSPACE_LINEAR		EGL_VG_COLORSPACE_LINEAR
#define EGL_ALPHA_FORMAT_NONPRE		EGL_VG_ALPHA_FORMAT_NONPRE
#define EGL_ALPHA_FORMAT_PRE		EGL_VG_ALPHA_FORMAT_PRE

    /* EGL extensions must request enum blocks from the Khronos
    * API Registrar, who maintains the enumerant registry. Submit
    * a bug in Khronos Bugzilla against task "Registry".
    */



    /* EGL Functions */

    EGLAPI EGLint EGLAPIENTRY eglGetError(void);

    EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id);
    EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
    EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy);

    EGLAPI const char * EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name);

    EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay dpy, EGLConfig *configs,
        EGLint config_size, EGLint *num_config);
    EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
        EGLConfig *configs, EGLint config_size,
        EGLint *num_config);
    EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
        EGLint attribute, EGLint *value);

    EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
        EGLNativeWindowType win,
        const EGLint *attrib_list);
    EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
        const EGLint *attrib_list);
    EGLAPI EGLSurface EGLAPIENTRY eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
        EGLNativePixmapType pixmap,
        const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
    EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
        EGLint attribute, EGLint *value);

    EGLAPI EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api);
    EGLAPI EGLenum EGLAPIENTRY eglQueryAPI(void);

    EGLAPI EGLBoolean EGLAPIENTRY eglWaitClient(void);

    EGLAPI EGLBoolean EGLAPIENTRY eglReleaseThread(void);

    EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferFromClientBuffer(
        EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
        EGLConfig config, const EGLint *attrib_list);

    EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface,
        EGLint attribute, EGLint value);
    EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
    EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);


    EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval);


    EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay dpy, EGLConfig config,
        EGLContext share_context,
        const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
    EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
        EGLSurface read, EGLContext ctx);

    EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext(void);
    EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw);
    EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay(void);
    EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay dpy, EGLContext ctx,
        EGLint attribute, EGLint *value);

    EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL(void);
    EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine);
    EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
    EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
        EGLNativePixmapType target);

    /* This is a generic function pointer type, whose name indicates it must
    * be cast to the proper type *and calling convention* before use.
    */
    typedef void(*__eglMustCastToProperFunctionPointerType)(void);

    /* Now, define eglGetProcAddress using the generic function ptr. type */
    EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY
        eglGetProcAddress(const char *procname);

#ifdef __cplusplus
}
#endif

#endif /* __egl_h_ */




#ifndef __eglext_h_
#define __eglext_h_

#ifdef __cplusplus
extern "C" {
#endif

    /*
    ** Copyright (c) 2007-2013 The Khronos Group Inc.
    **
    ** Permission is hereby granted, free of charge, to any person obtaining a
    ** copy of this software and/or associated documentation files (the
    ** "Materials"), to deal in the Materials without restriction, including
    ** without limitation the rights to use, copy, modify, merge, publish,
    ** distribute, sublicense, and/or sell copies of the Materials, and to
    ** permit persons to whom the Materials are furnished to do so, subject to
    ** the following conditions:
    **
    ** The above copyright notice and this permission notice shall be included
    ** in all copies or substantial portions of the Materials.
    **
    ** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    ** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    ** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    ** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    ** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    ** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
    ** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
    */

/* #include <EGL/eglplatform.h> */

    /*************************************************************/

    /* Header file version number */
    /* Current version at http://www.khronos.org/registry/egl/ */
    /* $Revision: 21254 $ on $Date: 2013-04-25 03:11:55 -0700 (Thu, 25 Apr 2013) $ */
#define EGL_EGLEXT_VERSION 16

#ifndef EGL_KHR_config_attribs
#define EGL_KHR_config_attribs 1
#define EGL_CONFORMANT_KHR			0x3042	/* EGLConfig attribute */
#define EGL_VG_COLORSPACE_LINEAR_BIT_KHR	0x0020	/* EGL_SURFACE_TYPE bitfield */
#define EGL_VG_ALPHA_FORMAT_PRE_BIT_KHR		0x0040	/* EGL_SURFACE_TYPE bitfield */
#endif

#ifndef EGL_KHR_lock_surface
#define EGL_KHR_lock_surface 1
#define EGL_READ_SURFACE_BIT_KHR		0x0001	/* EGL_LOCK_USAGE_HINT_KHR bitfield */
#define EGL_WRITE_SURFACE_BIT_KHR		0x0002	/* EGL_LOCK_USAGE_HINT_KHR bitfield */
#define EGL_LOCK_SURFACE_BIT_KHR		0x0080	/* EGL_SURFACE_TYPE bitfield */
#define EGL_OPTIMAL_FORMAT_BIT_KHR		0x0100	/* EGL_SURFACE_TYPE bitfield */
#define EGL_MATCH_FORMAT_KHR			0x3043	/* EGLConfig attribute */
#define EGL_FORMAT_RGB_565_EXACT_KHR		0x30C0	/* EGL_MATCH_FORMAT_KHR value */
#define EGL_FORMAT_RGB_565_KHR			0x30C1	/* EGL_MATCH_FORMAT_KHR value */
#define EGL_FORMAT_RGBA_8888_EXACT_KHR		0x30C2	/* EGL_MATCH_FORMAT_KHR value */
#define EGL_FORMAT_RGBA_8888_KHR		0x30C3	/* EGL_MATCH_FORMAT_KHR value */
#define EGL_MAP_PRESERVE_PIXELS_KHR		0x30C4	/* eglLockSurfaceKHR attribute */
#define EGL_LOCK_USAGE_HINT_KHR			0x30C5	/* eglLockSurfaceKHR attribute */
#define EGL_BITMAP_POINTER_KHR			0x30C6	/* eglQuerySurface attribute */
#define EGL_BITMAP_PITCH_KHR			0x30C7	/* eglQuerySurface attribute */
#define EGL_BITMAP_ORIGIN_KHR			0x30C8	/* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_RED_OFFSET_KHR		0x30C9	/* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR	0x30CA	/* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR	0x30CB	/* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR	0x30CC	/* eglQuerySurface attribute */
#define EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR	0x30CD	/* eglQuerySurface attribute */
#define EGL_LOWER_LEFT_KHR			0x30CE	/* EGL_BITMAP_ORIGIN_KHR value */
#define EGL_UPPER_LEFT_KHR			0x30CF	/* EGL_BITMAP_ORIGIN_KHR value */
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLBoolean EGLAPIENTRY eglLockSurfaceKHR(EGLDisplay display, EGLSurface surface, const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglUnlockSurfaceKHR(EGLDisplay display, EGLSurface surface);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface, const EGLint *attrib_list);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLUNLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface);
#endif

#ifndef EGL_KHR_image
#define EGL_KHR_image 1
#define EGL_NATIVE_PIXMAP_KHR			0x30B0	/* eglCreateImageKHR target */
    typedef void *EGLImageKHR;
#define EGL_NO_IMAGE_KHR			((EGLImageKHR)0)
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLImageKHR EGLAPIENTRY eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLImageKHR(EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImageKHR image);
#endif

#ifndef EGL_KHR_vg_parent_image
#define EGL_KHR_vg_parent_image 1
#define EGL_VG_PARENT_IMAGE_KHR			0x30BA	/* eglCreateImageKHR target */
#endif

#ifndef EGL_KHR_gl_texture_2D_image
#define EGL_KHR_gl_texture_2D_image 1
#define EGL_GL_TEXTURE_2D_KHR			0x30B1	/* eglCreateImageKHR target */
#define EGL_GL_TEXTURE_LEVEL_KHR		0x30BC	/* eglCreateImageKHR attribute */
#endif

#ifndef EGL_KHR_gl_texture_cubemap_image
#define EGL_KHR_gl_texture_cubemap_image 1
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR	0x30B3	/* eglCreateImageKHR target */
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR	0x30B4	/* eglCreateImageKHR target */
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR	0x30B5	/* eglCreateImageKHR target */
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR	0x30B6	/* eglCreateImageKHR target */
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR	0x30B7	/* eglCreateImageKHR target */
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR	0x30B8	/* eglCreateImageKHR target */
#endif

#ifndef EGL_KHR_gl_texture_3D_image
#define EGL_KHR_gl_texture_3D_image 1
#define EGL_GL_TEXTURE_3D_KHR			0x30B2	/* eglCreateImageKHR target */
#define EGL_GL_TEXTURE_ZOFFSET_KHR		0x30BD	/* eglCreateImageKHR attribute */
#endif

#ifndef EGL_KHR_gl_renderbuffer_image
#define EGL_KHR_gl_renderbuffer_image 1
#define EGL_GL_RENDERBUFFER_KHR			0x30B9	/* eglCreateImageKHR target */
#endif

#if KHRONOS_SUPPORT_INT64   /* EGLTimeKHR requires 64-bit uint support */
#ifndef EGL_KHR_reusable_sync
#define EGL_KHR_reusable_sync 1

    typedef void* EGLSyncKHR;
    typedef khronos_utime_nanoseconds_t EGLTimeKHR;

#define EGL_SYNC_STATUS_KHR			0x30F1
#define EGL_SIGNALED_KHR			0x30F2
#define EGL_UNSIGNALED_KHR			0x30F3
#define EGL_TIMEOUT_EXPIRED_KHR			0x30F5
#define EGL_CONDITION_SATISFIED_KHR		0x30F6
#define EGL_SYNC_TYPE_KHR			0x30F7
#define EGL_SYNC_REUSABLE_KHR			0x30FA
#define EGL_SYNC_FLUSH_COMMANDS_BIT_KHR		0x0001	/* eglClientWaitSyncKHR <flags> bitfield */
#define EGL_FOREVER_KHR				0xFFFFFFFFFFFFFFFFull
#define EGL_NO_SYNC_KHR				((EGLSyncKHR)0)
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLSyncKHR EGLAPIENTRY eglCreateSyncKHR(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglDestroySyncKHR(EGLDisplay dpy, EGLSyncKHR sync);
    EGLAPI EGLint EGLAPIENTRY eglClientWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
    EGLAPI EGLBoolean EGLAPIENTRY eglSignalSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode);
    EGLAPI EGLBoolean EGLAPIENTRY eglGetSyncAttribKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLSyncKHR(EGLAPIENTRYP PFNEGLCREATESYNCKHRPROC) (EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync);
    typedef EGLint(EGLAPIENTRYP PFNEGLCLIENTWAITSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLSIGNALSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLGETSYNCATTRIBKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value);
#endif
#endif

#ifndef EGL_KHR_image_base
#define EGL_KHR_image_base 1
    /* Most interfaces defined by EGL_KHR_image_pixmap above */
#define EGL_IMAGE_PRESERVED_KHR			0x30D2	/* eglCreateImageKHR attribute */
#endif

#ifndef EGL_KHR_image_pixmap
#define EGL_KHR_image_pixmap 1
    /* Interfaces defined by EGL_KHR_image above */
#endif

#ifndef EGL_IMG_context_priority
#define EGL_IMG_context_priority 1
#define EGL_CONTEXT_PRIORITY_LEVEL_IMG		0x3100
#define EGL_CONTEXT_PRIORITY_HIGH_IMG		0x3101
#define EGL_CONTEXT_PRIORITY_MEDIUM_IMG		0x3102
#define EGL_CONTEXT_PRIORITY_LOW_IMG		0x3103
#endif

#ifndef EGL_KHR_lock_surface2
#define EGL_KHR_lock_surface2 1
#define EGL_BITMAP_PIXEL_SIZE_KHR		0x3110
#endif

#ifndef EGL_NV_coverage_sample
#define EGL_NV_coverage_sample 1
#define EGL_COVERAGE_BUFFERS_NV			0x30E0
#define EGL_COVERAGE_SAMPLES_NV			0x30E1
#endif

#ifndef EGL_NV_depth_nonlinear
#define EGL_NV_depth_nonlinear 1
#define EGL_DEPTH_ENCODING_NV			0x30E2
#define EGL_DEPTH_ENCODING_NONE_NV 0
#define EGL_DEPTH_ENCODING_NONLINEAR_NV		0x30E3
#endif

#if KHRONOS_SUPPORT_INT64   /* EGLTimeNV requires 64-bit uint support */
#ifndef EGL_NV_sync
#define EGL_NV_sync 1
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_NV	0x30E6
#define EGL_SYNC_STATUS_NV			0x30E7
#define EGL_SIGNALED_NV				0x30E8
#define EGL_UNSIGNALED_NV			0x30E9
#define EGL_SYNC_FLUSH_COMMANDS_BIT_NV		0x0001
#define EGL_FOREVER_NV				0xFFFFFFFFFFFFFFFFull
#define EGL_ALREADY_SIGNALED_NV			0x30EA
#define EGL_TIMEOUT_EXPIRED_NV			0x30EB
#define EGL_CONDITION_SATISFIED_NV		0x30EC
#define EGL_SYNC_TYPE_NV			0x30ED
#define EGL_SYNC_CONDITION_NV			0x30EE
#define EGL_SYNC_FENCE_NV			0x30EF
#define EGL_NO_SYNC_NV				((EGLSyncNV)0)
    typedef void* EGLSyncNV;
    typedef khronos_utime_nanoseconds_t EGLTimeNV;
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLSyncNV EGLAPIENTRY eglCreateFenceSyncNV(EGLDisplay dpy, EGLenum condition, const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglDestroySyncNV(EGLSyncNV sync);
    EGLAPI EGLBoolean EGLAPIENTRY eglFenceNV(EGLSyncNV sync);
    EGLAPI EGLint EGLAPIENTRY eglClientWaitSyncNV(EGLSyncNV sync, EGLint flags, EGLTimeNV timeout);
    EGLAPI EGLBoolean EGLAPIENTRY eglSignalSyncNV(EGLSyncNV sync, EGLenum mode);
    EGLAPI EGLBoolean EGLAPIENTRY eglGetSyncAttribNV(EGLSyncNV sync, EGLint attribute, EGLint *value);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLSyncNV(EGLAPIENTRYP PFNEGLCREATEFENCESYNCNVPROC) (EGLDisplay dpy, EGLenum condition, const EGLint *attrib_list);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYSYNCNVPROC) (EGLSyncNV sync);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLFENCENVPROC) (EGLSyncNV sync);
    typedef EGLint(EGLAPIENTRYP PFNEGLCLIENTWAITSYNCNVPROC) (EGLSyncNV sync, EGLint flags, EGLTimeNV timeout);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLSIGNALSYNCNVPROC) (EGLSyncNV sync, EGLenum mode);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLGETSYNCATTRIBNVPROC) (EGLSyncNV sync, EGLint attribute, EGLint *value);
#endif
#endif

#if KHRONOS_SUPPORT_INT64   /* Dependent on EGL_KHR_reusable_sync which requires 64-bit uint support */
#ifndef EGL_KHR_fence_sync
#define EGL_KHR_fence_sync 1
    /* Reuses most tokens and entry points from EGL_KHR_reusable_sync */
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR	0x30F0
#define EGL_SYNC_CONDITION_KHR			0x30F8
#define EGL_SYNC_FENCE_KHR			0x30F9
#endif
#endif

#ifndef EGL_HI_clientpixmap
#define EGL_HI_clientpixmap 1

    /* Surface Attribute */
#define EGL_CLIENT_PIXMAP_POINTER_HI		0x8F74
    /*
    * Structure representing a client pixmap
    * (pixmap's data is in client-space memory).
    */
    struct EGLClientPixmapHI
    {
        void*		pData;
        EGLint		iWidth;
        EGLint		iHeight;
        EGLint		iStride;
    };
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLSurface EGLAPIENTRY eglCreatePixmapSurfaceHI(EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI* pixmap);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATEPIXMAPSURFACEHIPROC) (EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI* pixmap);
#endif	/* EGL_HI_clientpixmap */

#ifndef EGL_HI_colorformats
#define EGL_HI_colorformats 1
    /* Config Attribute */
#define EGL_COLOR_FORMAT_HI			0x8F70
    /* Color Formats */
#define EGL_COLOR_RGB_HI			0x8F71
#define EGL_COLOR_RGBA_HI			0x8F72
#define EGL_COLOR_ARGB_HI			0x8F73
#endif /* EGL_HI_colorformats */

#ifndef EGL_MESA_drm_image
#define EGL_MESA_drm_image 1
#define EGL_DRM_BUFFER_FORMAT_MESA		0x31D0	    /* CreateDRMImageMESA attribute */
#define EGL_DRM_BUFFER_USE_MESA			0x31D1	    /* CreateDRMImageMESA attribute */
#define EGL_DRM_BUFFER_FORMAT_ARGB32_MESA	0x31D2	    /* EGL_IMAGE_FORMAT_MESA attribute value */
#define EGL_DRM_BUFFER_MESA			0x31D3	    /* eglCreateImageKHR target */
#define EGL_DRM_BUFFER_STRIDE_MESA		0x31D4
#define EGL_DRM_BUFFER_USE_SCANOUT_MESA		0x00000001  /* EGL_DRM_BUFFER_USE_MESA bits */
#define EGL_DRM_BUFFER_USE_SHARE_MESA		0x00000002  /* EGL_DRM_BUFFER_USE_MESA bits */
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLImageKHR EGLAPIENTRY eglCreateDRMImageMESA(EGLDisplay dpy, const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglExportDRMImageMESA(EGLDisplay dpy, EGLImageKHR image, EGLint *name, EGLint *handle, EGLint *stride);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLImageKHR(EGLAPIENTRYP PFNEGLCREATEDRMIMAGEMESAPROC) (EGLDisplay dpy, const EGLint *attrib_list);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLEXPORTDRMIMAGEMESAPROC) (EGLDisplay dpy, EGLImageKHR image, EGLint *name, EGLint *handle, EGLint *stride);
#endif

#ifndef EGL_NV_post_sub_buffer
#define EGL_NV_post_sub_buffer 1
#define EGL_POST_SUB_BUFFER_SUPPORTED_NV	0x30BE
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLBoolean EGLAPIENTRY eglPostSubBufferNV(EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLPOSTSUBBUFFERNVPROC) (EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height);
#endif

#ifndef EGL_ANGLE_query_surface_pointer
#define EGL_ANGLE_query_surface_pointer 1
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLBoolean eglQuerySurfacePointerANGLE(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value);
#endif
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYSURFACEPOINTERANGLEPROC) (EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value);
#endif

#ifndef EGL_ANGLE_surface_d3d_texture_2d_share_handle
#define EGL_ANGLE_surface_d3d_texture_2d_share_handle 1
#define EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE	0x3200
#endif

#ifndef EGL_NV_coverage_sample_resolve
#define EGL_NV_coverage_sample_resolve 1
#define EGL_COVERAGE_SAMPLE_RESOLVE_NV		0x3131
#define EGL_COVERAGE_SAMPLE_RESOLVE_DEFAULT_NV	0x3132
#define EGL_COVERAGE_SAMPLE_RESOLVE_NONE_NV	0x3133
#endif

#if KHRONOS_SUPPORT_INT64   /* EGLuint64NV requires 64-bit uint support */
#ifndef EGL_NV_system_time
#define EGL_NV_system_time 1
    typedef khronos_utime_nanoseconds_t EGLuint64NV;
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLuint64NV EGLAPIENTRY eglGetSystemTimeFrequencyNV(void);
    EGLAPI EGLuint64NV EGLAPIENTRY eglGetSystemTimeNV(void);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLuint64NV(EGLAPIENTRYP PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC) (void);
    typedef EGLuint64NV(EGLAPIENTRYP PFNEGLGETSYSTEMTIMENVPROC) (void);
#endif
#endif

#if KHRONOS_SUPPORT_INT64 /* EGLuint64KHR requires 64-bit uint support */
#ifndef EGL_KHR_stream
#define EGL_KHR_stream 1
    typedef void* EGLStreamKHR;
    typedef khronos_uint64_t EGLuint64KHR;
#define EGL_NO_STREAM_KHR			((EGLStreamKHR)0)
#define EGL_CONSUMER_LATENCY_USEC_KHR		0x3210
#define EGL_PRODUCER_FRAME_KHR			0x3212
#define EGL_CONSUMER_FRAME_KHR			0x3213
#define EGL_STREAM_STATE_KHR			0x3214
#define EGL_STREAM_STATE_CREATED_KHR		0x3215
#define EGL_STREAM_STATE_CONNECTING_KHR		0x3216
#define EGL_STREAM_STATE_EMPTY_KHR		0x3217
#define EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR    0x3218
#define EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR    0x3219
#define EGL_STREAM_STATE_DISCONNECTED_KHR	0x321A
#define EGL_BAD_STREAM_KHR			0x321B
#define EGL_BAD_STATE_KHR			0x321C
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLStreamKHR EGLAPIENTRY eglCreateStreamKHR(EGLDisplay dpy, const EGLint *attrib_list);
    EGLAPI EGLBoolean EGLAPIENTRY eglDestroyStreamKHR(EGLDisplay dpy, EGLStreamKHR stream);
    EGLAPI EGLBoolean EGLAPIENTRY eglStreamAttribKHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value);
    EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamKHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint *value);
    EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamu64KHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR *value);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLStreamKHR(EGLAPIENTRYP PFNEGLCREATESTREAMKHRPROC)(EGLDisplay dpy, const EGLint *attrib_list);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLDESTROYSTREAMKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLSTREAMATTRIBKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYSTREAMKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint *value);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYSTREAMU64KHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR *value);
#endif
#endif

#ifdef EGL_KHR_stream /* Requires KHR_stream extension */
#ifndef EGL_KHR_stream_consumer_gltexture
#define EGL_KHR_stream_consumer_gltexture 1
#define EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR	0x321E
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerGLTextureExternalKHR(EGLDisplay dpy, EGLStreamKHR stream);
    EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerAcquireKHR(EGLDisplay dpy, EGLStreamKHR stream);
    EGLAPI EGLBoolean EGLAPIENTRY eglStreamConsumerReleaseKHR(EGLDisplay dpy, EGLStreamKHR stream);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLSTREAMCONSUMERACQUIREKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLSTREAMCONSUMERRELEASEKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
#endif
#endif

#ifdef EGL_KHR_stream /* Requires KHR_stream extension */
#ifndef EGL_KHR_stream_producer_eglsurface
#define EGL_KHR_stream_producer_eglsurface 1
#define EGL_STREAM_BIT_KHR			0x0800
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLSurface EGLAPIENTRY eglCreateStreamProducerSurfaceKHR(EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint *attrib_list);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLSurface(EGLAPIENTRYP PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC)(EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint *attrib_list);
#endif
#endif

#ifdef EGL_KHR_stream /* Requires KHR_stream extension */
#ifndef EGL_KHR_stream_producer_aldatalocator
#define EGL_KHR_stream_producer_aldatalocator 1
#endif
#endif

#ifdef EGL_KHR_stream /* Requires KHR_stream extension */
#ifndef EGL_KHR_stream_fifo
#define EGL_KHR_stream_fifo 1
    /* reuse EGLTimeKHR */
#define EGL_STREAM_FIFO_LENGTH_KHR		0x31FC
#define EGL_STREAM_TIME_NOW_KHR			0x31FD
#define EGL_STREAM_TIME_CONSUMER_KHR		0x31FE
#define EGL_STREAM_TIME_PRODUCER_KHR		0x31FF
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLBoolean EGLAPIENTRY eglQueryStreamTimeKHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR *value);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYSTREAMTIMEKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR *value);
#endif
#endif

#ifndef EGL_EXT_create_context_robustness
#define EGL_EXT_create_context_robustness 1
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT	0x30BF
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT 0x3138
#define EGL_NO_RESET_NOTIFICATION_EXT		0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_EXT		0x31BF
#endif

#ifndef EGL_ANGLE_d3d_share_handle_client_buffer
#define EGL_ANGLE_d3d_share_handle_client_buffer 1
    /* reuse EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE */
#endif

#ifndef EGL_KHR_create_context
#define EGL_KHR_create_context 1
#define EGL_CONTEXT_MAJOR_VERSION_KHR			    EGL_CONTEXT_CLIENT_VERSION
#define EGL_CONTEXT_MINOR_VERSION_KHR			    0x30FB
#define EGL_CONTEXT_FLAGS_KHR				    0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR		    0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR  0x31BD
#define EGL_NO_RESET_NOTIFICATION_KHR			    0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR			    0x31BF
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR		    0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR	    0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR	    0x00000004
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR		    0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR    0x00000002
#define EGL_OPENGL_ES3_BIT_KHR				    0x00000040
#endif

#ifndef EGL_KHR_surfaceless_context
#define EGL_KHR_surfaceless_context 1
    /* No tokens/entry points, just relaxes an error condition */
#endif

#ifdef EGL_KHR_stream /* Requires KHR_stream extension */
#ifndef EGL_KHR_stream_cross_process_fd
#define EGL_KHR_stream_cross_process_fd 1
    typedef int EGLNativeFileDescriptorKHR;
#define EGL_NO_FILE_DESCRIPTOR_KHR		((EGLNativeFileDescriptorKHR)(-1))
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLNativeFileDescriptorKHR EGLAPIENTRY eglGetStreamFileDescriptorKHR(EGLDisplay dpy, EGLStreamKHR stream);
    EGLAPI EGLStreamKHR EGLAPIENTRY eglCreateStreamFromFileDescriptorKHR(EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLNativeFileDescriptorKHR(EGLAPIENTRYP PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC)(EGLDisplay dpy, EGLStreamKHR stream);
    typedef EGLStreamKHR(EGLAPIENTRYP PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC)(EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);
#endif
#endif

#ifndef EGL_EXT_multiview_window
#define EGL_EXT_multiview_window 1
#define EGL_MULTIVIEW_VIEW_COUNT_EXT		0x3134
#endif

#ifndef EGL_KHR_wait_sync
#define EGL_KHR_wait_sync 1
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLint EGLAPIENTRY eglWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLint(EGLAPIENTRYP PFNEGLWAITSYNCKHRPROC)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);
#endif

#ifndef EGL_NV_post_convert_rounding
#define EGL_NV_post_convert_rounding 1
    /* No tokens or entry points, just relaxes behavior of SwapBuffers */
#endif

#ifndef EGL_NV_native_query
#define EGL_NV_native_query 1
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLBoolean EGLAPIENTRY eglQueryNativeDisplayNV(EGLDisplay dpy, EGLNativeDisplayType* display_id);
    EGLAPI EGLBoolean EGLAPIENTRY eglQueryNativeWindowNV(EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType* window);
    EGLAPI EGLBoolean EGLAPIENTRY eglQueryNativePixmapNV(EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType* pixmap);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYNATIVEDISPLAYNVPROC)(EGLDisplay dpy, EGLNativeDisplayType *display_id);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYNATIVEWINDOWNVPROC)(EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType *window);
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLQUERYNATIVEPIXMAPNVPROC)(EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType *pixmap);
#endif

#ifndef EGL_NV_3dvision_surface
#define EGL_NV_3dvision_surface 1
#define EGL_AUTO_STEREO_NV			0x3136
#endif

#ifndef EGL_ANDROID_framebuffer_target
#define EGL_ANDROID_framebuffer_target 1
#define EGL_FRAMEBUFFER_TARGET_ANDROID		0x3147
#endif

#ifndef EGL_ANDROID_blob_cache
#define EGL_ANDROID_blob_cache 1
    typedef khronos_ssize_t EGLsizeiANDROID;
    typedef void(*EGLSetBlobFuncANDROID) (const void *key, EGLsizeiANDROID keySize, const void *value, EGLsizeiANDROID valueSize);
    typedef EGLsizeiANDROID(*EGLGetBlobFuncANDROID) (const void *key, EGLsizeiANDROID keySize, void *value, EGLsizeiANDROID valueSize);
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI void EGLAPIENTRY eglSetBlobCacheFuncsANDROID(EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef void (EGLAPIENTRYP PFNEGLSETBLOBCACHEFUNCSANDROIDPROC)(EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get);
#endif

#ifndef EGL_ANDROID_image_native_buffer
#define EGL_ANDROID_image_native_buffer 1
#define EGL_NATIVE_BUFFER_ANDROID		0x3140
#endif

#ifndef EGL_ANDROID_native_fence_sync
#define EGL_ANDROID_native_fence_sync 1
#define EGL_SYNC_NATIVE_FENCE_ANDROID		0x3144
#define EGL_SYNC_NATIVE_FENCE_FD_ANDROID	0x3145
#define EGL_SYNC_NATIVE_FENCE_SIGNALED_ANDROID	0x3146
#define EGL_NO_NATIVE_FENCE_FD_ANDROID		-1
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLint EGLAPIENTRY eglDupNativeFenceFDANDROID(EGLDisplay dpy, EGLSyncKHR);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLint(EGLAPIENTRYP PFNEGLDUPNATIVEFENCEFDANDROIDPROC)(EGLDisplay dpy, EGLSyncKHR);
#endif

#ifndef EGL_ANDROID_recordable
#define EGL_ANDROID_recordable 1
#define EGL_RECORDABLE_ANDROID			0x3142
#endif

#ifndef EGL_EXT_buffer_age
#define EGL_EXT_buffer_age 1
#define EGL_BUFFER_AGE_EXT			0x313D
#endif

#ifndef EGL_EXT_image_dma_buf_import
#define EGL_EXT_image_dma_buf_import 1
#define EGL_LINUX_DMA_BUF_EXT			0x3270
#define EGL_LINUX_DRM_FOURCC_EXT		0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT		0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT		0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT		0x3274
#define EGL_DMA_BUF_PLANE1_FD_EXT		0x3275
#define EGL_DMA_BUF_PLANE1_OFFSET_EXT		0x3276
#define EGL_DMA_BUF_PLANE1_PITCH_EXT		0x3277
#define EGL_DMA_BUF_PLANE2_FD_EXT		0x3278
#define EGL_DMA_BUF_PLANE2_OFFSET_EXT		0x3279
#define EGL_DMA_BUF_PLANE2_PITCH_EXT		0x327A
#define EGL_YUV_COLOR_SPACE_HINT_EXT		0x327B
#define EGL_SAMPLE_RANGE_HINT_EXT		0x327C
#define EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT 0x327D
#define EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT 0x327E
#define EGL_ITU_REC601_EXT			0x327F
#define EGL_ITU_REC709_EXT			0x3280
#define EGL_ITU_REC2020_EXT			0x3281
#define EGL_YUV_FULL_RANGE_EXT			0x3282
#define EGL_YUV_NARROW_RANGE_EXT		0x3283
#define EGL_YUV_CHROMA_SITING_0_EXT		0x3284
#define EGL_YUV_CHROMA_SITING_0_5_EXT		0x3285
#endif

#ifndef EGL_ARM_pixmap_multisample_discard
#define EGL_ARM_pixmap_multisample_discard 1
#define EGL_DISCARD_SAMPLES_ARM			0x3286
#endif

#ifndef EGL_EXT_swap_buffers_with_damage
#define EGL_EXT_swap_buffers_with_damage 1
#ifdef EGL_EGLEXT_PROTOTYPES
    EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffersWithDamageEXT(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
#endif /* EGL_EGLEXT_PROTOTYPES */
    typedef EGLBoolean(EGLAPIENTRYP PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
#endif

/* #include <EGL/eglmesaext.h> */

#ifdef __cplusplus
}
#endif

#endif /* __eglext_h_ */



#endif /* _MSC_VER */
