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
#ifndef SDL_name_checking_h_
#define SDL_name_checking_h_


#ifdef __HAIKU__
/* nothing */
#elif defined(__clang_analyzer__) && !defined(SDL_DISABLE_ANALYZE_MACROS)
/* nothing */
#else

#pragma push_macro("abs")
#undef abs
#define abs Please_use_SDL_abs_instead

#pragma push_macro("acos")
#undef acos
#define acos Please_use_SDL_acos_instead

#pragma push_macro("acosf")
#undef acosf
#define acosf Please_use_SDL_acosf_instead

#pragma push_macro("asin")
#undef asin
#define asin Please_use_SDL_asin_instead

#pragma push_macro("asinf")
#undef asinf
#define asinf Please_use_SDL_asinf_instead

#pragma push_macro("asprintf")
#undef asprintf
#define asprintf Please_use_SDL_asprintf_instead

#pragma push_macro("atan")
#undef atan
#define atan Please_use_SDL_atan_instead

#pragma push_macro("atan2")
#undef atan2
#define atan2 Please_use_SDL_atan2_instead

#pragma push_macro("atan2f")
#undef atan2f
#define atan2f Please_use_SDL_atan2f_instead

#pragma push_macro("atanf")
#undef atanf
#define atanf Please_use_SDL_atanf_instead

#pragma push_macro("atof")
#undef atof
#define atof Please_use_SDL_atof_instead

#pragma push_macro("atoi")
#undef atoi
#define atoi Please_use_SDL_atoi_instead

#pragma push_macro("calloc")
#undef calloc
#define calloc Please_use_SDL_calloc_instead

#pragma push_macro("ceil")
#undef ceil
#define ceil Please_use_SDL_ceil_instead

#pragma push_macro("ceilf")
#undef ceilf
#define ceilf Please_use_SDL_ceilf_instead

#pragma push_macro("copysign")
#undef copysign
#define copysign Please_use_SDL_copysign_instead

#pragma push_macro("copysignf")
#undef copysignf
#define copysignf Please_use_SDL_copysignf_instead

#pragma push_macro("cos")
#undef cos
#define cos Please_use_SDL_cos_instead

#pragma push_macro("cosf")
#undef cosf
#define cosf Please_use_SDL_cosf_instead

#pragma push_macro("crc32")
#undef crc32
#define crc32 Please_use_SDL_crc32_instead

#pragma push_macro("exp")
#undef exp
#define exp Please_use_SDL_exp_instead

#pragma push_macro("expf")
#undef expf
#define expf Please_use_SDL_expf_instead

#pragma push_macro("fabs")
#undef fabs
#define fabs Please_use_SDL_fabs_instead

#pragma push_macro("fabsf")
#undef fabsf
#define fabsf Please_use_SDL_fabsf_instead

#pragma push_macro("floor")
#undef floor
#define floor Please_use_SDL_floor_instead

#pragma push_macro("floorf")
#undef floorf
#define floorf Please_use_SDL_floorf_instead

#pragma push_macro("fmod")
#undef fmod
#define fmod Please_use_SDL_fmod_instead

#pragma push_macro("fmodf")
#undef fmodf
#define fmodf Please_use_SDL_fmodf_instead

#pragma push_macro("getenv")
#undef getenv
#define getenv Please_use_SDL_getenv_instead

#pragma push_macro("isalnum")
#undef isalnum
#define isalnum Please_use_SDL_isalnum_instead

#pragma push_macro("isalpha")
#undef isalpha
#define isalpha Please_use_SDL_isalpha_instead

#pragma push_macro("isblank")
#undef isblank
#define isblank Please_use_SDL_isblank_instead

#pragma push_macro("iscntrl")
#undef iscntrl
#define iscntrl Please_use_SDL_iscntrl_instead

#pragma push_macro("isdigit")
#undef isdigit
#define isdigit Please_use_SDL_isdigit_instead

#pragma push_macro("isgraph")
#undef isgraph
#define isgraph Please_use_SDL_isgraph_instead

#pragma push_macro("islower")
#undef islower
#define islower Please_use_SDL_islower_instead

#pragma push_macro("isprint")
#undef isprint
#define isprint Please_use_SDL_isprint_instead

