/* Host-only desktop ABI lifecycle: STOP and invalid-start cleanup.
 * cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude src/ores_desktop.c \
 *    tests/test_gpt6_wave242_desktop_start_stop_20261106.c \
 *    -o /tmp/ores-desktop-wave242 && /tmp/ores-desktop-wave242
 * Not SDL3 rendering or compiled AttoLang guest execution.
 */
#include "ores_desktop.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static int mode;
static unsigned opens, starts, stops, closes, polls, frames, presents;

static ores_desktop_status opened(void *ignored, const ores_desktop_config *cfg) {
    (void)ignored;
    ++opens;
    assert(cfg->title_length == 4u && cfg->title[4] == '\0');
    return mode == 2 ? ORES_DESKTOP_BACKEND_ERROR : ORES_DESKTOP_OK;
}
static ores_desktop_status started(void *ignored) {
    (void)ignored;
    ++starts;
    return mode == 0 ? ORES_DESKTOP_STOP : ORES_DESKTOP_INVALID_ARGUMENT;
}
static void stopped(void *ignored) { (void)ignored; ++stops; }
static void closed(void *ignored) { (void)ignored; ++closes; }
static ores_desktop_status poll(void *ignored, ores_desktop_event *event, int *available) {
    (void)ignored; (void)event; (void)available; ++polls;
    return ORES_DESKTOP_BACKEND_ERROR;
}
static ores_desktop_status frame(void *ignored, uint64_t index) {
    (void)ignored; (void)index; ++frames;
    return ORES_DESKTOP_APP_ERROR;
}
static ores_desktop_status present(void *ignored) {
    (void)ignored; ++presents; return ORES_DESKTOP_BACKEND_ERROR;
}
static void reset(int next_mode) {
    mode = next_mode;
    opens = starts = stops = closes = polls = frames = presents = 0u;
}
int main(void) {
    const char nonterminated_title[4] = {'A','t','t','o'};
    ores_desktop_config cfg = {0};
    ores_desktop_app app = {0};
    ores_desktop_backend backend = {0};
    cfg.abi_version = ORES_DESKTOP_ABI_VERSION;
    cfg.width = 640; cfg.height = 480;
    cfg.max_events_per_frame = 1;
    cfg.frame_limit = 1;
    cfg.title = nonterminated_title;
    cfg.title_length = sizeof nonterminated_title;
    app.on_start = started; app.on_frame = frame; app.on_stop = stopped;
    backend.open = opened; backend.poll = poll;
    backend.present = present; backend.close = closed;

    reset(0);
    assert(ores_desktop_run(&cfg, &app, &backend) == ORES_DESKTOP_OK);
    assert(opens == 1u && starts == 1u && stops == 1u && closes == 1u);
    assert(polls == 0u && frames == 0u && presents == 0u);

    reset(1);
    assert(ores_desktop_run(&cfg, &app, &backend) == ORES_DESKTOP_APP_ERROR);
    assert(opens == 1u && starts == 1u && stops == 0u && closes == 1u);
    assert(polls == 0u && frames == 0u && presents == 0u);

    reset(2);
    assert(ores_desktop_run(&cfg, &app, &backend) == ORES_DESKTOP_BACKEND_ERROR);
    assert(opens == 1u && starts == 0u && stops == 0u && closes == 0u);
    assert(polls == 0u && frames == 0u && presents == 0u);
    puts("desktop early-stop and failed-open cleanup: PASS");
    return 0;
}
