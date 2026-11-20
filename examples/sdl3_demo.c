#include "ores_desktop_sdl3.h"
#include <stdio.h>
static ores_desktop_status event(void *ctx,const ores_desktop_event *e) {
    (void)ctx;
    if (e->type==ORES_DESKTOP_RESIZE)
        printf("Window resized: %d x %d\n",(int)e->x,(int)e->y);
    return ORES_DESKTOP_OK;
}
static ores_desktop_status frame(void *ctx,uint64_t n) {
    (void)ctx; (void)n; return ORES_DESKTOP_OK;
}
int main(void) {
    ores_desktop_sdl3_state state={0};
    static const char title[]="Oreslang Desktop (native host bootstrap)";
    ores_desktop_config config={ORES_DESKTOP_ABI_VERSION,800,500,64,0,
                                title,sizeof title-1u};
    ores_desktop_app app={0};
    ores_desktop_backend backend=ores_desktop_sdl3_backend(&state);
    ores_desktop_status result;
    app.on_event=event;
    app.on_frame=frame;
    result=ores_desktop_run(&config,&app,&backend);
    if (result!=ORES_DESKTOP_OK)
        fprintf(stderr,"desktop backend failed: %d\n",(int)result);
    return result==ORES_DESKTOP_OK?0:1;
}
