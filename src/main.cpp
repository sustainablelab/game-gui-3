#include <cstdlib>
#include "SDL.h"
#include "mg_colors.h"

// [ ] make my own audio data instead of audio from file

constexpr bool DEBUG    = true;                         // True: general debug prints
constexpr bool DEBUG_UI = true;                         // True: print unused UI events
constexpr bool DEBUG_AUDIO = true;                      // True: audio debug prints
constexpr bool AUDIO_CALLBACK = true;                   // False : queue audio instead of callback

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
        // TODO: loop_audio only affects queued audio. Extend to callback audio.
        bool loop_audio{true};
        bool load_audio_from_file{false};
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
    SDL_AudioDeviceID dev;                              // Audio playback device handle
    Uint32 dev_buf_size{};                              // Audio buffer size in bytes
    namespace Sound
    {
        Uint8* buf = NULL;                              // Sound buffer in memory
        Uint32 len{};                                   // Number of bytes in buffer
        // For callback (from loopwave.c)
        int pos{};                                      // Position rel to start of buffer
    }
    // Callback : from loopwave.c
    void SDLCALL fill_audio_dev(void* userdata, Uint8* stream, int len)
    { // Copied from libsdl.org/SDL2/test/loopwave.c
        (void)userdata;
        Uint8* waveptr; int waveleft;
        /* *************DOC***************
         * This callback loads from the source buffer into the device buffer.
         *
         * I normally have no access to the device buffer.
         * This callback lets me access its starting address and its length.
         *
         * userdata : no use for this yet
         * stream : starting address of the audio device byte buffer
         * len : length of the audio device byte buffer
         *
         * Sound::buf is my audio source.
         * Sound::len is the number of bytes in my source.
         *
         * Sound::buf is an absolute address.
         * Sound::pos and Sound::len are relative to Sound::buf.
         *
         * Similarly, "stream" is an absolute address and "len" is relative to stream.
         *
         * 0             15                 43 <--- Silly example numbers
         * Sound::buf    Sound::pos         Sound::len
         * ┬─────────    ┬─────────         ─────────┬
         * ↓             ↓                           ↓
         * ┌─────────────────────────────────────────┐
         * │             x                           │
         * └─────────────────────────────────────────┘
         * ---------------SOURCE BUFFER---------------
         *
         * 0                                
         * stream                                  len
         * ┬─────                                  ──┬
         * ↓                                         ↓
         * ┌─────────────────────────────────────────┐
         * │                                         │
         * └─────────────────────────────────────────┘
         * ---------------DEVICE BUFFER---------------
         * *******************************/
        waveptr = Sound::buf + Sound::pos;              // Source byte I'm up to
        waveleft = Sound::len - Sound::pos;             // Source bytes left to copy
        while(waveleft <= len)
        {
            // Copy from audio source data to audio device buffer
            SDL_memcpy(stream, waveptr, waveleft);
            // Advance audio device buffer
            stream += waveleft;
            // Update remaining space in audio device buffer
            len -= waveleft;
            // Point at start of audio source data
            waveptr = Sound::buf;
            waveleft = Sound::len;
            Sound::pos = 0;
        }
        SDL_memcpy(stream, waveptr, len);
        Sound::pos += len;
    }
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
    SDL_FreeWAV(GameAudio::Sound::buf);
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

        SDL_AudioSpec wav_spec{};
        if (UI::Flags::load_audio_from_file)
        { // 1. Load the WAV file
            const char* wav = "data/windy-lily.wav";
            /* const char* wav = "data/day01.wav"; */
            /* const char* wav = "data/Dry-Kick.wav"; */
            if (SDL_LoadWAV(wav, &wav_spec,
                    &GameAudio::Sound::buf, &GameAudio::Sound::len) == NULL)
            {
                printf("line %d : SDL error msg: \"%s\" ",__LINE__, SDL_GetError());
                shutdown(); return EXIT_FAILURE;
            }
        }
        else
        { // Set the audio spec manually and put my own sounds in the buffer
            // For now, I'm going to use the same WAV spec Audacity generates.
            wav_spec.freq = 44100;                      // 44100 samples per second
            wav_spec.channels = 1;
            wav_spec.silence = 0;
            wav_spec.samples = 4096;
            wav_spec.padding = 0;
            constexpr int BYTES_PER_SAMPLE = 2;
            wav_spec.size = wav_spec.samples * wav_spec.channels * BYTES_PER_SAMPLE;
            { // SDL_AudioFormat format: 16-bit little endian signed int
                constexpr uint8_t BITSIZE  = 8*BYTES_PER_SAMPLE;
                constexpr bool ISFLOAT     = false;     // int
                constexpr bool ISBIGENDIAN = false;     // little endian
                constexpr bool ISSIGNED    = true;      // signed
                wav_spec.format = BITSIZE;
                if(ISFLOAT)     wav_spec.format |= SDL_AUDIO_MASK_DATATYPE;
                if(ISBIGENDIAN) wav_spec.format |= SDL_AUDIO_MASK_ENDIAN;
                if(ISSIGNED)    wav_spec.format |= SDL_AUDIO_MASK_SIGNED;
            }
            wav_spec.userdata = NULL;

            ///////////////
            // MAKE A SOUND
            ///////////////
            // Let the buffer length be driven by the period of the sound
            constexpr int NUM_PERIODS = 110;            // If sound is periodic, how many?
            constexpr int NUM_SAMPLES = 400;            // Samples per period
            if(1) GameAudio::Sound::len = NUM_PERIODS*NUM_SAMPLES*BYTES_PER_SAMPLE;
            GameAudio::Sound::buf = (Uint8*)malloc(GameAudio::Sound::len);
            // 44100 [S/s] * / NUM_SAMPLES = tone Hz
            Uint8* buf = GameAudio::Sound::buf;       // buf : walk buf
            for(int i=0; i < (NUM_PERIODS*NUM_SAMPLES); i++)
            {
                int sample;                             // 16-bit signed little-endian
                float f;                                // sample as a float [-0.5:0.5]
                int A;                                  // Amplitude : max = (1<<15) - 1

                // NOTE: Why do I need my speaker 8x louder than my headphones?

                if(0)
                { // headphones
                    A = (1<<11) - 1;
                }
                if(1)
                { // speaker
                    A = (1<<14) - 1;
                }

                if(0)
                { // Sawtooth
                  // f = [0:1]
                  // Say NUM_SAMPLES=400, then f = [0/399:399/399]
                    f = (static_cast<float>(i%NUM_SAMPLES))/(NUM_SAMPLES-1);
                    // f = [-0.5:0.5]
                    f -= 0.5;
                    sample = static_cast<int>(A*f);     // Scale up to amplitude A
                }
                if(1)
                { // Triangle
                  // f = [0:1:0]
                    int TOP = NUM_SAMPLES/2;
                    int n = (i%TOP);
                    if(((i/TOP)%2)!=0) n = (TOP-1)-n;
                    f = (static_cast<float>(n))/(TOP-1);
                    // f = [-0.5:0.5]
                    f -= 0.5;
                    sample = static_cast<int>(A*f);     // Scale up to amplitude A
                }
                if(0)
                { // Noise
                    f = ((static_cast<float>(rand())/RAND_MAX));
                    f -= 0.5;
                    sample = static_cast<int>(A*f);
                }
                // Little Endian (LSB at lower address)
                *buf++ = (Uint8)(sample&0xFF);      // LSB
                *buf++ = (Uint8)(sample>>8);        // MSB
            }
        }
        if(AUDIO_CALLBACK)
        { // Wire callback into SDL_AudioSpec
            wav_spec.callback = GameAudio::fill_audio_dev;
        }
        SDL_AudioSpec dev_spec{};
        { // 2. Open an audio device to match WAV specs
            GameAudio::dev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &dev_spec, 0);
            GameAudio::dev_buf_size = dev_spec.size;
        }
        if(DEBUG)
        { // Print the audio spec for audio device or audio file

            /* *************SDL_AudioSpec***************
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
             *          - Compare with GameAudio::Sound::len : 2201596 bytes
             *
             * data/windy-lily.wav:
             *      GameAudio::Sound::len : 2201596 bytes
             *      Seconds of audio:
             2201596 / (2*44100*2.0) = 12.480703
             * data/Dry-Kick.wav:
             *      GameAudio::Sound::len : 28048 bytes
             *      Seconds of audio:
             28048 / (2*44100*2.0) = 0.159002
             * *******************************/

            SDL_AudioSpec spec;
            if(0)
            { // wav_spec
                spec = wav_spec;
                puts("\n--- Audio file audio spec ---\n");
            }
            else
            { // dev_spec
                spec = dev_spec;
                puts("\n--- Audio device audio spec ---\n");
            }
            printf("- spec.freq: %d samples per second\n", spec.freq);
            printf("- spec.format: %d SDL_AudioFormat (flags)\n", spec.format);
            printf("- spec.callback: %s\n", (spec.callback==NULL) ? "NULL" : "NOT NULL");
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
            printf("\t- Compare with GameAudio::Sound::len : %d bytes\n", GameAudio::Sound::len);
        }
        if(!AUDIO_CALLBACK)
        { // Queue the audio
            Uint32 queued = SDL_GetQueuedAudioSize(GameAudio::dev);
            while(queued < GameAudio::dev_buf_size)
            { // Queue multiple times if audio buffer is smaller than device buffer
                SDL_QueueAudio(GameAudio::dev,
                        GameAudio::Sound::buf, GameAudio::Sound::len);
                queued = SDL_GetQueuedAudioSize(GameAudio::dev);
            }
        }
        SDL_PauseAudioDevice(GameAudio::dev, 0);
    }

    srand(0);

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
                        case SDLK_ESCAPE:
                            if(DEBUG_UI) UnusedUI::msg(__LINE__,
                                "e.key SDL_KEYDOWN: e.key.keysym.sym SDLK_ESCAPE",
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
                        case SDLK_ESCAPE:
                            if(DEBUG_UI) UnusedUI::msg(__LINE__,
                                "e.key SDL_KEYUP: e.key.keysym.sym SDLK_ESCAPE",
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
                        char buf[128]; sprintf(buf,"e.text \"SDL_TEXTINPUT\" e.text: \"%s\"",e.text.text);
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

        /////////////
        // GAME SOUND
        /////////////

        if(!AUDIO_CALLBACK)
        { // Check if it's time to queue more audio
            // Copied from:
            // libsdl-org/SDL/test/loopwavequeue.c
            if(UI::Flags::loop_audio)
            {
                const Uint32 queued = SDL_GetQueuedAudioSize(GameAudio::dev);
                /* *************DOC***************
                 * 44100 / 60.0 = 735.0
                 * Samples per Frame: 44100 [S/s]* 1/60.0 [s/F] = 735.0 [S/F]
                 *
                 * Bytes per Sample: 2 (16-bit samples)
                 * 735 * 2 = 1470 
                 * Bytes per Frame = 735 [S/F] * 2 [B/S] = 1470 [B/F]
                 *
                 * I need to wait long enough that I'm not queueing too much, right?
                 * If I wait too long, I hear an audible cut.
                 * What's the sweet spot? I think it's the SDL_AudioSpec.size
                 *
                 * 2 channels * 2 bytes per sample * 4096 samples = spec.size
                 *
                 * I think it's unnecessary to guard against queuing too much: once the
                 * amount queued exceeds the device buffer size, this will stop queueing
                 * more. The idea is to fill the device buffer, even if the audio is super
                 * short and fills the buffer many times.
                 
                 * *******************************/
                if (queued < GameAudio::dev_buf_size)
                { // Audio device buffer is partially empty, queue again to loop
                    SDL_QueueAudio(GameAudio::dev,
                            GameAudio::Sound::buf, GameAudio::Sound::len);
                    if(DEBUG_AUDIO)
                    {
                        printf("BEFORE: queued %d bytes\n", queued);
                        const Uint32 queued = SDL_GetQueuedAudioSize(GameAudio::dev);
                        printf("AFTER: queued %d bytes\n", queued);
                    }
                }
            }

        }

        ///////////
        // GAME ART
        ///////////
        SDL_SetRenderTarget(ren, GameArt::tex);

        { // Game art background color
            SDL_Color c = Colors::darkgravel;
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
            SDL_RenderClear(ren);
        }
        { // Placeholder game art
            { // X
                uint8_t rand_r = (uint8_t)(std::rand()%256);
                uint8_t rand_b = (uint8_t)(std::rand()%256);
                uint8_t rand_g = (uint8_t)(std::rand()%256);
                SDL_Color c = {rand_r,rand_g,rand_b,128};
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);

                SDL_RenderDrawLine(ren, 0,0,GameArt::w,GameArt::h);
                SDL_RenderDrawLine(ren, GameArt::w,0,0,GameArt::h);
            }
            { // Mouse location
                SDL_Color c = Colors::lime;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 128);
                SDL_RenderDrawLine(ren, GameArt::w/2,GameArt::h/2,Mouse::x,Mouse::y);
            }
        }

        SDL_SetRenderTarget(ren, NULL);
        { // Set background color of window to match my Vim background color
            SDL_Color c = Colors::blackestgravel;
            SDL_SetRenderDrawColor(ren, c.r,c.g,c.b,c.a);
            SDL_RenderClear(ren);
        }
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
