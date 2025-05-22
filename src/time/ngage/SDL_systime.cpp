/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_TIME_NGAGE

#include <bautils.h>
#include <e32base.h>
#include <e32cons.h>
#include <e32std.h>

#ifdef __cplusplus
extern "C" {
#endif

static TTime UnixEpoch();

void SDL_GetSystemTimeLocalePreferences(SDL_DateFormat *df, SDL_TimeFormat *tf)
{
    TLanguage language = User::Language();

    switch (language) {
    case ELangFrench:
    case ELangSwissFrench:
    case ELangBelgianFrench:
    case ELangInternationalFrench:
    case ELangGerman:
    case ELangSwissGerman:
    case ELangAustrian:
    case ELangSpanish:
    case ELangInternationalSpanish:
    case ELangLatinAmericanSpanish:
    case ELangItalian:
    case ELangSwissItalian:
    case ELangSwedish:
    case ELangFinlandSwedish:
    case ELangDanish:
    case ELangNorwegian:
    case ELangNorwegianNynorsk:
    case ELangFinnish:
    case ELangPortuguese:
    case ELangBrazilianPortuguese:
    case ELangTurkish:
    case ELangCyprusTurkish:
    case ELangIcelandic:
    case ELangRussian:
    case ELangHungarian:
    case ELangDutch:
    case ELangBelgianFlemish:
    case ELangCzech:
    case ELangSlovak:
    case ELangPolish:
    case ELangSlovenian:
    case ELangTaiwanChinese:
    case ELangHongKongChinese:
    case ELangPrcChinese:
    case ELangJapanese:
    case ELangThai:
    case ELangAfrikaans:
    case ELangAlbanian:
    case ELangAmharic:
    case ELangArabic:
    case ELangArmenian:
    case ELangAzerbaijani:
    case ELangBelarussian:
    case ELangBengali:
    case ELangBulgarian:
    case ELangBurmese:
    case ELangCatalan:
    case ELangCroatian:
    case ELangEstonian:
    case ELangFarsi:
    case ELangScotsGaelic:
    case ELangGeorgian:
    case ELangGreek:
    case ELangCyprusGreek:
    case ELangGujarati:
    case ELangHebrew:
    case ELangHindi:
    case ELangIndonesian:
    case ELangIrish:
    case ELangKannada:
    case ELangKazakh:
    case ELangKhmer:
    case ELangKorean:
    case ELangLao:
    case ELangLatvian:
    case ELangLithuanian:
    case ELangMacedonian:
    case ELangMalay:
    case ELangMalayalam:
    case ELangMarathi:
    case ELangMoldavian:
    case ELangMongolian:
    case ELangPunjabi:
    case ELangRomanian:
    case ELangSerbian:
    case ELangSinhalese:
    case ELangSomali:
    case ELangSwahili:
    case ELangTajik:
    case ELangTamil:
    case ELangTelugu:
    case ELangTibetan:
    case ELangTigrinya:
    case ELangTurkmen:
    case ELangUkrainian:
    case ELangUrdu:
    case ELangUzbek:
    case ELangVietnamese:
    case ELangWelsh:
    case ELangZulu:
        *df = SDL_DATE_FORMAT_DDMMYYYY;
        *tf = SDL_TIME_FORMAT_24HR;
        break;
    case ELangAmerican:
    case ELangCanadianEnglish:
    case ELangInternationalEnglish:
    case ELangSouthAfricanEnglish:
    case ELangAustralian:
    case ELangNewZealand:
    case ELangCanadianFrench:
        *df = SDL_DATE_FORMAT_MMDDYYYY;
        *tf = SDL_TIME_FORMAT_12HR;
        break;
    case ELangEnglish:
    case ELangOther:
    default:
        *df = SDL_DATE_FORMAT_DDMMYYYY;
        *tf = SDL_TIME_FORMAT_24HR;
        break;
    }
}

bool SDL_GetCurrentTime(SDL_Time *ticks)
{
    if (!ticks) {
        return SDL_InvalidParamError("ticks");
    }

    TTime now;
    now.UniversalTime();

    TTimeIntervalMicroSeconds interval = now.MicroSecondsFrom(UnixEpoch());
    TInt64 interval_ns = interval.Int64() * 1000;
    Uint32 ns_low = interval_ns.Low();
    Uint32 ns_high = interval_ns.High();

    *ticks = ((Uint64)ns_high << 32) | ns_low;

    return true;
}

static TTime UnixEpoch()
{
    _LIT(KUnixEpoch, "19700101:000000.000000");
    TTime epochTime;
    epochTime.Set(KUnixEpoch);
    return epochTime;
}

#ifdef __cplusplus
}
#endif

#endif // SDL_TIME_NGAGE
