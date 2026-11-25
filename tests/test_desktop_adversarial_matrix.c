#include "ores_desktop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr,"FAIL matrix %s:%d: %s\n", __FILE__,__LINE__,#expr); exit(1); } } while (0)

typedef struct {
 int open_s, start_s, poll_s, available, event_type, event_s, frame_s, present_s;
 unsigned opens, closes, starts, stops, polls, events, frames, presents;
} test_state;
static ores_desktop_status status_at(int n) {
 static const ores_desktop_status s[] = {ORES_DESKTOP_OK,ORES_DESKTOP_STOP,ORES_DESKTOP_INVALID_ARGUMENT,ORES_DESKTOP_BACKEND_ERROR,ORES_DESKTOP_APP_ERROR};
 CHECK(n>=0 && n<5); return s[n];
}
static ores_desktop_status open_b(void *ctx,const ores_desktop_config *c) {
 test_state *s=ctx; CHECK(c->width==128); ++s->opens; return status_at(s->open_s);
}
static ores_desktop_status poll_b(void *ctx,ores_desktop_event *e,int *available) {
 test_state *s=ctx; ++s->polls;
 *available=s->available;
 e->type=s->event_type==0 ? ORES_DESKTOP_QUIT : s->event_type==1 ? ORES_DESKTOP_KEY_DOWN : 0;
 return status_at(s->poll_s);
}
static ores_desktop_status present_b(void *ctx) {test_state *s=ctx; ++s->presents;return status_at(s->present_s);}
static void close_b(void *ctx) {++((test_state *)ctx)->closes;}
static ores_desktop_status start_a(void *ctx){test_state *s=ctx;++s->starts;return status_at(s->start_s);}
static ores_desktop_status event_a(void *ctx,const ores_desktop_event *e){test_state *s=ctx;CHECK(e!=NULL);++s->events;return status_at(s->event_s);}
static ores_desktop_status frame_a(void *ctx,uint64_t frame){test_state *s=ctx;CHECK(frame==0);++s->frames;return status_at(s->frame_s);}
static void stop_a(void *ctx){++((test_state *)ctx)->stops;}

/* Independent one-frame contract model: never invokes the implementation. */
static ores_desktop_status expected(test_state *s) {
 if(s->open_s!=0) return ORES_DESKTOP_BACKEND_ERROR;
 if(s->start_s>1) return ORES_DESKTOP_APP_ERROR;
 if(s->start_s==1) return ORES_DESKTOP_OK;
 if(s->poll_s!=0 || s->available==2) return ORES_DESKTOP_BACKEND_ERROR;
 if(s->available==1) {
   if(s->event_type==2) return ORES_DESKTOP_BACKEND_ERROR;
   if(s->event_s>1) return ORES_DESKTOP_APP_ERROR;
   if(s->event_s==1 || s->event_type==0) return ORES_DESKTOP_OK;
 }
 /* Exactly one frame and one present when no terminal event/error. */
 if(s->frame_s>1) return ORES_DESKTOP_APP_ERROR;
 if(s->frame_s==1) return ORES_DESKTOP_OK;
 return s->present_s==0 ? ORES_DESKTOP_OK : ORES_DESKTOP_BACKEND_ERROR;
}
static void run_one(test_state *s) {
 ores_desktop_config cfg={ORES_DESKTOP_ABI_VERSION,128,256,1,1,"matrix",6};
 ores_desktop_app a={s,start_a,event_a,frame_a,stop_a};
 ores_desktop_backend b={s,open_b,poll_b,present_b,close_b};
 ores_desktop_status want=expected(s),got=ores_desktop_run(&cfg,&a,&b);
 if(got!=want){fprintf(stderr,"FAIL status open=%d start=%d poll=%d avail=%d eventtype=%d event=%d frame=%d present=%d got=%d expected=%d\n",s->open_s,s->start_s,s->poll_s,s->available,s->event_type,s->event_s,s->frame_s,s->present_s,(int)got,(int)want);exit(1);}
 CHECK(s->opens==1);
 CHECK(s->closes==(unsigned)(s->open_s==0));
 CHECK(s->starts==(unsigned)(s->open_s==0));
 CHECK(s->stops==(unsigned)(s->open_s==0 && s->start_s<=1));
 unsigned should_poll=(s->open_s==0 && s->start_s==0);
 CHECK(s->polls==should_poll);
 unsigned should_event=should_poll && s->poll_s==0 && s->available==1 && s->event_type<2;
 CHECK(s->events==should_event);
 unsigned should_frame=should_poll && s->poll_s==0 && s->available!=2 &&
    !(s->available==1 && (s->event_type==2 || s->event_s!=0 || s->event_type==0));
 CHECK(s->frames==should_frame);
 unsigned should_present=should_frame && s->frame_s==0;
 CHECK(s->presents==should_present);
}
int main(void) {
 unsigned count=0;
 for(int open=0;open<3;open++)for(int start=0;start<5;start++)
 for(int poll=0;poll<3;poll++)for(int avail=0;avail<3;avail++)
 for(int etype=0;etype<3;etype++)for(int event=0;event<5;event++)
 for(int frame=0;frame<5;frame++)for(int present=0;present<3;present++) {
   test_state s={0};s.open_s=open;s.start_s=start;s.poll_s=poll;
   s.available=avail;s.event_type=etype;s.event_s=event;s.frame_s=frame;s.present_s=present;
   run_one(&s);++count;
 }
 printf("desktop host state-transition matrix: PASS %u cases\n",count);
 return 0;
}
