/* Deterministic adversarial title-admission tests for the host C ABI.
 * Model: a modifier-only title is rejected before any callback; inserting a
 * visible ASCII anchor admits it; forbidden directional controls reject even
 * when the title contains a visible anchor. No allocation or external corpus.
 * This is host C validation only, not renderer-dependent Unicode proof. */
#include "ores_desktop.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr,"FAIL:%d: %s\n",__LINE__,#x); exit(1); \
} } while (0)

typedef struct {
    const unsigned char *original;
    size_t length;
    unsigned opens, starts, polls, frames, presents, stops, closes;
} counters;

static ores_desktop_status opened(void *p, const ores_desktop_config *cfg) {
    counters *c = (counters *)p;
    CHECK(cfg->title != (const char *)c->original);
    CHECK(cfg->title_length == c->length);
    CHECK(memcmp(cfg->title, c->original, c->length) == 0);
    CHECK(cfg->title[c->length] == '\0');
    ++c->opens;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status started(void *p) { ++((counters *)p)->starts; return ORES_DESKTOP_OK; }
static ores_desktop_status polled(void *p, ores_desktop_event *e, int *has) {
    (void)e; ++((counters *)p)->polls; *has=0; return ORES_DESKTOP_OK;
}
static ores_desktop_status framed(void *p, uint64_t frame) {
    CHECK(frame == 0); ++((counters *)p)->frames; return ORES_DESKTOP_OK;
}
static ores_desktop_status presented(void *p) { ++((counters *)p)->presents; return ORES_DESKTOP_OK; }
static void stopped(void *p) { ++((counters *)p)->stops; }
static void closed(void *p) { ++((counters *)p)->closes; }
static unsigned long total;
static void verify(const unsigned char *title, size_t length, int admitted) {
    counters c={0};
    ores_desktop_config cfg={ORES_DESKTOP_ABI_VERSION,640,480,1,1,(const char *)title,length};
    ores_desktop_app app={0};
    ores_desktop_backend backend={0};
    c.original=title; c.length=length;
    app.context=&c; app.on_start=started; app.on_frame=framed; app.on_stop=stopped;
    backend.context=&c; backend.open=opened; backend.poll=polled;
    backend.present=presented; backend.close=closed;
    CHECK(ores_desktop_run(&cfg,&app,&backend)==
          (admitted ? ORES_DESKTOP_OK : ORES_DESKTOP_INVALID_ARGUMENT));
    CHECK(c.opens==(unsigned)admitted);
    CHECK(c.starts==(unsigned)admitted);
    CHECK(c.polls==(unsigned)admitted);
    CHECK(c.frames==(unsigned)admitted);
    CHECK(c.presents==(unsigned)admitted);
    CHECK(c.stops==(unsigned)admitted);
    CHECK(c.closes==(unsigned)admitted);
    ++total;
}
static uint32_t random_state=0x91e10da5u;
static uint32_t next_random(void) {
    random_state ^= random_state<<13;
    random_state ^= random_state>>17;
    random_state ^= random_state<<5;
    return random_state;
}

typedef struct { const char *bytes; size_t length; } token;
static const token modifiers[]={
    {"\xe2\x80\x8c",3},        /* U+200C ZWNJ */
    {"\xe2\x80\x8d",3},        /* U+200D ZWJ */
    {"\xe3\x85\xa4",3},        /* U+3164 Hangul filler */
    {"\xef\xb8\x8f",3},        /* U+FE0F VS16 */
    {"\xe2\xa0\x80",3},        /* U+2800 blank braille */
    {"\xf3\xa0\x84\x80",4},    /* U+E0100 supplemental VS */
    {"\xf3\xa0\x81\xa1",4},    /* U+E0061 Unicode tag */
    {"\xe1\x9e\xb4",3},        /* U+17B4 Khmer invisible */
    {"\xe1\xa0\x8b",3}         /* U+180B Mongolian VS */
};
int main(void) {
    unsigned char bytes[128];
    unsigned iteration;
    for (iteration=0; iteration<4096; ++iteration) {
        size_t length=0;
        unsigned rounds=1u+(next_random()%35u), n;
        for (n=0;n<rounds;++n) {
            const token *t=&modifiers[next_random()%(sizeof(modifiers)/sizeof(modifiers[0]))];
            if (length+t->length>123u) break;
            memcpy(bytes+length,t->bytes,t->length);
            length+=t->length;
        }
        CHECK(length>0 && length<=123u);
        verify(bytes,length,0);      /* all modifiers: never admitted */
        memmove(bytes+1,bytes,length);
        bytes[0]='A';
        verify(bytes,length+1u,1);  /* visible anchor admits modifiers */
        memcpy(bytes+length+1u,"\xe2\x80\xae",3); /* U+202E override */
        verify(bytes,length+4u,0);  /* forbidden even alongside visible */
    }
    /* Explicit length, 127-byte non-NUL-terminated titles at the ABI edge. */
    memset(bytes,'A',sizeof bytes);
    verify(bytes,127u,1);
    verify(bytes,128u,0);
    /* Every possible unpaired UTF-8 continuation byte is invalid. */
    for (iteration=0x80u; iteration<=0xbfu; ++iteration) {
        bytes[0]='A'; bytes[1]=(unsigned char)iteration;
        verify(bytes,2u,0);
    }
    printf("deterministic title-fuzz host checks: %lu PASS\n",total);
    return 0;
}
