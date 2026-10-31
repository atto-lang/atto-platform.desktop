#include "ores_desktop_sdl3.h"
#include <limits.h>
#include <stddef.h>
#include <SDL3/SDL.h>

static int32_t coordinate(float v) {
    if (!(v == v)) return 0;
    if (v >= 2147483648.0f) return INT32_MAX;
    if (v <= -2147483648.0f) return INT32_MIN;
    return (int32_t)v;
}
static ores_desktop_status host_open(void *context,const ores_desktop_config *c) {
    ores_desktop_sdl3_state *s=(ores_desktop_sdl3_state*)context;
    if (!s || s->window || s->initialized) return ORES_DESKTOP_BACKEND_ERROR;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return ORES_DESKTOP_BACKEND_ERROR;
    s->initialized=1;
    s->window=SDL_CreateWindow(c->title,(int)c->width,(int)c->height,SDL_WINDOW_RESIZABLE);
    if (!s->window) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        s->initialized=0;
        return ORES_DESKTOP_BACKEND_ERROR;
    }
    return ORES_DESKTOP_OK;
}
static ores_desktop_status host_poll(void *context,ores_desktop_event *out,int *has_event) {
    SDL_Event ev;
    ores_desktop_sdl3_state *s=(ores_desktop_sdl3_state*)context;
    unsigned n;
    if (!s || !s->window || !out || !has_event) return ORES_DESKTOP_BACKEND_ERROR;
    *has_event=0;
    /* Do not spin forever discarding unrelated SDL events. */
    for (n=0;n<64 && SDL_PollEvent(&ev);++n) {
        /* SDL_PollEvent drains the process-wide queue. Never deliver another
         * window's close, keyboard, pointer or resize events to this app.
         * SDL_EVENT_QUIT is application-global and remains actionable.
         * Multiple window hosts still require one shared event dispatcher. */
        if (ev.type != SDL_EVENT_QUIT &&
            SDL_GetWindowFromEvent(&ev) != (SDL_Window*)s->window)
            continue;
        out->type=0; out->code=0; out->x=0; out->y=0;
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            out->type=ORES_DESKTOP_QUIT; break;
        case SDL_EVENT_WINDOW_RESIZED:
            out->type=ORES_DESKTOP_RESIZE;
            out->x=ev.window.data1; out->y=ev.window.data2; break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            out->type=ev.type==SDL_EVENT_KEY_DOWN ?
                ORES_DESKTOP_KEY_DOWN : ORES_DESKTOP_KEY_UP;
            out->code=(uint32_t)ev.key.scancode; break;
        case SDL_EVENT_MOUSE_MOTION:
            out->type=ORES_DESKTOP_POINTER_MOVE;
            out->x=coordinate(ev.motion.x); out->y=coordinate(ev.motion.y); break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            out->type=ev.type==SDL_EVENT_MOUSE_BUTTON_DOWN ?
                ORES_DESKTOP_POINTER_DOWN : ORES_DESKTOP_POINTER_UP;
            out->code=(uint32_t)ev.button.button;
            out->x=coordinate(ev.button.x); out->y=coordinate(ev.button.y); break;
        default: break;
        }
        if (out->type) { *has_event=1; return ORES_DESKTOP_OK; }
    }
    return ORES_DESKTOP_OK;
}
static ores_desktop_status host_present(void *context) {
    ores_desktop_sdl3_state *s=(ores_desktop_sdl3_state*)context;
    if (!s || !s->window) return ORES_DESKTOP_BACKEND_ERROR;
    /* No renderer in v0; pace event loop while window is open. */
    SDL_Delay(16);
    return ORES_DESKTOP_OK;
}
static void host_close(void *context) {
    ores_desktop_sdl3_state *s=(ores_desktop_sdl3_state*)context;
    if (!s) return;
    if (s->window) SDL_DestroyWindow((SDL_Window*)s->window);
    s->window=NULL;
    if (s->initialized) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    s->initialized=0;
}
ores_desktop_backend ores_desktop_sdl3_backend(ores_desktop_sdl3_state *state) {
    ores_desktop_backend b={state,host_open,host_poll,host_present,host_close};
    return b;
}
