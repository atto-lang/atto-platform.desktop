#include "ores_desktop.h"
#include <stdint.h>
#include <stdio.h>
/* Plain i64 symbols exported by the current LLVM native scalar compiler. */
extern int64_t desktop_initial_width(void);
extern int64_t desktop_initial_height(void);
extern int64_t desktop_next_frame(int64_t frame);
typedef struct {
    int opens, closes, presents;
} state;
static ores_desktop_status opened(void *context,const ores_desktop_config *c) {
    state *s=(state*)context;
    if (c->width!=800 || c->height!=500) return ORES_DESKTOP_BACKEND_ERROR;
    ++s->opens;
    return ORES_DESKTOP_OK;
}
static ores_desktop_status polled(void *context,ores_desktop_event *event,int *available) {
    (void)context; (void)event; *available=0; return ORES_DESKTOP_OK;
}
static ores_desktop_status presented(void *context) {
    ++((state*)context)->presents; return ORES_DESKTOP_OK;
}
static void closed(void *context) { ++((state*)context)->closes; }
static ores_desktop_status frame(void *context,uint64_t number) {
    (void)context;
    return number==0 ? ORES_DESKTOP_OK : ORES_DESKTOP_APP_ERROR;
}
int main(void) {
    int64_t w=desktop_initial_width(),h=desktop_initial_height();
    state s={0};
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,0,0,16,1,"LLVM native guest linkage"};
    ores_desktop_app a={0};
    ores_desktop_backend b={0};
    if (w<1 || w>16384 || h<1 || h>16384) return 2;
    c.width=(uint32_t)w; c.height=(uint32_t)h;
    a.on_frame=frame;
    b.context=&s; b.open=opened; b.poll=polled;
    b.present=presented; b.close=closed;
    if (ores_desktop_run(&c,&a,&b)!=ORES_DESKTOP_OK ||
        s.opens!=1 || s.presents!=1 || s.closes!=1) return 3;
    puts("LLVM exported scalar -> native desktop host: PASS (headless)");
    return 0;
}
