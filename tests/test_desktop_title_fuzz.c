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
    {"\xe1\xa0\x8b",3},        /* U+180B Mongolian VS */
    {"\xe2\x80\x80",3},        /* U+2000 en quad */
    {"\xe2\x80\x83",3},        /* U+2003 em space */
    {"\xe2\x80\x8a",3},        /* U+200A hair space */
    {"\xe2\x80\xaf",3},        /* U+202F narrow no-break space */
    {"\xe2\x81\x9f",3},        /* U+205F medium mathematical space */
    {"\xe3\x80\x80",3},        /* U+3000 ideographic space */
    {"\xe1\x9e\xb5",3},        /* U+17B5 Khmer invisible vowel */
    {"\xe1\xa0\x8f",3},        /* U+180F Mongolian free VS */
    {"\xef\xbe\xa0",3},        /* U+FFA0 halfwidth Hangul filler */
    {"\xef\xb8\x80",3},        /* U+FE00 VS1 */
    {"\xef\xbf\xb9",3},        /* U+FFF9 interlinear annotation anchor */
    {"\xef\xbf\xbb",3},        /* U+FFFB interlinear annotation terminator */
    {"\xf0\x9b\xb2\xa0",4},    /* U+1BCA0 shorthand format */
    {"\xf0\x9b\xb2\xa3",4},    /* U+1BCA3 shorthand format */
    {"\xf0\x9d\x85\xb3",4},    /* U+1D173 musical control */
    {"\xf3\xa0\x82\x80",4},    /* U+E0080 tag-plane format */
    {"\xf3\xa0\xbf\xbf",4},    /* U+E0FFF tag-plane format */
    {"\xf3\xa0\x87\xaf",4}     /* U+E01EF supplemental VS */
};

/* Test every policy token independently, not only random mixtures: changes
 * near Unicode range boundaries must fail visibly and before callbacks. */
static void verify_every_modifier(void) {
    unsigned char bytes[128];
    size_t i;
    for (i=0; i<sizeof(modifiers)/sizeof(modifiers[0]); ++i) {
        const token *t=&modifiers[i];
        CHECK(t->length>=1u && t->length<=4u);
        memcpy(bytes,t->bytes,t->length);
        verify(bytes,t->length,0);
        memmove(bytes+1,bytes,t->length);
        bytes[0]='A';
        verify(bytes,t->length+1u,1);
        memcpy(bytes+t->length+1u,"\xe2\x80\xae",3); /* U+202E RLO */
        verify(bytes,t->length+4u,0);
    }
}
/* Forbidden formatting controls must reject even when a real anchor exists.
 * Cover both ends and every scalar in U+2028..202E / U+2060..206F. */
static void verify_forbidden_controls(void) {
    static const token forbidden[]={
        {"\xc2\xad",2},       /* U+00AD soft hyphen */
        {"\xcd\x8f",2},       /* U+034F combining grapheme joiner */
        {"\xd8\x9c",2},       /* U+061C Arabic letter mark */
        {"\xe1\xa0\x8e",3},   /* U+180E Mongolian vowel separator */
        {"\xe2\x80\x8b",3},   /* U+200B zero width space */
        {"\xe2\x80\x8e",3},   /* U+200E left-to-right mark */
        {"\xe2\x80\x8f",3},   /* U+200F right-to-left mark */
        {"\xef\xbb\xbf",3}    /* U+FEFF BOM */
    };
    unsigned char bytes[8];
    size_t i;
    unsigned n;
    for (i=0; i<sizeof(forbidden)/sizeof(forbidden[0]); ++i) {
        memcpy(bytes,forbidden[i].bytes,forbidden[i].length);
        verify(bytes,forbidden[i].length,0);
        bytes[forbidden[i].length]='A';
        verify(bytes,forbidden[i].length+1u,0);
        memmove(bytes+1,bytes,forbidden[i].length);
        bytes[0]='A';
        verify(bytes,forbidden[i].length+1u,0);
    }
    for (n=0; n<7u; ++n) {
        bytes[0]=0xe2; bytes[1]=0x80; bytes[2]=(unsigned char)(0xa8u+n);
        verify(bytes,3u,0);
        bytes[3]='A';
        verify(bytes,4u,0);
    }
    for (n=0; n<16u; ++n) {
        bytes[0]=0xe2; bytes[1]=0x81; bytes[2]=(unsigned char)(0xa0u+n);
        verify(bytes,3u,0);
        bytes[3]='A';
        verify(bytes,4u,0);
    }
    /* UTF-8 encoded C1 controls remain forbidden, with or without anchors. */
    for (n=0x80u; n<=0x9fu; ++n) {
        bytes[0]=0xc2; bytes[1]=(unsigned char)n;
        verify(bytes,2u,0);
        bytes[2]='A';
        verify(bytes,3u,0);
    }
}
/* Canonical malformed UTF-8 classes: overlong, surrogate, > U+10FFFF,
 * invalid lead/continuation, truncation and embedded NUL, even with text. */
