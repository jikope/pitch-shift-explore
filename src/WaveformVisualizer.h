#ifndef WAVEFORM_VISUALIZER_H
#define WAVEFORM_VISUALIZER_H

#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_stdinc.h"
#include "Waveform.h"
#include <vector>

namespace PitchShift {
    class WaveformVisualizer {
        public:
        WaveformVisualizer(Uint32 width, Uint32 height, Uint32 y_pos);
        void LoadWaveform(const Waveform *waveform);
        void SetRenderedWaveform(size_t frame_start, size_t frame_end);
        void SetRenderedWaveformFull();
        void DrawWaveform(SDL_Renderer *renderer);

        private:
        Uint32 width;
        Uint32 height;
        Uint32 y_pos;
        float zoom_level;
        size_t scroll_offset;

        const Waveform* waveform;

        std::vector<SDL_FPoint> lines;
    };
}

#endif // WAVEFORM_VISUALIZER_H
