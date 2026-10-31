#include "ores_desktop_sdl3.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) {fprintf(stderr,"SDL mock test FAIL %s:%d %s\n",__FILE__,__LINE__,#x);exit(1);} } while (0)
struct SDL_Window {int identity;};
static struct SDL_Window window_object={23};
static struct {
 int fail_init, fail_create;
 unsigned init_count, quit_count, create_count, destroy_count, delay_count, poll_count;
 SDL_Event events[128]; unsigned queued, next;
} mock;
static struct {
 unsigned events, frames, starts, stops;
 ores_desktop_event last;
 ores_desktop_status event_result, start_result;
} app;
static void reset(void){ memset(&mock,0,sizeof mock);memset(&app,0,sizeof app); }
static void enqueue(SDL_Event e){CHECK(mock.queued<128);mock.events[mock.queued++]=e;}
bool SDL_InitSubSystem(uint32_t flags){CHECK(flags==SDL_INIT_VIDEO);++mock.init_count;return !mock.fail_init;}
void SDL_QuitSubSystem(uint32_t flags){CHECK(flags==SDL_INIT_VIDEO);++mock.quit_count;}
SDL_Window *SDL_CreateWindow(const char *title,int width,int height,uint64_t flags){
 CHECK(title && strcmp(title,"mock SDL3")==0);CHECK(width==320 && height==240);
 CHECK(flags==SDL_WINDOW_RESIZABLE);++mock.create_count;
 return mock.fail_create?NULL:&window_object;
}
void SDL_DestroyWindow(SDL_Window *p){CHECK(p==&window_object);++mock.destroy_count;}
bool SDL_PollEvent(SDL_Event *e){++mock.poll_count;if(mock.next==mock.queued)return false;*e=mock.events[mock.next++];return true;}
SDL_Window *SDL_GetWindowFromEvent(const SDL_Event *e){
 if (!e || e->type==SDL_EVENT_QUIT) return NULL;
 switch(e->type) {
 case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
 case SDL_EVENT_WINDOW_RESIZED:
 case SDL_EVENT_KEY_DOWN:
 case SDL_EVENT_KEY_UP:
 case SDL_EVENT_MOUSE_MOTION:
 case SDL_EVENT_MOUSE_BUTTON_DOWN:
 case SDL_EVENT_MOUSE_BUTTON_UP:
   return e->window.windowID==23u ? &window_object : NULL;
 default: return NULL;
 }
}
void SDL_Delay(uint32_t ms){CHECK(ms==16);++mock.delay_count;}
static ores_desktop_status start_app(void *x){(void)x;++app.starts;return app.start_result;}
static ores_desktop_status event_app(void *x,const ores_desktop_event *e){
 (void)x;CHECK(e);++app.events;app.last=*e;return app.event_result;
}
static ores_desktop_status frame_app(void *x,uint64_t frame){(void)x;CHECK(frame==app.frames);++app.frames;return ORES_DESKTOP_OK;}
static void stop_app(void *x){(void)x;++app.stops;}
static ores_desktop_status execute(ores_desktop_sdl3_state *s,unsigned frames){
 ores_desktop_backend b=ores_desktop_sdl3_backend(s);
 ores_desktop_config cfg={ORES_DESKTOP_ABI_VERSION,320,240,2,frames,"mock SDL3"};
 ores_desktop_app a={NULL,start_app,event_app,frame_app,stop_app};
 return ores_desktop_run(&cfg,&a,&b);
}
static void verify_closed(ores_desktop_sdl3_state *s,unsigned init,unsigned create,unsigned destroy,unsigned quit){
 CHECK(mock.init_count==init);CHECK(mock.create_count==create);
 CHECK(mock.destroy_count==destroy);CHECK(mock.quit_count==quit);
 CHECK(s->initialized==0 && !s->window);
}
static void test_lifecycle(void){
 ores_desktop_sdl3_state s={0};
 reset();CHECK(execute(&s,2)==ORES_DESKTOP_OK);verify_closed(&s,1,1,1,1);
 CHECK(app.starts==1 && app.stops==1 && app.frames==2 && app.events==0);
 CHECK(mock.delay_count==2);
 reset();mock.fail_init=1;CHECK(execute(&s,1)==ORES_DESKTOP_BACKEND_ERROR);
 verify_closed(&s,1,0,0,0);CHECK(app.starts==0 && app.stops==0);
 reset();mock.fail_create=1;CHECK(execute(&s,1)==ORES_DESKTOP_BACKEND_ERROR);
 verify_closed(&s,1,1,0,1);CHECK(app.starts==0 && app.stops==0);
 reset();app.start_result=ORES_DESKTOP_STOP;CHECK(execute(&s,1)==ORES_DESKTOP_OK);
 verify_closed(&s,1,1,1,1);CHECK(app.starts==1 && app.stops==1 && app.frames==0 && mock.delay_count==0);
 reset();app.event_result=ORES_DESKTOP_APP_ERROR;
 SDL_Event e={0};e.type=SDL_EVENT_KEY_DOWN;e.key.windowID=23u;enqueue(e);
 CHECK(execute(&s,2)==ORES_DESKTOP_APP_ERROR);verify_closed(&s,1,1,1,1);
 CHECK(app.events==1 && app.frames==0 && app.stops==1);
 reset();s.initialized=1;CHECK(execute(&s,1)==ORES_DESKTOP_BACKEND_ERROR);
 CHECK(mock.init_count==0 && mock.quit_count==0 && mock.destroy_count==0);
 s.initialized=0;
 reset();s.window=&window_object;CHECK(execute(&s,1)==ORES_DESKTOP_BACKEND_ERROR);
 CHECK(mock.init_count==0 && mock.quit_count==0 && mock.destroy_count==0);
 s.window=NULL;
}
static void check_event(SDL_Event e,uint32_t type,uint32_t code,int32_t x,int32_t y,int quit){
 ores_desktop_sdl3_state s={0};reset();
 if (e.type!=SDL_EVENT_QUIT) e.window.windowID=23u;
 enqueue(e);
 CHECK(execute(&s,2)==ORES_DESKTOP_OK);
 CHECK(app.events==1 && app.last.type==type && app.last.code==code);
 CHECK(app.last.x==x && app.last.y==y);
 CHECK(app.frames==(quit?0u:2u)); CHECK(mock.delay_count==(quit?0u:2u));
 CHECK(app.stops==1);verify_closed(&s,1,1,1,1);
}
static void test_event_translation(void){
 SDL_Event e={0};e.type=SDL_EVENT_QUIT;
 check_event(e,ORES_DESKTOP_QUIT,0,0,0,1);
 e.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;
 check_event(e,ORES_DESKTOP_QUIT,0,0,0,1);
 e.window.type=SDL_EVENT_WINDOW_RESIZED;e.window.data1=1440;e.window.data2=900;
 check_event(e,ORES_DESKTOP_RESIZE,0,1440,900,0);
 e=(SDL_Event){0};e.key.type=SDL_EVENT_KEY_DOWN;e.key.scancode=42;
 check_event(e,ORES_DESKTOP_KEY_DOWN,42,0,0,0);
 e.key.type=SDL_EVENT_KEY_UP;
 check_event(e,ORES_DESKTOP_KEY_UP,42,0,0,0);
 e=(SDL_Event){0};e.motion.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=1.9f;e.motion.y=-2.8f;
 check_event(e,ORES_DESKTOP_POINTER_MOVE,0,1,-2,0);
 e.motion.x=NAN;e.motion.y=1e30f;
 check_event(e,ORES_DESKTOP_POINTER_MOVE,0,0,INT32_MAX,0);
 e.motion.x=-1e30f;e.motion.y=INFINITY;
 check_event(e,ORES_DESKTOP_POINTER_MOVE,0,INT32_MIN,INT32_MAX,0);
 e=(SDL_Event){0};e.button.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
 e.button.button=3;e.button.x=12.8f;e.button.y=-4.9f;
 check_event(e,ORES_DESKTOP_POINTER_DOWN,3,12,-4,0);
 e.button.type=SDL_EVENT_MOUSE_BUTTON_UP;
 check_event(e,ORES_DESKTOP_POINTER_UP,3,12,-4,0);
}
static void test_foreign_window_events(void){
 ores_desktop_sdl3_state s={0}; reset(); SDL_Event e={0};
 // Foreign close must never terminate our window or reach app.on_event.
 e.window.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;e.window.windowID=99u;enqueue(e);
 e.key.type=SDL_EVENT_KEY_DOWN;e.key.windowID=23u;e.key.scancode=71;enqueue(e);
 e.motion.type=SDL_EVENT_MOUSE_MOTION;e.motion.windowID=99u;enqueue(e);
 e.window.type=SDL_EVENT_WINDOW_RESIZED;e.window.windowID=23u;
 e.window.data1=640;e.window.data2=480;enqueue(e);
 CHECK(execute(&s,2)==ORES_DESKTOP_OK);
 CHECK(app.events==2 && app.frames==2 && app.stops==1);
 CHECK(app.last.type==ORES_DESKTOP_RESIZE && app.last.x==640 && app.last.y==480);
 CHECK(mock.next==4);verify_closed(&s,1,1,1,1);
 // A foreign-only close must not suppress a frame or cause QUIT.
 reset();e.window.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED;
 e.window.windowID=99u;enqueue(e);
 CHECK(execute(&s,1)==ORES_DESKTOP_OK);
 CHECK(app.events==0 && app.frames==1 && app.stops==1);
 verify_closed(&s,1,1,1,1);
}
static void test_unrecognized_bound(void){
 ores_desktop_sdl3_state s={0};reset();SDL_Event e={0};e.type=0xf00u;
 for(unsigned i=0;i<65;i++)enqueue(e);
 CHECK(execute(&s,1)==ORES_DESKTOP_OK);
 CHECK(mock.poll_count==64 && mock.next==64 && app.events==0);
 CHECK(app.frames==1 && mock.delay_count==1);
 verify_closed(&s,1,1,1,1);
}
int main(void){
 test_lifecycle();test_event_translation();test_foreign_window_events();test_unrecognized_bound();
 puts("desktop SDL3 mock lifecycle/translation/bounded-drain: PASS (not real SDL3)");
 return 0;
}
