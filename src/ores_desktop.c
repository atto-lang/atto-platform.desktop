#include "ores_desktop.h"
#include <stddef.h>
uint32_t ores_desktop_abi_version(void) { return ORES_DESKTOP_ABI_VERSION; }
/* SDL3 window titles must be complete UTF-8 scalar strings. */
static int valid_utf8_title(const char *s, size_t length) {
    size_t i=0;
    while (i<length) {
        unsigned char c=(unsigned char)s[i];
        size_t extra, j;
        if (c<0x20 || c==0x7f) return 0;
        if (c<0x80) { ++i; continue; }
        if (c>=0xc2 && c<=0xdf) extra=1;
        else if (c>=0xe0 && c<=0xef) extra=2;
        else if (c>=0xf0 && c<=0xf4) extra=3;
        else return 0;
        if (length-i<=extra) return 0;
        for (j=1;j<=extra;++j) {
            unsigned char b=(unsigned char)s[i+j];
            if ((b&0xc0)!=0x80) return 0;
        }
        {
            unsigned char second=(unsigned char)s[i+1];
            if ((c==0xc2 && second>=0x80 && second<=0x9f) ||
                (c==0xe0 && second<0xa0) ||
                (c==0xed && second>0x9f) ||
                (c==0xf0 && second<0x90) ||
                (c==0xf4 && second>0x8f)) return 0;
        }
        /* Reject visual spoofing controls after validating the complete UTF-8
         * scalar. Keep ordinary RTL letters and U+200D emoji joiners valid. */
        {
            unsigned int scalar=c & (extra==1 ? 0x1fu : extra==2 ? 0x0fu : 0x07u);
            for (j=1;j<=extra;++j) {
                scalar=(scalar<<6) | ((unsigned char)s[i+j] & 0x3fu);
            }
            if (scalar==0x00adu || scalar==0x034fu || scalar==0x061cu ||
                scalar==0x180eu || scalar==0x200bu ||
                scalar==0x200eu || scalar==0x200fu ||
                (scalar>=0x2028u && scalar<=0x202eu) ||
                (scalar>=0x2060u && scalar<=0x206fu) ||
                scalar==0xfeffu) return 0;
        }
        i+=extra+1;
    }
    return 1;
}
static int snapshot_title(const char *source, size_t length, char destination[128]) {
    size_t i;
    if (!source || length == 0 || length > 127) return 0;
    if (!valid_utf8_title(source,length)) return 0;
    for (i=0; i<length; ++i) {
        if (source[i]=='\0') return 0; /* reject ambiguous/truncated UI titles */
        destination[i]=source[i];
    }
    destination[length]='\0';
    return 1;
}
static int valid_status(ores_desktop_status s) {
    return s==ORES_DESKTOP_OK || s==ORES_DESKTOP_STOP;
}
ores_desktop_status ores_desktop_validate_event(const ores_desktop_event *event) {
    if (!event || event->type < ORES_DESKTOP_QUIT ||
        event->type > ORES_DESKTOP_POINTER_UP)
        return ORES_DESKTOP_INVALID_ARGUMENT;
    if (event->type == ORES_DESKTOP_RESIZE &&
        (event->x < 0 || event->y < 0 ||
         event->x > 16384 || event->y > 16384))
        return ORES_DESKTOP_INVALID_ARGUMENT;
    return ORES_DESKTOP_OK;
}

ores_desktop_status ores_desktop_run(const ores_desktop_config *c,
                                     const ores_desktop_app *a,
                                     const ores_desktop_backend *b) {
    ores_desktop_config cfg;
    ores_desktop_app app;
    ores_desktop_backend backend;
    char title[128];
    uint32_t max_events;
    uint64_t frame_limit;
    uint64_t frame=0;
    int started=0;
    ores_desktop_status outcome=ORES_DESKTOP_OK;
    if (!c || !a || !b) return ORES_DESKTOP_INVALID_ARGUMENT;

    /* Snapshot caller-owned control tables before invoking callbacks. Callbacks may
     * mutate their own context or the original structures, but cannot redirect
     * later host control flow through a time-of-check/time-of-use rewrite. */
    cfg=*c;
    app=*a;
    backend=*b;
    if (cfg.abi_version!=ORES_DESKTOP_ABI_VERSION ||
        !cfg.width || cfg.width>16384 || !cfg.height || cfg.height>16384 ||
        !cfg.max_events_per_frame || cfg.max_events_per_frame>256 ||
        !snapshot_title(cfg.title,cfg.title_length,title) ||
        (!app.on_start && !app.on_event && !app.on_frame) ||
        !backend.open || !backend.poll || !backend.present || !backend.close)
        return ORES_DESKTOP_INVALID_ARGUMENT;
    cfg.title=title;
    max_events=cfg.max_events_per_frame;
    frame_limit=cfg.frame_limit;

    if (backend.open(backend.context,&cfg)!=ORES_DESKTOP_OK)
        return ORES_DESKTOP_BACKEND_ERROR;
    if (app.on_start) {
        ores_desktop_status s=app.on_start(app.context);
        if (!valid_status(s)) { outcome=ORES_DESKTOP_APP_ERROR; goto cleanup; }
        started=1;
        if (s==ORES_DESKTOP_STOP) goto cleanup;
    } else started=1;
    for (;;) {
        uint32_t i;
        for (i=0; i<max_events; ++i) {
            ores_desktop_event e={0,0,0,0};
            uint32_t polled_type;
            int available=-1;
            ores_desktop_status s=backend.poll(backend.context,&e,&available);
            if (s!=ORES_DESKTOP_OK || (available!=0 && available!=1)) {
                outcome=ORES_DESKTOP_BACKEND_ERROR; goto cleanup;
            }
            if (!available) break;
            if (ores_desktop_validate_event(&e) != ORES_DESKTOP_OK) {
                outcome=ORES_DESKTOP_BACKEND_ERROR; goto cleanup;
            }
            /* Do not let a callback's cast-away-const write change whether
             * the host honors an already validated quit event. */
            polled_type=e.type;
            if (app.on_event) {
                s=app.on_event(app.context,&e);
                if (!valid_status(s)) { outcome=ORES_DESKTOP_APP_ERROR; goto cleanup; }
                if (s==ORES_DESKTOP_STOP) goto cleanup;
            }
            if (polled_type==ORES_DESKTOP_QUIT) goto cleanup;
        }
        if (app.on_frame) {
            ores_desktop_status s=app.on_frame(app.context,frame);
            if (!valid_status(s)) { outcome=ORES_DESKTOP_APP_ERROR; goto cleanup; }
            if (s==ORES_DESKTOP_STOP) goto cleanup;
        }
        if (backend.present(backend.context)!=ORES_DESKTOP_OK) {
            outcome=ORES_DESKTOP_BACKEND_ERROR; goto cleanup;
        }
        if (frame==UINT64_MAX) goto cleanup; /* never wrap a public frame index */
        ++frame;
        if (frame_limit && frame>=frame_limit) goto cleanup;
    }
cleanup:
    if (started && app.on_stop) app.on_stop(app.context);
    backend.close(backend.context);
    return outcome;
}
