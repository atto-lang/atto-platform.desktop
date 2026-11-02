#include "ores_desktop_ui_mailbox.h"

ores_desktop_ui_result ores_desktop_ui_mailbox_init(
    ores_desktop_ui_mailbox *q, uint64_t generation) {
    if (!q || !generation) return ORES_DESKTOP_UI_INVALID;
    q->actor_generation = generation;
    atomic_init(&q->read_pos, 0);
    atomic_init(&q->write_pos, 0);
    atomic_init(&q->closed, 0);
    return ORES_DESKTOP_UI_ACCEPTED;
}

static int valid_message(const ores_desktop_ui_message *m) {
    if (!m || !m->window_id || !m->window_generation) return 0;
    if (m->command == ORES_DESKTOP_UI_SET_VISIBILITY)
        return m->value == 0 || m->value == 1;
    return m->command == ORES_DESKTOP_UI_REQUEST_REDRAW ||
           m->command == ORES_DESKTOP_UI_REQUEST_CLOSE;
}

ores_desktop_ui_result ores_desktop_ui_mailbox_push(
    ores_desktop_ui_mailbox *q, const ores_desktop_ui_message *msg) {
    uint64_t writer, reader;
    if (!q || !valid_message(msg)) return ORES_DESKTOP_UI_INVALID;
    if (msg->actor_generation != q->actor_generation)
        return ORES_DESKTOP_UI_STALE;
    if (atomic_load_explicit(&q->closed, memory_order_acquire))
        return ORES_DESKTOP_UI_CLOSED;
    writer = atomic_load_explicit(&q->write_pos, memory_order_relaxed);
    reader = atomic_load_explicit(&q->read_pos, memory_order_acquire);
    if (writer - reader >= ORES_DESKTOP_UI_MAILBOX_CAPACITY)
        return ORES_DESKTOP_UI_FULL;
    q->messages[writer % ORES_DESKTOP_UI_MAILBOX_CAPACITY] = *msg;
    atomic_store_explicit(&q->write_pos, writer + 1, memory_order_release);
    return atomic_load_explicit(&q->closed, memory_order_acquire)
        ? ORES_DESKTOP_UI_CLOSED : ORES_DESKTOP_UI_ACCEPTED;
}

ores_desktop_ui_result ores_desktop_ui_mailbox_pop(
    ores_desktop_ui_mailbox *q, ores_desktop_ui_message *out) {
    uint64_t reader, writer;
    ores_desktop_ui_message snapshot;
    if (!q || !out) return ORES_DESKTOP_UI_INVALID;
    if (atomic_load_explicit(&q->closed, memory_order_acquire))
        return ORES_DESKTOP_UI_CLOSED;
    reader = atomic_load_explicit(&q->read_pos, memory_order_relaxed);
    writer = atomic_load_explicit(&q->write_pos, memory_order_acquire);
    if (reader == writer) return ORES_DESKTOP_UI_EMPTY;
    snapshot = q->messages[reader % ORES_DESKTOP_UI_MAILBOX_CAPACITY];
    atomic_store_explicit(&q->read_pos, reader + 1, memory_order_release);
    if (atomic_load_explicit(&q->closed, memory_order_acquire))
        return ORES_DESKTOP_UI_CLOSED;
    if (snapshot.actor_generation != q->actor_generation)
        return ORES_DESKTOP_UI_STALE;
    *out = snapshot;
    return ORES_DESKTOP_UI_ACCEPTED;
}

ores_desktop_ui_result ores_desktop_ui_mailbox_close(
    ores_desktop_ui_mailbox *q) {
    if (!q) return ORES_DESKTOP_UI_INVALID;
    atomic_store_explicit(&q->closed, 1, memory_order_release);
    return ORES_DESKTOP_UI_ACCEPTED;
}
