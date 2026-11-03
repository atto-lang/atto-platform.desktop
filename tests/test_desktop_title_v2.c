#include "ores_desktop.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(1); \
} } while(0)

/* Host-only exact-title admission. Backend still sees ABI v1, while callers
 * can provide nonterminated v2 UTF-8 byte spans safely. */
typedef struct {
    const char *original_title;
    const char *expected_bytes;
    size_t expected_length;
    unsigned opens, polls, frames, presents, closes, stops;
} fixture;
static ores_desktop_status opened(void *p, const ores_desktop_config *cfg) {
    fixture *f=(fixture *)p;
    CHECK(cfg->abi_version==ORES_DESKTOP_ABI_VERSION);
    CHECK(cfg->title!=f->original_title);
    CHECK(memcmp(cfg->title,f->expected_bytes,f->expected_length)==0);
    CHECK(cfg->title[f->expected_length]=='\0');
    ++f->opens;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status polled(void *p, ores_desktop_event *event, int *available) {
    fixture *f=(fixture *)p;
    (void)event;
    ++f->polls;
    *available=0;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status framed(void *p, uint64_t frame) {
    fixture *f=(fixture *)p;
    CHECK(frame==0);
    ++f->frames;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status presented(void *p) {
    ++((fixture *)p)->presents;
    return ORES_DESKTOP_OK;
}
static void stopped(void *p) { ++((fixture *)p)->stops; }
static void closed(void *p) { ++((fixture *)p)->closes; }

static void check_v2(const char *bytes, uint32_t length, int accepted) {
    fixture f={0};
    ores_desktop_config_v2 cfg={ORES_DESKTOP_TITLE_ABI_V2,640,480,1,1,bytes,length};
    ores_desktop_app app={0};
    ores_desktop_backend backend={0};
    f.original_title=bytes; f.expected_bytes=bytes; f.expected_length=length;
    app.context=&f; app.on_frame=framed; app.on_stop=stopped;
    backend.context=&f; backend.open=opened; backend.poll=polled;
    backend.present=presented; backend.close=closed;
    CHECK(ores_desktop_run_v2(&cfg,&app,&backend)==
          (accepted ? ORES_DESKTOP_OK : ORES_DESKTOP_INVALID_ARGUMENT));
    CHECK(f.opens==(unsigned)accepted);
    CHECK(f.polls==(unsigned)accepted);
    CHECK(f.frames==(unsigned)accepted);
    CHECK(f.presents==(unsigned)accepted);
    CHECK(f.stops==(unsigned)accepted);
    CHECK(f.closes==(unsigned)accepted);
}
static void check_legacy_utf8(void) {
    fixture f={0};
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,640,480,1,1,"\xc0\xaf"};
    ores_desktop_app app={0};
    ores_desktop_backend backend={0};
    app.context=&f; app.on_frame=framed;
    backend.context=&f; backend.open=opened; backend.poll=polled;
    backend.present=presented; backend.close=closed;
    CHECK(ores_desktop_abi_version()==1u);
    CHECK(ores_desktop_run(&c,&app,&backend)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(f.opens==0 && f.closes==0 && f.frames==0);
}
static void check_config_fail_closed(void) {
    fixture f={0};
    ores_desktop_config_v2 c={ORES_DESKTOP_TITLE_ABI_V2,640,480,1,1,"ok",2};
    ores_desktop_app app={0};
    ores_desktop_backend backend={0};
    app.context=&f; app.on_frame=framed;
    backend.context=&f; backend.open=opened; backend.poll=polled;
    backend.present=presented; backend.close=closed;
    CHECK(ores_desktop_run_v2(NULL,&app,&backend)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(ores_desktop_run_v2(&c,NULL,&backend)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(ores_desktop_run_v2(&c,&app,NULL)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.abi_version=1u;
    CHECK(ores_desktop_run_v2(&c,&app,&backend)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.abi_version=ORES_DESKTOP_TITLE_ABI_V2; c.width=0;
    CHECK(ores_desktop_run_v2(&c,&app,&backend)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.width=640; c.max_events_per_frame=0;
    CHECK(ores_desktop_run_v2(&c,&app,&backend)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(f.opens==0 && f.closes==0 && f.frames==0);
}

int main(void) {
    char exact[127], too_long[128], embedded[3]={'A','\0','B'};
    const char *invalid[] = {
        "\x80", "\xc0\xaf", "\xc1\xbf",
        "\xe0\x80\x80", "\xed\xa0\x80",
        "\xf0\x8f\xbf\xbf", "\xf4\x90\x80\x80",
        "\xf5\x80\x80\x80", "\xe2\x82", "\xe2\x28\xa1",
        "\xf0\x90\x80", "\xf4\x8f\xbf\xc0",
        "\xff", "\xe0\x9f\xbf"
    };
    const char *valid[] = {
        "A", "caf\xc3\xa9", "\xe2\x82\xac",
        "\xf0\x9f\x8c\x8d", "\x7f", "\xc2\x80",
        "\xdf\xbf", "\xe0\xa0\x80", "\xed\x9f\xbf",
        "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf"
    };
    size_t i;
    check_legacy_utf8();
    check_config_fail_closed();
    check_v2(NULL,1u,0);
    check_v2("",0u,0);
    check_v2(embedded,3u,0);
    memset(exact,'Z',sizeof exact);
    check_v2(exact,(uint32_t)sizeof exact,1); /* no source terminator */
    memset(too_long,'X',sizeof too_long);
    check_v2(too_long,(uint32_t)sizeof too_long,0);
    for (i=0; i<sizeof valid/sizeof valid[0]; ++i)
        check_v2(valid[i],(uint32_t)strlen(valid[i]),1);
    for (i=0; i<sizeof invalid/sizeof invalid[0]; ++i)
        check_v2(invalid[i],(uint32_t)strlen(invalid[i]),0);
    puts("desktop ABI v1/v2 UTF-8 + exact-span admission: PASS (host-only)");
    return 0;
}
