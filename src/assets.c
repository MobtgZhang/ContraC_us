#include "assets.h"

#include <SDL3/SDL_stdinc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

char *join_asset_path(const char *directory, const char *name){
    const size_t directory_size = strlen(directory);
    const size_t name_size = strlen(name);
    const bool add_slash = directory_size && directory[directory_size - 1] != '/';
    if (directory_size > SIZE_MAX - 2 || name_size > SIZE_MAX - directory_size - 2)
        return NULL;

    const size_t prefix_size = directory_size + add_slash;
    char *path = SDL_malloc(prefix_size + name_size + 1);
    if (!path)
        return NULL;
    memcpy(path, directory, directory_size);
    if (add_slash)
        path[directory_size] = '/';
    memcpy(path + prefix_size, name, name_size + 1);
    return path;
}
