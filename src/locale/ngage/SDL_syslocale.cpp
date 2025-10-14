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
#include "../SDL_syslocale.h"
#include "SDL_internal.h"

#include <bautils.h>
#include <e32base.h>
#include <e32cons.h>
#include <e32std.h>

bool SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    TLanguage language = User::Language();
    const char *locale;

    switch (language) {
    case ELangFrench:
    case ELangSwissFrench:
        locale = "fr_CH";
        break;
    case ELangBelgianFrench:
        locale = "fr_BE";
        break;
    case ELangInternationalFrench:
        locale = "fr_FR";
        break;
    case ELangGerman:
    case ELangSwissGerman:
    case ELangAustrian:
        locale = "de_DE";
        break;
    case ELangSpanish:
    case ELangInternationalSpanish:
    case ELangLatinAmericanSpanish:
        locale = "es_ES";
        break;
    case ELangItalian:
    case ELangSwissItalian:
        locale = "it_IT";
        break;
    case ELangSwedish:
    case ELangFinlandSwedish:
        locale = "sv_SE";
        break;
    case ELangDanish:
        locale = "da_DK";
        break;
    case ELangNorwegian:
    case ELangNorwegianNynorsk:
        locale = "no_NO";
        break;
    case ELangFinnish:
        locale = "fi_FI";
        break;
    case ELangPortuguese:
        locale = "pt_PT";
        break;
    case ELangBrazilianPortuguese:
        locale = "pt_BR";
        break;
    case ELangTurkish:
    case ELangCyprusTurkish:
        locale = "tr_TR";
        break;
    case ELangIcelandic:
        locale = "is_IS";
        break;
    case ELangRussian:
        locale = "ru_RU";
        break;
    case ELangHungarian:
        locale = "hu_HU";
        break;
    case ELangDutch:
        locale = "nl_NL";
        break;
    case ELangBelgianFlemish:
        locale = "nl_BE";
        break;
    case ELangAustralian:
    case ELangNewZealand:
        locale = "en_AU";
        break;
    case ELangCzech:
        locale = "cs_CZ";
        break;
    case ELangSlovak:
        locale = "sk_SK";
        break;
    case ELangPolish:
        locale = "pl_PL";
        break;
    case ELangSlovenian:
        locale = "sl_SI";
        break;
    case ELangTaiwanChinese:
        locale = "zh_TW";
        break;
    case ELangHongKongChinese:
        locale = "zh_HK";
        break;
    case ELangPrcChinese:
        locale = "zh_CN";
        break;
    case ELangJapanese:
        locale = "ja_JP";
        break;
    case ELangThai:
        locale = "th_TH";
        break;
    case ELangAfrikaans:
        locale = "af_ZA";
        break;
    case ELangAlbanian:
        locale = "sq_AL";
        break;
    case ELangAmharic:
        locale = "am_ET";
        break;
    case ELangArabic:
        locale = "ar_SA";
        break;
    case ELangArmenian:
        locale = "hy_AM";
        break;
    case ELangAzerbaijani:
        locale = "az_AZ";
        break;
    case ELangBelarussian:
        locale = "be_BY";
        break;
    case ELangBengali:
        locale = "bn_IN";
        break;
    case ELangBulgarian:
        locale = "bg_BG";
        break;
    case ELangBurmese:
        locale = "my_MM";
        break;
    case ELangCatalan:
        locale = "ca_ES";
        break;
    case ELangCroatian:
        locale = "hr_HR";
        break;
    case ELangEstonian:
        locale = "et_EE";
        break;
    case ELangFarsi:
        locale = "fa_IR";
        break;
    case ELangCanadianFrench:
        locale = "fr_CA";
        break;
    case ELangScotsGaelic:
        locale = "gd_GB";
        break;
    case ELangGeorgian:
        locale = "ka_GE";
        break;
    case ELangGreek:
    case ELangCyprusGreek:
        locale = "el_GR";
        break;
    case ELangGujarati:
        locale = "gu_IN";
        break;
    case ELangHebrew:
        locale = "he_IL";
        break;
    case ELangHindi:
        locale = "hi_IN";
        break;
    case ELangIndonesian:
        locale = "id_ID";
        break;
    case ELangIrish:
        locale = "ga_IE";
        break;
    case ELangKannada:
        locale = "kn_IN";
        break;
    case ELangKazakh:
        locale = "kk_KZ";
        break;
    case ELangKhmer:
        locale = "km_KH";
        break;
    case ELangKorean:
        locale = "ko_KR";
        break;
    case ELangLao:
        locale = "lo_LA";
        break;
    case ELangLatvian:
        locale = "lv_LV";
        break;
    case ELangLithuanian:
        locale = "lt_LT";
        break;
    case ELangMacedonian:
        locale = "mk_MK";
        break;
    case ELangMalay:
        locale = "ms_MY";
        break;
    case ELangMalayalam:
        locale = "ml_IN";
        break;
    case ELangMarathi:
        locale = "mr_IN";
        break;
    case ELangMoldavian:
        locale = "ro_MD";
        break;
    case ELangMongolian:
        locale = "mn_MN";
        break;
    case ELangPunjabi:
        locale = "pa_IN";
        break;
    case ELangRomanian:
        locale = "ro_RO";
        break;
    case ELangSerbian:
        locale = "sr_RS";
        break;
    case ELangSinhalese:
        locale = "si_LK";
        break;
    case ELangSomali:
        locale = "so_SO";
        break;
    case ELangSwahili:
        locale = "sw_KE";
        break;
    case ELangTajik:
        locale = "tg_TJ";
        break;
    case ELangTamil:
        locale = "ta_IN";
        break;
    case ELangTelugu:
        locale = "te_IN";
        break;
    case ELangTibetan:
        locale = "bo_CN";
        break;
    case ELangTigrinya:
        locale = "ti_ET";
        break;
    case ELangTurkmen:
        locale = "tk_TM";
        break;
    case ELangUkrainian:
        locale = "uk_UA";
        break;
    case ELangUrdu:
        locale = "ur_PK";
        break;
    case ELangUzbek:
        locale = "uz_UZ";
        break;
    case ELangVietnamese:
        locale = "vi_VN";
        break;
    case ELangWelsh:
        locale = "cy_GB";
        break;
    case ELangZulu:
        locale = "zu_ZA";
        break;
    case ELangEnglish:
        locale = "en_GB";
        break;
    case ELangAmerican:
    case ELangCanadianEnglish:
    case ELangInternationalEnglish:
    case ELangSouthAfricanEnglish:
    default:
        locale = "en_US";
        break;
    }

    SDL_strlcpy(buf, locale, buflen);

    return true;
}
