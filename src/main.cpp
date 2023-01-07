#include <cstdlib>
#include "SDL.h"
#include "SDL_ttf.h"
#include "mg_colors.h"

/* *************Audio Tasks***************
   [x] make my own audio data instead of audio from file
    - this is easy if the audio is just one thing that never changes
    - but now I want the audio to change
    - that was a day's work, but I think I got the basic plumbing
   [x] control audio with UI events
   [x] how do I tie note frequency to wave_spec.freq (44100 or 48000)?
   [x] how do I keep waveforms periodic regardless of buffer size
       (so there are no audio hiccups each time I hit end of buffer)?
       Ans: use phase [0:1]. Ratio of desired freq (note) to sample rate is how much to
       increment phase each step. Use phase to calc waveform value, then advance phase one
       step. On wraparound (phase >= 1), do not reset to zero! Instead, subtract one from
       phase.
   [ ] Describe waveform with a curve (do a triangle wave to replace sawtooth)
   [ ] Use SDL_AudioStream
    - I'm doing stuff myself at the byte level for now
    - life will probably be much simpler if I use SDL_AudioStream
 * *******************************/
/* *************Latency***************
 * Audio device has a buffer. I decide how big the buffer is.
 *
 * Audio device buffer size tradeoff:
 * - smaller buffer : less latency between UI event and updating audio
 * - bigger buffer : execution is interrupted less often
 *
 * I think of my audio sound buffer as an audio tape that I write to.
 * I think of the audio device buffer as a playhead.
 * I write to tape just ahead of the playhead.
 *
 * - the audio device has a buffer of 2^N samples
 * - the audio device grabs those samples in the callback
 * - at the end of the callback, I write the next 2^N samples to the buffer
 *      - this guarantees that there is new audio to grab on the next callback
 *      - it also means there's no notion of unsafe sections to write over
 *      - the playhead sucks up a little audio, I drop a little pile just ahead, the
 *        playhead sucks that up, and on it goes
 *      - to kick off the whole process, I start off the audio by writing silence.
 *
 * - right now I'm only writing to the buffer in the callback
 * - so my UI latency is 4096/44100 = 93ms
 * - this is too much latency
 * - I'd like to write to the buffer more often
 * - a quick fix is to just make the audio device buffer much smaller
 * - this makes the callback happen way more often
 * - I found that a buffer of 512 samples feels right
 * - 512/44100 = 11.6ms
 * - my test is to move the mouse around to control the noise volume
 * - if I don't hear "jumps" in the audio level, then latency is good
 * *******************************/

constexpr bool DEBUG    = true;                         // True: general debug prints
constexpr bool DEBUG_UI = true;                         // True: print unused UI events
constexpr bool DEBUG_AUDIO = true;                      // True: audio debug prints
constexpr bool AUDIO_CALLBACK = true;                   // False : queue audio instead of callback
constexpr int A_MAX = (1<<12) - 1;                      // Maximum volume of any single sound
// Freq of 1st harmonic is UI::VCA::mouse_height*FREQ_H1_MAX
constexpr float FREQ_H1_MAX = 220;                      // Maximum freq of 1st harmonic

namespace Mouse
{ // Everyone wants to know about the mouse
    SDL_MouseMotionEvent motion{};                      // Info from motion event
    Sint32 x{};                                         // calculated mouse x
    Sint32 y{};                                         // calculated mouse y
    float xf{};                                         // calculated mouse x as float
    float yf{};                                         // calculated mouse y as float
}
namespace UI
{ // If UI event code does too much, move code out and make it a flag

    bool show_overlay{true};
    namespace Flags
    {
        bool window_size_changed{true};
        bool mouse_moved{};
        bool fullscreen_toggled{};
        // TODO: loop_audio only affects queued audio. Extend to callback audio.
        bool loop_audio{true};
        bool load_audio_from_file{false};               // Make my own audio in code!
        bool mouse_xy_isfloat{true};
        bool pressed_space{};
        bool pressed_shift_space{};
        bool pressed_j{};
        bool pressed_r{};
        // Play specific notes by warping mouse to x,y with numbers
        bool pressed_1{};
        bool pressed_2{};
        bool pressed_3{};
        bool pressed_4{};
        bool pressed_5{};
        bool pressed_6{};
        bool pressed_7{};
        bool pressed_8{};
        bool pressed_9{};
        bool pressed_0{};
        bool pressed_minus{};
        bool pressed_equals{};
        bool pressed_backspace{};
    }
    bool is_fullscreen{};
    namespace VCA
    {
        float mouse_center_dist;
        float mouse_height;
    }
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
void write_tape(Uint8* wpos, Uint32 NUM_SAMPLES);
namespace GameAudio
{
    SDL_AudioDeviceID dev;                              // Audio playback device handle
    Uint32 dev_buf_size{};                              // Audio buffer size in bytes
    Uint32 num_samples{};                               // Audio buffer size in samples

