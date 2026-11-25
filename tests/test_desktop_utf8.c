#include "ores_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Title strings cross SDL3/platform boundaries. These are host ABI tests,
 * not evidence of compiled Oreslang guest desktop support. */
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(1); \
} } while (0)

typedef struct {
    const char *original_title;
    unsigned opens, closes, polls, frames, presents, stops;
} state;

static ores_desktop_status opened(void *context, const ores_desktop_config *cfg) {
    state *s=(state *)context;
    CHECK(cfg->title!=s->original_title);
    CHECK(cfg->title_length==strlen(s->original_title));
    CHECK(memcmp(cfg->title,s->original_title,cfg->title_length)==0);
    CHECK(cfg->title[cfg->title_length]=='\0');
    ++s->opens;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status polled(void *context, ores_desktop_event *e, int *available) {
    state *s=(state *)context;
    (void)e;
    ++s->polls;
    *available=0;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status framed(void *context, uint64_t frame) {
    state *s=(state *)context;
    CHECK(frame==0);
    ++s->frames;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status presented(void *context) {
    ++((state *)context)->presents;
    return ORES_DESKTOP_OK;
}
static void stopped(void *context) { ++((state *)context)->stops; }
static void closed(void *context) { ++((state *)context)->closes; }

static void assert_title(const char *title, int valid) {
    state s={0};
    ores_desktop_config cfg={ORES_DESKTOP_ABI_VERSION,640,480,1,1,title,strlen(title)};
    ores_desktop_app app={0};
    ores_desktop_backend backend={0};
    s.original_title=title;
    app.context=&s; app.on_frame=framed; app.on_stop=stopped;
    backend.context=&s; backend.open=opened; backend.poll=polled;
    backend.present=presented; backend.close=closed;
    CHECK(ores_desktop_run(&cfg,&app,&backend)==
          (valid ? ORES_DESKTOP_OK : ORES_DESKTOP_INVALID_ARGUMENT));
    CHECK(s.opens==(unsigned)valid);
    CHECK(s.closes==(unsigned)valid);
    CHECK(s.polls==(unsigned)valid);
    CHECK(s.frames==(unsigned)valid);
    CHECK(s.presents==(unsigned)valid);
    CHECK(s.stops==(unsigned)valid);
}

int main(void) {
    char max_ascii[128], max_utf8[128], split_utf8[128], too_long[129];
    const char *bad[] = {
        "\x80",                       /* lone continuation */
        "\xc1\xbf",                   /* overlong two-byte edge */
        "\xe0\x9f\xbf",               /* three-byte lower boundary */
        "\xed\xa0\xbf",               /* upper surrogate */
        "\xf0\x80\x80\x80",           /* four-byte lower boundary */
        "\xf4\x8f\xbf\xc0",           /* invalid fourth continuation */
        "\xe2\x82\xff",               /* invalid third continuation */
        "\xf0\x90\x80",               /* incomplete four-byte scalar */
        "\xc0\xaf",                 /* overlong two-byte sequence */
        "\xe0\x80\x80",         /* overlong three-byte sequence */
        "\xed\xa0\x80",         /* UTF-16 surrogate */
        "\xf0\x8f\xbf\xbf", /* overlong four-byte sequence */
        "\xf4\x90\x80\x80", /* beyond Unicode maximum */
        "\xf5\x80\x80\x80", /* invalid leading byte */
        "\xe2\x82",                 /* truncated three-byte sequence */
        "\xe2\x28\xa1",         /* invalid continuation */
        "\xff"
    };
    size_t i;
    assert_title("ascii",1);
    assert_title("caf\xc3\xa9",1);
    assert_title("\xe2\x82\xac",1);
    assert_title("\xf0\x9f\x8c\x8d",1);
    /* Scalar edges: valid encodings still obey the title admission policy. */
    assert_title("\x7f",0);
    assert_title("\xc2\x80",0);
    assert_title("\xdf\xbf",1);
    assert_title("\xe0\xa0\x80",1);
    assert_title("\xed\x9f\xbf",1);
    assert_title("\xf0\x90\x80\x80",1);
    assert_title("\xf4\x8f\xbf\xbf",0);
    assert_title("\xf4\x8f\xbf\xbd",1);
    assert_title("",0);
    memset(max_ascii,'A',127); max_ascii[127]='\0';
    assert_title(max_ascii,1);
    memset(max_utf8,'A',125);
    max_utf8[125]=(char)0xc3; max_utf8[126]=(char)0xa9; max_utf8[127]='\0';
    assert_title(max_utf8,1);
    memset(split_utf8,'A',126);
    split_utf8[126]=(char)0xc3; split_utf8[127]='\0';
    assert_title(split_utf8,0);
    memset(too_long,'A',128); too_long[128]='\0';
    assert_title(too_long,0);
    for (i=0;i<sizeof bad/sizeof bad[0];++i) assert_title(bad[i],0);
    puts("desktop title UTF-8 scalar contract: PASS");
    return 0;
}
