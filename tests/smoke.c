#include "app.h"
#include "assets.h"
#include "game.h"
#include "globals.h"
#include "graphics.h"
#include "player.h"
#include "sound.h"

#if CONTRA_SDL == 3
#include <SDL3/SDL_main.h>
#else
#include <SDL_mixer.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MUSIC_SOURCE_BYTES = 1024 };
#if CONTRA_SDL == 3
typedef float AudioSample;
typedef SDL_Mutex AudioMutex;
typedef SDL_Condition AudioCondition;
enum { AUDIO_PREFIX_SAMPLES = 256 };
#define AUDIO_SILENCE 0.0f
#else
typedef Uint8 AudioSample;
typedef SDL_mutex AudioMutex;
typedef SDL_cond AudioCondition;
enum { AUDIO_PREFIX_SAMPLES = MUSIC_SOURCE_BYTES };
#define AUDIO_SILENCE 0x80
#endif

struct audio_capture {
    AudioMutex *mutex;
    AudioCondition *updated;
    const AudioSample *expected;
    bool silent;
    bool armed;
    bool matched;
    bool active;
};

static struct audio_capture capture;

static void cleanup(void){
#if CONTRA_SDL == 3
    if (capture.active)
        SDL_SetAudioPostmixCallback(sound_device(), NULL, NULL);
    if (capture.updated)
        SDL_DestroyCondition(capture.updated);
#else
    if (capture.active)
        Mix_SetPostMix(NULL, NULL);
    if (capture.updated)
        SDL_DestroyCond(capture.updated);
#endif
    if (capture.mutex)
        SDL_DestroyMutex(capture.mutex);
    shutdown_app();
}

static void require(bool condition, const char *failure){
    if (!condition){
        fprintf(stderr, "%s\n", failure);
        cleanup();
        exit(EXIT_FAILURE);
    }
}

static void push_key_event(SDL_Scancode key, bool pressed, bool repeat){
    SDL_Event event = {0};
    event.type = pressed ? CONTRA_KEY_DOWN : CONTRA_KEY_UP;
#if CONTRA_SDL == 3
    event.key.down = pressed;
    event.key.scancode = key;
#else
    event.key.keysym.scancode = key;
#endif
    event.key.repeat = repeat;
    require(SDL_PushEvent(&event) == 1, "Could not queue a keyboard event");
}

static void push_key(SDL_Scancode key, bool pressed){
    push_key_event(key, pressed, false);
}

static void frame(void){
    require(process_events(), "Unexpected quit event");
    update_game_frame();
    require(present_video(), "Could not render a game frame");
}

static void advance_until(bool (*done)(void), int limit, const char *failure){
    for (int count = 0; count < limit && !done(); ++count)
        frame();
    require(done(), failure);
}

static bool title_visible(void){
    return main_proc == 1 && hscroll == 0;
}

static bool stage_loading(void){
    return main_proc == 5;
}

static bool stage_playing(void){
    return game_proc == 4;
}

static bool player_spawned(void){
    return player.state == 1;
}

static void start_stage(int stage_index){
    advance_until(title_visible, 300, "Title did not appear");
    push_key(PLAYER_ST, true);
    frame();
    push_key(PLAYER_ST, false);
    advance_until(stage_loading, 400, "Enter did not start the game");
    stage = stage_index;
    advance_until(stage_playing, 500, "Stage did not enter gameplay");
    require(stage == stage_index && in_game && show_status, "Unexpected stage state");
}

