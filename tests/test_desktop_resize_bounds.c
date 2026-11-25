#include "ores_desktop.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <limits.h>

typedef struct {
    int32_t width, height;
    unsigned opens, polls, events, stops, closes;
} state;
static ores_desktop_status open_window(void *p, const ores_desktop_config *c) {
    state *s = p; assert(c->width == 128); ++s->opens; return ORES_DESKTOP_OK;
}
static ores_desktop_status poll_event(void *p, ores_desktop_event *e, int *available) {
    state *s = p; ++s->polls;
    *available = 1; e->type = ORES_DESKTOP_RESIZE;
    e->x = s->width; e->y = s->height;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status on_event(void *p, const ores_desktop_event *e) {
    state *s = p; ++s->events;
    assert(e->x == s->width && e->y == s->height);
    return ORES_DESKTOP_STOP;
}
static ores_desktop_status present(void *p) {(void)p; return ORES_DESKTOP_OK;}
static void close_window(void *p) {++((state *)p)->closes;}
static void stop_app(void *p) {++((state *)p)->stops;}
static void check_case(int32_t w, int32_t h, int valid) {
    state s = {0}; s.width = w; s.height = h;
    ores_desktop_config cfg = {ORES_DESKTOP_ABI_VERSION, 128, 128, 1, 1, "x", 1};
    ores_desktop_app app = {&s, NULL, on_event, NULL, stop_app};
    ores_desktop_backend backend = {&s, open_window, poll_event, present, close_window};
    ores_desktop_status status = ores_desktop_run(&cfg, &app, &backend);
    assert(status == (valid ? ORES_DESKTOP_OK : ORES_DESKTOP_BACKEND_ERROR));
    assert(s.opens == 1 && s.polls == 1 && s.closes == 1 && s.stops == 1);
    assert(s.events == (unsigned)valid);
}
int main(void) {
    check_case(0, 0, 1); /* minimized */
    check_case(1, 16384, 1);
    check_case(16384, 1, 1);
    check_case(-1, 100, 0);
    check_case(100, -1, 0);
    check_case(16385, 100, 0);
    check_case(100, 16385, 0);
    check_case(INT32_MAX, INT32_MAX, 0);
    return 0;
}
