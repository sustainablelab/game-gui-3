#include <cstdio>
#include "SDL.h"

constexpr bool DEBUG = true;

namespace IgnoredUI
{
    void msg(int line_num, const char* event_type_str, Uint32 event_timestamp_ms)
    { // Message content for ignored UI events
        printf("line %d :\tIgnored %s\tat %dms\n",line_num, event_type_str, event_timestamp_ms);
    }

}

#define IGNORED_UI(X) case X: if(DEBUG) IgnoredUI::msg(__LINE__,#X,e.common.timestamp); break

namespace GameArt
{
    namespace AspectRatio
    {
        constexpr int w = 16;
        constexpr int h = 9;
    }
    constexpr int scale = 1;
    constexpr int pixel_size = 3;
    constexpr int w = AspectRatio::w * scale * pixel_size;
    constexpr int h = AspectRatio::h * scale * pixel_size;
}

struct WindowInfo
{
    int x,y,w,h;
    Uint32 flags;
};

SDL_Window* win;
SDL_Renderer* ren;

void shutdown(void)
{
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

int main(int argc, char* argv[])
{
    WindowInfo wI = {.x=950, .y=550, .w=GameArt::w, .h=GameArt::h, .flags= SDL_WINDOW_RESIZABLE};
    { // Use the window x,y,w,h passed by Vim
        if(argc>1) wI.x = atoi(argv[1]);
        if(argc>2) wI.y = atoi(argv[2]);
        if(argc>3) wI.w = atoi(argv[3]);
        if(argc>4) wI.h = atoi(argv[4]);
    }
    if(argc > 1)
    { // Vim passed some window info, so make window borderless and on-top-always
        wI.flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_INPUT_GRABBED;
    }
    if(DEBUG) printf("Window (x,y): (%d,%d)\n", wI.x, wI.y);
    if(DEBUG) printf("Window W x H: %d x %d\n", wI.w, wI.h);
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
    win = SDL_CreateWindow(argv[0], wI.x, wI.y, wI.w, wI.h, wI.flags);
    ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_ACCELERATED);

    bool quit = false;
    while(!quit)
    {
        /////
        // UI
        /////
        SDL_Event e; while(SDL_PollEvent(&e))
        {
            switch(e.type)
            { // See SDL_EventType
                case SDL_QUIT: quit=true; break;

                //////////////////
                // UNHANDLED STUFF
                //////////////////
                { // Print e.type and e.timestamp for events seen but unused.
                   // e.adevice
                   IGNORED_UI(SDL_AUDIODEVICEADDED);

                   // e.window
                   IGNORED_UI(SDL_WINDOWEVENT);

                   // e.?
                   IGNORED_UI(SDL_RENDER_TARGETS_RESET);

                   // e.key
                   IGNORED_UI(SDL_KEYDOWN);
                   IGNORED_UI(SDL_KEYUP);

                   // e.edit
                   IGNORED_UI(SDL_TEXTEDITING);

                   // e.text
                   IGNORED_UI(SDL_TEXTINPUT);

                   // e.motion
                   IGNORED_UI(SDL_MOUSEMOTION);

                   // e.button
                   IGNORED_UI(SDL_MOUSEBUTTONDOWN);
                   IGNORED_UI(SDL_MOUSEBUTTONUP);

                   // e.wheel
                   IGNORED_UI(SDL_MOUSEWHEEL);
                }

                default:
                    if (DEBUG)
                    {
                        printf("line %d : TODO: Look up 0x%4X in enum SDL_EventType "
                               "and wrap in IGNORED_UI() macro "
                               "in section \"UNHANDLED STUFF\"\n", __LINE__, e.type);
                    }
                    break;
            }
        }

        /////////
        // RENDER
        /////////
        SDL_SetRenderTarget(ren, NULL);
        SDL_SetRenderDrawColor(ren, 0,0,0,0);
        SDL_RenderClear(ren);
        SDL_RenderPresent(ren);
        if(DEBUG) fflush(stdout);
    }

    shutdown();
    return EXIT_SUCCESS;
}
