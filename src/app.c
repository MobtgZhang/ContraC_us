#include "app.h"
#include "assets.h"
#include "globals.h"
#include "graphics.h"
#include "sound.h"
#include "timing.h"

#include <stdio.h>
#include <string.h>

bool g_keys[CONTRA_SCANCODE_COUNT];

bool init_app(const char *assets, bool fullscreen, bool audio){
#if CONTRA_SDL == 3
    const bool video_initialized = SDL_Init(SDL_INIT_VIDEO);
#else
    const bool video_initialized = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0;
#endif
    if (!video_initialized || !init_video(fullscreen)){
        fprintf(stderr, "Cannot initialize SDL video: %s\n", SDL_GetError());
        return false;
    }
#if CONTRA_SDL == 3
    SDL_HideCursor();
#else
    SDL_ShowCursor(SDL_DISABLE);
#endif

    const size_t gfx_size = 57568;
    size_t size = 0;
    char *path = join_asset_path(assets, "gfx.dat");
    if (!path){
        fprintf(stderr, "Cannot allocate graphics asset path\n");
        return false;
    }
    contra_gfx = SDL_LoadFile(path, &size);
    if (!contra_gfx || size != gfx_size){
        fprintf(stderr, "Missing or invalid graphics asset: %s (expected %zu bytes)\n",
                path, gfx_size);
        SDL_free(path);
        return false;
    }
    SDL_free(path);

    if (audio && !init_sound(assets))
        return false;
    if (!init_timer()){
        fprintf(stderr, "Cannot initialize the SDL frame timer\n");
        return false;
    }

    hi_score = 200;
    return true;
}

bool process_events(void){
    SDL_Event event;
    while (SDL_PollEvent(&event)){
#if CONTRA_SDL == 3
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
            return false;
        if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            memset(g_keys, 0, sizeof(g_keys));
#else
        if (event.type == SDL_QUIT)
            return false;
        if (event.type == SDL_WINDOWEVENT){
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                return false;
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                memset(g_keys, 0, sizeof(g_keys));
        }
#endif
        if (event.type != CONTRA_KEY_DOWN && event.type != CONTRA_KEY_UP)
            continue;

#if CONTRA_SDL == 3
        const SDL_Scancode key = event.key.scancode;
#else
        const SDL_Scancode key = event.key.keysym.scancode;
#endif
        if (event.type == CONTRA_KEY_DOWN){
            if (event.key.repeat)
                continue;
            if (key == SDL_SCANCODE_ESCAPE)
                return false;
            if (key == SDL_SCANCODE_F11 && !toggle_fullscreen())
                fprintf(stderr, "Cannot toggle fullscreen: %s\n", SDL_GetError());
        }
        if (key > SDL_SCANCODE_UNKNOWN && key < CONTRA_SCANCODE_COUNT)
            g_keys[key] = event.type == CONTRA_KEY_DOWN;
    }
    return true;
}

void shutdown_app(void){
    shutdown_sound();
    SDL_free(contra_gfx);
    contra_gfx = NULL;
    shutdown_video();
    SDL_Quit();
}
