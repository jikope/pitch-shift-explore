#include "WaveformVisualizer.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_stdinc.h"
#include <cmath>
#include <cstdio>

namespace PitchShift {
    void WaveformVisualizer::LoadWaveform(const Waveform *waveform) {
        this->waveform = waveform;
    }

    void WaveformVisualizer::SetRenderedWaveform(size_t frame_start, size_t frame_end) {
        size_t width = this->width;
        size_t center_y = this->height / 2;
        size_t y_pos = this->y_pos;
        size_t buf_len_ms = frame_end - frame_start;

        if (frame_end > this->waveform->n_frames) {
            return;
        }
        this->lines.resize(0);

        for (int i = 0; i < this->width; i++) {
            int s_start = i * buf_len_ms / width;
            int s_end = (i + 1) * buf_len_ms / width;

            float y_max = 0.0f;
            float y_min = 0.0f;

            if (s_start < waveform->n_frames - 1) {
                for (int j = s_start + frame_start; j < s_end + frame_start; j++) {
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

            lines.push_back(SDL_FPoint { .x = (float)i, .y = y_pos + (y_max * center_y) });
            lines.push_back(SDL_FPoint { .x = (float)i, .y = y_pos + (y_min * center_y) });
        }
    }

    void WaveformVisualizer::SetRenderedWaveformFull() {
        this->SetRenderedWaveform(0, this->waveform->n_frames);
    }

    void WaveformVisualizer::DrawWaveform(SDL_Renderer *renderer) {
        SDL_RenderLines(renderer, this->lines.data(), this->lines.size());
    }

    WaveformVisualizer::WaveformVisualizer(Uint32 width, Uint32 height, Uint32 y_pos) {
        this->width = width;
        this->height = height;
        this->y_pos = y_pos;

        this->zoom_level = 1.0f;
        this->scroll_offset = 0;
    }
}