static void verify_invalid_utf8(void) {
    static const token invalid[]={
        {"\x80",1}, {"\xbf",1}, {"\xff",1}, {"\xfe",1},
        {"\xc0\xaf",2}, {"\xc1\xbf",2}, {"\xe0\x80\x80",3},
        {"\xf0\x80\x80\x80",4}, {"\xed\xa0\x80",3},
        {"\xed\xbf\xbf",3}, {"\xf4\x90\x80\x80",4},
        {"\xf5\x80\x80\x80",4}, {"\xe2\x28\xa1",3},
        {"\xe2\x82",2}, {"\xf0\x9f\x92",3},
        {"\xc2\x41",2}, {"\xe2\x82\x41",3},
        {"\xf8\x88\x80\x80\x80",5},
        {"A\x00" "B",3}
    };
    unsigned char bytes[128];
    size_t i;
    for (i=0; i<sizeof(invalid)/sizeof(invalid[0]); ++i) {
        const token *t=&invalid[i];
        CHECK(t->length<=5u);
        memcpy(bytes,t->bytes,t->length);
        verify(bytes,t->length,0);
        memmove(bytes+1,bytes,t->length);
        bytes[0]='A';
        verify(bytes,t->length+1u,0);
        memcpy(bytes,t->bytes,t->length);
        bytes[t->length]='A';
        verify(bytes,t->length+1u,0);
    }
}
static void verify_ascii_and_length_boundaries(void) {
    unsigned char bytes[128];
    unsigned n;
    /* Reject ASCII C0 and DEL with visible anchors, before ANY callback. */
    for (n=0; n<32u; ++n) {
        bytes[0]=(unsigned char)n;
        verify(bytes,1u,0);
        bytes[1]='A';
        verify(bytes,2u,0);
    }
    bytes[0]=0x7f;
    verify(bytes,1u,0);
    bytes[1]='A';
    verify(bytes,2u,0);
    /* Preserve the pre-existing ASCII space and NBSP compatibility rules. */
    bytes[0]=' ';
    verify(bytes,1u,1);
    bytes[0]=0xc2; bytes[1]=0xa0;
    verify(bytes,2u,1);
    /* Exact upper boundary, including an invisible suffix. */
    memset(bytes,'A',sizeof bytes);
    verify(bytes,126u,1);
    verify(bytes,127u,1);
    verify(bytes,128u,0);
    memset(bytes,'A',125u);
    bytes[125]=0xc2; bytes[126]=0xa0; /* anchored NBSP at 127 bytes */
    verify(bytes,127u,1);
    memset(bytes,'A',124u);
    memcpy(bytes+124u,"\xe2\x80\x8c",3); /* anchored ZWNJ */
    verify(bytes,127u,1);
    bytes[0]=' ';
    memcpy(bytes+1u,"\xe2\x80\x8c",3); /* space is an anchor */
    verify(bytes,4u,1);
}


