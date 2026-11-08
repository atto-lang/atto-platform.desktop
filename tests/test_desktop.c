#include "ores_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(cond) do { if (!(cond)) { \
  fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond); exit(1); \
} } while (0)

typedef struct {
    int scenario, opens, closes, starts, stops, polls, events, frames, presents, alternate_presents;
    ores_desktop_config *live_config;
    ores_desktop_app *live_app;
    ores_desktop_backend *live_backend;
} fixture;
static ores_desktop_status opened(void *p,const ores_desktop_config *c) {
    fixture *f=(fixture*)p;
    CHECK(c->abi_version==ORES_DESKTOP_ABI_VERSION);
    CHECK(f->live_config != NULL);
    CHECK(c->title != f->live_config->title);
    CHECK(c->title_length==f->live_config->title_length);
    CHECK(memcmp(c->title,f->live_config->title,c->title_length)==0);
    CHECK(c->title[c->title_length]=='\0');
    ++f->opens;
    return f->scenario==8 ? ORES_DESKTOP_BACKEND_ERROR : ORES_DESKTOP_OK;
}
static ores_desktop_status polled(void *p,ores_desktop_event *e,int *available) {
    fixture *f=(fixture*)p;
    ++f->polls;
    *available=0;
    if (f->scenario==3) { *available=2; return ORES_DESKTOP_OK; }
    if (f->scenario==4) return ORES_DESKTOP_BACKEND_ERROR;
    if (f->scenario==1 || f->scenario==2 ||
        f->scenario==5 || f->scenario==11 || f->scenario==13) {
        *available=1;
        e->type=(f->scenario==1 || f->scenario==13) ? ORES_DESKTOP_QUIT :
            f->scenario==5 ? 1000 : ORES_DESKTOP_KEY_DOWN;
    }
    return ORES_DESKTOP_OK;
}
static ores_desktop_status presented(void *p) {
    fixture *f=(fixture*)p;
    ++f->presents;
    return f->scenario==9 ? ORES_DESKTOP_BACKEND_ERROR : ORES_DESKTOP_OK;
}
static ores_desktop_status alternate_presented(void *p) {
    ++((fixture*)p)->alternate_presents;
    return ORES_DESKTOP_OK;
}
static void closed(void *p) { ++((fixture*)p)->closes; }
static ores_desktop_status started(void *p) {
    fixture *f=(fixture*)p;
    ++f->starts;
    if (f->scenario==12) {
        f->live_config->frame_limit=1;
        f->live_app->on_frame=NULL;
        f->live_backend->present=alternate_presented;
    }
    return f->scenario==7 ? ORES_DESKTOP_APP_ERROR :
           f->scenario==10 ? ORES_DESKTOP_STOP : ORES_DESKTOP_OK;
}
static ores_desktop_status event_cb(void *p,const ores_desktop_event *e) {
    fixture *f=(fixture*)p;
    CHECK(e->type>=ORES_DESKTOP_QUIT && e->type<=ORES_DESKTOP_POINTER_UP);
    ++f->events;
    if (f->scenario==13) {
        /* Adversarial callback must not suppress an already-polled quit. */
        ((ores_desktop_event *)(void *)e)->type=ORES_DESKTOP_KEY_DOWN;
    }
    return f->scenario==11 ? ORES_DESKTOP_STOP : ORES_DESKTOP_OK;
}
static ores_desktop_status frame_cb(void *p,uint64_t frame) {
    fixture *f=(fixture*)p;
    CHECK(frame==(uint64_t)f->frames);
    ++f->frames;
    return f->scenario==6 ? ORES_DESKTOP_APP_ERROR : ORES_DESKTOP_OK;
}
static void stopped(void *p) { ++((fixture*)p)->stops; }
static void run_scenario(int scenario,ores_desktop_status expected_status,int frames,int events,
                         int presented_frames,int stopped_calls,int closed_calls) {
    fixture f={0};
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,800,600,2,3,"Desktop test",12u};
    ores_desktop_app a={0};
    ores_desktop_backend b={0};
    f.scenario=scenario;
    f.live_config=&c; f.live_app=&a; f.live_backend=&b;
    a.context=&f; a.on_start=started; a.on_event=event_cb;
    a.on_frame=frame_cb; a.on_stop=stopped;
    b.context=&f; b.open=opened; b.poll=polled;
    b.present=presented; b.close=closed;
    CHECK(ores_desktop_run(&c,&a,&b)==expected_status);
    CHECK(f.opens==1);
    CHECK(f.starts==(scenario==8 ? 0 : 1));
    CHECK(f.frames==frames);
    CHECK(f.events==events);
    CHECK(f.presents==presented_frames);
    CHECK(f.stops==stopped_calls);
    CHECK(f.closes==closed_calls);
    CHECK(f.alternate_presents==0);
}
static void bad_arguments(void) {
    fixture f={0};
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,800,600,2,3,"Valid",5u};
    ores_desktop_app a={0};
    ores_desktop_backend b={0};
    char too_long[129];
    a.context=&f; a.on_frame=frame_cb;
    b.context=&f; b.open=opened; b.poll=polled;
    b.present=presented; b.close=closed;
    CHECK(ores_desktop_run(NULL,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(ores_desktop_run(&c,NULL,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(ores_desktop_run(&c,&a,NULL)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.abi_version=ORES_DESKTOP_ABI_VERSION+1u;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.abi_version=ORES_DESKTOP_ABI_VERSION;
    c.width=0;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.width=800; c.height=16385;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.height=600; c.max_events_per_frame=0;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.max_events_per_frame=257;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.max_events_per_frame=2; c.title=""; c.title_length=0;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    memset(too_long,'X',128); too_long[128]='\0'; c.title=too_long; c.title_length=128;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.title="A\0B"; c.title_length=3;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.title="A\nB"; c.title_length=3;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    {
        const char overlong[]={(char)0xc0,(char)0x80};
        const char partial[]={(char)0xe2,(char)0x82};
        const char surrogate[]={(char)0xed,(char)0xa0,(char)0x80};
        const char too_high[]={(char)0xf4,(char)0x90,(char)0x80,(char)0x80};
        const char stray[]={(char)0x80};
        const char c1_control[]={(char)0xc2,(char)0x85};
        const char *invalid[]={overlong,partial,surrogate,too_high,stray,c1_control};
        const size_t sizes[]={2u,2u,3u,4u,1u,2u};
        size_t i;
        for (i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
            c.title=invalid[i]; c.title_length=sizes[i];
            CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
        }
    }
    CHECK(f.opens==0);
    /* Valid non-ASCII UTF-8 must remain supported by the real host ABI. */
    c.title="\xc3\xa9"; c.title_length=2; c.frame_limit=1;
    f.live_config=&c;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_OK);
    CHECK(f.opens==1 && f.closes==1);
}

/* Byte-length-exact title admission matrix. Each fixture uses a fresh host
 * lifecycle and proves rejection occurs before any callback is invoked.
 * In particular the 127-byte array is NOT NUL-terminated by the caller. */
static void check_title_bytes(const char *bytes, size_t length, int accepted) {
    fixture f={0};
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,800,600,1,1,bytes,length};
    ores_desktop_app a={0};
    ores_desktop_backend b={0};
    f.live_config=&c;
    a.context=&f; a.on_frame=frame_cb; a.on_stop=stopped;
    b.context=&f; b.open=opened; b.poll=polled;
    b.present=presented; b.close=closed;
    CHECK(ores_desktop_run(&c,&a,&b)==
          (accepted ? ORES_DESKTOP_OK : ORES_DESKTOP_INVALID_ARGUMENT));
    CHECK(f.opens==accepted);
    CHECK(f.closes==accepted);
    CHECK(f.polls==accepted);
    CHECK(f.frames==accepted);
    CHECK(f.presents==accepted);
    CHECK(f.stops==accepted);
}
static void title_boundary_matrix(void) {
    unsigned char raw[127], ctrl[2];
    unsigned n;
    memset(raw,'A',sizeof raw);
    check_title_bytes((const char *)raw,sizeof raw,1);
    /* Positive exact-boundary sequences, ending on byte 127 without NUL. */
    memset(raw,'A',sizeof raw);
    raw[125]=0xc3; raw[126]=0xa9;
    check_title_bytes((const char *)raw,sizeof raw,1);
    memset(raw,'A',sizeof raw);
    raw[124]=0xe2; raw[125]=0x82; raw[126]=0xac;
    check_title_bytes((const char *)raw,sizeof raw,1);
    memset(raw,'A',sizeof raw);
    raw[123]=0xf0; raw[124]=0x9f; raw[125]=0x8c; raw[126]=0x8d;
    check_title_bytes((const char *)raw,sizeof raw,1);
    /* A truncated scalar at the final byte must not be completed from
     * adjacent memory; the explicit length is the only admission boundary. */
    memset(raw,'A',sizeof raw);
    raw[126]=0xe2;
    check_title_bytes((const char *)raw,sizeof raw,0);
    memset(raw,'A',sizeof raw);
    raw[125]=0xf0; raw[126]=0x9f;
    check_title_bytes((const char *)raw,sizeof raw,0);
    /* Reject every ASCII control and DEL, even at the beginning. */
    for (n=0;n<0x20;n++) {
        ctrl[0]=(unsigned char)n;
        check_title_bytes((const char *)ctrl,1,0);
    }
    ctrl[0]=0x7f;
    check_title_bytes((const char *)ctrl,1,0);
    /* U+0080..U+009F are valid UTF-8 scalar encodings but forbidden UI
     * C1 controls; U+00A0 immediately above the boundary remains valid. */
    for (n=0x80;n<=0x9f;n++) {
        ctrl[0]=0xc2; ctrl[1]=(unsigned char)n;
        check_title_bytes((const char *)ctrl,2,0);
    }
    ctrl[0]=0xc2; ctrl[1]=0xa0;
    check_title_bytes((const char *)ctrl,2,1);
    /* Visually invisible or bidirectional format scalars must never reach
     * the backend window-open callback, even in valid UTF-8. */
    {
        static const struct {
            const char *bytes;
            size_t length;
        } forbidden[] = {
            {"\xc2\xad",2},       /* U+00AD soft hyphen */
            {"\xcd\x8f",2},       /* U+034F grapheme joiner */
            {"\xd8\x9c",2},       /* U+061C Arabic letter mark */
            {"\xe1\xa0\x8e",3},   /* U+180E invisible separator */
            {"\xe2\x80\x8b",3},   /* U+200B zero-width space */
            {"\xe2\x80\x8e",3},   /* U+200E left-to-right mark */
            {"\xe2\x80\x8f",3},   /* U+200F right-to-left mark */
            {"\xe2\x80\xa8",3},   /* U+2028 line separator */
            {"\xe2\x80\xa9",3},   /* U+2029 paragraph separator */
            {"\xe2\x80\xaa",3},   /* U+202A left-to-right embedding */
            {"\xe2\x80\xae",3},   /* U+202E right-to-left override */
            {"\xe2\x81\xa0",3},   /* U+2060 word joiner */
            {"\xe2\x81\xa6",3},   /* U+2066 left-to-right isolate */
            {"\xe2\x81\xa9",3},   /* U+2069 pop directional isolate */
            {"\xe2\x81\xaf",3},   /* U+206F deprecated format */
            {"\xef\xbb\xbf",3}    /* U+FEFF BOM */
        };
        for (n=0;n<sizeof forbidden/sizeof forbidden[0];n++) {
            check_title_bytes(forbidden[n].bytes,forbidden[n].length,0);
        }
    }
    /* Keep ordinary international scripts and emoji ZWJ sequences valid. */
    check_title_bytes("\xd7\xa9",2,1); /* Hebrew letter */
    check_title_bytes("\xe2\x9c\x93",3,1); /* Unicode check mark */
    check_title_bytes("\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb",11,1);
    memset(raw,'A',sizeof raw);
    raw[124]=0xe2; raw[125]=0x80; raw[126]=0xae;
    check_title_bytes((const char *)raw,sizeof raw,0);
    {
        const unsigned char max_noncharacter[]={0xf4,0x8f,0xbf,0xbf};
        const unsigned char max_neighbor[]={0xf4,0x8f,0xbf,0xbd}; /* U+10FFFD, valid */
        const unsigned char beyond_max[]={0xf4,0x90,0x80,0x80};
        const unsigned char surrogate[]={0xed,0xbf,0xbf};
        check_title_bytes((const char *)max_noncharacter,sizeof max_noncharacter,0);
        check_title_bytes((const char *)max_neighbor,sizeof max_neighbor,1);
        check_title_bytes((const char *)beyond_max,sizeof beyond_max,0);
        check_title_bytes((const char *)surrogate,sizeof surrogate,0);
    }
}

/* Issue #33: reject modifier/filler-only valid UTF-8 before ANY callback,
 * without banning joiners, variation selectors, or international text when
 * accompanied by a visible anchor. This intentionally does not change the
 * previously admitted standalone U+00A0 compatibility case. */
static void title_invisible_only_matrix(void) {
    static const struct {
        const char *bytes;
        size_t length;
    } invisible[] = {
        {"\xe2\x80\x8c",3},       /* U+200C ZWNJ */
        {"\xe2\x80\x8d",3},       /* U+200D ZWJ */
        {"\xe3\x85\xa4",3},       /* U+3164 Hangul filler */
        {"\xe1\x85\x9f",3},       /* U+115F Hangul choseong filler */
        {"\xe1\x85\xa0",3},       /* U+1160 Hangul jungseong filler */
        {"\xef\xbe\xa0",3},       /* U+FFA0 halfwidth Hangul filler */
        {"\xef\xb8\x8f",3},       /* U+FE0F emoji variation selector */
        {"\xef\xb8\x80",3},       /* U+FE00 variation selector */
        {"\xf3\xa0\x84\x80",4},   /* U+E0100 supplementary VS */
        {"\xf3\xa0\x81\xa1",4},   /* U+E0061 Unicode tag character */
        {"\xe2\xa0\x80",3},       /* U+2800 blank braille cell */
        {"\xe1\x9e\xb4",3},       /* U+17B4 Khmer invisible vowel */
        {"\xe1\xa0\x8b",3},       /* U+180B Mongolian VS */
        {"\xf0\x9b\xb2\xa0",4}    /* U+1BCA0 shorthand format */
    };
    unsigned char raw[127];
    size_t i;
    for (i=0;i<sizeof invisible/sizeof invisible[0];++i) {
        check_title_bytes(invisible[i].bytes,invisible[i].length,0);
    }
    /* Reconcile #34's Unicode blank ranges with #35's modifier coverage.
     * Reject standalone blank/format scalars before ANY callback. */
    check_title_bytes("\xe2\x80\x80",3,0); /* U+2000 */
    check_title_bytes("\xe2\x80\x83",3,0); /* U+2003 */
    check_title_bytes("\xe2\x80\x8a",3,0); /* U+200A */
    check_title_bytes("\xe2\x80\xaf",3,0); /* U+202F */
    check_title_bytes("\xe2\x81\x9f",3,0); /* U+205F */
    check_title_bytes("\xe3\x80\x80",3,0); /* U+3000 */
    check_title_bytes("\xef\xbf\xb9",3,0); /* U+FFF9 */
    check_title_bytes("\xf3\xa0\x82\x80",4,0); /* U+E0080 */
    check_title_bytes("\xf0\x9d\x85\xb3",4,0); /* U+1D173 */
    /* Existing ASCII-space/NBSP behavior and visible text remain stable. */
    check_title_bytes(" ",1,1);
    check_title_bytes("\xc2\xa0",2,1);
    check_title_bytes("A\xe2\x80\x80",4,1);
    check_title_bytes("A\xe2\x80\xaf",4,1);
    check_title_bytes("A\xe3\x80\x80",4,1);
    check_title_bytes("A\xef\xbf\xb9",4,1);
    check_title_bytes("\xe2\x80\x8c\xef\xb8\x8f\xe3\x85\xa4",9,0);
    check_title_bytes("\xe2\x80\x8d\xe2\x80\x8d",6,0);
    check_title_bytes("\xef\xb8\x8f\xef\xb8\x8f",6,0);
    check_title_bytes("\xe2\x80\x8d" "Atto",7,1);
    check_title_bytes("Atto\xe2\x80\x8c",7,1);
    check_title_bytes("\xd8\xa7\xe2\x80\x8c\xd9\x84",7,1); /* Arabic with ZWNJ */
    check_title_bytes("\xe2\x9d\xa4\xef\xb8\x8f",6,1);     /* heart + VS16 */
    check_title_bytes("\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb",11,1);
    check_title_bytes("\xc2\xa0",2,1);                     /* existing U+00A0 */
    /* Exact 127-byte non-NUL-terminated invisible-only string. */
    for (i=0;i<123;i+=3) memcpy(raw+i,"\xef\xb8\x8f",3);
    memcpy(raw+123,"\xf3\xa0\x81\xa1",4);
    check_title_bytes((const char *)raw,sizeof raw,0);
    /* Long string with one visible byte remains admissible. */
    raw[0]='A';
    for (i=1;i<sizeof raw;i+=3) memcpy(raw+i,"\xe2\x80\x8d",3);
    check_title_bytes((const char *)raw,sizeof raw,1);
    /* A broken final UTF-8 scalar is never rescued by an earlier anchor. */
    raw[126]=0xe2;
    check_title_bytes((const char *)raw,sizeof raw,0);
}

int main(void) {
    CHECK(ores_desktop_abi_version()==ORES_DESKTOP_ABI_VERSION);
    bad_arguments();
    title_boundary_matrix();
    title_invisible_only_matrix();
    run_scenario(0,ORES_DESKTOP_OK,3,0,3,1,1);
    run_scenario(1,ORES_DESKTOP_OK,0,1,0,1,1);
    run_scenario(2,ORES_DESKTOP_OK,3,6,3,1,1);
    run_scenario(3,ORES_DESKTOP_BACKEND_ERROR,0,0,0,1,1);
    run_scenario(4,ORES_DESKTOP_BACKEND_ERROR,0,0,0,1,1);
    run_scenario(5,ORES_DESKTOP_BACKEND_ERROR,0,0,0,1,1);
    run_scenario(6,ORES_DESKTOP_APP_ERROR,1,0,0,1,1);
    run_scenario(7,ORES_DESKTOP_APP_ERROR,0,0,0,0,1);
    run_scenario(8,ORES_DESKTOP_BACKEND_ERROR,0,0,0,0,0);
    run_scenario(9,ORES_DESKTOP_BACKEND_ERROR,1,0,1,1,1);
    run_scenario(10,ORES_DESKTOP_OK,0,0,0,1,1);
    run_scenario(11,ORES_DESKTOP_OK,0,1,0,1,1);
    run_scenario(12,ORES_DESKTOP_OK,3,0,3,1,1);
    run_scenario(13,ORES_DESKTOP_OK,0,1,0,1,1);
    puts("desktop host ABI/lifecycle contract: PASS");
    return 0;
}
