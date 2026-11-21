#ifndef SOUND_H
#define SOUND_H

#include <SDL3/SDL_audio.h>
#include <stdbool.h>

bool init_sound(const char *assets);
void shutdown_sound(void);
SDL_AudioDeviceID sound_device(void);
void set_bgm_tracks(int level, int boss);
void play_bgm(int index);
void stop_bgm(void);
void pause_bgm(void);
void resume_bgm(void);

#endif
