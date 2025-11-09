#pragma once
#include <d3d11.h>
#include "GameEngine.h"
#include <xaudio2.h>

template <class T>
// Release
inline void SafeRelease(T** ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease)
    {
        (*ppInterfaceToRelease)->Release();
        *ppInterfaceToRelease = nullptr;
    }
}

// IXAudio2SourceVoice
template <>
inline void SafeRelease<IXAudio2SourceVoice>(IXAudio2SourceVoice** ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease)
    {
        (*ppInterfaceToRelease)->DestroyVoice();
        *ppInterfaceToRelease = nullptr;
    }
}

// IXAudio2MasteringVoice
template <>
inline void SafeRelease<IXAudio2MasteringVoice>(IXAudio2MasteringVoice** ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease)
    {
        (*ppInterfaceToRelease)->DestroyVoice();
        *ppInterfaceToRelease = nullptr;
    }
}

// IXAudio2
template <>
inline void SafeRelease<IXAudio2>(IXAudio2** ppInterfaceToRelease)
{
    if (*ppInterfaceToRelease)
    {
        (*ppInterfaceToRelease)->Release();
        *ppInterfaceToRelease = nullptr;
    }
}