    // For audio I make (not audio from file)
    constexpr int SAMPLE_RATE = 44100;                  // 44100 samples per second
    constexpr int BYTES_PER_SAMPLE = 2;                 // 16-bit audio

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
        // The example code is nice and general: source buffer size is decoupled from device
        // buffer size. But why should I care? Why not just let source buffer always be an
        // exact multiple of device buffer size? Like 10x? That would eliminate checking for
        // the wraparound, which makes the code much simpler.
        // TODO: try that. Any difference?
        // TODO: change callback name to "read_tape_callback"
        //       SDL callback where the audio device reads in the next bit of audio tape.
        //       The callback then writes the bit of audio tape just after the bit that was
        //       read.

        /* *************DOC***************
         * userdata : stuff I pass to callback, no use for this yet
         * stream : audio device buffer
         * len : size of audio device buffer
         *
         * Usually, this callback just does this:
         *
         *      // Get position in the audio tape
         *      play_head = Sound::buf + Sound::pos;        // Source byte I'm up to
         *
         *      // Copy a len-sized bit from the tape to the audio device
         *      SDL_memcpy(stream, play_head, len);
         *
         *      // Update the position in the audio tape
         *      Sound::pos += len;
         *
         * Eventually, the audio tape reaches the end, or close to it.
         * This is when the tape left is less than the audio device buffer size.
         *
         *      // How much tape is left?
         *      tapeleft = Sound::len - Sound::pos;         // Source bytes until wrap around
         *
         *      // Is it enough to fill the device buffer?
         *      if(tapeleft < len)
         *      {
         *          ...
         *
         * When that happens, first just copy whatever is left:
         *
         *      if(tapeleft < len)
         *      {
                    SDL_memcpy(stream, play_head, tapeleft);
         *      }
         *
         *      // Update device buffer position and remaining space to fill
         *      stream += tapeleft; len -= tapeleft;
         *
         * Note, the len will reset to the full buffer size the next time the callback is
         * called. The function takes int len, not int* len. Similarly, the stream will
         * reset to point at the start of the stream because the function takes Uint*
         * stream, not Uint** stream.
         *
         *      // Tape wraparound 
         *      play_head = Sound::buf;
         *      tapeleft = Sound::len;
         *      Sound::pos = 0;
         *
         *      // Copy a len-sized bit from the tape to the audio device
         *      SDL_memcpy(stream, play_head, len);
         *
         *      // Update the position in the audio tape
         *      Sound::pos += len;
         *
         * Note these last two lines read the same as the usual non-wraparound case, so it
         * is the same code. But when these lines happen after a wraparound, stream and len
         * are different from the usual values. stream is somewhere inside the device buffer
         * and len is less than the buffer length.
         * *******************************/
        { // Copy from Sound::buf to audio device
            (void)userdata;
            Uint8* play_head; int tapeleft;
            /* *************DOC***************
             * This callback loads from the source buffer (audio tape) into the device
             * buffer.
             *
             * The source buffer is much larger than the device buffer.
             *      Say the device buffer is 4096 mono 16-bit samples.
             *      That's 2^12 * 2 bytes, or 8192 bytes.
             *      For a stereo wav file, the device buffer is double that (8192 bytes for
             *      each channel).
             *      Playing that at 44100 samples per second, the device buffer holds only
             *      about 92ms of audio.
             *      For generating audio from UI events, the device buffer should be smaller
             *      to avoid latency (delay between when event happens and when sound
             *      updates). UI events are handled once per frame, at 60 FPS that is about
             *      16ms. For mono 16-bit samples at 44100 samples per second, use 2^9
             *      samples. 2^9 samples * 2 bytes is 1024 bytes. This is a latency of
             *      512/44100 = 0.01161 seconds, about 12ms, which means the callback
             *      happens about once per frame. Any faster would be overkill (UI events
             *      don't get processed any faster). And the latency feels OK.
             *      So the device buffer is something like 512 samples or 4096 samples,
             *      depending on whether I care about latency or if I just default to
             *      whatever the wav file sample size is.
             *
             *      Say the source buffer is one seconds worth of audio.
             *      Playing that at 44100 samples per second, the source buffer holds 44100
             *      samples. The source buffer is 86x to 10x bigger than the device buffer.
             *
             *      Since these are mono 16-bit samples, that's just 2 bytes per sample,
             *      so the source buffer is 88,200 bytes.
             *      That's big, but not crazy big. It's 200 bytes larger than what I've been
             *      playing with in my initial tests.
             *
             *      Allocate that when the program starts and only deallocate at shutdown.
             *      It can definitely be smaller, I just don't have a good sense of the
             *      constraints yet. But it's fine at this size, so just go for it.
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
             * 0               16              42 <--- Silly example numbers
             * Sound::buf      Sound::pos      Sound::len
             * ┬─────────      ┬─────────      ─────────┬
             * ↓               ↓                        ↓
             * ┌─────────────────────────────────────────┐
             * 0               x                        !│
             * └─────────────────────────────────────────┘
             * ---------------SOURCE BUFFER---------------
             *
             * 0
             * stream       len
             * ┬─────       ──┬
             * ↓              ↓
             * ┌───────────────┐
             * 0              !│
             * └───────────────┘
             * --DEVICE BUFFER--
             *
             * Here is an example of how this plays out using made-up tiny numbers.
             *
             * len = 16 unless otherwise noted
             *
             * SDL_memcpy(stream, play_head, len)
             * |               SDL_memcpy(stream, play_head, len)
             * |               |               SDL_memcpy(stream, play_head, tapeleft)
             * |               |               |                             = 10
             * |               |               |         SDL_memcpy(stream, play_head, len)
             * |               |               |         |                  = 0      = 6
             * |               |               |         |     SDL_memcpy(stream, play_head, len)
             * |               |               |         |     |               SDL_memcpy(stream, play_head, len)
             * |               |               |         |     |               |               SDL_memcpy(stream, play_head, tapeleft)
             * |               |               |         |     |               |               |                             = 4
             * v               v               v         v     v               v               v
             * 0               16              32        0     6               22              38
             * play_head       play_head  play_head  play_head play_head       play_head       play_head
             * ┬──────         ┬──────    ─────┬──── ────┬──── ┬────────       ┬──────         ┬──────
             * ↓               ↓               ↓         ↓     ↓               ↓               ↓
             * ┌───────────────|───────────────|─────────┌─────|───────────────|───────────────|───┐
             * 0              !0              !0        !0    !0              !0              !0   │
             * └───────────────|───────────────|─────────└─────────────────────|───────────────|───┘
             * 0--------------SOURCE BUFFER--------------0--------------SOURCE BUFFER--------------
             * ┌───────────────┌───────────────┌─────────|─────┌───────────────┌───────────────┐
             * 0              !0              !0        !0    !0              !0              !│
             * └───────────────└───────────────└─────────|─────└───────────────└───────────────┘
             * 0-DEVICE BUFFER-0-DEVICE BUFFER-0-DEVICE BUFFER-0-DEVICE BUFFER-0-DEVICE BUFFER-
             * *******************************/
            play_head = Sound::buf + Sound::pos;        // Source byte I'm up to
            tapeleft = Sound::len - Sound::pos;         // Source bytes until wrap around
            if(tapeleft <= len)
            { // Near end of Sound::buf, copy the last bit of sound to device
                /* *************DOC***************
                 * tapeleft : amount of audio left until the "end of the tape"
                 * len : size of audio device buffer (tiny compared to size of audio tape)
                 *
                 * See my sketch above that shows this branch is the wraparound case.
                 *
                 * ******************************/
                // Copy from Sound::buf to audio device buffer
                SDL_memcpy(stream, play_head, tapeleft);
                // Advance audio device buffer
                stream += tapeleft;
                // Update remaining space in audio device buffer
                len -= tapeleft;
                // Wrap back around to start of Sound::buf
                // Point at start of Sound::buf
                play_head = Sound::buf;
                tapeleft = Sound::len;
                Sound::pos = 0;
            }
            SDL_memcpy(stream, play_head, len);
            Sound::pos += len;
        }
        { // Write next bit of sound for consumption in next callback
            Uint8* write_head = Sound::buf + Sound::pos;// write_head : walk Sound::buf
            int NUM_SAMPLES = GameAudio::num_samples;   // Samples I want to write

            int bytesleft = Sound::len - Sound::pos;    // Bytes until wraparound
            int samplesleft=bytesleft/BYTES_PER_SAMPLE; // Samples until wraparound
            if(samplesleft <  NUM_SAMPLES)
            { // Not enough room: write part of it, then wraparound and write the rest
                write_tape(write_head, samplesleft);       // Final write before wrap around
                // Set up to write the rest after wraparound
                NUM_SAMPLES -= samplesleft;
                write_head = Sound::buf;                      // Point back at start of Sound::buf
            }
            if(0)
            { // Debug prints! Show samplesleft and NUM_SAMPLES
                printf("%d : samplesleft: %d\n",__LINE__, samplesleft);
                printf("%d : NUM_SAMPLES: %d\n",__LINE__, NUM_SAMPLES);
                fflush(stdout);
            }
            // Write the rest (or all of it if there was enough room)
            write_tape(write_head, NUM_SAMPLES);           // Usually a full dev buf write
        }
    }
}
namespace Waveform
{
    ////////////
    // WAVEFORMS
    ////////////
    // Waveforms:
    // - return a float in range -0.5 to 0.5
    // - use phase to calculate the return value (if waveform is periodic)
    float sawtooth(float phase) { return (-0.5*(1-phase)) + (0.5*phase); }
    float noise(void) { return (static_cast<float>(rand())/RAND_MAX) - 0.5; }
    void advance(float* phase, float freq)
    {
        /* *************DOC***************
         * phase : time location [0:1] in one period of the waveform
         * freq : float [Hz] of waveform
         *
         * Advance the waveform phase based on the frequency parameter.
         *
         * Advance the phase by some fraction of a period.
         * The fraction of a period is in units of [Periods per Sample]:
         *               freq / SAMPLE_RATE        = Fraction of a period
         * Periods per second / Samples per second = Periods per Sample
         *
         * WHEN phase hits 1:
         * - reached end of period
         * - DO NOT reset phase to zero!
         * - Subtract 1 instead
         * - This allows the phase to "wraparound"
         * - If I reset phase to zero, freq is noticeably quantized at high freq
         * *******************************/
        *phase += (freq / static_cast<float>(GameAudio::SAMPLE_RATE));
        if(*phase >= 1) *phase -= 1;
    }
}
namespace Envelope
{
    bool enabled{};
    float phase = 1;
    ////////////
    // ENVELOPES
    ////////////
    // Envelopes:
    // - return a float in range 0 to 1
    // - use phase to calculate the return value (if waveform is periodic)
    float straight_R(float phase) { return 1-phase; } // Linear release
    void advance(float* phase, float period)
    {
        if (enabled)
        {
            float freq = 1/period;
            *phase += (freq / static_cast<float>(GameAudio::SAMPLE_RATE));
            if(*phase >= 1)
            { // Envelope is single-shot
                *phase = 1;
                enabled = false;
            }
        }
    }
}
// Array of phase values for each voice
namespace Voices
{
    constexpr int MAX_COUNT = 8;
    int count = 1;
    float phase[MAX_COUNT]{};                               // [0:1] : location in waveform
}
void write_tape(Uint8* wpos, Uint32 NUM_SAMPLES)
{ // Write `NUM_SAMPLES` to position `wpos` in audio tape
    // TODO: Move sound generation and amplitude stuff out to a different
    //       function that generates the waveform samples. This function should literally
    //       just write samples to tape -- so it will read values from somewhere, it won't
    //       generate any samples.
    //       The noise generation and amplitude scaling here is just a placeholder.
    int sample; float a;                                // Amplitude
    for(Uint32 i=0; i<NUM_SAMPLES; i++)
    {
        if(1) // Play all Voices as a single mix of sawtooth harmonics
        { // Parametric waveform -- use mouse to vary pitch, not amplitude
            sample = 0;                                 // reset sample
            for(int i=0; i<Voices::count; i++)
            {
                // Get waveform value for this voice
                a = Waveform::sawtooth(Voices::phase[i]);
                // Base amplitude and frequency on harmonic
                int harmonic = i+1;                     // harmonic : simple int multiple
                // Convert to a 16-bit sample and add to this sample
                if(1) sample += static_cast<int>(A_MAX*a);
                if(0) sample += static_cast<int>((A_MAX*a)/Voices::count);
                // Advance phase, get freq from mouse distance to center and harmonic
                // freq is set by mouse height, max freq is FREQ_H1_MAX*harmonic
                Waveform::advance(
                        &Voices::phase[i],
                        UI::VCA::mouse_height*FREQ_H1_MAX*harmonic);
            }
        }
        if(1)
        { // Use an envelope (retrigger with `j`)
            a = Envelope::straight_R(Envelope::phase);
            sample = static_cast<int>(sample*a);
            Envelope::advance(&Envelope::phase, 0.2);
        }
        if(1)
        { // Noise -- mouse vary amplitude, add noise to other sounds
            a = Waveform::noise();
            sample += static_cast<int>(UI::VCA::mouse_center_dist*a*A_MAX/2);
        }
        // Little Endian (LSB at lower address)
        *wpos++ = (Uint8)(sample&0xFF);      // LSB
        *wpos++ = (Uint8)(sample>>8);        // MSB
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
TTF_Font* ttf;

namespace Notes
{ // Notes from traditional even-tempered music theory
    // Calculate 2^(i/12) for i = 0 : 12

    constexpr float _0 = 1.0;
    constexpr float _1 = 1.0594630943592953;
    constexpr float _2 = 1.122462048309373;
    constexpr float _3 = 1.189207115002721;
    constexpr float _4 = 1.2599210498948732;
    constexpr float _5 = 1.3348398541700344;
    constexpr float _6 = 1.4142135623730951;
    constexpr float _7 = 1.4983070768766815;
    constexpr float _8 = 1.5874010519681994;
    constexpr float _9 = 1.681792830507429;
    constexpr float _10 = 1.7817974362806785;
    constexpr float _11 = 1.8877486253633868;
    constexpr float _12 = 2.0;
    constexpr float _12th_root_of_2[] = {_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12};
    void mouse_to_note(int index)
    { // Move mouse to y = root note
        SDL_assert(index>=0); SDL_assert(index<=12);
        // W = (k*G) + Offset
        // This is the y-value where I want the mouse to go
        // 110Hz : mouse_h = 0.5 (GameArt::h * 0.5)
        // 220Hz : mouse_h = 1.0 (GameArt::h * 0)

        // TODO: pick the octave to operate in using the arrow keys

        // mouse_height|note
        // 0.5         | 1
        // 1.0         | 2
        // Mapping : (mouse_height-0.5)*2 + 1 = note
        //
        // So to go back the other way:
        //
        // (note - 1)/2 + 0.5 = mouse_height
        //
        // Notes      |note   | mouse_height
        // Notes::_0  |1.0    | 0.5
        // Notes::_1  |1.0595 | 0.529732
        // ...
        // Notes::_12 |2.0    | 1.0
        //
        // Get the note number using traditional music theory values from Notes
        // (Notes::_1 Notes::_2 Notes::_3 etc)
        /* UI::VCA::mouse_height = */
        /*     (GameArt::h - Mouse::yf) / */
        /*     static_cast<float>(GameArt::h); */
        // Pick the octave:
        /* float root = static_cast<float>(GameArt::h/8); */
        float root = static_cast<float>(GameArt::h/4);
        /* float root = static_cast<float>(GameArt::h/2); */
        float game_y = GameArt::h - root*_12th_root_of_2[index];
        // Transform that to the y-value in the actual window
        float win_y = GtoW::scale*game_y + GtoW::Offset::y;
        // Keep same mouse_x, just warp mouse_y
        // Still need to transform Mouse::x from Game to Win coordinates
        float win_x = GtoW::scale*Mouse::x + GtoW::Offset::x;
        SDL_WarpMouseInWindow(win, win_x, win_y);
    }
}

void shutdown(void)
{
    TTF_CloseFont(ttf);
    TTF_Quit();
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
            wI.h = GameWin::h + 200;                    // 200 : room for overlay
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
        // TODO: error-handle these SDL_Create calls
        win = SDL_CreateWindow(argv[0], wI.x, wI.y, wI.w, wI.h, wI.flags);
        ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_PRESENTVSYNC|SDL_RENDERER_ACCELERATED);
        if(argc==1)
        { // Try setting window opacity to 50% (when run with ;r<Space>)
            if( SDL_SetWindowOpacity(win, 0.5) < 0 )
            { // Not a big deal if this fails.
                if(DEBUG) printf("%d : SDL error msg: %s\n",__LINE__,SDL_GetError());
            }
        }

        ///////////
        // GAME ART
        ///////////
        if(0) SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND); // Workhorse
        if(1) SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);   // Cool lighting effect!
        GameArt::tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, GameArt::w, GameArt::h);
        if(SDL_SetTextureBlendMode(GameArt::tex, SDL_BLENDMODE_BLEND) == -1)
        { // TODO: why set tex blend mode? Makes no difference. Just ren blend mode.
            // Maybe the idea is to set the render draw blend mode to WHATEVER the texture
            // blend mode is?
            printf("line %d : SDL error msg: \"%s\" ",__LINE__, SDL_GetError());
            shutdown(); return EXIT_FAILURE;
        }
        { // Set up font
            TTF_Init();
            // TODO: load font
        }

        /////////////
        // GAME AUDIO
        /////////////

        SDL_AudioSpec wav_spec{};
        // If loading from file Sound::buf is set by file size.
        // Else, making my own sound, Sound::buf is sized for 1s of audio.
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

            /* *************Audio Format***************
             * For now, I'm going to use the same WAV spec Audacity generates.
             * Except I'll do mono (for now) to keep it simple.
             * And I'll use a much smaller wav_spec.samples because that ends up being the
             * audio device buffer size. I want a small buffer, like 2^9 samples, for low
             * latency between UI events and audio changes.
             * *******************************/

            /////////////////////
            // AUDIO DEVICE SETUP : See `wav_spec.freq` and `wav_sec.size`
            /////////////////////
            wav_spec.freq = GameAudio::SAMPLE_RATE;     // 44100 samples per second
            wav_spec.channels = 1;                      // mono
            wav_spec.silence = 0;
            wav_spec.samples = (1<<9);                  // buffer size in samples
            wav_spec.padding = 0;
            /* *************Audio Device Buffer Size***************
             * wav_spec.size : audio device buffer size in bytes
             *
             *     When audio device is almost out of data to play,
             *     it calls the callback to get more data.
             *
             *     The smaller wav_spec.samples is, the more often the callback gets called.
             *     The larger wav_spec.samples is, the more delay between UI and audio.
             *
             *     Note it's wav_spec.samples that determines this timing tradeoff.
             *     wav_spec.size is just wav_spec.samples scaled up by however many bytes it
             *     takes to represent one sample:
             *      - 16-bit audio is 2-bytes per sample
             *      - stereo audio is 2-channels per sample
             *      - so stereo 16-bit means wav_spec.size = 4*wav_spec.samples
             *      - and mono 16-bit means wav_spec.size = 2*wav_spec.samples
             *      - two different wav_spec.size values
             *      - but same latency (because wav_spec.samples is the same)
             * *******************************/
            wav_spec.size = wav_spec.samples * wav_spec.channels * GameAudio::BYTES_PER_SAMPLE;
            { // SDL_AudioFormat format: 16-bit little endian signed int
                constexpr uint8_t BITSIZE  = 8*GameAudio::BYTES_PER_SAMPLE;
                constexpr bool ISFLOAT     = false;     // int
                constexpr bool ISBIGENDIAN = false;     // little endian
                constexpr bool ISSIGNED    = true;      // signed
                wav_spec.format = BITSIZE;
                if(ISFLOAT)     wav_spec.format |= SDL_AUDIO_MASK_DATATYPE;
                if(ISBIGENDIAN) wav_spec.format |= SDL_AUDIO_MASK_ENDIAN;
                if(ISSIGNED)    wav_spec.format |= SDL_AUDIO_MASK_SIGNED;
            }
            wav_spec.userdata = NULL;                   // Nothing extra to send to callback

            ///////////////////////////////////////////////
            // MAKE SOUND BUFFER AND INITIAL BIT OF SILENCE
            ///////////////////////////////////////////////

            constexpr int SECONDS = 1;
            { // Let the buffer length be one second
                           /* [ bytes = Samples/sec   * bytes/sample     * sec ] */
                GameAudio::Sound::len = wav_spec.freq * GameAudio::BYTES_PER_SAMPLE * SECONDS;
            }
            if(DEBUG)
            { // Print Sound::buf size and audio device buffer size
                printf("--- AUDIO SETUP (line %d) ---\n", __LINE__);
                printf(" Audio \"source tape\" length: %6d bytes = %6d samples = %6f sec\n",
                        GameAudio::Sound::len,
                        GameAudio::Sound::len/GameAudio::BYTES_PER_SAMPLE,
                        (float)GameAudio::Sound::len/(wav_spec.freq * GameAudio::BYTES_PER_SAMPLE)
                      );
                printf("Audio device buffer size:   %6d bytes = %6d samples = %6f sec\n",
                        wav_spec.size,
                        wav_spec.size/GameAudio::BYTES_PER_SAMPLE,
                        (float)wav_spec.size/(wav_spec.freq * GameAudio::BYTES_PER_SAMPLE)
                        );
            }
            { // Allocate memory for buf (memory is freed during shutdown)
                GameAudio::Sound::buf = (Uint8*)malloc(GameAudio::Sound::len);
            }
            { // Start off with only enough samples to fill device buffer once.
                GameAudio::num_samples = wav_spec.size/GameAudio::BYTES_PER_SAMPLE;
            }
            Uint8* buf = GameAudio::Sound::buf;       // buf : walk Sound::buf
            { // Write silence in the initial bit of audio tape
                for(Uint32 i=0; i<GameAudio::num_samples; i++) { *buf++ = 0; *buf++ = 0; }
            }
        }
        if(AUDIO_CALLBACK)
        { // Wire callback into SDL_AudioSpec
            wav_spec.callback = GameAudio::fill_audio_dev;
        }
        SDL_AudioSpec dev_spec{};
        { // 2. Open an audio device to match WAV specs
            GameAudio::dev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &dev_spec, 0);
            if(dev_spec.size != wav_spec.size)
            {
                if(DEBUG) printf("%d : Audio device buffer size is %d bytes, "
                                 "expected %d bytes\n",
                                __LINE__, dev_spec.size, wav_spec.size
                                );
                shutdown(); return EXIT_FAILURE;
            }
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
             *  - spec.samples: 4096 (file) or 512 (me)
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
        SDL_PauseAudioDevice(GameAudio::dev, 0);        // Start device playback!
    }

    srand(0);

    bool quit = false;
    while(!quit)
    {

        /////////////////////
        // UI - EVENT HANDLER
        /////////////////////

        SDL_Keymod kmod = SDL_GetModState();            // Check for modifier keys
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
                        case SDLK_SLASH:
                            if(kmod&KMOD_SHIFT) UI::show_overlay = !UI::show_overlay;
                            break;
                        case SDLK_SPACE:
                            if(kmod&KMOD_SHIFT) UI::Flags::pressed_shift_space = true;
                            else                UI::Flags::pressed_space = true;
                            break;
                        case SDLK_j:
                            UI::Flags::pressed_j = true;
                            break;
                        case SDLK_r:
                            UI::Flags::pressed_r = true;
                            break;
                        case SDLK_1:
                            UI::Flags::pressed_1 = true;
                            break;
                        case SDLK_2:
                            UI::Flags::pressed_2 = true;
                            break;
                        case SDLK_3:
                            UI::Flags::pressed_3 = true;
                            break;
                        case SDLK_4:
                            UI::Flags::pressed_4 = true;
                            break;
                        case SDLK_5:
                            UI::Flags::pressed_5 = true;
                            break;
                        case SDLK_6:
                            UI::Flags::pressed_6 = true;
                            break;
                        case SDLK_7:
                            UI::Flags::pressed_7 = true;
                            break;
                        case SDLK_8:
                            UI::Flags::pressed_8 = true;
                            break;
                        case SDLK_9:
                            UI::Flags::pressed_9 = true;
                            break;
                        case SDLK_0:
                            UI::Flags::pressed_0 = true;
                            break;
                        case SDLK_MINUS:
                            UI::Flags::pressed_minus = true;
                            break;
                        case SDLK_EQUALS:
                            UI::Flags::pressed_equals = true;
                            break;
                        case SDLK_BACKSPACE:
                            UI::Flags::pressed_backspace = true;
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

        /////////////////
        // PHYSICS UPDATE
        /////////////////
        if(UI::Flags::mouse_moved)
        { // Update mouse x,y (Get GameArt coordinates)
            UI::Flags::mouse_moved = false;
            // Get floats for pitch
            Mouse::xf = (Mouse::motion.x - GtoW::Offset::x)/static_cast<float>(GtoW::scale);
            Mouse::yf = (Mouse::motion.y - GtoW::Offset::y)/static_cast<float>(GtoW::scale);
            // Get ints too
            Mouse::x = (Mouse::motion.x - GtoW::Offset::x)/GtoW::scale;
            Mouse::y = (Mouse::motion.y - GtoW::Offset::y)/GtoW::scale;
            // Clamp mouse x, y to game window:
            if(Mouse::y < 0) Mouse::y = 0;
            if(Mouse::x < 0) Mouse::x = 0;
            if(Mouse::yf < 0) Mouse::yf = 0;
            if(Mouse::xf < 0) Mouse::xf = 0;
            if(Mouse::y > GameArt::h) Mouse::y = GameArt::h;
            if(Mouse::x > GameArt::w) Mouse::x = GameArt::w;
            if(Mouse::yf > GameArt::h) Mouse::yf = (float)GameArt::h;
            if(Mouse::xf > GameArt::w) Mouse::xf = (float)GameArt::w;
            if(DEBUG_UI)
            {
                if(UI::Flags::mouse_xy_isfloat)
                    {printf("Mouse x,y : %.3f,%.3f\n", Mouse::xf, Mouse::yf);}
                else
                    {printf("Mouse x,y : %d,%d\n", Mouse::x, Mouse::y);}
            }
            // Use mouse distance from game art center to set VCA
            { // Set VCA amplitudes based on mouse position
                if(0)
                { // Method 1 : Abs value diff along x axis
                  // Volume only depends on distance from center in x-direction
                    int abs_diff;
                    {
                        abs_diff = Mouse::x - (GameArt::w/2);
                        if (abs_diff < 0 ) abs_diff *= -1;
                        if(DEBUG)
                        {
                            if(abs_diff < 0)
                            {
                                printf("%d : Expected abs_diff >= 0, abs_diff = %d\n",__LINE__,abs_diff);
                            }
                        }
                    }
                    UI::VCA::mouse_center_dist =
                        ((GameArt::w/2) - abs_diff) /
                        static_cast<float>(GameArt::w/2);
                }
                if(1)
                { // Method 2 : Sum of square distances from center
                  // Only silent when mouse is in the corners of the screen
                    int cx = GameArt::w/2; int cy = GameArt::h/2;
                    int max = ((cx*cx)+(cy*cy));
                    if(UI::Flags::mouse_xy_isfloat)
                    {
                        float dx = Mouse::xf - cx;
                        float dy = Mouse::yf - cy;
                        UI::VCA::mouse_center_dist =
                            (max - ((dx*dx) + (dy*dy))) /
                            static_cast<float>(max);
                        UI::VCA::mouse_height =
                            (GameArt::h - Mouse::yf) /
                            static_cast<float>(GameArt::h);
                        if(DEBUG_UI) printf("%d : VCA mouse_center : %0.3f\n",__LINE__,UI::VCA::mouse_center_dist);
                        if(DEBUG_UI) printf("%d : VCA mouse_height : %0.3f\n",__LINE__,UI::VCA::mouse_height);
                    }
                    else
                    {
                        int dx = Mouse::x - cx;
                        int dy = Mouse::y - cy;
                        UI::VCA::mouse_center_dist =
                            (max - ((dx*dx) + (dy*dy))) /
                            static_cast<float>(max);
                        UI::VCA::mouse_height =
                            (GameArt::h - Mouse::y) /
                            static_cast<float>(GameArt::h);
                        if(DEBUG_UI) printf("%d : VCA mouse_center : %0.3f\n",__LINE__,UI::VCA::mouse_center_dist);
                        if(DEBUG_UI) printf("%d : VCA mouse_height : %0.3f\n",__LINE__,UI::VCA::mouse_height);
                    }
                }
            }
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
        if(UI::Flags::pressed_space)
        { // Space is my go-to for things I want to play with temporarily
            UI::Flags::pressed_space = false;
            if (0) UI::Flags::mouse_xy_isfloat = !UI::Flags::mouse_xy_isfloat;
            if (1)
            { // Increment number of voices
                Voices::count++;
                if(Voices::count > Voices::MAX_COUNT) Voices::count = 1;
            }
        }
        if(UI::Flags::pressed_shift_space)
        { // Decrease voice count
            UI::Flags::pressed_shift_space = false;
            Voices::count--;
            if(Voices::count < 1) Voices::count = Voices::MAX_COUNT;
        }
        if(UI::Flags::pressed_r)
        { // Trigger a note
            UI::Flags::pressed_r = false;
            Envelope::enabled = false;               // Turn off envelope
            Envelope::phase = 0;                        // Start sound
            if(DEBUG) printf("freq 1st-harmonic: %0.3fHz\n",UI::VCA::mouse_height*FREQ_H1_MAX);
        }
        if(UI::Flags::pressed_j)
        { // Trigger a note
            UI::Flags::pressed_j = false;
            Envelope::enabled = true;                   // Turn on envelope
            Envelope::phase = 0;                        // Trigger envelope
            if(DEBUG) printf("freq 1st-harmonic: %0.3fHz\n",UI::VCA::mouse_height*FREQ_H1_MAX);
        }
        // Set note by warping mouse to xy
        if(UI::Flags::pressed_1)
        {
            UI::Flags::pressed_1 = false;
            Notes::mouse_to_note(0);
        }
        if(UI::Flags::pressed_2)
        {
            UI::Flags::pressed_2 = false;
            Notes::mouse_to_note(1);
        }
        if(UI::Flags::pressed_3)
        {
            UI::Flags::pressed_3 = false;
            Notes::mouse_to_note(2);
        }
        if(UI::Flags::pressed_4)
        {
            UI::Flags::pressed_4 = false;
            Notes::mouse_to_note(3);
        }
        if(UI::Flags::pressed_5)
        {
            UI::Flags::pressed_5 = false;
            Notes::mouse_to_note(4);
        }
        if(UI::Flags::pressed_6)
        {
            UI::Flags::pressed_6 = false;
            Notes::mouse_to_note(5);
        }
        if(UI::Flags::pressed_7)
        {
            UI::Flags::pressed_7 = false;
            Notes::mouse_to_note(6);
        }
        if(UI::Flags::pressed_8)
        {
            UI::Flags::pressed_8 = false;
            Notes::mouse_to_note(7);
        }
        if(UI::Flags::pressed_9)
        {
            UI::Flags::pressed_9 = false;
            Notes::mouse_to_note(8);
        }
        if(UI::Flags::pressed_0)
        {
            UI::Flags::pressed_0 = false;
            Notes::mouse_to_note(9);
        }
        if(UI::Flags::pressed_minus)
        {
            UI::Flags::pressed_minus = false;
            Notes::mouse_to_note(10);
        }
        if(UI::Flags::pressed_equals)
        {
            UI::Flags::pressed_equals = false;
            Notes::mouse_to_note(11);
        }
        if(UI::Flags::pressed_backspace)
        {
            UI::Flags::pressed_backspace = false;
            Notes::mouse_to_note(12);
        }
        // TODO: warp mouse when I press a pitch key, EVEN IF THE MOUSE IS MOVING!
        // I tried this, but it seems to make no difference.
        // I think I need to filter the pitch keys instead of polling them.
        if( UI::Flags::pressed_1 ||
            UI::Flags::pressed_2 ||
            UI::Flags::pressed_3 ||
            UI::Flags::pressed_4 ||
            UI::Flags::pressed_5 ||
            UI::Flags::pressed_6 ||
            UI::Flags::pressed_7 ||
            UI::Flags::pressed_8 ||
            UI::Flags::pressed_9 ||
            UI::Flags::pressed_0 ||
            UI::Flags::pressed_minus ||
            UI::Flags::pressed_equals ||
            UI::Flags::pressed_backspace
            ) UI::Flags::mouse_moved = true;

        /////////
        // RENDER
        /////////

        ////////////////////
        // RENDER GAME SOUND (Not used : only runs if NOT using callback)
        ////////////////////

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

        //////////////////
        // RENDER GAME ART
        //////////////////
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
            if(1)
            { // Blue box
                SDL_Color c = Colors::tardis;
                int mod = UI::VCA::mouse_height*255;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, mod);
                int w = UI::VCA::mouse_center_dist*GameArt::w;
                int h = UI::VCA::mouse_center_dist*GameArt::h;
                int x = GameArt::w/2 - w/2;
                int y = GameArt::h/2 - h/2;
                SDL_Rect r{x,y,w,h};
                SDL_RenderFillRect(ren, &r);
            }
            { // Show number of voices in use
                constexpr int size = 10; int w = size; int h = size;
                constexpr int gap = size/2;
                int x0 = 10; int y = 10;
                for (int i=0; i<Voices::MAX_COUNT; i++)
                {
                    int x = x0 + (i*(size+gap));
                    SDL_Rect r{x,y,w,h};
                    SDL_Color c = Colors::orange;
                    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
                    if(Voices::count >= (i+1)) SDL_RenderFillRect(ren, &r);
                    else                       SDL_RenderDrawRect(ren, &r);
                }
            }
        }

        ///////////////////
        // RENDER OS WINDOW
        ///////////////////
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
        if(UI::show_overlay)
        { // Show debug/help overlay
            constexpr int OVERLAY_H = 100;
            { // Darken light stuff
                SDL_Color c = Colors::coal;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a>>1); // 50% darken
                SDL_Rect rect = {.x=0, .y=0, .w=wI.w, .h=OVERLAY_H};
                SDL_RenderFillRect(ren, &rect);             // Draw filled rect
            }
            { // Lighten dark stuff
                SDL_Color c = Colors::snow;
                SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a>>3); // 12% lighten
                SDL_Rect rect = {.x=0, .y=0, .w=wI.w, .h=OVERLAY_H};
                SDL_RenderFillRect(ren, &rect);             // Draw filled rect
            }
        }
        SDL_RenderPresent(ren);
        if(DEBUG) fflush(stdout);
    }

    shutdown();
    return EXIT_SUCCESS;
}
