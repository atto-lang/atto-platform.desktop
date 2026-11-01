#ifndef ORES_DESKTOP_H
#define ORES_DESKTOP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* HOST C ABI only: not yet callable from general Oreslang guest code. */
#define ORES_DESKTOP_ABI_VERSION 1u
typedef enum {
    ORES_DESKTOP_OK=0, ORES_DESKTOP_STOP=1,
    ORES_DESKTOP_INVALID_ARGUMENT=2, ORES_DESKTOP_BACKEND_ERROR=3,
    ORES_DESKTOP_APP_ERROR=4
} ores_desktop_status;
typedef enum {
    ORES_DESKTOP_QUIT=1, ORES_DESKTOP_RESIZE=2,
    ORES_DESKTOP_KEY_DOWN=3, ORES_DESKTOP_KEY_UP=4,
    ORES_DESKTOP_POINTER_MOVE=5, ORES_DESKTOP_POINTER_DOWN=6,
    ORES_DESKTOP_POINTER_UP=7
} ores_desktop_event_type;
/* In-process layout, not a wire protocol. key codes are backend scancodes. */
typedef struct {
    uint32_t type, code;
    int32_t x, y;
} ores_desktop_event;
typedef struct {
    uint32_t abi_version, width, height, max_events_per_frame;
    uint64_t frame_limit; /* 0 means unbounded */
    const char *title;    /* borrowed UTF-8, NUL terminated, 1..127 bytes */
} ores_desktop_config;
/* Borrowed callback event pointer expires on return. Return OK or STOP only.
 * The host honors the original polled QUIT event even if callback code
 * improperly casts away const and mutates the event object. */
typedef struct {
    void *context;
    ores_desktop_status (*on_start)(void *);
    ores_desktop_status (*on_event)(void *, const ores_desktop_event *);
    ores_desktop_status (*on_frame)(void *, uint64_t);
    void (*on_stop)(void *);
} ores_desktop_app;
/* All backend callbacks required; failed open cleans up its own partial state. */
typedef struct {
    void *context;
    ores_desktop_status (*open)(void *, const ores_desktop_config *);
    ores_desktop_status (*poll)(void *, ores_desktop_event *, int *has_event);
    ores_desktop_status (*present)(void *);
    void (*close)(void *);
} ores_desktop_backend;
uint32_t ores_desktop_abi_version(void);
/* Single-threaded UI loop. Successful open always gets one close. */
ores_desktop_status ores_desktop_run(const ores_desktop_config *,
                                    const ores_desktop_app *,
                                    const ores_desktop_backend *);
#ifdef __cplusplus
}
#endif
#endif