static void test_input(void){
    start_stage(0);
    advance_until(player_spawned, 150, "Player did not spawn");

    push_key(PLAYER_ST, true);
    frame();
    require(pause, "Enter did not pause gameplay");
    push_key(PLAYER_ST, false);
    frame();
    push_key_event(PLAYER_ST, true, true);
    frame();
    require(pause, "Keyboard repeat unpaused gameplay");
    push_key(PLAYER_ST, true);
    frame();
    require(!pause, "Enter did not resume gameplay");
    push_key(PLAYER_ST, false);

    const int old_x = chr_xs[0];
    push_key(SDL_SCANCODE_RIGHT, true);
    push_key(PLAYER_B, true);
    for (int count = 0; count < 12; ++count)
        frame();
    require(chr_xs[0] > old_x, "Right arrow did not move the player");
    bool fired = false;
    for (int index = 0; index < 16; ++index)
        fired |= p_bullet[index].atype != 0;
    require(fired, "Z did not fire a bullet");
    push_key(PLAYER_B, false);
    push_key(SDL_SCANCODE_RIGHT, false);

    push_key(PLAYER_A, true);
    frame();
    require(player.jump_f, "X did not jump");
    push_key(PLAYER_A, false);

    push_key(CONTRA_SCANCODE_COUNT, true);
    push_key(SDL_SCANCODE_UP, true);
    require(process_events() && g_keys[SDL_SCANCODE_UP], "Arrow key did not register");
    SDL_Event focus = {0};
#if CONTRA_SDL == 3
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
#else
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
#endif
    require(SDL_PushEvent(&focus) == 1, "Could not queue focus event");
    require(process_events() && !g_keys[SDL_SCANCODE_UP], "Focus loss left key pressed");

    push_key(SDL_SCANCODE_ESCAPE, true);
    require(!process_events(), "Escape did not exit");
    require(process_events(), "Unexpected remaining event after Escape");
    SDL_Event quit = {0};
#if CONTRA_SDL == 3
    quit.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
#else
    quit.type = SDL_WINDOWEVENT;
    quit.window.event = SDL_WINDOWEVENT_CLOSE;
#endif
    require(SDL_PushEvent(&quit) == 1 && !process_events(), "Window close did not exit");
}

static void test_stage(int stage_index){
    start_stage(stage_index);
    for (int count = 0; count < 720; ++count){
        if (count % 80 == 0){
            push_key(SDL_SCANCODE_RIGHT, true);
            push_key(PLAYER_RAPIDB, true);
            push_key(PLAYER_A, true);
        }else if (count % 80 == 20){
            push_key(PLAYER_A, false);
        }
        if (main_proc != 5 || game_proc >= 5)
            break;
        frame();
    }
    require(stage == stage_index && main_proc == 5, "Unexpected exit from stage");
}

static bool ending_started(void){
    return main_proc == 6;
}

static bool ending_finished(void){
    return !ending_started();
}

static void test_ending(void){
    start_stage(7);
    stage_clear = 1;
    game_proc = 5;
    advance_until(ending_started, 2, "Last stage did not enter ending");
    advance_until(ending_finished, 6200, "Ending did not complete");
    require(num_of_allclears == 1, "Ending did not register a clear");
}

static bool packet_matches(const struct audio_capture *audio, const AudioSample *packet, size_t count){
    if (audio->expected){
        if (count < AUDIO_PREFIX_SAMPLES)
            return false;
#if CONTRA_SDL == 3
        for (size_t index = 0; index < AUDIO_PREFIX_SAMPLES; ++index){
            if (SDL_fabsf(packet[index] - audio->expected[index]) > 0.0001f)
                return false;
        }
        return true;
#else
        return memcmp(packet, audio->expected, AUDIO_PREFIX_SAMPLES) == 0;
#endif
    }
    for (size_t index = 0; index < count; ++index){
        if (packet[index] != AUDIO_SILENCE)
            return !audio->silent;
    }
    return audio->silent;
}

