#include "ores_desktop_ui_mailbox.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

#define COUNT 50000
typedef struct {
    ores_desktop_ui_mailbox queue;
    _Atomic int failure;
    _Atomic int after_close;
} fixture;

static void *producer(void *arg) {
    fixture *f = (fixture*)arg;
    int i;
    for (i = 0; i < COUNT; i++) {
        ores_desktop_ui_message message = {100, 1, 5,
            ORES_DESKTOP_UI_REQUEST_REDRAW, i};
        for (;;) {
            ores_desktop_ui_result r =
                ores_desktop_ui_mailbox_push(&f->queue, &message);
            if (r == ORES_DESKTOP_UI_ACCEPTED) break;
            if (r != ORES_DESKTOP_UI_FULL) {
                atomic_store(&f->failure, 1);
                return NULL;
            }
            sched_yield();
        }
    }
    return NULL;
}

static void *consumer(void *arg) {
    fixture *f = (fixture*)arg;
    int i;
    for (i = 0; i < COUNT; i++) {
        ores_desktop_ui_message message;
        for (;;) {
            ores_desktop_ui_result r =
                ores_desktop_ui_mailbox_pop(&f->queue, &message);
            if (r == ORES_DESKTOP_UI_ACCEPTED) break;
            if (r != ORES_DESKTOP_UI_EMPTY) {
                atomic_store(&f->failure, 2);
                return NULL;
            }
            sched_yield();
        }
        if (message.actor_generation != 100 || message.window_id != 1 ||
            message.window_generation != 5 ||
            message.command != ORES_DESKTOP_UI_REQUEST_REDRAW ||
            message.value != i) {
            atomic_store(&f->failure, 3);
            return NULL;
        }
    }
    return NULL;
}

static int wraparound_case(void) {
    ores_desktop_ui_mailbox q;
    ores_desktop_ui_message m, out;
    uint32_t i;
    if (ores_desktop_ui_mailbox_init(&q, 401) !=
        ORES_DESKTOP_UI_ACCEPTED) return 1;
    /* No threads own this new queue yet: place both counters just before
     * wraparound, then prove ordering across UINT32_MAX -> 0. */
    atomic_store(&q.read_pos, UINT32_MAX - 1u);
    atomic_store(&q.write_pos, UINT32_MAX - 1u);
    for (i = 0; i < ORES_DESKTOP_UI_MAILBOX_CAPACITY; ++i) {
        m = (ores_desktop_ui_message){401, 1, 1,
            ORES_DESKTOP_UI_REQUEST_REDRAW, (int32_t)i};
        if (ores_desktop_ui_mailbox_push(&q, &m) !=
            ORES_DESKTOP_UI_ACCEPTED) return 2;
    }
    m.value = 99;
    if (ores_desktop_ui_mailbox_push(&q, &m) !=
        ORES_DESKTOP_UI_FULL) return 3;
    for (i = 0; i < ORES_DESKTOP_UI_MAILBOX_CAPACITY; ++i) {
        if (ores_desktop_ui_mailbox_pop(&q, &out) !=
            ORES_DESKTOP_UI_ACCEPTED || out.value != (int32_t)i)
            return 4;
    }
    return ores_desktop_ui_mailbox_pop(&q, &out) == ORES_DESKTOP_UI_EMPTY
        ? 0 : 5;
}

static void *attempt_after_close(void *arg) {
    fixture *f = (fixture*)arg;
    ores_desktop_ui_message m = {100, 1, 5,
        ORES_DESKTOP_UI_REQUEST_REDRAW, 0};
    while (!atomic_load(&f->after_close)) sched_yield();
    if (ores_desktop_ui_mailbox_push(&f->queue, &m) !=
        ORES_DESKTOP_UI_CLOSED)
        atomic_store(&f->failure, 4);
    return NULL;
}

int main(void) {
    fixture f = {0};
    pthread_t writer, reader;
    if (wraparound_case() != 0) {
        fputs("counter wraparound admission failed\n", stderr);
        return 8;
    }
    if (ores_desktop_ui_mailbox_init(&f.queue, 100) !=
        ORES_DESKTOP_UI_ACCEPTED) return 2;
    if (pthread_create(&writer, NULL, producer, &f) != 0) return 3;
    if (pthread_create(&reader, NULL, consumer, &f) != 0) return 4;
    if (pthread_join(writer, NULL) != 0 || pthread_join(reader, NULL) != 0)
        return 5;
    if (atomic_load(&f.failure) != 0) {
        fprintf(stderr, "SPSC ordering failed: %d\\n", atomic_load(&f.failure));
        return 6;
    }
    if (pthread_create(&writer, NULL, attempt_after_close, &f) != 0)
        return 9;
    if (ores_desktop_ui_mailbox_close(&f.queue) !=
        ORES_DESKTOP_UI_ACCEPTED) return 7;
    atomic_store(&f.after_close, 1);
    if (pthread_join(writer, NULL) != 0 || atomic_load(&f.failure) != 0)
        return 10;
    puts("desktop SPSC concurrent 50k FIFO, wraparound and closure: PASS (host-only)");
    return 0;
}
