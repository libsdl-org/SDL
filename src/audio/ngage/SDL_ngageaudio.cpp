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

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_ngageaudio.h"
#include "../SDL_sysaudio.h"
#include "SDL_internal.h"

#ifdef __cplusplus
}
#endif

#ifdef SDL_AUDIO_DRIVER_NGAGE

#include "SDL_ngageaudio.hpp"

CAudio::CAudio() : CActive(EPriorityStandard), iBufDes(NULL, 0) {}

CAudio *CAudio::NewL(TInt aLatency)
{
    CAudio *self = new (ELeave) CAudio();
    CleanupStack::PushL(self);
    self->ConstructL(aLatency);
    CleanupStack::Pop(self);
    return self;
}

void CAudio::ConstructL(TInt aLatency)
{
    CActiveScheduler::Add(this);
    User::LeaveIfError(iTimer.CreateLocal());
    iTimerCreated = ETrue;

    iStream = CMdaAudioOutputStream::NewL(*this);
    if (!iStream) {
        SDL_Log("Error: Failed to create audio stream");
        User::Leave(KErrNoMemory);
    }

    iLatency = aLatency;
    iLatencySamples = aLatency * 8; // 8kHz.

    // Determine minimum and maximum number of samples to write with one
    // WriteL request.
    iMinWrite = iLatencySamples / 8;
    iMaxWrite = iLatencySamples / 2;

    // Set defaults.
    iState = EStateNone;
    iTimerCreated = EFalse;
    iTimerActive = EFalse;
}

CAudio::~CAudio()
{
    if (iStream) {
        iStream->Stop();

        while (iState != EStateDone) {
            User::After(100000); // 100ms.
        }

        delete iStream;
    }
}

void CAudio::Start()
{
    if (iStream) {
        // Set to 8kHz mono audio.
        iStreamSettings.iChannels = TMdaAudioDataSettings::EChannelsMono;
        iStreamSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;
        iStream->Open(&iStreamSettings);
        iState = EStateOpening;
    } else {
        SDL_Log("Error: Failed to open audio stream");
    }
}



void CAudio::RunL()
{
    iTimerActive = EFalse;

}

void CAudio::DoCancel()
{
    iTimerActive = EFalse;
    iTimer.Cancel();
}

void CAudio::StartThread()
{
    TInt heapMinSize = 8192;        // 8 KB initial heap size.
    TInt heapMaxSize = 1024 * 1024; // 1 MB maximum heap size.


    TInt err = iProcess.Create(_L("ProcessThread"), ProcessThreadCB, KDefaultStackSize * 2, heapMinSize, heapMaxSize, this);
    if (err == KErrNone)
    {
        TThreadPriority prio = EPriorityLess;

        const char *prioHint = SDL_GetHint(SDL_HINT_AUDIO_NGAGE_PROCESS_PRIORITY);
        if (prioHint) {
            // Symbian priorities: 10 (MuchLess), 20 (Less), 30 (Normal), 40 (More)
            prio = (TThreadPriority)SDL_atoi(prioHint);
            RThread().SetPriority(prio);
        }


        iProcess.SetPriority(prio);
        iProcess.Resume();
    } else {
        SDL_Log("Error: Failed to create audio processing thread: %d", err);
    }
}

void CAudio::StopThread()
{
    if (iStreamStarted) {
        iProcess.Kill(KErrNone);
        iProcess.Close();
        iStreamStarted = EFalse;
    }
}

/***************************************************
* ProcessThreadCB -
*
* This thread calls the SDL mixer when the buffer is ready and self->iState == EStatePlaying (basically other than initial stated, when not writing)
*
* It only mixes, never calls WriteL
****************************************************/

TInt CAudio::ProcessThreadCB(TAny *aPtr)
{
    CTrapCleanup *cleanup = CTrapCleanup::New();
    if (!cleanup)
        return KErrNoMemory;
  
    CAudio *self = static_cast<CAudio *>(aPtr);
    SDL_AudioDevice *device = NGAGE_GetAudioDeviceAddr();


    TInt processTick = 40000; // Default 40ms
    const char *tickHint = SDL_GetHint(SDL_HINT_AUDIO_NGAGE_PROCESS_TICK);
    if (tickHint)
    {
        processTick = SDL_atoi(tickHint) * 1000;
    }

    while (self->iStreamStarted)
    {
        if (self->iState == EStatePlaying && !self->iBufferReady)
        {
             /* Ask SDL to mix audio into buffer[fill_index]*/
            SDL_PlaybackAudioThreadIterate(device);

             /*  Signal AudioThreadCB to write it*/
            self->iBufferReady = ETrue;
        }
        else
        {
            /*if we are not ready to obtain the mix data we sleep a bit this thread*/
            User::After(processTick);
        }
    }

    delete cleanup;
    return KErrNone;
}

static TBool gAudioRunning;
/***************************************************
* AudioThreadCB -
*
* This thread owns the scheduler and calls WriteL, wich queues the assigned sound buffer to be played
****************************************************/