#if CONTRA_SDL == 3
static void SDLCALL record_audio(void *context, const SDL_AudioSpec *spec, float *data, int length){
    (void)spec;
#else
static void SDLCALL record_audio(void *context, Uint8 *data, int length){
#endif
    struct audio_capture *audio = context;
    if (length <= 0)
        return;
    SDL_LockMutex(audio->mutex);
    if (audio->armed && packet_matches(audio, data, (size_t)length / sizeof(*data))){
        audio->matched = true;
        audio->armed = false;
#if CONTRA_SDL == 3
        SDL_SignalCondition(audio->updated);
#else
        SDL_CondSignal(audio->updated);
#endif
    }
    SDL_UnlockMutex(audio->mutex);
}

static void expect_audio(const AudioSample *expected, bool silent){
    SDL_LockMutex(capture.mutex);
    capture.expected = expected;
    capture.silent = silent;
    capture.armed = true;
    capture.matched = false;
    SDL_UnlockMutex(capture.mutex);
}

static bool wait_for_audio(void){
    const Uint32 start = (Uint32)SDL_GetTicks();
    const Uint32 timeout = 2000;
    SDL_LockMutex(capture.mutex);
    while (!capture.matched){
        const Uint32 elapsed = (Uint32)SDL_GetTicks() - start;
        if (elapsed >= timeout)
            break;
#if CONTRA_SDL == 3
        if (!SDL_WaitConditionTimeout(capture.updated, capture.mutex, (Sint32)(timeout - elapsed)))
#else
        if (SDL_CondWaitTimeout(capture.updated, capture.mutex, timeout - elapsed) != 0)
#endif
            break;
    }
    const bool matched = capture.matched;
    capture.armed = false;
    capture.expected = NULL;
    SDL_UnlockMutex(capture.mutex);
    return matched;
}

static void read_first_samples(const char *assets, const char *name, AudioSample *samples){
    char *path = join_asset_path(assets, name);
    require(path != NULL, "Could not allocate music path");
    FILE *file = fopen(path, "rb");
    SDL_free(path);
    require(file != NULL, "Could not open the expected music file");
    Uint8 source[MUSIC_SOURCE_BYTES];
    const bool read = fseek(file, 4, SEEK_SET) == 0 && fread(source, 1, sizeof(source), file) == sizeof(source);
    fclose(file);
    require(read, "Could not read the expected music samples");
#if CONTRA_SDL == 3
    const SDL_AudioSpec input = { .format = SDL_AUDIO_U8, .channels = 1, .freq = 22050 };
    SDL_AudioSpec output;
    require(SDL_GetAudioDeviceFormat(sound_device(), &output, NULL), "Could not get audio output format");
    output.format = SDL_AUDIO_F32;
    Uint8 *converted = NULL;
    int converted_size = 0;
    require(SDL_ConvertAudioSamples(&input, source, sizeof(source), &output, &converted, &converted_size),
            "Could not convert the expected music samples");
    const int prefix_size = AUDIO_PREFIX_SAMPLES * sizeof(*samples);
    const bool has_prefix = converted_size >= prefix_size;
    if (has_prefix)
        memcpy(samples, converted, prefix_size);
    SDL_free(converted);
    require(has_prefix, "Too few converted music samples");
#else
    memcpy(samples, source, sizeof(source));
#endif
}

static void test_audio(const char *assets){
    AudioSample level[AUDIO_PREFIX_SAMPLES];
    AudioSample boss[AUDIO_PREFIX_SAMPLES];
    read_first_samples(assets, "bgm/Contra_00.raw", level);
    read_first_samples(assets, "bgm/Contra_boss1.raw", boss);

    capture.mutex = SDL_CreateMutex();
#if CONTRA_SDL == 3
    capture.updated = SDL_CreateCondition();
    require(capture.mutex && capture.updated, "Could not initialize audio capture");
    require(SDL_SetAudioPostmixCallback(sound_device(), record_audio, &capture), "Could not observe mixed audio");
#else
    capture.updated = SDL_CreateCond();
    require(capture.mutex && capture.updated, "Could not initialize audio capture");
    Mix_SetPostMix(record_audio, &capture);
#endif
    capture.active = true;
    set_bgm_tracks(0, 6);
    expect_audio(level, false);
    play_bgm(0);
    require(wait_for_audio(), "Level music not mixed");
    pause_bgm();
    expect_audio(NULL, true);
    require(wait_for_audio(), "Paused music was not silent");
    resume_bgm();
    expect_audio(NULL, false);
    require(wait_for_audio(), "Music did not resume");
    expect_audio(boss, false);
    play_bgm(1);
    require(wait_for_audio(), "Boss music not mixed");
    stop_bgm();
    expect_audio(NULL, true);
    require(wait_for_audio(), "Stopped music was not silent");
    expect_audio(NULL, false);
    play_sound(0, TITLE_SND);
    require(wait_for_audio(), "Sound effect not mixed");
}

int main(int argc, char *argv[]){
    require(argc == 3, "Expected asset directory and scenario");
    const char *assets = argv[1];
    const char *scenario = argv[2];
    const bool audio = strcmp(scenario, "audio") == 0;
    lives_setting = 2;
    require(init_app(assets, false, audio), "Could not initialize game");
    if (strcmp(scenario, "input") == 0)
        test_input();
    else if (audio)
        test_audio(assets);
    else if (strcmp(scenario, "ending") == 0)
        test_ending();
    else {
        require(strlen(scenario) == 1 && scenario[0] >= '1' && scenario[0] <= '8', "Unknown scenario");
        test_stage(scenario[0] - '1');
    }
    cleanup();
    return EXIT_SUCCESS;
}
