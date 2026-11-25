#include "ores_desktop_ui_mailbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\\n", __FILE__, __LINE__, #condition); \
    exit(1); \
} } while (0)

static ores_desktop_ui_message message(uint64_t index) {
    ores_desktop_ui_message m = {7, 100 + index, 50 + index,
                                  ORES_DESKTOP_UI_REQUEST_REDRAW, 0};
    return m;
}

int main(void) {
    ores_desktop_ui_mailbox q;
    ores_desktop_ui_message out = {0};
    ores_desktop_ui_message m = message(1);
    ores_desktop_ui_message foreign = message(2);
    uint64_t i;

    REQUIRE(ores_desktop_ui_mailbox_init(NULL, 7) == ORES_DESKTOP_UI_INVALID);
    REQUIRE(ores_desktop_ui_mailbox_init(&q, 0) == ORES_DESKTOP_UI_INVALID);
    REQUIRE(ores_desktop_ui_mailbox_init(&q, 7) == ORES_DESKTOP_UI_ACCEPTED);
    REQUIRE(ores_desktop_ui_mailbox_push(NULL, &m) == ORES_DESKTOP_UI_INVALID);
    REQUIRE(ores_desktop_ui_mailbox_push(&q, NULL) == ORES_DESKTOP_UI_INVALID);
    REQUIRE(ores_desktop_ui_mailbox_pop(&q, NULL) == ORES_DESKTOP_UI_INVALID);
    REQUIRE(ores_desktop_ui_mailbox_pop(&q, &out) == ORES_DESKTOP_UI_EMPTY);
    foreign.actor_generation = 8;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &foreign) == ORES_DESKTOP_UI_STALE);
    m.window_id = 0;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_INVALID);
    m = message(1);
    m.window_generation = 0;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_INVALID);
    m = message(1);
    m.command = 999;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_INVALID);
    m.command = ORES_DESKTOP_UI_SET_VISIBILITY;
    m.value = 2;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_INVALID);
    /* Requests with no payload must reject smuggled values. */
    m = message(1);
    m.value = 17;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_INVALID);
    m.command = ORES_DESKTOP_UI_REQUEST_CLOSE;
    m.value = -1;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_INVALID);
    m.command = ORES_DESKTOP_UI_SET_VISIBILITY;
    m.value = 1;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_ACCEPTED);
    m.value = 0; /* Mutating the caller-owned struct cannot change queued copy. */
    REQUIRE(ores_desktop_ui_mailbox_pop(&q, &out) == ORES_DESKTOP_UI_ACCEPTED);
    REQUIRE(out.command == ORES_DESKTOP_UI_SET_VISIBILITY && out.value == 1);
    REQUIRE(ores_desktop_ui_mailbox_pop(&q, &out) == ORES_DESKTOP_UI_EMPTY);

    for (i = 0; i < ORES_DESKTOP_UI_MAILBOX_CAPACITY; i++) {
        m = message(i + 1);
        REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_ACCEPTED);
    }
    m = message(1000);
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_FULL);
    for (i = 0; i < ORES_DESKTOP_UI_MAILBOX_CAPACITY; i++) {
        REQUIRE(ores_desktop_ui_mailbox_pop(&q, &out) == ORES_DESKTOP_UI_ACCEPTED);
        REQUIRE(out.window_id == 101 + i && out.window_generation == 51 + i);
    }
    for (i = 0; i < 4 * ORES_DESKTOP_UI_MAILBOX_CAPACITY; ++i) {
        m = message(i + 1);
        REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_ACCEPTED);
        REQUIRE(ores_desktop_ui_mailbox_pop(&q, &out) == ORES_DESKTOP_UI_ACCEPTED);
        REQUIRE(out.window_id == m.window_id);
    }

    m = message(77);
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_ACCEPTED);
    memset(&out, 0x7f, sizeof out);
    REQUIRE(ores_desktop_ui_mailbox_close(&q) == ORES_DESKTOP_UI_ACCEPTED);
    REQUIRE(ores_desktop_ui_mailbox_pop(&q, &out) == ORES_DESKTOP_UI_CLOSED);
    REQUIRE(out.actor_generation == UINT64_C(0x7f7f7f7f7f7f7f7f));
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &m) == ORES_DESKTOP_UI_CLOSED);
    /* Terminal close takes precedence over a stale but well-formed command. */
    foreign = message(88);
    foreign.actor_generation = 999;
    REQUIRE(ores_desktop_ui_mailbox_push(&q, &foreign) == ORES_DESKTOP_UI_CLOSED);
    REQUIRE(ores_desktop_ui_mailbox_close(&q) == ORES_DESKTOP_UI_ACCEPTED);
    REQUIRE(ores_desktop_ui_mailbox_close(NULL) == ORES_DESKTOP_UI_INVALID);
    puts("desktop SPSC UI actor bridge: PASS (host-only)");
    return 0;
}
