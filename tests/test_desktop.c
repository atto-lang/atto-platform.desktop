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
    CHECK(strcmp(c->title,f->live_config->title)==0);
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
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,800,600,2,3,"Desktop test"};
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
    ores_desktop_config c={ORES_DESKTOP_ABI_VERSION,800,600,2,3,"Valid"};
    ores_desktop_app a={0};
    ores_desktop_backend b={0};
    char too_long[129];
    a.context=&f; a.on_frame=frame_cb;
    b.context=&f; b.open=opened; b.poll=polled;
    b.present=presented; b.close=closed;
    CHECK(ores_desktop_run(NULL,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(ores_desktop_run(&c,NULL,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(ores_desktop_run(&c,&a,NULL)==ORES_DESKTOP_INVALID_ARGUMENT);
    c.abi_version=2;
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
    c.max_events_per_frame=2; c.title="";
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    memset(too_long,'X',128); too_long[128]='\0'; c.title=too_long;
    CHECK(ores_desktop_run(&c,&a,&b)==ORES_DESKTOP_INVALID_ARGUMENT);
    CHECK(f.opens==0);
}
int main(void) {
    CHECK(ores_desktop_abi_version()==ORES_DESKTOP_ABI_VERSION);
    bad_arguments();
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
