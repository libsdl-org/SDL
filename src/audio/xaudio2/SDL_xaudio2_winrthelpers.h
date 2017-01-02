/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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

//
// Re-implementation of methods removed from XAudio2 (in WinRT):
//

typedef struct XAUDIO2_DEVICE_DETAILS
{
    WCHAR DeviceID[256];
    WCHAR DisplayName[256];
    /* Other fields exist in the pre-Windows 8 version of this struct, however
       they weren't used by SDL, so they weren't added.
    */
} XAUDIO2_DEVICE_DETAILS;


#ifdef __cplusplus
extern "C" {
#endif

HRESULT IXAudio2_GetDeviceCount(IXAudio2 * unused, UINT32 * devcount);
HRESULT IXAudio2_GetDeviceDetails(IXAudio2 * unused, UINT32 index, XAUDIO2_DEVICE_DETAILS * details);

#ifdef __cplusplus
}
#endif


//
// C-style macros to call XAudio2's methods in C++:
//
#ifdef __cplusplus
/*
#define IXAudio2_CreateMasteringVoice(A, B, C, D, E, F, G) (A)->CreateMasteringVoice((B), (C), (D), (E), (F), (G))
#define IXAudio2_CreateSourceVoice(A, B, C, D, E, F, G, H) (A)->CreateSourceVoice((B), (C), (D), (E), (F), (G), (H))
#define IXAudio2_QueryInterface(A, B, C) (A)->QueryInterface((B), (C))
#define IXAudio2_Release(A) (A)->Release()
#define IXAudio2_StartEngine(A) (A)->StartEngine()
#define IXAudio2_StopEngine(A) (A)->StopEngine()

#define IXAudio2MasteringVoice_DestroyVoice(A) (A)->DestroyVoice()

#define IXAudio2SourceVoice_DestroyVoice(A) (A)->DestroyVoice()
#define IXAudio2SourceVoice_Discontinuity(A) (A)->Discontinuity()
#define IXAudio2SourceVoice_FlushSourceBuffers(A) (A)->FlushSourceBuffers()
#define IXAudio2SourceVoice_GetState(A, B) (A)->GetState((B))
#define IXAudio2SourceVoice_Start(A, B, C) (A)->Start((B), (C))
#define IXAudio2SourceVoice_Stop(A, B, C) (A)->Stop((B), (C))
#define IXAudio2SourceVoice_SubmitSourceBuffer(A, B, C) (A)->SubmitSourceBuffer((B), (C))
*/
#endif // ifdef __cplusplus
