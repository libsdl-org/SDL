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

#ifndef SDL_ngageaudio_hpp
#define SDL_ngageaudio_hpp

#include <e32base.h>
#include <e32std.h>
#include <mda/common/audio.h>
#include <mdaaudiooutputstream.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../SDL_sysaudio.h"
#include "SDL_ngageaudio.h"

#ifdef __cplusplus
}
#endif

TBool AudioIsReady();
void InitAudio(TInt *aLatency);
void DeinitAudio();

enum TAudioState
{
    EStateNone = 0,
    EStateOpening,
    EStatePlaying,
    EStateWriting,
    EStateDone
};


class CAudio : public CActive, public MMdaAudioOutputStreamCallback
{
  public:
    static CAudio *NewL(TInt aLatency);
    ~CAudio();

    void ConstructL(TInt aLatency);
    void Start();


    void RunL();
    void DoCancel();

    static TInt ProcessThreadCB(TAny * /*aPtr*/);

    void MaoscOpenComplete(TInt aError);
    void MaoscBufferCopied(TInt aError, const TDesC8 &aBuffer);
    void MaoscPlayComplete(TInt aError);


    TAudioState iState;
    CMdaAudioOutputStream *iStream; /*CMdaAudioOutputStream handler*/
    TPtr8 iBufDes;                  /* Descriptor for the buffer.*/
    TBool iStreamStarted;           /* have we initialized the audio stream?*/
    RThread iProcess;               /* thread handler */
    TBool iBufferReady;             /*  Signal AudioThreadCB the buffer is ready*/
  private:
    CAudio();
    void StartThread();
    void StopThread();

  
    TMdaAudioDataSettings iStreamSettings;
   
    TInt iLatency;           // Latency target in ms
    TInt iLatencySamples;    // Latency target in samples.
    TInt iMinWrite;          // Min number of samples to write per turn.
    TInt iMaxWrite;          // Max number of samples to write per turn.
   

    RTimer iTimer;
    TBool iTimerCreated;
    TBool iTimerActive;

};

#endif // SDL_ngageaudio_hpp