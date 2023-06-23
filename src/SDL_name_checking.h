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

#ifdef isalpha
#undef isalpha
#endif

#ifdef isalnum
#undef isalnum
#endif

#ifdef isblank
#undef isblank
#endif
#ifdef iscntrl
#undef iscntrl
#endif

#ifdef isdigit
#undef isdigit
#endif

#ifdef isupper
#undef isupper
#endif

#ifdef isxdigit
#undef isxdigit
#endif

#ifdef tolower
#undef tolower
#endif

#ifdef toupper
#undef toupper
#endif

#ifdef isgraph
#undef isgraph
#endif

#ifdef islower
#undef islower
#endif

#ifdef isprint
#undef isprint
#endif

#ifdef ispunct
#undef ispunct
#endif

#ifdef isspace
#undef isspace
#endif









#define abs Please_use_SDL_abs_instead
#define acosf Please_use_SDL_acosf_instead
#define acos Please_use_SDL_acos_instead
#define asinf Please_use_SDL_asinf_instead
#define asin Please_use_SDL_asin_instead
#define asprintf Please_use_SDL_asprintf_instead
#define atan2f Please_use_SDL_atan2f_instead
#define atan2 Please_use_SDL_atan2_instead
#define atanf Please_use_SDL_atanf_instead
#define atan Please_use_SDL_atan_instead
#define atof Please_use_SDL_atof_instead
#define atoi Please_use_SDL_atoi_instead
#define calloc Please_use_SDL_calloc_instead
#define ceilf Please_use_SDL_ceilf_instead
#define ceil Please_use_SDL_ceil_instead
#define copysignf Please_use_SDL_copysignf_instead
#define copysign Please_use_SDL_copysign_instead
#define cosf Please_use_SDL_cosf_instead
#define cos Please_use_SDL_cos_instead
#define crc32 Please_use_SDL_crc32_instead
#define expf Please_use_SDL_expf_instead
#define exp Please_use_SDL_exp_instead
#define fabsf Please_use_SDL_fabsf_instead
#define fabs Please_use_SDL_fabs_instead
#define floorf Please_use_SDL_floorf_instead
#define floor Please_use_SDL_floor_instead
#define fmodf Please_use_SDL_fmodf_instead
#define fmod Please_use_SDL_fmod_instead
#define getenv Please_use_SDL_getenv_instead
#define isalnum Please_use_SDL_isalnum_instead
#define isalpha Please_use_SDL_isalpha_instead
#define isblank Please_use_SDL_isblank_instead
#define iscntrl Please_use_SDL_iscntrl_instead
#define isdigit Please_use_SDL_isdigit_instead
#define isgraph Please_use_SDL_isgraph_instead
#define islower Please_use_SDL_islower_instead
#define isprint Please_use_SDL_isprint_instead
#define ispunct Please_use_SDL_ispunct_instead
#define isspace Please_use_SDL_isspace_instead
#define isupper Please_use_SDL_isupper_instead
#define isxdigit Please_use_SDL_isxdigit_instead
#define itoa Please_use_SDL_itoa_instead
#define lltoa Please_use_SDL_lltoa_instead
#define log10f Please_use_SDL_log10f_instead
#define log10 Please_use_SDL_log10_instead
#define logf Please_use_SDL_logf_instead
#define lroundf Please_use_SDL_lroundf_instead
#define lround Please_use_SDL_lround_instead
#define ltoa Please_use_SDL_ltoa_instead
#define malloc Please_use_SDL_malloc_instead
#define memcmp Please_use_SDL_memcmp_instead
#define memcpy4 Please_use_SDL_memcpy4_instead
#define memcpy Please_use_SDL_memcpy_instead
#define memmove Please_use_SDL_memmove_instead
#define memset Please_use_SDL_memset_instead
#define powf Please_use_SDL_powf_instead
#define pow Please_use_SDL_pow_instead
#define qsort Please_use_SDL_qsort_instead
#define realloc Please_use_SDL_realloc_instead
#define roundf Please_use_SDL_roundf_instead
#define round Please_use_SDL_round_instead
#define scalbnf Please_use_SDL_scalbnf_instead
#define scalbn Please_use_SDL_scalbn_instead
#define setenv Please_use_SDL_setenv_instead
#define sinf Please_use_SDL_sinf_instead
#define sin Please_use_SDL_sin_instead
#define snprintf Please_use_SDL_snprintf_instead
#define sqrtf Please_use_SDL_sqrtf_instead
#define sqrt Please_use_SDL_sqrt_instead
#define sscanf Please_use_SDL_sscanf_instead
#define strcasecmp Please_use_SDL_strcasecmp_instead
#define strchr Please_use_SDL_strchr_instead
#define strcmp Please_use_SDL_strcmp_instead
#define strdup Please_use_SDL_strdup_instead
#define strlcat Please_use_SDL_strlcat_instead
#define strlcpy Please_use_SDL_strlcpy_instead
#define strlen Please_use_SDL_strlen_instead
#define strlwr Please_use_SDL_strlwr_instead
#define strncasecmp Please_use_SDL_strncasecmp_instead
#define strncmp Please_use_SDL_strncmp_instead
#define strrchr Please_use_SDL_strrchr_instead
#define strrev Please_use_SDL_strrev_instead
#define strstr Please_use_SDL_strstr_instead
#define strtod Please_use_SDL_strtod_instead
#define strtokr Please_use_SDL_strtokr_instead
#define strtoll Please_use_SDL_strtoll_instead
#define strtol Please_use_SDL_strtol_instead
#define strtoul Please_use_SDL_strtoul_instead
#define strupr Please_use_SDL_strupr_instead
#define tanf Please_use_SDL_tanf_instead
#define tan Please_use_SDL_tan_instead
#define tolower Please_use_SDL_tolower_instead
#define toupper Please_use_SDL_toupper_instead
#define truncf Please_use_SDL_truncf_instead
#define trunc Please_use_SDL_trunc_instead
#define uitoa Please_use_SDL_uitoa_instead
#define ulltoa Please_use_SDL_ulltoa_instead
#define ultoa Please_use_SDL_ultoa_instead
#define utf8strlcpy Please_use_SDL_utf8strlcpy_instead
#define utf8strlen Please_use_SDL_utf8strlen_instead
#define vasprintf Please_use_SDL_vasprintf_instead
#define vsnprintf Please_use_SDL_vsnprintf_instead
#define vsscanf Please_use_SDL_vsscanf_instead
#define wcscasecmp Please_use_SDL_wcscasecmp_instead
#define wcscmp Please_use_SDL_wcscmp_instead
#define wcsdup Please_use_SDL_wcsdup_instead
#define wcslcat Please_use_SDL_wcslcat_instead
#define wcslcpy Please_use_SDL_wcslcpy_instead
#define wcslen Please_use_SDL_wcslen_instead
#define wcsncasecmp Please_use_SDL_wcsncasecmp_instead
#define wcsncmp Please_use_SDL_wcsncmp_instead
#define wcsstr Please_use_SDL_wcsstr_instead

#endif /* SDL_name_checking_h_ */
