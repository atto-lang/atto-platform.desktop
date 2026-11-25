/* Host C ABI regression for maximum title bytes and callback ownership.
 * Build: cc -std=c11 -Wall -Wextra -Werror -Iinclude \
 *   src/ores_desktop.c tests/test_desktop_title_snapshot_wave31_20261105.c -o /tmp/atto-desktop-title-test
 * No compiler guest, UI rendering, mobile runtime or SDL3 parity claim.
 */
#include "ores_desktop.h"
#include <stdio.h>
#include <string.h>

static int opened_count, closed_count, started_count;
static const char *caller_title;
static int title_was_snapshotted;

static ores_desktop_status opened(void *context, const ores_desktop_config *cfg) {
    (void)context;
    ++opened_count;
    title_was_snapshotted =
        cfg->title != caller_title &&
        cfg->title_length == 127u &&
        strlen(cfg->title) == 127u &&
        memcmp(cfg->title, caller_title, 127u) == 0 &&
        cfg->title[127] == '\0';
    return ORES_DESKTOP_OK;
}
static ores_desktop_status started(void *context) {
    (void)context;
    ++started_count;
    return ORES_DESKTOP_STOP;
}
static ores_desktop_status polled(void *context, ores_desktop_event *event, int *available) {
    (void)context;
    (void)event;
    *available = 0;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status presented(void *context) {
    (void)context;
    return ORES_DESKTOP_OK;
}
static void closed(void *context) {
    (void)context;
    ++closed_count;
}

int main(void) {
    char title[128];
    ores_desktop_config cfg = {0};
    ores_desktop_app app = {0};
    ores_desktop_backend backend = {0};
    memset(title, 'X', 127u);
    title[127] = '\0';
    caller_title = title;
    cfg.abi_version = ORES_DESKTOP_ABI_VERSION;
    cfg.width = 800u; cfg.height = 600u;
    cfg.max_events_per_frame = 1u; cfg.frame_limit = 1u;
    cfg.title = title; cfg.title_length = 127u;
    app.on_start = started;
    backend.open = opened; backend.poll = polled;
    backend.present = presented; backend.close = closed;

    if (ores_desktop_run(&cfg, &app, &backend) != ORES_DESKTOP_OK ||
        opened_count != 1 || started_count != 1 || closed_count != 1 ||
        !title_was_snapshotted) return 1;

    /* Crossing the title budget must fail before any callback or close. */
    cfg.title_length = 128u;
    if (ores_desktop_run(&cfg, &app, &backend) != ORES_DESKTOP_INVALID_ARGUMENT ||
        opened_count != 1 || started_count != 1 || closed_count != 1) return 2;

    puts("desktop title snapshot and length fence: PASS");
    return 0;
}
