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
#ifndef SDL_disable_name_checking_h_
#define SDL_disable_name_checking_h_

#ifdef __HAIKU__
/* nothing */
#elif defined(__clang_analyzer__) && !defined(SDL_DISABLE_ANALYZE_MACROS)
/* nothing */
#else

#pragma pop_macro("abs")
#pragma pop_macro("acos")
#pragma pop_macro("acosf")
#pragma pop_macro("asin")
#pragma pop_macro("asinf")
#pragma pop_macro("asprintf")
#pragma pop_macro("atan")
#pragma pop_macro("atan2")
#pragma pop_macro("atan2f")
#pragma pop_macro("atanf")
#pragma pop_macro("atof")
#pragma pop_macro("atoi")
#pragma pop_macro("calloc")
#pragma pop_macro("ceil")
#pragma pop_macro("ceilf")
#pragma pop_macro("copysign")
#pragma pop_macro("copysignf")
#pragma pop_macro("cos")
#pragma pop_macro("cosf")
#pragma pop_macro("crc32")
#pragma pop_macro("exp")
#pragma pop_macro("expf")
#pragma pop_macro("fabs")
#pragma pop_macro("fabsf")
#pragma pop_macro("floor")
#pragma pop_macro("floorf")
#pragma pop_macro("fmod")
#pragma pop_macro("fmodf")
#pragma pop_macro("getenv")
#pragma pop_macro("isalnum")
#pragma pop_macro("isalpha")
#pragma pop_macro("isblank")
#pragma pop_macro("iscntrl")
#pragma pop_macro("isdigit")
#pragma pop_macro("isgraph")
#pragma pop_macro("islower")
#pragma pop_macro("isprint")
#pragma pop_macro("ispunct")
#pragma pop_macro("isspace")
#pragma pop_macro("isupper")
#pragma pop_macro("isxdigit")
#pragma pop_macro("itoa")
#pragma pop_macro("lltoa")
#pragma pop_macro("log10")
#pragma pop_macro("log10f")
#pragma pop_macro("logf")
#pragma pop_macro("lround")
#pragma pop_macro("lroundf")
#pragma pop_macro("ltoa")
#pragma pop_macro("malloc")
#pragma pop_macro("memcmp")
#pragma pop_macro("memcpy")
#pragma pop_macro("memcpy4")
#pragma pop_macro("memmove")
#pragma pop_macro("pow")
#pragma pop_macro("powf")
#pragma pop_macro("qsort")
#pragma pop_macro("realloc")
#pragma pop_macro("round")
#pragma pop_macro("roundf")
#pragma pop_macro("scalbn")
#pragma pop_macro("scalbnf")
#pragma pop_macro("setenv")
#pragma pop_macro("sin")
#pragma pop_macro("sinf")
#pragma pop_macro("snprintf")
#pragma pop_macro("sqrt")
#pragma pop_macro("sqrtf")
#pragma pop_macro("sscanf")
#pragma pop_macro("strcasecmp")
#pragma pop_macro("strchr")
#pragma pop_macro("strcmp")
#pragma pop_macro("strdup")
#pragma pop_macro("strlcat")
#pragma pop_macro("strlcpy")
#pragma pop_macro("strlen")
#pragma pop_macro("strlwr")
#pragma pop_macro("strncasecmp")
#pragma pop_macro("strncmp")
#pragma pop_macro("strrchr")
#pragma pop_macro("strrev")
#pragma pop_macro("strstr")
#pragma pop_macro("strtod")
#pragma pop_macro("strtokr")
#pragma pop_macro("strtol")
#pragma pop_macro("strtoll")
#pragma pop_macro("strtoul")
#pragma pop_macro("strupr")
#pragma pop_macro("tan")
#pragma pop_macro("tanf")
#pragma pop_macro("tolower")
#pragma pop_macro("toupper")
#pragma pop_macro("trunc")
#pragma pop_macro("truncf")
#pragma pop_macro("uitoa")
#pragma pop_macro("ulltoa")
#pragma pop_macro("ultoa")
#pragma pop_macro("utf8strlcpy")
#pragma pop_macro("utf8strlen")
#pragma pop_macro("vasprintf")
#pragma pop_macro("vsnprintf")
#pragma pop_macro("vsscanf")
#pragma pop_macro("wcscasecmp")
#pragma pop_macro("wcscmp")
#pragma pop_macro("wcsdup")
#pragma pop_macro("wcslcat")
#pragma pop_macro("wcslcpy")
#pragma pop_macro("wcslen")
#pragma pop_macro("wcsncasecmp")
#pragma pop_macro("wcsncmp")
#pragma pop_macro("wcsstr")

#endif

#endif /* SDL_disable_name_checking_h_ */
