#include "ores_desktop.h"
#include <stddef.h>
uint32_t ores_desktop_abi_version(void) { return ORES_DESKTOP_ABI_VERSION; }
/* RFC 3629 strict Unicode scalar validation, shared by v1/v2 title inputs.
 * Bounded: never reads beyond the verified span and never allocates.
 * Rejects overlong sequences, surrogate code points and > U+10FFFF. */
static int title_utf8_valid(const char *bytes, size_t n) {
    size_t i=0;
    while (i<n) {
        unsigned char first=(unsigned char)bytes[i++];
        unsigned needed=0, j;
        unsigned char low=0x80u, high=0xbfu;
        if (first<=0x7fu) continue;
        if (first>=0xc2u && first<=0xdfu) needed=1;
        else if (first==0xe0u) { needed=2; low=0xa0u; }
        else if (first>=0xe1u && first<=0xecu) needed=2;
        else if (first==0xedu) { needed=2; high=0x9fu; }
        else if (first>=0xeeu && first<=0xefu) needed=2;
        else if (first==0xf0u) { needed=3; low=0x90u; }
        else if (first>=0xf1u && first<=0xf3u) needed=3;
        else if (first==0xf4u) { needed=3; high=0x8fu; }
        else return 0;
        if (n-i<needed) return 0;
        if ((unsigned char)bytes[i]<low ||
            (unsigned char)bytes[i]>high) return 0;
        ++i;
        for (j=1; j<needed; ++j) {
            unsigned char continuation=(unsigned char)bytes[i++];
            if (continuation<0x80u || continuation>0xbfu) return 0;
        }
    }
    return 1;
}
static int snapshot_title(const char *source, char destination[128]) {
    size_t i;
    if (!source) return 0;
    for (i=0; i<=127; ++i) {
        destination[i]=source[i];
        if (source[i]=='\0') return i>0 && title_utf8_valid(destination,i);
    }
    return 0;
}
static int valid_status(ores_desktop_status s) {
    return s==ORES_DESKTOP_OK || s==ORES_DESKTOP_STOP;
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
        !snapshot_title(cfg.title,title) ||
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
            if (e.type<ORES_DESKTOP_QUIT || e.type>ORES_DESKTOP_POINTER_UP) {
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

/* ABI v2 adapter: validate an exact byte span before calling the stable v1
 * lifecycle engine. Backend.open sees a v1 config, including a private title
 * copy, for continued compatibility with SDL3 and existing host callbacks.
 */
ores_desktop_status ores_desktop_run_v2(
    const ores_desktop_config_v2 *source,
    const ores_desktop_app *app,
    const ores_desktop_backend *backend) {
    ores_desktop_config_v2 cfg;
    ores_desktop_config legacy;
    char title[128];
    size_t length, i;
    if (!source || !app || !backend) return ORES_DESKTOP_INVALID_ARGUMENT;
    cfg=*source;
    if (cfg.abi_version!=ORES_DESKTOP_TITLE_ABI_V2 ||
        !cfg.title || cfg.title_length==0 || cfg.title_length>127)
        return ORES_DESKTOP_INVALID_ARGUMENT;
    length=(size_t)cfg.title_length;
    for (i=0; i<length; ++i) {
        char byte=cfg.title[i];
        if (byte=='\0') return ORES_DESKTOP_INVALID_ARGUMENT;
        title[i]=byte;
    }
    title[length]='\0';
    if (!title_utf8_valid(title,length)) return ORES_DESKTOP_INVALID_ARGUMENT;
    legacy.abi_version=ORES_DESKTOP_ABI_VERSION;
    legacy.width=cfg.width;
    legacy.height=cfg.height;
    legacy.max_events_per_frame=cfg.max_events_per_frame;
    legacy.frame_limit=cfg.frame_limit;
    legacy.title=title;
    return ores_desktop_run(&legacy,app,backend);
}
