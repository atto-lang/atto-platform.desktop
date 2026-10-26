#include "ores_desktop.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "failed %s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)

typedef struct {
    unsigned opens, closes, starts, stops, polls, events, frames, presents;
    int stop_at_event;
} state;

static ores_desktop_status open_backend(void *ctx, const ores_desktop_config *cfg) {
    state *s = (state *)ctx;
    CHECK(cfg->abi_version == ORES_DESKTOP_ABI_VERSION);
    CHECK(cfg->title != NULL && cfg->title_length == 127u);
    CHECK(strlen(cfg->title) == 127u);
    ++s->opens;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status poll_backend(void *ctx, ores_desktop_event *e, int *available) {
    state *s = (state *)ctx;
    ++s->polls;
    *available = 1; /* An infinite busy input source must not starve frames. */
    e->type = ORES_DESKTOP_KEY_DOWN;
    e->code = 17;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status present_backend(void *ctx) {
    ++((state *)ctx)->presents;
    return ORES_DESKTOP_OK;
}
static void close_backend(void *ctx) { ++((state *)ctx)->closes; }
static ores_desktop_status start_app(void *ctx) {
    ++((state *)ctx)->starts;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status event_app(void *ctx, const ores_desktop_event *e) {
    state *s = (state *)ctx;
    CHECK(e->type == ORES_DESKTOP_KEY_DOWN && e->code == 17);
    ++s->events;
    return s->stop_at_event && s->events == (unsigned)s->stop_at_event
        ? ORES_DESKTOP_STOP : ORES_DESKTOP_OK;
}
static ores_desktop_status frame_app(void *ctx, uint64_t frame) {
    state *s = (state *)ctx;
    CHECK(frame == s->frames);
    ++s->frames;
    return ORES_DESKTOP_OK;
}
static void stop_app(void *ctx) { ++((state *)ctx)->stops; }

static void check_burst(uint32_t budget, uint64_t frame_limit, int stop_at_event) {
    char title[128];
    ores_desktop_config cfg;
    ores_desktop_app app;
    ores_desktop_backend backend;
    state s = {0};
    memset(title, 'N', sizeof title); /* deliberately no source NUL */
    cfg.abi_version = ORES_DESKTOP_ABI_VERSION;
    cfg.width = 16384;
    cfg.height = 16384;
    cfg.max_events_per_frame = budget;
    cfg.frame_limit = frame_limit;
    cfg.title = title;
    cfg.title_length = sizeof title;
    app.context = &s;
    app.on_start = start_app;
    app.on_event = event_app;
    app.on_frame = frame_app;
    app.on_stop = stop_app;
    backend.context = &s;
    backend.open = open_backend;
    backend.poll = poll_backend;
    backend.present = present_backend;
    backend.close = close_backend;
    s.stop_at_event = stop_at_event;
    CHECK(ores_desktop_run(&cfg, &app, &backend) == ORES_DESKTOP_OK);
    CHECK(s.opens == 1 && s.starts == 1 && s.stops == 1 && s.closes == 1);
    CHECK(s.events == s.polls);
    if (stop_at_event) {
        CHECK(s.events == (unsigned)stop_at_event);
        CHECK(s.frames == (unsigned)((stop_at_event - 1) / (int)budget));
        CHECK(s.presents == s.frames);
    } else {
        CHECK(s.events == budget * frame_limit);
        CHECK(s.frames == frame_limit);
        CHECK(s.presents == frame_limit);
    }
}

int main(void) {
    check_burst(1u, 3u, 0);
    check_burst(4u, 3u, 0);
    check_burst(256u, 2u, 0);
    check_burst(4u, 100u, 7);
    puts("desktop bounded burst/fairness lifecycle: PASS");
    return 0;
}
