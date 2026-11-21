#include "sound.h"
#include "assets.h"
#include "globals.h"

#if CONTRA_SDL == 3
#include <limits.h>
#else
#include <SDL_mixer.h>
#endif
#include <stdio.h>
#include <string.h>

static const char *const effect_names[] = {
    "sfx/title.wav", "sfx/pause.wav", "sfx/p_landing.wav", "sfx/p_shock.wav", "sfx/p_death.wav",
    "sfx/n_gun.wav", "sfx/m_gun.wav", "sfx/f_gun.wav", "sfx/s_gun.wav", "sfx/l_gun.wav",
    "sfx/p_1up.wav", "sfx/bonus.wav", "sfx/hitsnd0.wav", "sfx/hitsnd1.wav", "sfx/hitsnd2.wav",
    "sfx/bombing0.wav", "sfx/bombing1.wav", "sfx/stage_clear.wav", "sfx/bombing2.wav", "sfx/stone_landing.wav",
    "sfx/pipebomb.wav", "sfx/flame.wav", "sfx/gameover.wav", "sfx/alarm.wav", "sfx/bombing3.wav",
    "sfx/motor.wav", "sfx/bombing4.wav", "sfx/robot_landing.wav", "sfx/airplane_motor.wav", "sfx/ending.wav",
};
_Static_assert(SDL_arraysize(effect_names) == MAX_SOUNDS, "Sound effect table must match MAX_SOUNDS");

static const char *const track_names[] = {
    "bgm/Contra_00.raw", "bgm/Contra_01.raw", "bgm/Contra_02.raw", "bgm/Contra_04.raw", "bgm/Contra_05.raw",
    "bgm/Contra_07.raw", "bgm/Contra_boss1.raw", "bgm/Contra_boss2.raw", "bgm/Contra_boss3.raw", "bgm/newboss.raw",
};
static const size_t loop_header_size = sizeof(Uint32);

struct track {
    Uint8 *data;
    size_t size;
    size_t loop_start;
};

#if CONTRA_SDL == 3
static const SDL_AudioSpec music_format = { .format = SDL_AUDIO_U8, .channels = 1, .freq = 22050 };
static struct effect {
    SDL_AudioSpec format;
    Uint8 *data;
    Uint32 size;
} effects[MAX_SOUNDS];
static SDL_AudioDeviceID device;
static SDL_AudioStream *channels[8];
static SDL_AudioStream *music_stream;
#else
static Mix_Chunk *effects[MAX_SOUNDS];
static SDL_mutex *music_mutex;
#endif
static struct track tracks[SDL_arraysize(track_names)];
static int selected_tracks[2];
static bool audio_open;
static const struct track *playing;
static size_t music_position;
static bool music_paused;

static size_t bgm_chunk_size(size_t remaining){
    const size_t available = playing->size - music_position;
    return remaining < available ? remaining : available;
}

static void advance_bgm(size_t count){
    music_position += count;
    if (music_position == playing->size)
        music_position = playing->loop_start;
}

#if CONTRA_SDL == 3
static void SDLCALL feed_bgm(void *context, SDL_AudioStream *stream, int additional, int total){
    (void)context;
    (void)total;
    if (!playing || music_paused || additional <= 0)
        return;

    size_t remaining = (size_t)additional;
    const Uint8 *samples = playing->data + loop_header_size;
    while (remaining){
        const size_t count = bgm_chunk_size(remaining);
        if (!SDL_PutAudioStreamData(stream, samples + music_position, (int)count))
            return;
        remaining -= count;
        advance_bgm(count);
    }
}
#else
static void SDLCALL feed_bgm(void *context, Uint8 *output, int length){
    (void)context;
    if (SDL_LockMutex(music_mutex) < 0){
        memset(output, 0x80, length);
        return;
    }
    if (!playing || music_paused){
        memset(output, 0x80, length);
        SDL_UnlockMutex(music_mutex);
        return;
    }

    size_t remaining = (size_t)length;
    const Uint8 *samples = playing->data + loop_header_size;
    while (remaining){
        const size_t count = bgm_chunk_size(remaining);
        memcpy(output, samples + music_position, count);
        output += count;
        remaining -= count;
        advance_bgm(count);
    }
    SDL_UnlockMutex(music_mutex);
}
#endif

