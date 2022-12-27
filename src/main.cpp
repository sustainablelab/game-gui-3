#include "SDL.h"

// [x] add audio from file
// [ ] loop from file?
// [ ] add audio from callback (replaces add from file)

constexpr bool DEBUG    = true;                         // True: general debug prints
constexpr bool DEBUG_UI = true;                         // True: print unused UI events

namespace Mouse
{ // Everyone wants to know about the mouse
    SDL_MouseMotionEvent motion{};                      // Info from motion event
    Sint32 x{};                                         // calculated mouse x
    Sint32 y{};                                         // calculated mouse y
}
namespace UI
{ // If UI event code does too much, move code out and make it a flag
    namespace Flags
    {
        bool window_size_changed{true};
        bool mouse_moved{};
        bool fullscreen_toggled{};
    }
    bool is_fullscreen{};
}
namespace UnusedUI
{ // Debug print info about unused UI events (DEBUG_UI==true)
    void msg(int line_num, const char* event_type_str, Uint32 event_timestamp_ms)
    { // Message content for unused UI events
        /* *************Example inputs***************
         * line_num : __LINE__
         * event_type_str : "SDL_AUDIODEVICEADDED"
         * event_timestamp_ms : e.common.timestamp
         * *******************************/
        printf("line %d :\tUnused %s\tat %dms\n",
                line_num, event_type_str, event_timestamp_ms);
    }
}
namespace UnknownUI
{
    void msg(int line_num, const char* event_type_str, const char* event_id_str, Uint32 event_id)
    { // Message content for unknown UI events
        printf("line %d :\tUnknown %s\t%s: %d\n",
                line_num, event_type_str, event_id_str, event_id);
    }
}
namespace GameArt
{ // Render on GameArt::tex so stuff looks good independent of OS window
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
    constexpr int scale = 20;                           // scale : game is scale * 16:9 
    constexpr int pixel_size = 4;                       // [1:big] -- bigger is chunkier
    constexpr int w = AspectRatio::w * scale;
    constexpr int h = AspectRatio::h * scale;

    SDL_Texture *tex;
}
namespace GameWin
{ // Size of actual game in the OS window -- pixel_size > 1 makes it chunky
    int w = GameArt::w * GameArt::pixel_size;
    int h = GameArt::h * GameArt::pixel_size;
}
namespace GameAudio
{
    SDL_AudioDeviceID dev;
    Uint8* buf = NULL;
    Uint32 len{};
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
{ // OS Window size and flags
    int x,y,w,h;
    Uint32 flags;
};

SDL_Window* win;
SDL_Renderer* ren;

void shutdown(void)
{
    SDL_FreeWAV(GameAudio::buf);
    SDL_CloseAudioDevice(GameAudio::dev);
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
            wI.x = 1000; // wI.x = 10;
            wI.y = 60;
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
        ///////////
        // GAME ART
        ///////////
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        GameArt::tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, GameArt::w, GameArt::h);
        if(SDL_SetTextureBlendMode(GameArt::tex, SDL_BLENDMODE_BLEND) == -1)
        {
            printf("line %d : SDL error msg: \"%s\" ",__LINE__, SDL_GetError());
            shutdown(); return EXIT_FAILURE;
        }
        /////////////
        // GAME AUDIO
        /////////////

