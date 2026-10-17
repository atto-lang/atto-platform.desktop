#ifndef ORES_DESKTOP_SDL3_H
#define ORES_DESKTOP_SDL3_H
#include "ores_desktop.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Built only with ORES_DESKTOP_WITH_SDL3=ON. Initialize state to {0}. */
typedef struct {
    void *window;        /* private SDL_Window* */
    uint32_t initialized;
} ores_desktop_sdl3_state;
ores_desktop_backend ores_desktop_sdl3_backend(ores_desktop_sdl3_state *);
#ifdef __cplusplus
}
#endif
#endif
