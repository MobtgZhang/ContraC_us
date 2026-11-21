#define _POSIX_C_SOURCE 200809L
#include "app.h"
#include "assets.h"
#include "game.h"
#include "globals.h"
#include "graphics.h"
#include "timing.h"

#include <SDL3/SDL_main.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef CONTRA_INSTALLED_ASSETS
#include <sys/stat.h>
#endif

struct settings {
    bool fullscreen;
    int lives;
    int boss_music;
};

static char *trim(char *text){
    text += strspn(text, " \t\r\n");
    char *end = text + strlen(text);
    while (end > text && strchr(" \t\r\n", end[-1]))
        --end;
    *end = '\0';
    return text;
}

static struct settings read_settings(const char *path){
    struct settings settings = { .lives = 3 };
    FILE *file = fopen(path, "r");
    if (!file)
        return settings;

    char *line = NULL;
    size_t capacity = 0;
    bool in_section = false;
    while (getline(&line, &capacity, file) >= 0){
        line[strcspn(line, ";#")] = '\0';
        char *text = trim(line);
        if (*text == '['){
            in_section = SDL_strcasecmp(text, "[CONTRA]") == 0;
            continue;
        }
        char *split = strchr(text, '=');
        if (!in_section || !split)
            continue;
        *split = '\0';
        const char *key = trim(text);
        const char *value = trim(split + 1);
        if (*value != '-' && (*value < '0' || *value > '9'))
            continue;
        char *end;
        errno = 0;
        const long number = strtol(value, &end, 10);
        if (end == value || *end || errno == ERANGE || number < INT_MIN || number > INT_MAX)
            continue;
        if (SDL_strcasecmp(key, "FULLSCREEN") == 0)
            settings.fullscreen = number != 0;
        else if (SDL_strcasecmp(key, "LIVES") == 0)
            settings.lives = number < 1 ? 1 : number > 9 ? 9 : (int)number;
        else if (SDL_strcasecmp(key, "BOSSBGM") == 0)
            settings.boss_music = number != 0;
    }
    free(line);
    fclose(file);
    return settings;
}

static void usage(FILE *output){
    fprintf(output, "Usage: contra [--windowed | --fullscreen] [--no-audio] [--help]\n");
}

int main(int argc, char *argv[]){
    int fullscreen = -1;
    bool audio = true;
    for (int index = 1; index < argc; ++index){
        const char *argument = argv[index];
        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0){
            usage(stdout);
            return 0;
        }
        if (strcmp(argument, "--fullscreen") == 0)
            fullscreen = 1;
        else if (strcmp(argument, "--windowed") == 0)
            fullscreen = 0;
        else if (strcmp(argument, "--no-audio") == 0)
            audio = false;
        else{
            fprintf(stderr, "Unknown option: %s\n", argument);
            usage(stderr);
            return 2;
        }
    }

    const char *base = SDL_GetBasePath();
    if (!base){
        fprintf(stderr, "Cannot find the executable directory: %s\n", SDL_GetError());
        return 1;
    }
    char *assets = join_asset_path(base, "resource");
#ifdef CONTRA_INSTALLED_ASSETS
    struct stat bundled_assets;
    if (assets && (stat(assets, &bundled_assets) != 0 || !S_ISDIR(bundled_assets.st_mode))){
        SDL_free(assets);
        assets = join_asset_path(base, CONTRA_INSTALLED_ASSETS);
    }
#endif
    char *config = assets ? join_asset_path(assets, "contra.ini") : NULL;
    if (!config){
        fprintf(stderr, "Cannot allocate the asset paths\n");
        SDL_free(assets);
        return 1;
    }
    const struct settings settings = read_settings(config);
    SDL_free(config);
    lives_setting = settings.lives - 1;
    bossbgm = settings.boss_music;

    const bool ready = init_app(assets, fullscreen < 0 ? settings.fullscreen : fullscreen != 0, audio);
    SDL_free(assets);
    int result = ready ? 0 : 1;
    while (ready && process_events()){
        update_game_frame();
        if (!present_video()){
            fprintf(stderr, "Cannot render game frame: %s\n", SDL_GetError());
            result = 1;
            break;
        }
        trim_speed();
    }
    shutdown_app();
    return result;
}