#pragma push_macro("ispunct")
#undef ispunct
#define ispunct Please_use_SDL_ispunct_instead

#pragma push_macro("isspace")
#undef isspace
#define isspace Please_use_SDL_isspace_instead

#pragma push_macro("isupper")
#undef isupper
#define isupper Please_use_SDL_isupper_instead

#pragma push_macro("isxdigit")
#undef isxdigit
#define isxdigit Please_use_SDL_isxdigit_instead

#pragma push_macro("itoa")
#undef itoa
#define itoa Please_use_SDL_itoa_instead

#pragma push_macro("lltoa")
#undef lltoa
#define lltoa Please_use_SDL_lltoa_instead

#pragma push_macro("log10")
#undef log10
#define log10 Please_use_SDL_log10_instead

#pragma push_macro("log10f")
#undef log10f
#define log10f Please_use_SDL_log10f_instead

#pragma push_macro("logf")
#undef logf
#define logf Please_use_SDL_logf_instead

#pragma push_macro("lround")
#undef lround
#define lround Please_use_SDL_lround_instead

#pragma push_macro("lroundf")
#undef lroundf
#define lroundf Please_use_SDL_lroundf_instead

#pragma push_macro("ltoa")
#undef ltoa
#define ltoa Please_use_SDL_ltoa_instead

#pragma push_macro("malloc")
#undef malloc
#define malloc Please_use_SDL_malloc_instead

#pragma push_macro("memcmp")
#undef memcmp
#define memcmp Please_use_SDL_memcmp_instead

#pragma push_macro("memcpy")
#undef memcpy
#define memcpy Please_use_SDL_memcpy_instead

#pragma push_macro("memcpy4")
#undef memcpy4
#define memcpy4 Please_use_SDL_memcpy4_instead

#pragma push_macro("memmove")
#undef memmove
#define memmove Please_use_SDL_memmove_instead

#pragma push_macro("pow")
#undef pow
#define pow Please_use_SDL_pow_instead

#pragma push_macro("powf")
#undef powf
#define powf Please_use_SDL_powf_instead

#pragma push_macro("qsort")
#undef qsort
#define qsort Please_use_SDL_qsort_instead

#pragma push_macro("realloc")
#undef realloc
#define realloc Please_use_SDL_realloc_instead

#pragma push_macro("round")
#undef round
#define round Please_use_SDL_round_instead

#pragma push_macro("roundf")
#undef roundf
#define roundf Please_use_SDL_roundf_instead

#pragma push_macro("scalbn")
#undef scalbn
#define scalbn Please_use_SDL_scalbn_instead

#pragma push_macro("scalbnf")
#undef scalbnf
#define scalbnf Please_use_SDL_scalbnf_instead

#pragma push_macro("setenv")
#undef setenv
#define setenv Please_use_SDL_setenv_instead

#pragma push_macro("sin")
#undef sin
#define sin Please_use_SDL_sin_instead

#pragma push_macro("sinf")
#undef sinf
#define sinf Please_use_SDL_sinf_instead

#pragma push_macro("snprintf")
#undef snprintf
#define snprintf Please_use_SDL_snprintf_instead

#pragma push_macro("sqrt")
#undef sqrt
#define sqrt Please_use_SDL_sqrt_instead

#pragma push_macro("sqrtf")
#undef sqrtf
#define sqrtf Please_use_SDL_sqrtf_instead

#pragma push_macro("sscanf")
#undef sscanf
#define sscanf Please_use_SDL_sscanf_instead

#pragma push_macro("strcasecmp")
#undef strcasecmp
#define strcasecmp Please_use_SDL_strcasecmp_instead

#pragma push_macro("strchr")
#undef strchr
#define strchr Please_use_SDL_strchr_instead

#pragma push_macro("strcmp")
#undef strcmp
#define strcmp Please_use_SDL_strcmp_instead

#pragma push_macro("strdup")
#undef strdup
#define strdup Please_use_SDL_strdup_instead

#pragma push_macro("strlcat")
#undef strlcat
#define strlcat Please_use_SDL_strlcat_instead

#pragma push_macro("strlcpy")
#undef strlcpy
#define strlcpy Please_use_SDL_strlcpy_instead

#pragma push_macro("strlen")
#undef strlen
#define strlen Please_use_SDL_strlen_instead

