#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_timer.h"
#include "SDL3_mixer/SDL_mixer.h"
#include "Waveform.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>
#include <stdio.h>
#include <math.h>

const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 720;
const int CENTER_Y = WINDOW_HEIGHT / 2;

void Draw_WaveformFull(
    PitchShift::Waveform *waveform,
    int buf_len_ms,
    SDL_Renderer* renderer,
    int y_offset
);


int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    SDL_Window* window = SDL_CreateWindow("Pitch Shift", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioDeviceID audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!audioDevice) {
        printf("Failed to open audio device: %s\n", SDL_GetError());
        return 1;
    }

    MIX_Init();
    MIX_Mixer* mixer = MIX_CreateMixerDevice(audioDevice, NULL);
    if (!mixer) {
        SDL_Log("Couldn't create mixer on default device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // const char* path = "/media/data/Programming/pitch-shift-probe/C_Maj.ogg";
    const char* path = "/media/data/Programming/pitch-shift-probe/cinderella-cut.ogg";

    SDL_AudioSpec spec;
    SDL_GetAudioDeviceFormat(audioDevice, &spec, NULL);
    spec.format = SDL_AUDIO_F32;

    PitchShift::Waveform waveform;
    waveform.LoadFile(path, spec);

    MIX_Audio* audio = MIX_LoadRawAudio(mixer, waveform.samples, waveform.sample_count, &spec);
    if (!audio) {
        SDL_Log("Couldn't load raw audio: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    MIX_Track* track = MIX_CreateTrack(mixer);
    if (!track) {
        SDL_Log("Couldn't create track: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    MIX_SetTrackAudio(track, audio);

    PitchShift::Waveform stretched_waveform;
    stretched_waveform.Copy(&waveform);
    stretched_waveform.ApplyPitchShift(5);

    MIX_Audio* audio2 = MIX_LoadRawAudio(mixer, stretched_waveform.samples, stretched_waveform.sample_count, &spec);
    if (!audio2) {
        SDL_Log("Couldn't load raw audio: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    MIX_Track* track2 = MIX_CreateTrack(mixer);
    if (!track) {
        SDL_Log("Couldn't create track: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    MIX_SetTrackAudio(track2, audio2);

    bool quit = false;
    SDL_Event e;

    float speed = 300.0f;
    Uint64 last = SDL_GetTicks();

    int offset = (0.010 * spec.freq);

    while (!quit) {
        Uint64 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_J) {
                    bool playing = MIX_PlayTrack(track, 0);
                    if (!playing) {
                        SDL_Log("Couldn't play track: %s", SDL_GetError());
                        return SDL_APP_FAILURE;
                    }
                }
                else if (e.key.key == SDLK_K) {
                    bool playing = MIX_PlayTrack(track2, 0);
                    if (!playing) {
                        SDL_Log("Couldn't play track: %s", SDL_GetError());
                        return SDL_APP_FAILURE;
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255);
        // Draw_Waveform(buffer_f32, buf_len_ms,renderer, 1);

        int longest = std::max(waveform.n_frames, stretched_waveform.n_frames);
        Draw_WaveformFull(&waveform, longest, renderer, CENTER_Y / 2);
        Draw_WaveformFull(&stretched_waveform, longest, renderer, CENTER_Y / 2 + CENTER_Y);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}


void Draw_WaveformFull(
    PitchShift::Waveform *waveform,
    int buf_len_ms,
    SDL_Renderer* renderer,
    int y_offset = 0
) {
    for (int i = 0; i < WINDOW_WIDTH; i++) {
        int s_start = i * buf_len_ms / WINDOW_WIDTH;
        int s_end = (i + 1) * buf_len_ms / WINDOW_WIDTH;

        float y_max = 0.0f;
        float y_min = 0.0f;

        if (s_start < waveform->n_frames - 1) {
            for (int j = s_start; j < s_end; j++) {
                y_max = fmax(y_max, waveform->samples_f32[j * 2]);
                y_min = fmin(y_min, waveform->samples_f32[j * 2]);
            }

            if (y_max > 1.0f) {
                y_max = 1.0f;
            }
            if (y_min < -1.0f) {
                y_min = -1.0f;
            }

        }


        SDL_RenderLine(renderer,
            i, y_offset + (y_max * CENTER_Y),
            i, y_offset + (y_min * CENTER_Y)
        );
    }
}
