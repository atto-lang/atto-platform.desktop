#include "ores_desktop.h"
#include <stddef.h>
uint32_t ores_desktop_abi_version(void) { return ORES_DESKTOP_ABI_VERSION; }
static int valid_title(const char *title) {
    size_t i;
    if (!title) return 0;
    for (i=0; i<=127; ++i) if (title[i]=='\0') return i>0;
    return 0;
}
static int valid_status(ores_desktop_status s) {
    return s==ORES_DESKTOP_OK || s==ORES_DESKTOP_STOP;
}
ores_desktop_status ores_desktop_run(const ores_desktop_config *c,
                                     const ores_desktop_app *a,
                                     const ores_desktop_backend *b) {
    uint64_t frame=0;
    int started=0;
    ores_desktop_status outcome=ORES_DESKTOP_OK;
    if (!c || !a || !b || c->abi_version!=ORES_DESKTOP_ABI_VERSION ||
        !c->width || c->width>16384 || !c->height || c->height>16384 ||
        !c->max_events_per_frame || c->max_events_per_frame>256 ||
        !valid_title(c->title) ||
        (!a->on_start && !a->on_event && !a->on_frame) ||
        !b->open || !b->poll || !b->present || !b->close)
        return ORES_DESKTOP_INVALID_ARGUMENT;
    if (b->open(b->context,c)!=ORES_DESKTOP_OK)
        return ORES_DESKTOP_BACKEND_ERROR;
    if (a->on_start) {
        ores_desktop_status s=a->on_start(a->context);
        if (!valid_status(s)) { outcome=ORES_DESKTOP_APP_ERROR; goto cleanup; }
        started=1;
        if (s==ORES_DESKTOP_STOP) goto cleanup;
    } else started=1;
    for (;;) {
        uint32_t i;
        for (i=0; i<c->max_events_per_frame; ++i) {
            ores_desktop_event e={0,0,0,0};
            int available=-1;
            ores_desktop_status s=b->poll(b->context,&e,&available);
            if (s!=ORES_DESKTOP_OK || (available!=0 && available!=1)) {
                outcome=ORES_DESKTOP_BACKEND_ERROR; goto cleanup;
            }
            if (!available) break;
            if (e.type<ORES_DESKTOP_QUIT || e.type>ORES_DESKTOP_POINTER_UP) {
                outcome=ORES_DESKTOP_BACKEND_ERROR; goto cleanup;
            }
            if (a->on_event) {
                s=a->on_event(a->context,&e);
                if (!valid_status(s)) { outcome=ORES_DESKTOP_APP_ERROR; goto cleanup; }
                if (s==ORES_DESKTOP_STOP) goto cleanup;
            }
            if (e.type==ORES_DESKTOP_QUIT) goto cleanup;
        }
        if (a->on_frame) {
            ores_desktop_status s=a->on_frame(a->context,frame);
            if (!valid_status(s)) { outcome=ORES_DESKTOP_APP_ERROR; goto cleanup; }
            if (s==ORES_DESKTOP_STOP) goto cleanup;
        }
        if (b->present(b->context)!=ORES_DESKTOP_OK) {
            outcome=ORES_DESKTOP_BACKEND_ERROR; goto cleanup;
        }
        ++frame;
        if (c->frame_limit && frame>=c->frame_limit) goto cleanup;
    }
cleanup:
    if (started && a->on_stop) a->on_stop(a->context);
    b->close(b->context);
    return outcome;
}
