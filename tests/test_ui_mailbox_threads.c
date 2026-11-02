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

int main(void) {
    fixture f = {0};
    pthread_t writer, reader;
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
    if (ores_desktop_ui_mailbox_close(&f.queue) !=
        ORES_DESKTOP_UI_ACCEPTED) return 7;
    puts("desktop SPSC concurrent 50k FIFO messages: PASS (host-only)");
    return 0;
}
