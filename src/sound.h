#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

#if CONTRA_SDL == 3
#include <SDL3/SDL_audio.h>
SDL_AudioDeviceID sound_device(void);
#endif

bool init_sound(const char *assets);
void shutdown_sound(void);
void set_bgm_tracks(int level, int boss);
void play_bgm(int index);
void stop_bgm(void);
void pause_bgm(void);
void resume_bgm(void);

#endif
