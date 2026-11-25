/* Host-only native ABI: poll-flag/event admission and single cleanup.
 * cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude src/ores_desktop.c \
 *   tests/test_gpt6_wave244_invalid_poll_flags_20261106.c \
 *   -o /tmp/ores-wave244-desktop && /tmp/ores-wave244-desktop
 * No SDL3 or compiler guest claim.
 */
#include "ores_desktop.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int mode;
static unsigned opened, started, polled, events, frames, presented, stopped, closed;

static ores_desktop_status on_open(void *unused, const ores_desktop_config *config) {
    (void)unused;
    assert(config->title_length == 4u && config->title[4] == '\0');
    ++opened;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status on_start(void *unused) {
    (void)unused;
    ++started;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status on_poll(void *unused, ores_desktop_event *event, int *available) {
    (void)unused;
    ++polled;
    *available = mode == 0 ? 2 : 1;
    event->type = mode == 1 ? 999u : ORES_DESKTOP_QUIT;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status on_event(void *unused, const ores_desktop_event *event) {
    (void)unused;
    ++events;
    if (mode == 2) ((ores_desktop_event *)(void *)event)->type = ORES_DESKTOP_POINTER_UP;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status on_frame(void *unused, uint64_t frame) {
    (void)unused; (void)frame;
    ++frames;
    return ORES_DESKTOP_APP_ERROR;
}
static ores_desktop_status on_present(void *unused) {
    (void)unused; ++presented; return ORES_DESKTOP_BACKEND_ERROR;
}
static void on_stop(void *unused) { (void)unused; ++stopped; }
static void on_close(void *unused) { (void)unused; ++closed; }

int main(void) {
    const char title[4] = {'W','a','v','e'};
    ores_desktop_config cfg = {0};
    ores_desktop_app app = {0};
    ores_desktop_backend backend = {0};
    cfg.abi_version = ORES_DESKTOP_ABI_VERSION;
    cfg.width = 640; cfg.height = 480; cfg.max_events_per_frame = 1;
    cfg.frame_limit = 1; cfg.title = title; cfg.title_length = sizeof title;
    app.on_start = on_start; app.on_event = on_event;
    app.on_frame = on_frame; app.on_stop = on_stop;
    backend.open = on_open; backend.poll = on_poll;
    backend.present = on_present; backend.close = on_close;

    for (mode = 0; mode < 3; ++mode) {
        opened = started = polled = events = frames = presented = stopped = closed = 0u;
        ores_desktop_status result = ores_desktop_run(&cfg, &app, &backend);
        assert(result == (mode == 2 ? ORES_DESKTOP_OK : ORES_DESKTOP_BACKEND_ERROR));
        assert(opened == 1u && started == 1u && polled == 1u);
        assert(events == (mode == 2 ? 1u : 0u));
        assert(frames == 0u && presented == 0u);
        assert(stopped == 1u && closed == 1u);
    }
    puts("desktop poll admission and quit immutability: PASS");
    return 0;
}
