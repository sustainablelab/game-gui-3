#include <cstdio>
#include "SDL.h"

// [ ] resize and recenter with window size
// [ ] add audio

constexpr bool DEBUG    = true;                         // True: general debug prints
constexpr bool DEBUG_UI = false;                        // True: print unused UI events

namespace Mouse
{
    SDL_MouseMotionEvent motion{};                      // Info from motion event
    Sint32 x{};                                         // calculated mouse x
    Sint32 y{};                                         // calculated mouse y
}
namespace UI
{
    namespace Flags
    {
        bool window_size_changed{};
        bool mouse_moved{};
    }
}
namespace UnusedUI
{
    void msg(int line_num, const char* event_type_str, Uint32 event_timestamp_ms)
    { // Message content for ignored UI events
        printf("line %d :\tUnused e.type:\t%s\tat %dms\n",
                line_num, event_type_str, event_timestamp_ms);
    }
#define UNUSED_UI(X) case X: if(DEBUG_UI) UnusedUI::msg(__LINE__,#X,e.common.timestamp); break
}
namespace GameArt
{ // Render here so stuff looks good independent of OS window
    namespace AspectRatio
    {
        constexpr int w = 16;
        constexpr int h = 9;
    }

    ////////////////
    // CHUNKY PIXELS
    ////////////////
    // ┌───┐
    // │   │
    // └───┘
    constexpr int scale = 10;                           // scale : game is scale * 16:9 
    constexpr int pixel_size = 2;                       // [1:big] -- bigger is chunkier
    constexpr int w = AspectRatio::w * scale;
    constexpr int h = AspectRatio::h * scale;

