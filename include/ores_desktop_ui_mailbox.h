#ifndef ORES_DESKTOP_UI_MAILBOX_H
#define ORES_DESKTOP_UI_MAILBOX_H

#include <stdatomic.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C11 host-only SPSC bridge: one actor producer, one UI-thread consumer.
 * The queue is not itself an AttoLang guest import, shared actor mailbox,
 * native window registry, or cross-language wire representation.
 * Do not copy or move this object once initialized and published.
 * Caller owns storage; there is no allocation, blocking, or OS access.
 */
#define ORES_DESKTOP_UI_MAILBOX_CAPACITY 64u

typedef enum {
    ORES_DESKTOP_UI_ACCEPTED = 0,
    ORES_DESKTOP_UI_INVALID = 1,
    ORES_DESKTOP_UI_FULL = 2,
    ORES_DESKTOP_UI_EMPTY = 3,
    ORES_DESKTOP_UI_CLOSED = 4,
    ORES_DESKTOP_UI_STALE = 5
} ores_desktop_ui_result;

typedef enum {
    ORES_DESKTOP_UI_REQUEST_REDRAW = 1,
    ORES_DESKTOP_UI_REQUEST_CLOSE = 2,
    ORES_DESKTOP_UI_SET_VISIBILITY = 3
} ores_desktop_ui_command;

/* No pointers, strings, OS handles, or owned resources may cross this edge.
 * Window generation must be validated by the UI-owned window registry before
 * dispatch. Actor generation is pinned to one mailbox incarnation.
 */
typedef struct {
    uint64_t actor_generation;
    uint64_t window_id;
    uint64_t window_generation;
    uint32_t command;
    int32_t value;
} ores_desktop_ui_message;

typedef struct {
    _Atomic uint64_t read_pos;
    _Atomic uint64_t write_pos;
    _Atomic int closed;
    uint64_t actor_generation;
    ores_desktop_ui_message messages[ORES_DESKTOP_UI_MAILBOX_CAPACITY];
} ores_desktop_ui_mailbox;

/* Initialize once before threads obtain pointers to this mailbox.
 * Never reinitialize or free while a producer or consumer can access it.
 * Use a new mailbox object for a new actor generation.
 */
ores_desktop_ui_result ores_desktop_ui_mailbox_init(
    ores_desktop_ui_mailbox *mailbox, uint64_t actor_generation);

/* Bounded, lock-free SPSC. Each operation is used exclusively by its named
 * thread role: exactly one producer calls push and one UI consumer calls pop.
 * These functions do not call guest code or OS APIs.
 */
ores_desktop_ui_result ores_desktop_ui_mailbox_push(
    ores_desktop_ui_mailbox *mailbox, const ores_desktop_ui_message *message);
ores_desktop_ui_result ores_desktop_ui_mailbox_pop(
    ores_desktop_ui_mailbox *mailbox, ores_desktop_ui_message *out);

/* Terminal shutdown. UI code MUST serialize window close with dispatch on
 * its own thread and recheck window generations before acting on any result.
 * Racing pushes may be physically published, but are never intentionally
 * dispatched after close; their return value may be ACCEPTED or CLOSED.
 */
ores_desktop_ui_result ores_desktop_ui_mailbox_close(
    ores_desktop_ui_mailbox *mailbox);

#ifdef __cplusplus
}
#endif

#endif
