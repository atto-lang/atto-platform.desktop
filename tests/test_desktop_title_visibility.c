/* Targeted host-C-ABI checks, no guest compiler or native window rendering. */
#include "ores_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct { int opens,starts,stops,polls,frames,presents,closes; const unsigned char *original; size_t length; } counts;
static ores_desktop_status opened(void *p,const ores_desktop_config *c) {
    counts *f=(counts*)p;
    CHECK(c->title!= (const char*)f->original);
    CHECK(c->title_length==f->length);
    CHECK(memcmp(c->title,f->original,f->length)==0);
    CHECK(c->title[f->length]=='\0');
    ++f->opens; return ORES_DESKTOP_OK;
}
static ores_desktop_status started(void *p) { ++((counts*)p)->starts; return ORES_DESKTOP_OK; }
static ores_desktop_status polled(void *p,ores_desktop_event *e,int *available) {
    (void)e; ++((counts*)p)->polls; *available=0; return ORES_DESKTOP_OK;
}
static ores_desktop_status framed(void *p,uint64_t n) { CHECK(n==0); ++((counts*)p)->frames; return ORES_DESKTOP_OK; }
static ores_desktop_status presented(void *p) { ++((counts*)p)->presents; return ORES_DESKTOP_OK; }
static void stopped(void *p) { ++((counts*)p)->stops; }
static void closed(void *p) { ++((counts*)p)->closes; }
static unsigned checks;
static void verify(const unsigned char *data,size_t size,int accept) {
    counts f={0};
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,800,600,1,1,(const char*)data,size};
    ores_desktop_app a={0};
    ores_desktop_backend b={0};
    f.original=data; f.length=size;
    a.context=&f; a.on_start=started; a.on_frame=framed; a.on_stop=stopped;
    b.context=&f; b.open=opened; b.poll=polled; b.present=presented; b.close=closed;
    CHECK(ores_desktop_run(&c,&a,&b) == (accept?ORES_DESKTOP_OK:ORES_DESKTOP_INVALID_ARGUMENT));
    CHECK(f.opens==accept && f.starts==accept && f.stops==accept);
    CHECK(f.polls==accept && f.frames==accept && f.presents==accept && f.closes==accept);
    ++checks;
}
#define VERIFY(x,accept) verify((const unsigned char*)(x),sizeof(x)-1u,(accept))
int main(void) {
    unsigned char raw[128];
    size_t i;
    /* Previously admitted standalone format/filler scalars must fail pre-callback. */
    VERIFY("\xe2\x80\x8c",0); /* ZWNJ */
    VERIFY("\xe2\x80\x8d",0); /* ZWJ */
    VERIFY("\xe3\x85\xa4",0); /* Hangul filler */
    VERIFY("\xef\xb8\x8f",0); /* VS16 */
    VERIFY("\xef\xb8\x80",0); /* VS1 */
    VERIFY("\xe1\x85\x9f",0); /* Hangul choseong filler */
    VERIFY("\xe1\x85\xa0",0); /* Hangul jungseong filler */
    VERIFY("\xef\xbe\xa0",0); /* halfwidth hangul filler */
    VERIFY("\xe1\xa0\x8b",0); /* Mongolian free variation selector */
    VERIFY("\xe1\xa0\x8f",0); /* Mongolian FVS4 */
    VERIFY("\xe1\x9e\xb4",0); /* Khmer inherent aq */
    VERIFY("\xf3\xa0\x84\x80",0); /* supplementary VS */
    VERIFY("\xf3\xa0\x80\xa0",0); /* tag character */
    VERIFY("\xf0\x9b\xb2\xa0",0); /* shorthand format */
    VERIFY("\xf0\x9d\x85\xb3",0); /* musical format */
    VERIFY("\xe2\x80\x8c\xe2\x80\x8d",0); /* only two invisibles */
    VERIFY("\xef\xb8\x8f\xe3\x85\xa4",0); /* mixed invisibles */
    VERIFY("\xe2\x80\x8d\xe2\x80\x8d",0); /* repeated ZWJ */
    /* Non-DI Unicode blanks must not pass as stand-alone visible titles. */
    VERIFY("\xe2\xa0\x80",0); /* U+2800 braille pattern blank */
    VERIFY("\xe2\x80\x80",0); /* U+2000 en quad */
    VERIFY("\xe2\x80\x83",0); /* U+2003 em space */
    VERIFY("\xe2\x80\x8a",0); /* U+200A hair space */
    VERIFY("\xe2\x80\xaf",0); /* U+202F narrow no-break space */
    VERIFY("\xe2\x81\x9f",0); /* U+205F medium mathematical space */
    VERIFY("\xe3\x80\x80",0); /* U+3000 ideographic space */
    VERIFY("A\xe2\xa0\x80",1); /* visible letter + Braille blank */
    VERIFY("A\xe2\x80\x80",1); /* visible letter + en quad */
    /* Existing rejection policy remains fail closed even with visible letters. */
    VERIFY("A\xe2\x80\xae",0); /* bidi override */
    VERIFY("A\xe2\x80\x8b",0); /* zero width space */
    VERIFY("\xe2\x80\x8c\xe2\x82",0); /* malformed trailing scalar */
    VERIFY("\xed\xa0\x80",0); /* UTF-8 surrogate */
    VERIFY("A\x00\x42",0); /* embedded NUL */
    /* Legitimate title shaping must survive: Arabic/Persian joiners, emoji. */
    VERIFY("AttoLang",1);
    VERIFY("\xe2\x80\x8c\x41",1);
    VERIFY("A\xe2\x80\x8d",1);
    VERIFY("\xe2\x9c\x94\xef\xb8\x8f",1); /* emoji-style VS16 */
    VERIFY("\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x92\xbb",1);
    VERIFY("\xd9\x85\xdb\x8c\xe2\x80\x8c\xd8\xb4",1); /* Persian */
    VERIFY("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d",1); /* Hebrew */
    VERIFY("\xe6\x97\xa5\xe6\x9c\xac",1); /* Japanese */
    VERIFY("\xc2\xa0",1); /* preserve standalone NBSP compatibility */
    VERIFY(" ",1); /* preserve ASCII-space admission contract */
    /* Strict size fence, including non-NUL terminated input at 127 bytes. */
    memset(raw,'A',127); raw[127]='B';
    verify(raw,127,1);
    verify(raw,128,0);
    for (i=0;i<42;i++) { raw[3*i]=0xe2; raw[3*i+1]=0x80; raw[3*i+2]=0x8c; }
    verify(raw,126,0); /* all ZWNJ */
    raw[126]='A'; verify(raw,127,1);
    for (i=0;i<31;i++) { raw[4*i]=0xf3; raw[4*i+1]=0xa0; raw[4*i+2]=0x84; raw[4*i+3]=0x80; }
    raw[124]=0xe3; raw[125]=0x85; raw[126]=0xa4;
    verify(raw,127,0); /* all invisible exactly 127 bytes */
    raw[124]='X'; verify(raw,125,1);
    printf("desktop title visibility: %u cases PASS\n",checks);
    return 0;
}
