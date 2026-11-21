#include "sound.h"
#include "assets.h"
#include "globals.h"

#include <limits.h>
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
static const SDL_AudioSpec music_format = { .format = SDL_AUDIO_U8, .channels = 1, .freq = 22050 };

struct effect {
    SDL_AudioSpec format;
    Uint8 *data;
    Uint32 size;
};

struct track {
    Uint8 *data;
    size_t size;
    size_t loop_start;
};

static struct effect effects[MAX_SOUNDS];
static struct track tracks[SDL_arraysize(track_names)];
static int selected_tracks[2];
static SDL_AudioDeviceID device;
static SDL_AudioStream *channels[8];
static SDL_AudioStream *music_stream;
static const struct track *playing;
static size_t music_position;
static bool music_paused;

static void SDLCALL feed_bgm(void *context, SDL_AudioStream *stream, int additional, int total){
    (void)context;
    (void)total;
    if (!playing || music_paused || additional <= 0)
        return;

    size_t remaining = (size_t)additional;
    const Uint8 *samples = playing->data + loop_header_size;
    while (remaining){
        const size_t available = playing->size - music_position;
        const size_t count = remaining < available ? remaining : available;
        if (!SDL_PutAudioStreamData(stream, samples + music_position, (int)count))
            return;
        remaining -= count;
        music_position += count;
        if (music_position == playing->size)
            music_position = playing->loop_start;
    }
}

static bool load_track(struct track *track, const char *path){
    size_t file_size = 0;
    track->data = SDL_LoadFile(path, &file_size);
    if (!track->data || file_size <= loop_header_size){
        fprintf(stderr, "Cannot load music data: %s\n", path);
        return false;
    }
    Uint32 loop_start;
    memcpy(&loop_start, track->data, loop_header_size);
    track->loop_start = SDL_Swap32LE(loop_start);
    track->size = file_size - loop_header_size;
    if (track->loop_start >= track->size){
        fprintf(stderr, "Invalid music loop point: %s\n", path);
        return false;
    }
    return true;
}

bool init_sound(const char *assets){
    const bool initialized = SDL_InitSubSystem(SDL_INIT_AUDIO);
    if (initialized)
        device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &music_format);
    if (!device){
        fprintf(stderr, "Audio unavailable; continuing without sound: %s\n", SDL_GetError());
        if (initialized)
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return true;
    }

    for (size_t index = 0; index < SDL_arraysize(effects); ++index){
        char *path = join_asset_path(assets, effect_names[index]);
        if (!path){
            fprintf(stderr, "Cannot allocate sound effect path: %s\n", effect_names[index]);
            return false;
        }
        struct effect *effect = &effects[index];
        const bool loaded = SDL_LoadWAV(path, &effect->format, &effect->data, &effect->size);
        if (!loaded)
            fprintf(stderr, "Cannot load %s: %s\n", path, SDL_GetError());
        else if (effect->size > INT_MAX)
            fprintf(stderr, "Sound effect is too large: %s\n", path);
        SDL_free(path);
        if (!loaded || effect->size > INT_MAX)
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
    return true;
}

void shutdown_sound(void){
    if (!device)
        return;
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
    playing = NULL;
    music_paused = false;
    for (size_t index = 0; index < SDL_arraysize(tracks); ++index){
        SDL_free(tracks[index].data);
        tracks[index] = (struct track){0};
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

SDL_AudioDeviceID sound_device(void){
    return device;
}

void play_sound(int channel, int effect){
    if (!device || effect < 0 || effect >= MAX_SOUNDS)
        return;
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
}

void stop_sound(int channel){
    if (!device)
        return;
    if (channel == -1){
        for (size_t index = 0; index < SDL_arraysize(channels); ++index)
            SDL_ClearAudioStream(channels[index]);
    } else if (channel >= 0 && channel < (int)SDL_arraysize(channels)){
        SDL_ClearAudioStream(channels[channel]);
    }
}

void set_bgm_tracks(int level, int boss){
    selected_tracks[0] = level;
    selected_tracks[1] = boss;
}

void play_bgm(int index){
    if (!music_stream || !SDL_LockAudioStream(music_stream))
        return;
    SDL_ClearAudioStream(music_stream);
    playing = &tracks[selected_tracks[index]];
    music_position = 0;
    music_paused = false;
    SDL_UnlockAudioStream(music_stream);
}

void stop_bgm(void){
    if (!music_stream || !SDL_LockAudioStream(music_stream))
        return;
    playing = NULL;
    music_paused = false;
    SDL_ClearAudioStream(music_stream);
    SDL_UnlockAudioStream(music_stream);
}

static void set_bgm_paused(bool paused){
    if (!music_stream || !SDL_LockAudioStream(music_stream))
        return;
    music_paused = paused;
    SDL_UnlockAudioStream(music_stream);
}

void pause_bgm(void){
    set_bgm_paused(true);
}

void resume_bgm(void){
    set_bgm_paused(false);
}