static bool load_track(struct track *track, const char *path){
    size_t file_size = 0;
    track->data = SDL_LoadFile(path, &file_size);
    if (!track->data || file_size <= loop_header_size){
        fprintf(stderr, "Cannot load music data: %s\n", path);
        return false;
    }
    Uint32 loop_start;
    memcpy(&loop_start, track->data, loop_header_size);
#if CONTRA_SDL == 3
    track->loop_start = SDL_Swap32LE(loop_start);
#else
    track->loop_start = SDL_SwapLE32(loop_start);
#endif
    track->size = file_size - loop_header_size;
    if (track->loop_start >= track->size){
        fprintf(stderr, "Invalid music loop point: %s\n", path);
        return false;
    }
    return true;
}

bool init_sound(const char *assets){
#if CONTRA_SDL == 3
    const bool initialized = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (initialized)
        device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &music_format);
    audio_open = device != 0;
#else
    const bool initialized = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
    audio_open = initialized && Mix_OpenAudioDevice(22050, AUDIO_U8, 1, 1024, NULL, 0) == 0;
#endif
    if (!audio_open){
        fprintf(stderr, "Audio unavailable; continuing without sound: %s\n", SDL_GetError());
        if (initialized)
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return true;
    }
#if CONTRA_SDL == 2
    music_mutex = SDL_CreateMutex();
    if (!music_mutex){
        fprintf(stderr, "Cannot initialize the music lock: %s\n", SDL_GetError());
        return false;
    }
#endif

    for (size_t index = 0; index < SDL_arraysize(effects); ++index){
        char *path = join_asset_path(assets, effect_names[index]);
        if (!path){
            fprintf(stderr, "Cannot allocate sound effect path: %s\n", effect_names[index]);
            return false;
        }
#if CONTRA_SDL == 3
        struct effect *effect = &effects[index];
        const bool loaded = SDL_LoadWAV(path, &effect->format, &effect->data, &effect->size);
        const bool too_large = loaded && effect->size > INT_MAX;
#else
        effects[index] = Mix_LoadWAV(path);
        const bool loaded = effects[index] != NULL;
        const bool too_large = false;
#endif
        if (!loaded)
            fprintf(stderr, "Cannot load %s: %s\n", path, SDL_GetError());
        if (too_large)
            fprintf(stderr, "Sound effect is too large: %s\n", path);
        SDL_free(path);
        if (!loaded || too_large)
            return false;
    }

    for (size_t index = 0; index < SDL_arraysize(tracks); ++index){
        char *path = join_asset_path(assets, track_names[index]);
        if (!path){
            fprintf(stderr, "Cannot allocate music path: %s\n", track_names[index]);
            return false;
        }
        const bool loaded = load_track(&tracks[index], path);
        SDL_free(path);
        if (!loaded)
            return false;
    }

#if CONTRA_SDL == 3
    for (size_t index = 0; index < SDL_arraysize(channels); ++index){
        channels[index] = SDL_CreateAudioStream(&music_format, NULL);
        if (!channels[index]){
            fprintf(stderr, "Cannot create sound effect stream: %s\n", SDL_GetError());
            return false;
        }
    }
    music_stream = SDL_CreateAudioStream(&music_format, NULL);
    if (!music_stream || !SDL_SetAudioStreamGetCallback(music_stream, feed_bgm, NULL) ||
        !SDL_BindAudioStreams(device, channels, SDL_arraysize(channels)) ||
        !SDL_BindAudioStream(device, music_stream)){
        fprintf(stderr, "Cannot initialize audio streams: %s\n", SDL_GetError());
        return false;
    }
#else
    Mix_HookMusic(feed_bgm, NULL);
#endif
    return true;
}

