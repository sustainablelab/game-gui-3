#include <cstdio>
#include "SDL.h"

constexpr bool DEBUG = true;

namespace UnusedUI
{
    void msg(int line_num, const char* event_type_str, Uint32 event_timestamp_ms)
    { // Message content for ignored UI events
        printf("line %d :\tIgnored %s\tat %dms\n",
                line_num, event_type_str, event_timestamp_ms);
    }
}

#define UNUSED_UI(X) case X: if(DEBUG) UnusedUI::msg(__LINE__,#X,e.common.timestamp); break

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

                // e.key
                case SDL_KEYDOWN:
                    switch(e.key.keysym.sym)
                    {
                        case SDLK_q: quit=true; break;
                        default:
                            if(DEBUG)
                            {
                                char buf[16]; sprintf(buf,"SDL_KEYDOWN: e.key.keysym.sym '%c'",e.key.keysym.sym);
                                UnusedUI::msg(__LINE__,buf,e.common.timestamp);
                            }
                            break;
                    }
                    break;
                //////////////////
                // UNHANDLED STUFF
                //////////////////
                { // Print e.type and e.timestamp for events seen but unused.
                    // e.adevice
                    UNUSED_UI(SDL_AUDIODEVICEADDED);

                    // e.window
                    UNUSED_UI(SDL_WINDOWEVENT);

                    // e.?
                    UNUSED_UI(SDL_RENDER_TARGETS_RESET);

                    // e.key
                    case SDL_KEYUP:
                        if(DEBUG)
                        {
                            char buf[16]; sprintf(buf,"SDL_KEYUP e.key.keysym.sym: '%c'",e.key.keysym.sym);
                            UnusedUI::msg(__LINE__,buf,e.common.timestamp);
                        }
                        break;

                    // e.edit
                    UNUSED_UI(SDL_TEXTEDITING);

                    // e.text
                    /* *************TextInput***************
                     * SDL_StartTextInput()
                     * SDL_StopTextInput()
                     * These are really enable/disable, not start/stop.
                     *
                     * See https://wiki.libsdl.org/SDL2/Tutorials-TextInput
                     *
                     * Starts/Stops SDL looking at all keyboard input as potential for an
                     * alternate input method, like entering chinese characters.
                     *
                     * It seems by default that text input is enabled.
                     * So any direct text (not IME), generates this event with the
                     * single-character. Maybe that's useful? Or maybe it's wasting cycles
                     * and I should disable this during setup?
                     *
                     * I wanted to use this for a Vim style keybinding, but I can't figure
                     * out how to collect multiple characters with this "text input" API.
                     * *******************************/
                    case SDL_TEXTINPUT:
                        if(DEBUG)
                        {
                            char buf[64]; sprintf(buf,"SDL_TEXTINPUT e.text: \"%s\"",e.text.text);
                            UnusedUI::msg(__LINE__,buf,e.common.timestamp);
                        }
                        break;

                    // e.motion
                    UNUSED_UI(SDL_MOUSEMOTION);

                    // e.button
                    UNUSED_UI(SDL_MOUSEBUTTONDOWN);
                    UNUSED_UI(SDL_MOUSEBUTTONUP);

                    // e.wheel
                    UNUSED_UI(SDL_MOUSEWHEEL);
                }

                default:
                    if (DEBUG)
                    {
                        printf("line %d : TODO: Look up 0x%4X in enum SDL_EventType "
                               "and wrap in UNUSED_UI() macro "
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
