#define _POSIX_C_SOURCE 200809L
#include "timing.h"

#include "sdl_compat.h"
#include <time.h>

static const Uint64 frames_per_second = 60;
static Uint64 frequency;
static Uint64 frame_ticks;
static Uint64 next_frame;
static unsigned char random_byte;

bool init_timer(void){
    frequency = SDL_GetPerformanceFrequency();
    frame_ticks = frequency / frames_per_second;
    next_frame = SDL_GetPerformanceCounter() + frame_ticks;
    return frame_ticks != 0;
}

void trim_speed(void){
    Uint64 now = SDL_GetPerformanceCounter();
    random_byte = (unsigned char)now;
    while (now < next_frame){
        const Uint64 ticks = next_frame - now;
        const struct timespec delay = { .tv_nsec = (long)(ticks * 1000000000 / frequency) };
        nanosleep(&delay, NULL);
        now = SDL_GetPerformanceCounter();
    }
    if (now - next_frame >= frame_ticks)
        next_frame = now;
    next_frame += frame_ticks;
}

unsigned char get_rand8(void){
    return random_byte;
}
