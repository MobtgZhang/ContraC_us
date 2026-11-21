#include "app.h"
#include "assets.h"
#include "game.h"
#include "globals.h"
#include "graphics.h"
#include "player.h"
#include "sound.h"

#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { AUDIO_PACKET_SIZE = 1024 };

struct audio_capture {
    SDL_mutex *mutex;
    SDL_cond *updated;
    const Uint8 *expected;
    bool silent;
    bool armed;
    bool matched;
    bool active;
};

static struct audio_capture capture;

static void cleanup(void){
    if (capture.active)
        Mix_SetPostMix(NULL, NULL);
    if (capture.updated)
        SDL_DestroyCond(capture.updated);
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
    event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.scancode = key;
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

    push_key(SDL_NUM_SCANCODES, true);
    push_key(SDL_SCANCODE_UP, true);
    require(process_events() && g_keys[SDL_SCANCODE_UP], "Arrow key did not register");
    SDL_Event focus = {0};
    focus.type = SDL_WINDOWEVENT;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    require(SDL_PushEvent(&focus) == 1, "Could not queue focus event");
    require(process_events() && !g_keys[SDL_SCANCODE_UP], "Focus loss left key pressed");

    push_key(SDL_SCANCODE_ESCAPE, true);
    require(!process_events(), "Escape did not exit");
    require(process_events(), "Unexpected remaining event after Escape");
    SDL_Event quit = {0};
    quit.type = SDL_WINDOWEVENT;
    quit.window.event = SDL_WINDOWEVENT_CLOSE;
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

static bool packet_matches(const struct audio_capture *audio, const Uint8 *packet){
    if (audio->expected)
        return memcmp(packet, audio->expected, AUDIO_PACKET_SIZE) == 0;
    for (size_t index = 0; index < AUDIO_PACKET_SIZE; ++index){
        if (packet[index] != 0x80)
            return !audio->silent;
    }
    return audio->silent;
}

static void SDLCALL record_audio(void *context, Uint8 *data, int length){
    struct audio_capture *audio = context;
    if (length != AUDIO_PACKET_SIZE || SDL_LockMutex(audio->mutex) < 0)
        return;
    if (audio->armed && packet_matches(audio, data)){
        audio->matched = true;
        audio->armed = false;
        SDL_CondSignal(audio->updated);
    }
    SDL_UnlockMutex(audio->mutex);
}

static void expect_audio(const Uint8 *expected, bool silent){
    require(SDL_LockMutex(capture.mutex) == 0, "Could not lock audio capture");
    capture.expected = expected;
    capture.silent = silent;
    capture.armed = true;
    capture.matched = false;
    SDL_UnlockMutex(capture.mutex);
}

static bool wait_for_audio(void){
    const Uint32 start = SDL_GetTicks();
    const Uint32 timeout = 2000;
    if (SDL_LockMutex(capture.mutex) < 0)
        return false;
    while (!capture.matched){
        const Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed >= timeout || SDL_CondWaitTimeout(capture.updated, capture.mutex, timeout - elapsed) < 0)
            break;
    }
    const bool matched = capture.matched;
    capture.armed = false;
    capture.expected = NULL;
    SDL_UnlockMutex(capture.mutex);
    return matched;
}

static void read_first_samples(const char *assets, const char *name, Uint8 *samples){
    char *path = join_asset_path(assets, name);
    require(path != NULL, "Could not allocate music path");
    FILE *file = fopen(path, "rb");
    SDL_free(path);
    require(file != NULL, "Could not open the expected music file");
    const bool read = fseek(file, 4, SEEK_SET) == 0 && fread(samples, 1, AUDIO_PACKET_SIZE, file) == AUDIO_PACKET_SIZE;
    fclose(file);
    require(read, "Could not read the expected music samples");
}

static void test_audio(const char *assets){
    Uint8 level[AUDIO_PACKET_SIZE];
    Uint8 boss[AUDIO_PACKET_SIZE];
    read_first_samples(assets, "bgm/Contra_00.raw", level);
    read_first_samples(assets, "bgm/Contra_boss1.raw", boss);

    capture.mutex = SDL_CreateMutex();
    capture.updated = SDL_CreateCond();
    require(capture.mutex && capture.updated, "Could not initialize audio capture");
    Mix_SetPostMix(record_audio, &capture);
    capture.active = true;
    set_bgm_tracks(0, 6);
    expect_audio(level, false);
    play_bgm(0);
    require(wait_for_audio(), "Level music not mixed");
    expect_audio(NULL, true);
    pause_bgm();
    require(wait_for_audio(), "Paused music was not silent");
    expect_audio(NULL, false);
    resume_bgm();
    require(wait_for_audio(), "Music did not resume");
    expect_audio(boss, false);
    play_bgm(1);
    require(wait_for_audio(), "Boss music not mixed");
    expect_audio(NULL, true);
    stop_bgm();
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