#pragma push_macro("strlwr")
#undef strlwr
#define strlwr Please_use_SDL_strlwr_instead

#pragma push_macro("strncasecmp")
#undef strncasecmp
#define strncasecmp Please_use_SDL_strncasecmp_instead

#pragma push_macro("strncmp")
#undef strncmp
#define strncmp Please_use_SDL_strncmp_instead

#pragma push_macro("strrchr")
#undef strrchr
#define strrchr Please_use_SDL_strrchr_instead

#pragma push_macro("strrev")
#undef strrev
#define strrev Please_use_SDL_strrev_instead

#pragma push_macro("strstr")
#undef strstr
#define strstr Please_use_SDL_strstr_instead

#pragma push_macro("strtod")
#undef strtod
#define strtod Please_use_SDL_strtod_instead

#pragma push_macro("strtokr")
#undef strtokr
#define strtokr Please_use_SDL_strtokr_instead

#pragma push_macro("strtol")
#undef strtol
#define strtol Please_use_SDL_strtol_instead

#pragma push_macro("strtoll")
#undef strtoll
#define strtoll Please_use_SDL_strtoll_instead

#pragma push_macro("strtoul")
#undef strtoul
#define strtoul Please_use_SDL_strtoul_instead

#pragma push_macro("strupr")
#undef strupr
#define strupr Please_use_SDL_strupr_instead

#pragma push_macro("tan")
#undef tan
#define tan Please_use_SDL_tan_instead

#pragma push_macro("tanf")
#undef tanf
#define tanf Please_use_SDL_tanf_instead

#pragma push_macro("tolower")
#undef tolower
#define tolower Please_use_SDL_tolower_instead

#pragma push_macro("toupper")
#undef toupper
#define toupper Please_use_SDL_toupper_instead

#pragma push_macro("trunc")
#undef trunc
#define trunc Please_use_SDL_trunc_instead

#pragma push_macro("truncf")
#undef truncf
#define truncf Please_use_SDL_truncf_instead

#pragma push_macro("uitoa")
#undef uitoa
#define uitoa Please_use_SDL_uitoa_instead

#pragma push_macro("ulltoa")
#undef ulltoa
#define ulltoa Please_use_SDL_ulltoa_instead

#pragma push_macro("ultoa")
#undef ultoa
#define ultoa Please_use_SDL_ultoa_instead

#pragma push_macro("utf8strlcpy")
#undef utf8strlcpy
#define utf8strlcpy Please_use_SDL_utf8strlcpy_instead

#pragma push_macro("utf8strlen")
#undef utf8strlen
#define utf8strlen Please_use_SDL_utf8strlen_instead

#pragma push_macro("vasprintf")
#undef vasprintf
#define vasprintf Please_use_SDL_vasprintf_instead

#pragma push_macro("vsnprintf")
#undef vsnprintf
#define vsnprintf Please_use_SDL_vsnprintf_instead

#pragma push_macro("vsscanf")
#undef vsscanf
#define vsscanf Please_use_SDL_vsscanf_instead

#pragma push_macro("wcscasecmp")
#undef wcscasecmp
#define wcscasecmp Please_use_SDL_wcscasecmp_instead

#pragma push_macro("wcscmp")
#undef wcscmp
#define wcscmp Please_use_SDL_wcscmp_instead

#pragma push_macro("wcsdup")
#undef wcsdup
#define wcsdup Please_use_SDL_wcsdup_instead

#pragma push_macro("wcslcat")
#undef wcslcat
#define wcslcat Please_use_SDL_wcslcat_instead

#pragma push_macro("wcslcpy")
#undef wcslcpy
#define wcslcpy Please_use_SDL_wcslcpy_instead

#pragma push_macro("wcslen")
#undef wcslen
#define wcslen Please_use_SDL_wcslen_instead

#pragma push_macro("wcsncasecmp")
#undef wcsncasecmp
#define wcsncasecmp Please_use_SDL_wcsncasecmp_instead

#pragma push_macro("wcsncmp")
#undef wcsncmp
#define wcsncmp Please_use_SDL_wcsncmp_instead

#pragma push_macro("wcsstr")
#undef wcsstr
#define wcsstr Please_use_SDL_wcsstr_instead

#endif

#endif /* SDL_name_checking_h_ */