        SDL_AudioSpec wav_spec;
        { // 1. Load the WAV file
            const char* wav = "data/windy-lily.wav";
            if (SDL_LoadWAV(wav, &wav_spec, &GameAudio::buf, &GameAudio::len) == NULL)
            {
                printf("line %d : SDL error msg: \"%s\" ",__LINE__, SDL_GetError());
                shutdown(); return EXIT_FAILURE;
            }
        }
        SDL_AudioSpec dev_spec;
        { // 2. Open an audio device to match WAV specs
            GameAudio::dev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &dev_spec, 0);
        }
        if(DEBUG)
        { // Print the Audio device audio spec

            /* *************DOC***************
             *  --- Audio device audio spec ---
             *
             *  - spec.freq: 44100 samples per second
             *  - spec.format: 32784 SDL_AudioFormat (flags)
             *          - bit size: 16
             *          - is float: no
             *          - is int: yes
             *          - is bigendian: no
             *          - is littleendian: yes
             *          - is signed: yes
             *          - is unsigned: no
             *  - spec.channels: 2 (stereo)
             *  - spec.silence: 0
             *  - spec.samples: 4096
             *  - spec.padding: 0
             *  - spec.size: 16384 bytes
             *          - Compare with GameAudio::len : 2201596 bytes
             * *******************************/

            SDL_AudioSpec spec = dev_spec;
            puts("\n--- Audio device audio spec ---\n");
            printf("- spec.freq: %d samples per second\n", spec.freq);
            printf("- spec.format: %d SDL_AudioFormat (flags)\n", spec.format);
            printf("\t- bit size: %d\n", SDL_AUDIO_BITSIZE(spec.format));
            printf("\t- is float: %s\n", SDL_AUDIO_ISFLOAT(spec.format)?"yes":"no");
            printf("\t- is int: %s\n", SDL_AUDIO_ISINT(spec.format)?"yes":"no");
            printf("\t- is bigendian: %s\n", SDL_AUDIO_ISBIGENDIAN(spec.format)?"yes":"no");
            printf("\t- is littleendian: %s\n", SDL_AUDIO_ISLITTLEENDIAN(spec.format)?"yes":"no");
            printf("\t- is signed: %s\n", SDL_AUDIO_ISSIGNED(spec.format)?"yes":"no");
            printf("\t- is unsigned: %s\n", SDL_AUDIO_ISUNSIGNED(spec.format)?"yes":"no");
            printf("- spec.channels: %d (%s)\n", spec.channels, (spec.channels==1)?"mono":((spec.channels==2)?"stereo":"not mono or stereo!"));
            printf("- spec.silence: %d\n", spec.silence);
            printf("- spec.samples: %d\n", spec.samples);
            printf("- spec.padding: %d\n", spec.padding);
            printf("- spec.size: %d bytes\n", spec.size);
            printf("\t- Compare with GameAudio::len : %d bytes\n", GameAudio::len);
        }
        SDL_QueueAudio(GameAudio::dev, GameAudio::buf, GameAudio::len);
        SDL_PauseAudioDevice(GameAudio::dev, 0);
    }

    bool quit = false;
    while(!quit)
    {
        /////
        // UI
        /////
        SDL_Event e; while(SDL_PollEvent(&e))
        { // Process all events, set flags for tricky ones
            switch(e.type)
            { // See SDL_EventType
                case SDL_QUIT: quit=true; break;

                // e.key
                case SDL_KEYDOWN:
                    switch(e.key.keysym.sym)
                    { // Respond to keyboard input
                        case SDLK_q: quit=true; break;
                        case SDLK_F11:
                             UI::Flags::fullscreen_toggled = true;
                             break;
                        ////////////////////////
                        // UNUSED KEYDOWN EVENTS
                        ////////////////////////
                        case SDLK_RETURN:
                            if(DEBUG_UI) UnusedUI::msg(__LINE__,
                                "e.key SDL_KEYDOWN: e.key.keysym.sym SDLK_RETURN",
                                e.common.timestamp
                                );
                            break;
                        default:
                            if(DEBUG_UI)
                            { // Print unused keydown events
                                char buf[64];
                                sprintf(buf,
                                    "e.key \"SDL_KEYDOWN\": e.key.keysym.sym '%c'",
                                    e.key.keysym.sym
                                    );
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
                    switch(e.window.event)
                    { // Respond to window resize
                        case SDL_WINDOWEVENT_SIZE_CHANGED:
                            // SDL_WINDOWEVENT_SIZE_CHANGED occurs once on a resize
                            // SDL_WINDOWEVENT_RESIZED occurs twice on a resize
                            // So I use SDL_WINDOWEVENT_SIZE_CHANGED.
                            UI::Flags::window_size_changed = true;
                            if(DEBUG)
                            { // Print event name, timestamp, window size, game art size
                                printf("%d : e.window.event \"SDL_WINDOWEVENT_SIZE_CHANGED\" at %dms\n", __LINE__, e.window.timestamp);
                                printf("BEFORE: \tWindow W x H: %d x %d\tGameArt W x H: %d x %d\tGameWin W x H: %d x %d\tGtoW::scale: %d\n", wI.w, wI.h, GameArt::w, GameArt::h, GameWin::w, GameWin::h, GtoW::scale);
                            }
                            break;
                        default:
                            if(DEBUG_UI)
                            { // Print unused window events
                                ///////////////////////
                                // UNUSED WINDOW EVENTS
                                ///////////////////////
                                int w,h;
                                switch(e.window.event)
                                {
                                    case SDL_WINDOWEVENT_SHOWN:
                                        UnusedUI::msg(__LINE__,
                                            "e.window.event \"SDL_WINDOWEVENT_SHOWN\"",
                                            e.window.timestamp);
                                        break;
                                    case SDL_WINDOWEVENT_MOVED:
                                        UnusedUI::msg(__LINE__,
                                            "e.window.event \"SDL_WINDOWEVENT_MOVED\"",
                                            e.window.timestamp);
                                        break;
                                    case SDL_WINDOWEVENT_EXPOSED:
                                        UnusedUI::msg(__LINE__,
                                            "e.window.event \"SDL_WINDOWEVENT_EXPOSED\"",
                                            e.window.timestamp);
                                        SDL_GetWindowSize(win, &w, &h);
                                        printf("\tSDL_GetWindowSize:         W x H: %d x %d\n", w, h);
                                         SDL_GetRendererOutputSize(ren, &w, &h);
                                        printf("\tSDL_GetRendererOutputSize: W x H: %d x %d\n", w, h);
                                        break;
                                    case SDL_WINDOWEVENT_RESIZED:
                                        UnusedUI::msg(__LINE__,
                                            "e.window.event \"SDL_WINDOWEVENT_RESIZED\"",
                                            e.window.timestamp);
                                        SDL_GetWindowSize(win, &w, &h);
                                        printf("\tSDL_GetWindowSize:         W x H: %d x %d\n", w, h);
                                         SDL_GetRendererOutputSize(ren, &w, &h);
                                        printf("\tSDL_GetRendererOutputSize: W x H: %d x %d\n", w, h);
                                        break;
                                    case SDL_WINDOWEVENT_ENTER:
                                        UnusedUI::msg(__LINE__,
                                            "e.window.event \"SDL_WINDOWEVENT_ENTER\"",
                                            e.window.timestamp);
                                        break;
                                    case SDL_WINDOWEVENT_LEAVE:
                                        UnusedUI::msg(__LINE__,
                                            "e.window.event \"SDL_WINDOWEVENT_LEAVE\"",
                                            e.window.timestamp);
                                        break;
                                    default:
                                        UnknownUI::msg(__LINE__,
                                            "e.type \"SDL_WINDOWEVENT\"",
                                            "SDL_WindowEventID",e.window.event);
                                        break;
                                }
                            }
                            break;
                    }
                    break;

                ////////////////
                // UNUSED EVENTS
                ////////////////

                // e.key
                case SDL_KEYUP:
                    switch(e.key.keysym.sym)
                    {
                        case SDLK_RETURN:
                            if(DEBUG_UI) UnusedUI::msg(__LINE__,
                                "e.key SDL_KEYUP: e.key.keysym.sym SDLK_RETURN",
                                e.common.timestamp
                                );
                            break;
                        default:
                            if(DEBUG_UI)
                            {
                                char buf[64];
                                sprintf(buf,"e.key \"SDL_KEYUP\" e.key.keysym.sym: '%c'",
                                        e.key.keysym.sym
                                        );
                                UnusedUI::msg(__LINE__,buf,e.common.timestamp);
                            }
                            break;
                    }
                    break;

                // e.adevice
                case SDL_AUDIODEVICEADDED:
                    if(DEBUG_UI) UnusedUI::msg(__LINE__, "e.type \"SDL_AUDIODEVICEADDED\"", e.common.timestamp);
                    break;

                // e.?
                case SDL_RENDER_TARGETS_RESET:
                    if(DEBUG_UI) UnusedUI::msg(__LINE__, "e.type \"SDL_RENDER_TARGETS_RESET\"", e.common.timestamp);
                    break;

                // e.edit
                case SDL_TEXTEDITING:
                    if(DEBUG_UI) UnusedUI::msg(__LINE__, "e.type \"SDL_TEXTEDITING\"", e.common.timestamp);
                    break;

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
                        char buf[64]; sprintf(buf,"e.text \"SDL_TEXTINPUT\" e.text: \"%s\"",e.text.text);
                        UnusedUI::msg(__LINE__,buf,e.common.timestamp);
                    }
                    break;

                // e.button
                case SDL_MOUSEBUTTONDOWN:
                    if(DEBUG_UI) UnusedUI::msg(__LINE__, "e.type \"SDL_MOUSEBUTTONDOWN\"", e.common.timestamp);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if(DEBUG_UI) UnusedUI::msg(__LINE__, "e.type \"SDL_MOUSEBUTTONUP\"", e.common.timestamp);
                    break;

                // e.wheel
                case SDL_MOUSEWHEEL:
                    if(DEBUG_UI) UnusedUI::msg(__LINE__, "e.type \"SDL_MOUSEWHEEL\"", e.common.timestamp);
                    break;

                default:
                    if (DEBUG_UI)
                    { // Catch any events I haven't made cases for
                        printf("line %d : TODO: Look up 0x%4X in enum SDL_EventType "
                               "and put it in section \"UNUSED EVENTS\"\n", __LINE__, e.type);
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
        if(UI::Flags::fullscreen_toggled)
        {
            UI::Flags::fullscreen_toggled = false;
            UI::is_fullscreen = !UI::is_fullscreen;
            if(UI::is_fullscreen)
            { // Go fullscreen
                // Don't bother dealing with videomode mode change
                // SDL_WINDOW_FULLSCREEN_DESKTOP is way easier and faster than
                // SDL_WINDOW_FULLSCREEN
                SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            else
            { // Go back to windowed
                SDL_SetWindowFullscreen(win, 0);
            }
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
                    int ratio_h = wI.h/GameArt::h;
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
            if(DEBUG) printf("AFTER: \tWindow W x H: %d x %d\tGameArt W x H: %d x %d\tGameWin W x H: %d x %d\tGtoW::scale: %d\n", wI.w, wI.h, GameArt::w, GameArt::h, GameWin::w, GameWin::h, GtoW::scale);
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
