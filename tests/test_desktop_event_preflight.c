#include "ores_desktop.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <limits.h>
int main(void) {
    ores_desktop_event e = {ORES_DESKTOP_QUIT,0u,0,0};
    unsigned kind;
    assert(ores_desktop_validate_event(NULL) == ORES_DESKTOP_INVALID_ARGUMENT);
    for (kind=ORES_DESKTOP_QUIT;kind<=ORES_DESKTOP_POINTER_UP;++kind) {
        e.type=kind; e.x=INT32_MIN; e.y=INT32_MAX;
        assert(ores_desktop_validate_event(&e) ==
               (kind==ORES_DESKTOP_RESIZE ? ORES_DESKTOP_INVALID_ARGUMENT:ORES_DESKTOP_OK));
    }
    e.type=0u; assert(ores_desktop_validate_event(&e)==ORES_DESKTOP_INVALID_ARGUMENT);
    e.type=8u; assert(ores_desktop_validate_event(&e)==ORES_DESKTOP_INVALID_ARGUMENT);
    e.type=ORES_DESKTOP_RESIZE;
    e.x=0;e.y=0;assert(ores_desktop_validate_event(&e)==ORES_DESKTOP_OK);
    e.x=16384;e.y=16384;assert(ores_desktop_validate_event(&e)==ORES_DESKTOP_OK);
    e.x=-1;assert(ores_desktop_validate_event(&e)==ORES_DESKTOP_INVALID_ARGUMENT);
    e.x=100;e.y=16385;assert(ores_desktop_validate_event(&e)==ORES_DESKTOP_INVALID_ARGUMENT);
    return 0;
}
