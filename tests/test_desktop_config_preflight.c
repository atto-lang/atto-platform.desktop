#include "ores_desktop.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    char title[] = {'O','r','e','s'};
    ores_desktop_config cfg = {ORES_DESKTOP_ABI_VERSION, 800, 600, 32, 1, title, 4};
    assert(ores_desktop_validate_config(NULL) == ORES_DESKTOP_INVALID_ARGUMENT);
    assert(ores_desktop_validate_config(&cfg) == ORES_DESKTOP_OK);
    assert(cfg.title == title && cfg.title_length == 4 && memcmp(title, "Ores", 4) == 0);
    cfg.width = 0;
    assert(ores_desktop_validate_config(&cfg) == ORES_DESKTOP_INVALID_ARGUMENT);
    cfg.width = 16384;
    cfg.height = 16385;
    assert(ores_desktop_validate_config(&cfg) == ORES_DESKTOP_INVALID_ARGUMENT);
    cfg.height = 600;
    cfg.max_events_per_frame = 257;
    assert(ores_desktop_validate_config(&cfg) == ORES_DESKTOP_INVALID_ARGUMENT);
    cfg.max_events_per_frame = 32;
    title[1] = '\0';
    assert(ores_desktop_validate_config(&cfg) == ORES_DESKTOP_INVALID_ARGUMENT);
    title[1] = 'r';
    cfg.abi_version++;
    assert(ores_desktop_validate_config(&cfg) == ORES_DESKTOP_INVALID_ARGUMENT);
    return 0;
}
