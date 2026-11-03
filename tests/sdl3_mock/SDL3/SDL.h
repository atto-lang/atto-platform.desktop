#ifndef ORES_TEST_ONLY_SDL3_STUB_H
#define ORES_TEST_ONLY_SDL3_STUB_H
/* Test-only shim; deliberately not an SDL3 implementation or an ABI contract. */
#include <stdbool.h>
#include <stdint.h>
typedef struct SDL_Window SDL_Window;
#define SDL_INIT_VIDEO 0x20u
#define SDL_WINDOW_RESIZABLE 0x40u
#define SDL_EVENT_QUIT 0x101u
#define SDL_EVENT_WINDOW_CLOSE_REQUESTED 0x102u
#define SDL_EVENT_WINDOW_RESIZED 0x103u
#define SDL_EVENT_KEY_DOWN 0x104u
#define SDL_EVENT_KEY_UP 0x105u
#define SDL_EVENT_MOUSE_MOTION 0x106u
#define SDL_EVENT_MOUSE_BUTTON_DOWN 0x107u
#define SDL_EVENT_MOUSE_BUTTON_UP 0x108u
typedef union SDL_Event {
    uint32_t type;
    struct {uint32_t type; int32_t data1, data2;} window;
    struct {uint32_t type; int32_t scancode;} key;
    struct {uint32_t type; float x, y;} motion;
    struct {uint32_t type; float x, y; unsigned char button;} button;
} SDL_Event;
bool SDL_InitSubSystem(uint32_t flags);
void SDL_QuitSubSystem(uint32_t flags);
SDL_Window *SDL_CreateWindow(const char *title,int width,int height,uint64_t flags);
void SDL_DestroyWindow(SDL_Window *window);
bool SDL_PollEvent(SDL_Event *event);
void SDL_Delay(uint32_t ms);
#endif