/* Exercise both sides of every exceptional UTF-8 second-byte boundary:
 * E0 rejects overlong 3-byte encodings, ED rejects surrogates, F0 rejects
 * overlong 4-byte encodings, and F4 rejects codepoints above U+10FFFF.
 * A visible ASCII prefix must neither rescue invalid UTF-8 nor prevent
 * otherwise valid international scalar titles. */
static void verify_utf8_second_byte_edges(void) {
    static const struct { unsigned char first, low, high; size_t length; } ranges[] = {
        {0xe0u, 0xa0u, 0xbfu, 3u},
        {0xedu, 0x80u, 0x9fu, 3u},
        {0xf0u, 0x90u, 0xbfu, 4u},
        {0xf4u, 0x80u, 0x8fu, 4u}
    };
    unsigned char bytes[5];
    size_t i;
    unsigned second;
    for (i=0; i<sizeof(ranges)/sizeof(ranges[0]); ++i) {
        for (second=0x80u; second<=0xbfu; ++second) {
            int valid = second>=ranges[i].low && second<=ranges[i].high;
            bytes[0]='A';
            bytes[1]=ranges[i].first;
            bytes[2]=(unsigned char)second;
            bytes[3]=0x80u;
            bytes[4]=0x80u;
            verify(bytes+1,ranges[i].length,valid);
            verify(bytes,ranges[i].length+1u,valid);
        }
    }
}

/* The host must snapshot borrowed title bytes before backend.open. The
 * callback is allowed to modify the original writable caller buffer, but
 * the validated, NUL-terminated title handed to it must not change. */
static ores_desktop_status opened_poison_borrowed_title(void *p,
                                                       const ores_desktop_config *cfg) {
    counters *c=(counters *)p;
    unsigned char *writable=(unsigned char *)(void *)c->original;
    CHECK(cfg->title_length==4u);
    CHECK(cfg->title!=(const char *)writable);
    CHECK(memcmp(cfg->title,"Atto",4u)==0);
    CHECK(cfg->title[4]=='\0');
    ++c->opens;
    writable[0]=0u;
    writable[1]=0xffu;
    writable[2]='!';
    writable[3]='!';
    CHECK(memcmp(cfg->title,"Atto",5u)==0);
    return ORES_DESKTOP_OK;
}
static void verify_borrowed_title_snapshot(void) {
    unsigned char borrowed[]={'A','t','t','o'};
    counters c={0};
    ores_desktop_config cfg={ORES_DESKTOP_ABI_VERSION,640,480,1,1,
                              (const char *)borrowed,sizeof borrowed};
    ores_desktop_app app={0};
    ores_desktop_backend backend={0};
    c.original=borrowed;
    c.length=sizeof borrowed;
    app.context=&c;
    app.on_start=started;
    app.on_frame=framed;
    app.on_stop=stopped;
    backend.context=&c;
    backend.open=opened_poison_borrowed_title;
    backend.poll=polled;
    backend.present=presented;
    backend.close=closed;
    CHECK(ores_desktop_run(&cfg,&app,&backend)==ORES_DESKTOP_OK);
    CHECK(borrowed[0]==0u && borrowed[1]==0xffu);
    CHECK(c.opens==1u && c.starts==1u && c.polls==1u && c.frames==1u);
    CHECK(c.presents==1u && c.stops==1u && c.closes==1u);
    ++total;
}


/* Unicode's designated noncharacter scalar values are valid UTF-8 encodings,
 * but are not a visible window-title anchor. This is separate from rejecting
 * malformed UTF-8, whose byte sequences cannot denote Unicode scalars. */
