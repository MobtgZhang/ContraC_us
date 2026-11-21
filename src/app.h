#ifndef APP_H
#define APP_H

#include <stdbool.h>

bool init_app(const char *assets, bool fullscreen, bool audio);
bool process_events(void);
void shutdown_app(void);

#endif