TInt AudioThreadCB(TAny *aParams)
{
    CTrapCleanup *cleanup = CTrapCleanup::New();
    CActiveScheduler *scheduler = new CActiveScheduler();
    CActiveScheduler::Install(scheduler);
    
    TRAPD(err, {
        TInt latency = *(TInt *)aParams;
        CAudio *audio = CAudio::NewL(latency);
        CleanupStack::PushL(audio);

        audio->iBufferReady = EFalse;

        gAudioRunning = ETrue;
        audio->Start();
      

        TInt processTick = 5000; // Default 5ms
        const char *tickHint = SDL_GetHint(SDL_HINT_AUDIO_NGAGE_PROCESS_TICK);
        if (tickHint) {
            processTick = SDL_atoi(tickHint) * 1000;
        }


        while (gAudioRunning)
        {
            TInt error;
            CActiveScheduler::RunIfReady(error, CActive::EPriorityIdle);
           
            /*there is some mix data sound ready*/
            if (audio->iBufferReady)
            {
                audio->iBufferReady = EFalse;

                SDL_AudioDevice *device = NGAGE_GetAudioDeviceAddr();

                if (device && device->hidden)
                {
                    SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;
                    audio->iState = EStateWriting;
                    /*sends the chuck mixed to the queue*/
                    audio->iBufDes.Set(phdata->buffer[phdata->fill_index], device->buffer_size, device->buffer_size);
                    TRAPD(werr, audio->iStream->WriteL(audio->iBufDes));

                    if (werr != KErrNone)
                    {
                        /*asks ProcessThreadCB to bring another mix chunk*/
                        audio->iState = EStatePlaying;
                    }
                    else
                    {
                        /*swap buffers so while this buffer is being played we can get the mix of the next one if we can*/
                        phdata->fill_index = 1 - phdata->fill_index;
                    }
                }
            }

            /*sleep a bit this thread not to hog the CPU*/
            User::After(processTick);
        }

        CleanupStack::PopAndDestroy(audio);
    });

    delete scheduler;
    delete cleanup;
    return err;
}

/***************************************************
* MaoscOpenComplete -
*
* Opens the audiostream
*
* *******************************************************/

void CAudio::MaoscOpenComplete(TInt aError)
{
    if (aError == KErrNone)
    {
        /*setting the volume to max, users can change the volume later of their channels individually in code*/
        iStream->SetVolume(iStream->MaxVolume());
        iStreamStarted = ETrue;

        /* Wait until SDL has set devptr and hidden data*/
        SDL_AudioDevice *device = NULL;
        while (!device || !device->hidden) {
            User::After(10000); // 10ms poll
            device = NGAGE_GetAudioDeviceAddr();
        }

        /* Now start the ProcessThreadCB thread*/
        StartThread();

        /* Kickstart: device is guaranteed valid now*/
        this->iState = EStatePlaying;
       

    }
    else
    {
        SDL_Log("Error: Failed to open audio stream: %d", aError);
    }
}

/***************************************************
 * MaoscOpenComplete -
 *
 * This signals the mixed data has been finally copied to the designated audio buffer
 *
 * *******************************************************/

void CAudio::MaoscBufferCopied(TInt aError, const TDesC8 & /*aBuffer*/)
{
    if (aError == KErrNone)
    {
        iState = EStatePlaying;
    }
    else if (aError == KErrAbort)
    {
        /* The stream has been stopped.*/
        iState = EStateDone;
    }
    else
    {
        SDL_Log("Error: Failed to copy audio buffer: %d", aError);
    }
}

/***************************************************
 * MaoscPlayComplete -
 *
 * The result after playing the mixed chunk
 *
 * *******************************************************/

void CAudio::MaoscPlayComplete(TInt aError)
{
   
    /* If we finish due to an underflow, we'll need to restart playback.
     Normally KErrUnderlow is raised   at stream end, but in our case the API
     should never see the stream end -- we are continuously feeding it more
     data!  Many underflow errors mean that the latency target is too low.*/
    if (aError == KErrUnderflow)
    {
        /*  Restart the stream hardware */
        iStream->Stop();
        TInt ignoredError;
        TRAP(ignoredError, iStream->SetAudioPropertiesL(TMdaAudioDataSettings::ESampleRate8000Hz, TMdaAudioDataSettings::EChannelsMono));

        /* This wakes up ProcessThreadCB so it can call SDL_PlaybackAudioThreadIterate*/
        iState = EStatePlaying;

        return;
    } else if (aError != KErrNone) {
        
    }

    /* We shouldn't get here.*/
    SDL_Log("%s: %d", SDL_FUNCTION, aError);
}



TBool AudioIsReady()
{
    return gAudioRunning;
}



RThread audioThread;

void InitAudio(TInt *aLatency)
{
    // Check if the user has provided a custom latency value via a hint
    const char *hint = SDL_GetHint(SDL_HINT_AUDIO_NGAGE_LATENCY);
    if (hint) {
        *aLatency = (TInt)SDL_atoi(hint);
    }

    _LIT(KAudioThreadName, "AudioThread");

    TInt err = audioThread.Create(KAudioThreadName, AudioThreadCB, KDefaultStackSize, 0, aLatency);
    if (err != KErrNone) {
        User::Leave(err);
    }

    audioThread.Resume();
}

void DeinitAudio()
{
    gAudioRunning = EFalse;

    TRequestStatus status;
    audioThread.Logon(status);
    User::WaitForRequest(status);

    audioThread.Close();
}

#endif // SDL_AUDIO_DRIVER_NGAGE