static size_t encode_scalar(uint32_t value, unsigned char bytes[4]) {
    if (value<=0x7fu) {
        bytes[0]=(unsigned char)value;
        return 1u;
    }
    if (value<=0x7ffu) {
        bytes[0]=(unsigned char)(0xc0u | (value>>6));
        bytes[1]=(unsigned char)(0x80u | (value & 0x3fu));
        return 2u;
    }
    if (value<=0xffffu) {
        bytes[0]=(unsigned char)(0xe0u | (value>>12));
        bytes[1]=(unsigned char)(0x80u | ((value>>6) & 0x3fu));
        bytes[2]=(unsigned char)(0x80u | (value & 0x3fu));
        return 3u;
    }
    CHECK(value<=0x10ffffu);
    bytes[0]=(unsigned char)(0xf0u | (value>>18));
    bytes[1]=(unsigned char)(0x80u | ((value>>12) & 0x3fu));
    bytes[2]=(unsigned char)(0x80u | ((value>>6) & 0x3fu));
    bytes[3]=(unsigned char)(0x80u | (value & 0x3fu));
    return 4u;
}
static void verify_noncharacter_scalar(uint32_t scalar) {
    unsigned char bytes[6];
    size_t length=encode_scalar(scalar,bytes);
    verify(bytes,length,0);
    memmove(bytes+1u,bytes,length);
    bytes[0]='A';
    verify(bytes,length+1u,0);
    memmove(bytes,bytes+1u,length);
    bytes[length]='A';
    verify(bytes,length+1u,0);
}
static void verify_unicode_noncharacter_and_ogham_policy(void) {
    unsigned char bytes[128];
    unsigned plane;
    uint32_t scalar;
    size_t length;
    /* All 32 noncharacters in the permanently reserved BMP range. */
    for (scalar=0xfdd0u; scalar<=0xfdefu; ++scalar)
        verify_noncharacter_scalar(scalar);
    /* Every plane's FFFE/FFFF, including the valid UTF-8 max U+10FFFF. */
    for (plane=0u; plane<=16u; ++plane) {
        verify_noncharacter_scalar(((uint32_t)plane<<16)|0xfffeu);
        verify_noncharacter_scalar(((uint32_t)plane<<16)|0xffffu);
        /* U+xxFFFD immediately below the noncharacter pair stays admitted. */
        scalar=((uint32_t)plane<<16)|0xfffdu;
        length=encode_scalar(scalar,bytes);
        verify(bytes,length,1);
    }
    /* Noncharacters must not widen the rejected range into neighbors. */
    length=encode_scalar(0xfdcfu,bytes);
    verify(bytes,length,1);
    length=encode_scalar(0xfdf0u,bytes);
    verify(bytes,length,1);
    length=encode_scalar(0x1681u,bytes); /* Ogham letter, not Ogham space */
    verify(bytes,length,1);

    /* U+1680 Ogham SPACE MARK must not itself be the only anchor. */
    length=encode_scalar(0x1680u,bytes);
    CHECK(length==3u);
    verify(bytes,length,0);
    memmove(bytes+1u,bytes,length);
    bytes[0]='A';
    verify(bytes,length+1u,1);
    bytes[0]=(unsigned char)0xe1u;
    bytes[1]=(unsigned char)0x9au;
    bytes[2]=(unsigned char)0x80u;
    bytes[3]='A';
    verify(bytes,4u,1);
    /* 42 complete Ogham spaces occupy 126 bytes, no visible content. */
    for (plane=0u; plane<42u; ++plane) {
        bytes[plane*3u]=0xe1u;
        bytes[plane*3u+1u]=0x9au;
        bytes[plane*3u+2u]=0x80u;
    }
    verify(bytes,126u,0);
    bytes[126u]='A';
    verify(bytes,127u,1);
}

int main(void) {
    unsigned char bytes[128];
    verify_every_modifier();
    verify_forbidden_controls();
    verify_invalid_utf8();
    verify_ascii_and_length_boundaries();
    verify_utf8_second_byte_edges();
    verify_borrowed_title_snapshot();
    verify_unicode_noncharacter_and_ogham_policy();
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
