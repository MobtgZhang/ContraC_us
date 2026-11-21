#include "sound.h"
#include "assets.h"
#include "globals.h"

#include <SDL_mixer.h>
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

static Mix_Chunk *effects[MAX_SOUNDS];
static struct track tracks[SDL_arraysize(track_names)];
static int selected_tracks[2];
static bool audio_open;
static SDL_mutex *music_mutex;
static const struct track *playing;
static size_t music_position;
static bool music_paused;

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
        const size_t available = playing->size - music_position;
        const size_t count = remaining < available ? remaining : available;
        memcpy(output, samples + music_position, count);
        output += count;
        remaining -= count;
        music_position += count;
        if (music_position == playing->size)
            music_position = playing->loop_start;
    }
    SDL_UnlockMutex(music_mutex);
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
    track->loop_start = SDL_SwapLE32(loop_start);
    track->size = file_size - loop_header_size;
    if (track->loop_start >= track->size){
        fprintf(stderr, "Invalid music loop point: %s\n", path);
        return false;
    }
    return true;
}

bool init_sound(const char *assets){
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0 ||
        Mix_OpenAudioDevice(22050, AUDIO_U8, 1, 1024, NULL, 0) < 0){
        fprintf(stderr, "Audio unavailable; continuing without sound: %s\n", SDL_GetError());
        return true;
    }
    audio_open = true;
    music_mutex = SDL_CreateMutex();
    if (!music_mutex){
        fprintf(stderr, "Cannot initialize the music lock: %s\n", SDL_GetError());
        return false;
    }

    for (size_t index = 0; index < SDL_arraysize(effects); ++index){
        char *path = join_asset_path(assets, effect_names[index]);
        if (!path){
            fprintf(stderr, "Cannot allocate sound effect path: %s\n", effect_names[index]);
            return false;
        }
        effects[index] = Mix_LoadWAV(path);
        if (!effects[index])
            fprintf(stderr, "Cannot load %s: %s\n", path, Mix_GetError());
        SDL_free(path);
        if (!effects[index])
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

    Mix_HookMusic(feed_bgm, NULL);
    return true;
}

void shutdown_sound(void){
    if (!audio_open)
        return;
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
    playing = NULL;
    music_paused = false;
    for (size_t index = 0; index < SDL_arraysize(tracks); ++index){
        SDL_free(tracks[index].data);
        tracks[index] = (struct track){0};
    }
    audio_open = false;
}

void play_sound(int channel, int effect){
    if (audio_open)
        Mix_PlayChannel(channel, effects[effect], 0);
}

void stop_sound(int channel){
    if (audio_open)
        Mix_HaltChannel(channel);
}

void set_bgm_tracks(int level, int boss){
    selected_tracks[0] = level;
    selected_tracks[1] = boss;
}

void play_bgm(int index){
    if (!music_mutex || SDL_LockMutex(music_mutex) < 0)
        return;
    playing = &tracks[selected_tracks[index]];
    music_position = 0;
    music_paused = false;
    SDL_UnlockMutex(music_mutex);
}

void stop_bgm(void){
    if (!music_mutex || SDL_LockMutex(music_mutex) < 0)
        return;
    playing = NULL;
    music_paused = false;
    SDL_UnlockMutex(music_mutex);
}

static void set_bgm_paused(bool paused){
    if (!music_mutex || SDL_LockMutex(music_mutex) < 0)
        return;
    music_paused = paused;
    SDL_UnlockMutex(music_mutex);
}

void pause_bgm(void){
    set_bgm_paused(true);
}

void resume_bgm(void){
    set_bgm_paused(false);
}