    SDL_Texture *tex;
}
namespace GameWin
{ // Size of actual game in the OS window
    int w = GameArt::w * GameArt::pixel_size;
    int h = GameArt::h * GameArt::pixel_size;
}
namespace GtoW
{ // Coordinate transform from GameArt coordinates to Window coordinates
    /* *************DOC***************
     * W = (k*G) + Offset
     * G = (W - Offset)/k
     * Offset = W - (k*G)
    *******************************/
    namespace Offset
    {
        int x=0;
        int y=0;
    }
    int scale = GameArt::pixel_size;
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
    SDL_DestroyTexture(GameArt::tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
}

int main(int argc, char* argv[])
{
    WindowInfo wI{};
    { // Window setup
        { // Window x,y,w,h defaults (use these if Vim passes no args)
            wI.x = 10;
            wI.y = 10;
            SDL_assert(GameArt::pixel_size >= 1);       // 1 : high-def, >1 : chunky
            wI.w = GameWin::w;
            wI.h = GameWin::h;
            wI.flags = SDL_WINDOW_RESIZABLE;
        };
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
        if(DEBUG)
        { // Print window x,y,w,h
            printf("Window (x,y): (%d,%d)\n", wI.x, wI.y);
            printf("Window W x H: %d x %d\n", wI.w, wI.h);
        }
    }
    { // SDL Setup
        SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
        win = SDL_CreateWindow(argv[0], wI.x, wI.y, wI.w, wI.h, wI.flags);
        ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_ACCELERATED);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        GameArt::tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, GameArt::w, GameArt::h);
        if(SDL_SetTextureBlendMode(GameArt::tex, SDL_BLENDMODE_BLEND) == -1)
        {
            printf("line %d : SDL error msg: \"%s\" ",__LINE__, SDL_GetError()); shutdown(); return EXIT_FAILURE;
        }
    }

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
                            if(DEBUG_UI)
                            {
                                char buf[64]; sprintf(buf,"SDL_KEYDOWN: e.key.keysym.sym '%c'",e.key.keysym.sym);
                                UnusedUI::msg(__LINE__,buf,e.common.timestamp);
                            }
                            break;
                    }
                    break;

                // e.motion
                case SDL_MOUSEMOTION:
                    UI::Flags::mouse_moved = true;
                    Mouse::motion = e.motion;           // Store the mouse motion
                    break;

                // e.window
                case SDL_WINDOWEVENT:
                    if(DEBUG_UI)
                    { // Unused window events
                        int w,h;
                        switch(e.window.event)
                        {
                            case SDL_WINDOWEVENT_SHOWN:
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_SHOWN\" at %dms\n",
                                        __LINE__, e.window.timestamp);
                                break;
                            case SDL_WINDOWEVENT_MOVED:
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_MOVED\" at %dms\n",
                                        __LINE__, e.window.timestamp);
                                break;
                            case SDL_WINDOWEVENT_EXPOSED:
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_EXPOSED\" at %dms\n",
                                        __LINE__, e.window.timestamp);
                                SDL_GetWindowSize(win, &w, &h);
                                printf("\tSDL_GetWindowSize:         W x H: %d x %d\n", w, h);
                                 SDL_GetRendererOutputSize(ren, &w, &h);
                                printf("\tSDL_GetRendererOutputSize: W x H: %d x %d\n", w, h);
                                break;
                            case SDL_WINDOWEVENT_RESIZED:
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_RESIZED\" at %dms\n",
                                        __LINE__, e.window.timestamp);
                                SDL_GetWindowSize(win, &w, &h);
                                printf("\tSDL_GetWindowSize:         W x H: %d x %d\n", w, h);
                                 SDL_GetRendererOutputSize(ren, &w, &h);
                                printf("\tSDL_GetRendererOutputSize: W x H: %d x %d\n", w, h);
                                break;
                            case SDL_WINDOWEVENT_ENTER:
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_ENTER\" at %dms\n",
                                        __LINE__, e.window.timestamp);
                                break;
                            case SDL_WINDOWEVENT_LEAVE:
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_LEAVE\" at %dms\n",
                                        __LINE__, e.window.timestamp);
                                break;
                            default:
                                    printf("%d : e.type \"SDL_WINDOWEVENT\" SDL_WindowEventID %d\n",__LINE__,e.window.event);
                                break;
                        }
                    }
                    switch(e.window.event)
                    { // SDL_WINDOWEVENT_SIZE_CHANGED occurs once on a resize
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                            if(DEBUG)
                            { // Print event name, timestamp, window size, game art size
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_SIZE_CHANGED\" at %dms\n", __LINE__, e.window.timestamp);
                                printf("BEFORE: \tWindow W x H: %d x %d\tGameArt W x H: %d x %d\tGameWin W x H: %d x %d\n", wI.w, wI.h, GameArt::w, GameArt::h, GameWin::w, GameWin::h);
                            }
                            UI::Flags::window_size_changed = true;
                            break;
                    }
                    break;

                { // Print e.type and e.timestamp for events seen but unused.

                    //////////////////
                    // UNHANDLED STUFF
                    //////////////////

                    // e.adevice
                    UNUSED_UI(SDL_AUDIODEVICEADDED);

                    // e.?
                    UNUSED_UI(SDL_RENDER_TARGETS_RESET);

                    // e.key
                    case SDL_KEYUP:
                        if(DEBUG_UI)
                        {
                            char buf[64]; sprintf(buf,"SDL_KEYUP e.key.keysym.sym: '%c'",e.key.keysym.sym);
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
                        if(DEBUG_UI)
                        {
                            char buf[64]; sprintf(buf,"SDL_TEXTINPUT e.text: \"%s\"",e.text.text);
                            UnusedUI::msg(__LINE__,buf,e.common.timestamp);
                        }
                        break;

                    // e.button
                    UNUSED_UI(SDL_MOUSEBUTTONDOWN);
                    UNUSED_UI(SDL_MOUSEBUTTONUP);

                    // e.wheel
                    UNUSED_UI(SDL_MOUSEWHEEL);
                }

                default:
                    if (DEBUG_UI)
                    {
                        printf("line %d : TODO: Look up 0x%4X in enum SDL_EventType "
                               "and wrap in UNUSED_UI() macro "
                               "in section \"UNHANDLED STUFF\"\n", __LINE__, e.type);
                    }
                    break;
            }
        }

        //////////
        // PHYSICS
        //////////
        if(UI::Flags::mouse_moved)
        { // Update mouse x,y
            // TODO: need to use floats for this? Doesn't seem like it.
            Mouse::x = (Mouse::motion.x - GtoW::Offset::x)/GtoW::scale;
            Mouse::y = (Mouse::motion.y - GtoW::Offset::y)/GtoW::scale;
        }
        if(UI::Flags::window_size_changed)
        { // Update stuff that depends on window size
            UI::Flags::window_size_changed = false;
            SDL_GetWindowSize(win, &wI.w, &wI.h);
            { // Resize game art to fit window
                /* *************Resize***************
                 * Use largest pixel size for GameArt to fit in Window.
                 * If window is smaller than GameArt, use GameArt w x h and let it clip.
                 * *******************************/
                if((wI.w<GameArt::w)||(wI.h<GameArt::h)) GtoW::scale = 1;
                else
                {
                    int ratio_w = wI.w/GameArt::w;
                    int ratio_h = wI.w/GameArt::h;
                    // Use the smaller of the two ratios as the scaling factor
                    GtoW::scale = (ratio_w > ratio_h) ? ratio_h : ratio_w;
                }
                GameWin::w = GtoW::scale * GameArt::w;
                GameWin::h = GtoW::scale * GameArt::h;
            }
            { // Recenter game art in window
                /* *************Recenter***************
                 * If window is bigger, recenter game art.
                 * If window is smaller, pin game art topleft to window topleft.
                 * *******************************/
                if(wI.w>GameWin::w) GtoW::Offset::x = (wI.w-GameWin::w)/2;
                else GtoW::Offset::x = 0;
                if(wI.h>GameWin::h) GtoW::Offset::y = (wI.h-GameWin::h)/2;
                else GtoW::Offset::y = 0;
            }
            if(DEBUG) printf("AFTER: \tWindow W x H: %d x %d\tGameArt W x H: %d x %d\tGameWin W x H: %d x %d\n", wI.w, wI.h, GameArt::w, GameArt::h, GameWin::w, GameWin::h);
        }

        /////////
        // RENDER
        /////////

        ///////////
        // GAME ART
        ///////////
        SDL_SetRenderTarget(ren, GameArt::tex);

        { // Game art background color
            SDL_Color c = {20,20,20,255};
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
            SDL_RenderClear(ren);
        }
        { // Placeholder game art
            SDL_Color c = {255,255,20,128};
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
            SDL_RenderDrawLine(ren, 0,0,GameArt::w,GameArt::h);
            SDL_RenderDrawLine(ren, GameArt::w,0,0,GameArt::h);
            SDL_RenderDrawLine(ren, GameArt::w/2,GameArt::h/2,Mouse::x,Mouse::y);
        }

        SDL_SetRenderTarget(ren, NULL);
        SDL_SetRenderDrawColor(ren, 0,0,0,0);
        SDL_RenderClear(ren);
        { // Stretch game art to window to get chunky pixels
            SDL_Rect src = SDL_Rect{0,0,GameArt::w,GameArt::h};
            SDL_Rect dst;                               // Destination window
            dst.x = GtoW::Offset::x;
            dst.y = GtoW::Offset::y;
            dst.w = GameWin::w;
            dst.h = GameWin::h;
            if(SDL_RenderCopy(ren, GameArt::tex, &src, &dst))
            {
                if(DEBUG) printf("%d : SDL error msg: %s\n",__LINE__,SDL_GetError());
            }
        }
        SDL_RenderPresent(ren);
        if(DEBUG) fflush(stdout);
    }

    shutdown();
    return EXIT_SUCCESS;
}
