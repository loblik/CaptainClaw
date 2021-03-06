#include <SDL2/SDL.h>
#include <fstream>
#include <vector>

#include "Audio.h"
#include "../Events/EventMgr.h"
#include "../Events/Events.h"

#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#include "../../../MidiProc/midiproc.h"
#endif

#ifdef PlaySound
#undef PlaySound
#endif

using namespace std;

const uint32_t MIDI_RPC_MAX_HANDSHAKE_TRIES = 250;

//############################################
//################# API ######################
//############################################

Audio::Audio()
    :
    m_bIsServerInitialized(false),
    m_bIsClientInitialized(false),
    m_bIsMidiRpcInitialized(false),
    m_bIsAudioInitialized(false),
    m_RpcBindingString(NULL),
    m_SoundVolume(0),
    m_MusicVolume(0),
    m_bSoundOn(true),
    m_bMusicOn(true)
{

}

Audio::~Audio()
{
    Terminate();
}

bool Audio::Initialize(const GameOptions& config)
{
    if (!SDL_WasInit(SDL_INIT_AUDIO))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Attempted to initialize Audio subsystem before SDL2 was initialized");
        return false;
    }

    // Setup audio mode
    if (Mix_OpenAudio(config.frequency, MIX_DEFAULT_FORMAT, config.soundChannels, config.chunkSize) != 0)
    {
        LOG_ERROR(std::string(Mix_GetError()));
        return false;
    }

    Mix_AllocateChannels(config.mixingChannels);

    m_SoundVolume = config.soundVolume;
    m_MusicVolume = config.musicVolume;
    m_bSoundOn = config.soundOn;
    m_bMusicOn = config.musicOn;

#ifdef _WIN32
    m_bIsMidiRpcInitialized = InitializeMidiRPC(config.midiRpcServerPath);
    if (!m_bIsMidiRpcInitialized)
    {
        return false;
    }
#endif //_WIN32

    SetSoundVolume(m_SoundVolume);
    SetMusicVolume(m_MusicVolume);

    m_bIsAudioInitialized = true;

    return true;
}

void Audio::Terminate()
{
#ifdef _WIN32
    TerminateMidiRPC();
#endif //_WIN32
}

void Audio::PlayMusic(const char* musicData, size_t musicSize, bool looping)
{
    if (!m_bMusicOn)
    {
        return;
    }
    
#ifdef _WIN32
    RpcTryExcept
    {
        MidiRPC_PrepareNewSong();
        MidiRPC_AddChunk(musicSize, (byte*)musicData);
        MidiRPC_PlaySong(looping);
        MidiRPC_ChangeVolume(m_MusicVolume);
    }
    RpcExcept(1)
    {
        //__LOG_ERROR("Audio::SetMusicVolume: Failed due to RPC exception");
    }
    RpcEndExcept;
#else
    SDL_RWops* pRWops = SDL_RWFromMem((void*)musicData, musicSize);
    Mix_Music* pMusic = Mix_LoadMUS_RW(pRWops, 0);
    if(!pMusic) {
        LOG_ERROR("Mix_LoadMUS_RW: " + std::string(Mix_GetError()));
    }
    Mix_PlayMusic(pMusic, looping ? -1 : 0);
#endif //_WIN32
}

// This is probably slow as fuck, should be removed, only used for debugging afaik
void Audio::PlayMusic(const char* musicPath, bool looping)
{
    if (!m_bMusicOn)
    {
        return;
    }

    std::ifstream musicFileStream(musicPath, std::ios::binary);
    if (!musicFileStream.is_open())
    {
        return;
    }

    // Read whole file
    std::vector<char> musicFileContents((std::istreambuf_iterator<char>(musicFileStream)), std::istreambuf_iterator<char>());
    if (!musicFileStream.good())
    {
        return;
    }

    PlayMusic(musicFileContents.data(), musicFileContents.size(), looping);
}

void Audio::PauseMusic()
{
#ifdef _WIN32
    RpcTryExcept
    {
        MidiRPC_PauseSong();
    }
        RpcExcept(1)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Audio::PauseMusic: Failed due to RPC exception");
    }
    RpcEndExcept
#else
    Mix_PauseMusic();
#endif //_WIN32
}

void Audio::ResumeMusic()
{
#ifdef _WIN32
    RpcTryExcept
    {
        MidiRPC_ResumeSong();
    }
        RpcExcept(1)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Audio::ResumeMusic: Failed due to RPC exception");
    }
    RpcEndExcept
#else
    Mix_ResumeMusic();
#endif //_WIN32
}

void Audio::StopMusic()
{
#ifdef _WIN32
    RpcTryExcept
    {
        MidiRPC_StopSong();
    }
        RpcExcept(1)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioMgr::StopMusic: Failed due to RPC exception");
    }
    RpcEndExcept
#else
    Mix_HaltMusic();
#endif //_WIN32
}

void Audio::SetMusicVolume(int volumePercentage)
{
    // Music has ~ 5x more potency than sound, so max is 20 instead of 100
    volumePercentage = min(volumePercentage, 20);
    if (volumePercentage < 0)
    {
        volumePercentage = 0;
    }
    m_MusicVolume = (int)((((float)volumePercentage) / 100.0f) * (float)MIX_MAX_VOLUME);

#ifdef _WIN32
    RpcTryExcept
    {
        MidiRPC_ChangeVolume(m_MusicVolume);
    }
        RpcExcept(1)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "AudioMgr::SetMusicVolume: Failed due to RPC exception");
    }
    RpcEndExcept
