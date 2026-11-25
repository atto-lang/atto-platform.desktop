/* Host-only native C ABI regression: user-owned control-table mutation at open.
 * Example:
 * cc -std=c11 -Wall -Wextra -Werror -Iinclude src/ores_desktop.c \
 *   tests/test_desktop_callback_table_snapshot_gpt6_wave241_20261106.c \
 *   -o /tmp/ores-desktop-callback-snapshot && /tmp/ores-desktop-callback-snapshot
 * This does not attest to SDL3, graphical output or compiled AttoLang.
 */
#include "ores_desktop.h"
#include <stdint.h>
#include <stdio.h>

static ores_desktop_config *caller_config;
static ores_desktop_app *caller_app;
static ores_desktop_backend *caller_backend;
static unsigned opened_count, started_count, polled_count, frame_count;
static unsigned presented_count, stopped_count, closed_count, trap_count;
static int bad_frame_index;

static ores_desktop_status trap(void *ignored) {
    (void)ignored;
    ++trap_count;
    return ORES_DESKTOP_APP_ERROR;
}
static ores_desktop_status trap_poll(void *ignored, ores_desktop_event *event, int *has_event) {
    (void)ignored; (void)event; (void)has_event;
    ++trap_count;
    return ORES_DESKTOP_BACKEND_ERROR;
}
static ores_desktop_status trap_frame(void *ignored, uint64_t frame) {
    (void)ignored; (void)frame;
    ++trap_count;
    return ORES_DESKTOP_APP_ERROR;
}
static void trap_close(void *ignored) { (void)ignored; ++trap_count; }

static ores_desktop_status opened(void *ignored, const ores_desktop_config *snapshot) {
    (void)ignored;
    ++opened_count;
    if (snapshot->frame_limit != 1 || snapshot->width != 640 ||
        snapshot->max_events_per_frame != 1) return ORES_DESKTOP_BACKEND_ERROR;

    /* A hostile or buggy callback modifies the caller's original pointers
     * after admission. The host must keep using its independent snapshots. */
    caller_config->width = 0;
    caller_config->frame_limit = 0;
    caller_config->max_events_per_frame = 0;
    caller_app->on_start = trap;
    caller_app->on_frame = trap_frame;
    caller_app->on_stop = trap_close;
    caller_backend->poll = trap_poll;
    caller_backend->present = trap;
    caller_backend->close = trap_close;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status started(void *ignored) {
    (void)ignored;
    ++started_count;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status polled(void *ignored, ores_desktop_event *event, int *has_event) {
    (void)ignored; (void)event;
    ++polled_count;
    *has_event = 0;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status frame_tick(void *ignored, uint64_t index) {
    (void)ignored;
    ++frame_count;
    if (index != 0) bad_frame_index = 1;
    /* Stop at frame 1 if a mutation accidentally removes the bounded limit. */
    return frame_count > 1 ? ORES_DESKTOP_STOP : ORES_DESKTOP_OK;
}
static ores_desktop_status presented(void *ignored) {
    (void)ignored;
    ++presented_count;
    return ORES_DESKTOP_OK;
}
static void stopped(void *ignored) { (void)ignored; ++stopped_count; }
static void closed(void *ignored) { (void)ignored; ++closed_count; }

int main(void) {
    const char title[] = {'T','e','s','t'};
    ores_desktop_config config = {0};
    ores_desktop_app app = {0};
    ores_desktop_backend backend = {0};

    config.abi_version = ORES_DESKTOP_ABI_VERSION;
    config.width = 640; config.height = 480;
    config.max_events_per_frame = 1; config.frame_limit = 1;
    config.title = title; config.title_length = sizeof(title);
    app.on_start = started; app.on_frame = frame_tick; app.on_stop = stopped;
    backend.open = opened; backend.poll = polled;
    backend.present = presented; backend.close = closed;

    caller_config = &config; caller_app = &app; caller_backend = &backend;
    if (ores_desktop_run(&config, &app, &backend) != ORES_DESKTOP_OK ||
        opened_count != 1 || started_count != 1 || polled_count != 1 ||
        frame_count != 1 || presented_count != 1 ||
        stopped_count != 1 || closed_count != 1 ||
        trap_count != 0 || bad_frame_index) {
        fputs("desktop caller-table snapshot violated\n", stderr);
        return 1;
    }
    puts("desktop callback table and frame-limit snapshot: PASS");
    return 0;
}
