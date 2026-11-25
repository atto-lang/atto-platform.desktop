/* Actual SDL3 adapter smoke, not a synthetic header or an Oreslang guest.
 * Run with SDL_VIDEODRIVER=dummy on headless Linux; real SDL_InitSubSystem,
 * SDL_CreateWindow, SDL_PollEvent, SDL_GetWindowFromEvent and SDL_DestroyWindow
 * are linked and exercised.
 */
#include "ores_desktop_sdl3.h"
#include <stdio.h>

static unsigned frames;
static unsigned stops;
static ores_desktop_status on_frame(void *ctx, uint64_t tick) {
    (void)ctx;
    if (tick != frames) return ORES_DESKTOP_APP_ERROR;
    ++frames;
    return ORES_DESKTOP_OK;
}
static void on_stop(void *ctx) { (void)ctx; ++stops; }

static int one_run(void) {
    ores_desktop_sdl3_state state = {0};
    const ores_desktop_config config = {
        ORES_DESKTOP_ABI_VERSION, 320, 240, 16, 2, "AttoLang SDL3 real smoke", sizeof("AttoLang SDL3 real smoke") - 1u
    };
    const ores_desktop_app app = { NULL, NULL, NULL, on_frame, on_stop };
    const ores_desktop_backend backend = ores_desktop_sdl3_backend(&state);
    const ores_desktop_status status = ores_desktop_run(&config, &app, &backend);
    if (status != ORES_DESKTOP_OK || state.window != NULL ||
        state.initialized != 0) {
        fprintf(stderr, "SDL3_REAL_SMOKE_FAIL status=%d frames=%u stops=%u\n",
                (int)status, frames, stops);
        return 1;
    }
    return 0;
}
int main(void) {
    if (one_run() || frames != 2 || stops != 1) return 1;
    // A second actual SDL3 session must succeed: the event pump owner
    // cannot be leaked by shutdown.
    frames = 0;
    if (one_run() || frames != 2 || stops != 2) return 1;
    puts("SDL3_REAL_BACKEND_SMOKE_PASS (real SDL3 host, not .ores guest)");
    return 0;
}
