#ifndef ORES_DESKTOP_H
#define ORES_DESKTOP_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* HOST C ABI only: not yet callable from general Oreslang guest code. */
#define ORES_DESKTOP_ABI_VERSION 2u
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
    const char *title;    /* borrowed UTF-8 without controls; host snapshots + terminates */
    size_t title_length;  /* 1..127; source need not be NUL terminated */
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
/* Validate host configuration without opening a native window or invoking
 * callbacks. The title is inspected only for this call. A later run
 * independently snapshots/validates it to avoid a stale preflight result.
 * This is a host C ABI utility, not compiled-guest availability proof. */
ores_desktop_status ores_desktop_validate_config(const ores_desktop_config *config);
/* Single-threaded UI loop. Successful open always gets one close. */
ores_desktop_status ores_desktop_run(const ores_desktop_config *,
                                    const ores_desktop_app *,
                                    const ores_desktop_backend *);
/* Deterministic host-only defaults; returned title points to static immutable bytes. */
ores_desktop_config ores_desktop_default_config(void);

#ifdef __cplusplus
}
#endif
#endif
