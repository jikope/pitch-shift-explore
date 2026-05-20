#ifndef WAVEFORM_H
#define WAVEFORM_H


#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_stdinc.h"
namespace PitchShift {
    class Waveform {
    public:
        ~Waveform();
        int LoadFile(const char *path, SDL_AudioSpec spec);
        int Copy(const Waveform *waveform);
        int GenerateSinewave(Uint32 sample_rate, Uint32 frequency);

        void HarmonicPercussivePitchShift(int semitones);
        void PhaseVocoderPitchShift(int semitones);

        void* samples;
        Uint64 sample_count;
        float* samples_f32;
        Uint64 samples_f32_count;

        SDL_AudioSpec spec;
        Uint64 n_frames;
        Uint16 channels;
        Uint16 sample_size;
    };
}

#endif // WAVEFORM_H