void shutdown_sound(void){
    if (!audio_open)
        return;
#if CONTRA_SDL == 3
    SDL_CloseAudioDevice(device);
    device = 0;
    if (music_stream)
        SDL_DestroyAudioStream(music_stream);
    music_stream = NULL;
    for (size_t index = 0; index < SDL_arraysize(channels); ++index){
        if (channels[index])
            SDL_DestroyAudioStream(channels[index]);
        channels[index] = NULL;
    }
    for (size_t index = 0; index < SDL_arraysize(effects); ++index){
        SDL_free(effects[index].data);
        effects[index] = (struct effect){0};
    }
#else
    Mix_HookMusic(NULL, NULL);
    Mix_HaltChannel(-1);
    for (size_t index = 0; index < SDL_arraysize(effects); ++index){
        if (effects[index])
            Mix_FreeChunk(effects[index]);
        effects[index] = NULL;
    }
    Mix_CloseAudio();
    if (music_mutex)
        SDL_DestroyMutex(music_mutex);
    music_mutex = NULL;
#endif
    playing = NULL;
    music_paused = false;
    for (size_t index = 0; index < SDL_arraysize(tracks); ++index){
        SDL_free(tracks[index].data);
        tracks[index] = (struct track){0};
    }
    audio_open = false;
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

#if CONTRA_SDL == 3
SDL_AudioDeviceID sound_device(void){
    return device;
}
#endif

void play_sound(int channel, int effect){
    if (!audio_open || effect < 0 || effect >= MAX_SOUNDS)
        return;
#if CONTRA_SDL == 3
    if (channel == -1){
        for (channel = 0; channel < (int)SDL_arraysize(channels); ++channel){
            if (SDL_GetAudioStreamQueued(channels[channel]) == 0)
                break;
        }
    }
    if (channel < 0 || channel >= (int)SDL_arraysize(channels))
        return;
    SDL_AudioStream *stream = channels[channel];
    const struct effect *sound = &effects[effect];
    if (!SDL_LockAudioStream(stream))
        return;
    if (SDL_ClearAudioStream(stream) && SDL_SetAudioStreamFormat(stream, &sound->format, NULL) &&
        SDL_PutAudioStreamData(stream, sound->data, (int)sound->size))
        SDL_FlushAudioStream(stream);
    SDL_UnlockAudioStream(stream);
#else
    Mix_PlayChannel(channel, effects[effect], 0);
#endif
}

void stop_sound(int channel){
    if (!audio_open)
        return;
#if CONTRA_SDL == 3
    if (channel == -1){
        for (size_t index = 0; index < SDL_arraysize(channels); ++index)
            SDL_ClearAudioStream(channels[index]);
    }else if (channel >= 0 && channel < (int)SDL_arraysize(channels)){
        SDL_ClearAudioStream(channels[channel]);
    }
#else
    Mix_HaltChannel(channel);
#endif
}

void set_bgm_tracks(int level, int boss){
    selected_tracks[0] = level;
    selected_tracks[1] = boss;
}

static bool lock_music(void){
#if CONTRA_SDL == 3
    return music_stream && SDL_LockAudioStream(music_stream);
#else
    return music_mutex && SDL_LockMutex(music_mutex) == 0;
#endif
}

static void unlock_music(void){
#if CONTRA_SDL == 3
    SDL_UnlockAudioStream(music_stream);
#else
    SDL_UnlockMutex(music_mutex);
#endif
}

static void change_bgm(const struct track *track){
    if (!lock_music())
        return;
#if CONTRA_SDL == 3
    SDL_ClearAudioStream(music_stream);
#endif
    playing = track;
    music_position = 0;
    music_paused = false;
    unlock_music();
}

void play_bgm(int index){
    change_bgm(&tracks[selected_tracks[index]]);
}

void stop_bgm(void){
    change_bgm(NULL);
}

static void set_bgm_paused(bool paused){
    if (!lock_music())
        return;
    music_paused = paused;
    unlock_music();
}

void pause_bgm(void){
    set_bgm_paused(true);
}

void resume_bgm(void){
    set_bgm_paused(false);
}