#else
    Mix_VolumeMusic(m_MusicVolume);
#endif //_WIN32
}

int Audio::GetMusicVolume()
{
    return ceil(((float)m_MusicVolume / (float)MIX_MAX_VOLUME) * 100.0f);
}

bool Audio::PlaySound(const char* soundData, size_t soundSize, int volumePercentage, int loops)
{
    SDL_RWops* soundRwOps = SDL_RWFromMem((void*)soundData, soundSize);
    Mix_Chunk* soundChunk = Mix_LoadWAV_RW(soundRwOps, 1);

    return PlaySound(soundChunk, volumePercentage, loops);
}

bool Audio::PlaySound(Mix_Chunk* sound, int volumePercentage, int loops)
{
    if (!m_bSoundOn)
    {
        return true;
    }

    int chunkVolume = (int)((((float)volumePercentage) / 100.0f) * (float)m_SoundVolume);

    Mix_VolumeChunk(sound, chunkVolume);
    if (Mix_PlayChannel(-1, sound, loops) == -1)
    {
        LOG_ERROR("Failed to play chunk: " + std::string(Mix_GetError()));
        return false;
    }

    return true;
}

void Audio::SetSoundVolume(int volumePercentage)
{
    volumePercentage = min(volumePercentage, 100);
    if (volumePercentage < 0)
    {
        volumePercentage = 0;
    }
    m_SoundVolume = (int)((((float)volumePercentage) / 100.0f) * (float)MIX_MAX_VOLUME);

    Mix_Volume(-1, m_SoundVolume);
}

int Audio::GetSoundVolume()
{
    return ceil(((float)m_SoundVolume / (float)MIX_MAX_VOLUME) * 100.0f);
}

void Audio::StopAllSounds()
{
    Mix_HaltChannel(-1);
    StopMusic();
}

void Audio::PauseAllSounds()
{
    Mix_Pause(-1);
#ifdef _WIN32
    MidiRPC_PauseSong();
#endif //_WIN32
}

void Audio::ResumeAllSounds()
{
    Mix_Resume(-1);
#ifdef _WIN32
    MidiRPC_ResumeSong();
#endif //_WIN32
}

#ifdef _WIN32
//############################################
//############## MIDI RPC ####################
//############################################

bool Audio::InitializeMidiRPC(const std::string& midiRpcServerPath)
{
    if (!InitializeMidiRPCServer(midiRpcServerPath))
    {
        return false;
    }

    if (!InitializeMidiRPCClient())
    {
        return false;
    }

    return true;
}

bool Audio::InitializeMidiRPCServer(const std::string& midiRpcServerPath)
{
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    BOOL doneCreateProc = CreateProcess(midiRpcServerPath.c_str(), NULL, NULL, NULL, FALSE,
                                           0, NULL, NULL, &si, &pi);
    if (doneCreateProc)
    {
        m_bIsServerInitialized = true;
        LOG("MIDI RPC Server started. [" + std::string(midiRpcServerPath) + "]");
    }
    else
    {
        LOG_ERROR("FAILED to start RPC MIDI Server. [" + std::string(midiRpcServerPath) + "]");
    }

    return (doneCreateProc != 0);
}

bool Audio::InitializeMidiRPCClient()
{
    RPC_STATUS rpcStatus;

    if (!m_bIsServerInitialized)
    {
        LOG_ERROR("Failed to initialize RPC MIDI Client - server was was not initialized");
        return false;
    }

    rpcStatus = RpcStringBindingCompose(NULL,
                                       (RPC_CSTR)("ncalrpc"),
                                       NULL,
                                       (RPC_CSTR)("2d4dc2f9-ce90-4080-8a00-1cb819086970"),
                                       NULL,
                                       &m_RpcBindingString);

    if (rpcStatus != 0)
    {
        LOG_ERROR("Failed to initialize RPC MIDI Client - RPC binding composition failed");
        return false;
    }

    rpcStatus = RpcBindingFromStringBinding(m_RpcBindingString, &hMidiRPCBinding);

    if (rpcStatus != 0)
    {
        LOG_ERROR("Failed to initialize RPC MIDI Client - RPC client binding failed");
        return false;
    }

    LOG("RPC Client successfully initialized");

    m_bIsClientInitialized = true;

    bool isServerListening = IsRPCServerListening();
    if (!isServerListening)
    {
        LOG_ERROR("Handshake between RPC Server and Client failed");
        return false;
    }
    else
    {
        LOG_ERROR("RPC Server and Client successfully handshaked");
    }

    return true;
}

bool Audio::IsRPCServerListening()
{
    if (!m_bIsClientInitialized || !m_bIsServerInitialized)
    {
        return false;
    }

    uint16_t tries = 0;
    while (RpcMgmtIsServerListening(hMidiRPCBinding) != RPC_S_OK)
    {
        SDL_Delay(10);
        if (tries++ >= MIDI_RPC_MAX_HANDSHAKE_TRIES)
        {
            return false;
        }
    }

    return true;
}

void Audio::TerminateMidiRPC()
{
    RpcTryExcept
    {
        MidiRPC_StopServer();
    }
    RpcExcept(1)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Audio::TerminateMidiRPC: Failed due to RPC exception");
    }
    RpcEndExcept;
}
#endif //_WIN32
