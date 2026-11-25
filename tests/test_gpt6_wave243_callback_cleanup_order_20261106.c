/* Native host-only temporal contract: callback order and exactly-once close.
 * Build with src/ores_desktop.c, C11 -Wall -Wextra -Werror.
 * Not SDL3 GUI, AttoLang guest or AOT parity evidence.
 */
#include "ores_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum scenario {
    OPEN_ERROR, START_STOP, EVENT_QUIT, EVENT_STOP, INVALID_POLL,
    FRAME_STOP, PRESENT_ERROR, NORMAL_FRAME
};
static enum scenario mode;
static char logbuf[32];
static size_t used;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"desktop temporal %s:%d %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static void logcall(char c) {
    CHECK(used + 1 < sizeof logbuf);
    logbuf[used++] = c;
    logbuf[used] = '\0';
}
static ores_desktop_status open_backend(void *ctx, const ores_desktop_config *cfg) {
    (void)ctx;
    CHECK(cfg->frame_limit == 1);
    logcall('O');
    return mode == OPEN_ERROR ? ORES_DESKTOP_BACKEND_ERROR : ORES_DESKTOP_OK;
}
static ores_desktop_status start_app(void *ctx) {
    (void)ctx;
    logcall('S');
    return mode == START_STOP ? ORES_DESKTOP_STOP : ORES_DESKTOP_OK;
}
static ores_desktop_status poll_backend(void *ctx, ores_desktop_event *event, int *available) {
    (void)ctx;
    logcall('P');
    *available = 0;
    if (mode == INVALID_POLL) *available = 2;
    if (mode == EVENT_QUIT || mode == EVENT_STOP) {
        *available = 1;
        event->type = mode == EVENT_QUIT ? ORES_DESKTOP_QUIT : ORES_DESKTOP_KEY_DOWN;
    }
    return ORES_DESKTOP_OK;
}
static ores_desktop_status event_app(void *ctx, const ores_desktop_event *event) {
    (void)ctx;
    CHECK(event->type == ORES_DESKTOP_QUIT || event->type == ORES_DESKTOP_KEY_DOWN);
    logcall('E');
    return mode == EVENT_STOP ? ORES_DESKTOP_STOP : ORES_DESKTOP_OK;
}
static ores_desktop_status frame_app(void *ctx, uint64_t frame) {
    (void)ctx;
    CHECK(frame == 0);
    logcall('F');
    return mode == FRAME_STOP ? ORES_DESKTOP_STOP : ORES_DESKTOP_OK;
}
static ores_desktop_status present_backend(void *ctx) {
    (void)ctx;
    logcall('V');
    return mode == PRESENT_ERROR ? ORES_DESKTOP_BACKEND_ERROR : ORES_DESKTOP_OK;
}
static void stop_app(void *ctx) { (void)ctx; logcall('T'); }
static void close_backend(void *ctx) { (void)ctx; logcall('C'); }

static void run_case(enum scenario s, const char *expect, ores_desktop_status want) {
    ores_desktop_config cfg = {ORES_DESKTOP_ABI_VERSION, 640u, 480u, 1u, 1u, "T", 1u};
    ores_desktop_app app = {NULL, start_app, event_app, frame_app, stop_app};
    ores_desktop_backend backend = {NULL, open_backend, poll_backend, present_backend, close_backend};
    mode = s;
    used = 0;
    logbuf[0] = '\0';
    CHECK(ores_desktop_run(&cfg, &app, &backend) == want);
    if (strcmp(logbuf, expect) != 0) {
        fprintf(stderr, "case %d expected %s, got %s\n", (int)s, expect, logbuf);
        exit(1);
    }
}
int main(void) {
    run_case(OPEN_ERROR, "O", ORES_DESKTOP_BACKEND_ERROR);
    run_case(START_STOP, "OSTC", ORES_DESKTOP_OK);
    run_case(EVENT_QUIT, "OSPETC", ORES_DESKTOP_OK);
    run_case(EVENT_STOP, "OSPETC", ORES_DESKTOP_OK);
    run_case(INVALID_POLL, "OSPTC", ORES_DESKTOP_BACKEND_ERROR);
    run_case(FRAME_STOP, "OSPFTC", ORES_DESKTOP_OK);
    run_case(PRESENT_ERROR, "OSPFVTC", ORES_DESKTOP_BACKEND_ERROR);
    run_case(NORMAL_FRAME, "OSPFVTC", ORES_DESKTOP_OK);
    puts("desktop callback order and cleanup on eight paths: PASS");
    return 0;
}
