#include "ores_desktop_ui_mailbox.h"

/* Counters wrap modulo 2^32; the live producer-consumer distance stays
 * <=64. C11 release/acquire and Win32 Interlocked full fences preserve FIFO.
 */
#if defined(_MSC_VER) && !defined(__clang__)
static uint32_t read_counter(ores_desktop_atomic_counter *p) {
    return (uint32_t)InterlockedCompareExchange(p, 0, 0);
}
static void write_counter(ores_desktop_atomic_counter *p, uint32_t v) {
    (void)InterlockedExchange(p, (LONG)v);
}
static void initialize_counters(ores_desktop_ui_mailbox *q) {
    q->read_pos = 0; q->write_pos = 0; q->closed = 0;
}
static int supports_lock_free(ores_desktop_ui_mailbox *q) {
    (void)q;
    return 1;
}
#else
static uint32_t read_counter(ores_desktop_atomic_counter *p) {
    return atomic_load_explicit(p, memory_order_acquire);
}
static void write_counter(ores_desktop_atomic_counter *p, uint32_t v) {
    atomic_store_explicit(p, v, memory_order_release);
}
static void initialize_counters(ores_desktop_ui_mailbox *q) {
    atomic_init(&q->read_pos, 0);
    atomic_init(&q->write_pos, 0);
    atomic_init(&q->closed, 0);
}
static int supports_lock_free(ores_desktop_ui_mailbox *q) {
    return atomic_is_lock_free(&q->read_pos) &&
           atomic_is_lock_free(&q->write_pos) &&
           atomic_is_lock_free(&q->closed);
}
#endif


ores_desktop_ui_result ores_desktop_ui_mailbox_init(
    ores_desktop_ui_mailbox *q, uint64_t generation) {
    if (!q || !generation) return ORES_DESKTOP_UI_INVALID;
    initialize_counters(q);
    if (!supports_lock_free(q)) return ORES_DESKTOP_UI_INVALID;
    q->actor_generation = generation;
    return ORES_DESKTOP_UI_ACCEPTED;
}

static int valid_message(const ores_desktop_ui_message *m) {
    if (!m || !m->window_id || !m->window_generation) return 0;
    if (m->command == ORES_DESKTOP_UI_SET_VISIBILITY)
        return m->value == 0 || m->value == 1;
    /* Requests without an integer payload must be encoded canonically.
     * Reject hidden/trailing data rather than forward it to a future UI layer. */
    if (m->command == ORES_DESKTOP_UI_REQUEST_REDRAW ||
        m->command == ORES_DESKTOP_UI_REQUEST_CLOSE)
        return m->value == 0;
    return 0;
}

ores_desktop_ui_result ores_desktop_ui_mailbox_push(
    ores_desktop_ui_mailbox *q, const ores_desktop_ui_message *msg) {
    uint32_t writer, reader;
    if (!q || !valid_message(msg)) return ORES_DESKTOP_UI_INVALID;
    if (read_counter(&q->closed))
        return ORES_DESKTOP_UI_CLOSED;
    if (msg->actor_generation != q->actor_generation)
        return ORES_DESKTOP_UI_STALE;
    writer = read_counter(&q->write_pos);
    reader = read_counter(&q->read_pos);
    if (writer - reader >= ORES_DESKTOP_UI_MAILBOX_CAPACITY)
        return read_counter(&q->closed) ? ORES_DESKTOP_UI_CLOSED
                                        : ORES_DESKTOP_UI_FULL;
    q->messages[writer % ORES_DESKTOP_UI_MAILBOX_CAPACITY] = *msg;
    write_counter(&q->write_pos, writer + 1);
    return read_counter(&q->closed)
        ? ORES_DESKTOP_UI_CLOSED : ORES_DESKTOP_UI_ACCEPTED;
}

ores_desktop_ui_result ores_desktop_ui_mailbox_pop(
    ores_desktop_ui_mailbox *q, ores_desktop_ui_message *out) {
    uint32_t reader, writer;
    ores_desktop_ui_message snapshot;
    if (!q || !out) return ORES_DESKTOP_UI_INVALID;
    if (read_counter(&q->closed))
        return ORES_DESKTOP_UI_CLOSED;
    reader = read_counter(&q->read_pos);
    writer = read_counter(&q->write_pos);
    if (reader == writer)
        return read_counter(&q->closed) ? ORES_DESKTOP_UI_CLOSED
                                        : ORES_DESKTOP_UI_EMPTY;
    snapshot = q->messages[reader % ORES_DESKTOP_UI_MAILBOX_CAPACITY];
    write_counter(&q->read_pos, reader + 1);
    if (read_counter(&q->closed))
        return ORES_DESKTOP_UI_CLOSED;
    if (snapshot.actor_generation != q->actor_generation)
        return ORES_DESKTOP_UI_STALE;
    *out = snapshot;
    return ORES_DESKTOP_UI_ACCEPTED;
}

ores_desktop_ui_result ores_desktop_ui_mailbox_close(
    ores_desktop_ui_mailbox *q) {
    if (!q) return ORES_DESKTOP_UI_INVALID;
    write_counter(&q->closed, 1);
    return ORES_DESKTOP_UI_ACCEPTED;
}
