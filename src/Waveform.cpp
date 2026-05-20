#include "Waveform.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3_mixer/SDL_mixer.h"
#include <algorithm>
#include <kiss_fft.h>
#include <numbers>
#include <vector>

namespace PitchShift {
    float LinearInterpolation(float s1, float s2, float frac) {
        return s1 + frac * (s2 - s1);
    }

    float HermiteInterpolation(float s1, float s2, float s3, float s4, float frac) {
        float c_l_0 = s2;
        float c_l_1 = 0.5f * (s3 - s1);
        float c_l_2 = s1 - 2.5f * s2 + 2.0f * s3 - 0.5f * s4;
        float c_l_3 = 0.5f * (s4 - s2) + 1.5f * (s2 - s3);

        return ((c_l_3 * frac + c_l_2) * frac + c_l_1) * frac + c_l_0;
    }

    int Waveform::LoadFile(const char *path, SDL_AudioSpec spec) {
        MIX_AudioDecoder* audioDecoder = MIX_CreateAudioDecoder(path, 0);
        if (!audioDecoder) {
            SDL_Log("Couldn't load %s: %s", path, SDL_GetError());
            return SDL_APP_FAILURE;
        }

        int is_sample_int = SDL_AUDIO_ISINT(spec.format);
        int sample_size = SDL_AUDIO_BYTESIZE(spec.format);

        this->spec = spec;
        this->channels = spec.channels;
        this->sample_size = SDL_AUDIO_BYTESIZE(spec.format);

        this->n_frames =  (10 * spec.freq);
        this->sample_count = this->n_frames * spec.channels * sample_size;

        this->samples = malloc(sample_count);
        int n_decoded = MIX_DecodeAudio(audioDecoder, this->samples, sample_count, &spec);
        if (n_decoded < 0) {
            SDL_Log("Couldn't decode audio: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        this->samples_f32 = (float*)this->samples;
        this->samples_f32_count = n_frames * channels;

        return 0;
    }

    int Waveform::Copy(const Waveform *waveform) {
        this->spec = waveform->spec;
        this->n_frames = waveform->n_frames;
        this->sample_count = waveform->sample_count;
        this->sample_size = waveform->sample_size;
        this->channels = waveform->channels;
        this->samples = malloc(waveform->sample_count);
        memcpy(this->samples, waveform->samples, waveform->sample_count);

        this->samples_f32 = (float*)this->samples;
        this->samples_f32_count = n_frames * channels;

        return 0;
    }

    void Waveform::HarmonicPercussivePitchShift(int semitones) {
        // A Review of Time-Scale Modification of Music Signals by Jonathan Driedger and Meinard Müller
        // https://www.audiolabs-erlangen.de/resources/MIR/FMP/C8/C8S1_HPS.html
        // https://dafx10.iem.at/papers/DerryFitzGerald_DAFx10_P15.pdf

        int nfft = 1024;
        int n_overlap = 4;
        int analysis_hop_size = nfft / n_overlap;

        size_t n_frames = this->n_frames / analysis_hop_size;

        kiss_fft_cfg fft_cfg = kiss_fft_alloc(nfft, 0, 0, 0);
        std::vector<std::vector<kiss_fft_cpx>> stft_l(n_frames, std::vector<kiss_fft_cpx>(nfft, { .r = 0, .i = 0 }));
        std::vector<std::vector<kiss_fft_cpx>> stft_r(n_frames, std::vector<kiss_fft_cpx>(nfft, { .r = 0, .i = 0 }));

        std::vector<kiss_fft_cpx> samples_l (nfft);
        std::vector<kiss_fft_cpx> samples_r (nfft);
        for (size_t f = 0; f < n_frames; f++) {
            for (size_t n = 0; n < nfft; n++) {
                size_t index = f * analysis_hop_size + n;

                samples_l[n].r = this->samples_f32[index * 2];
                samples_l[n].i = 0;
                samples_r[n].r = this->samples_f32[index * 2 + 1];
                samples_r[n].i = 0;
            }
            kiss_fft(fft_cfg, samples_l.data(), stft_l[f].data());
            kiss_fft(fft_cfg, samples_r.data(), stft_r[f].data());
        }

        // Power spectrogram
        std::vector<std::vector<float>> spectrogram_l;
        std::vector<std::vector<float>> spectrogram_r;
        for (size_t f = 0; f < n_frames; f++) {
            for (size_t n = 0; n < nfft; n++) {
                spectrogram_l[f][n] = std::powf(stft_l[f][n].r, 2);
                spectrogram_r[f][n] = std::powf(stft_r[f][n].r, 2);
            }
        }

        // Find median & build mask
        int h_kernel_size = 5;
        int p_kernel_size = 5;
        std::vector<float> h_kernel (h_kernel_size);
        std::vector<float> p_kernel (p_kernel_size);

        std::vector<std::vector<float>> h_mask_l (n_frames);
        std::vector<std::vector<float>> p_mask_l (n_frames);
        // std::vector<std::vector<float>> mask_r;
        for (size_t f = 0; f < n_frames; f++) {
            for (size_t n = 0; n < nfft; n++) {
                for (size_t next = 0; next < p_kernel_size; next++) {
                    if (f + next >= n_frames)  {
                        h_kernel[next] = 0;
                    } else {
                        h_kernel[next] = spectrogram_l[f + next][n];
                    }

                    if (n + next >= nfft)  {
                        p_kernel[next] = 0;
                    } else {
                        p_kernel[next] = spectrogram_l[f][n + next];
                    }
                }
                std::sort(h_kernel.begin(), h_kernel.end());
                std::sort(p_kernel.begin(), p_kernel.end());

                float p_median = p_kernel[2];
                float h_median = h_kernel[2];

                h_mask_l[f][n] = (h_median + (1e-10f / 2)) / (h_median + p_median + 1e-10f);
                p_mask_l[f][n] = (p_median + (1e-10f / 2)) / (p_median + h_median + 1e-10f);
            }
        }

        kiss_fft_free(fft_cfg);
    }

    void Waveform::PhaseVocoderPitchShift(int semitones) {
        // https://www.guitarpitchshifter.com/algorithm.html
        // https://www.youtube.com/watch?v=PjKlMXhxtTM
        // https://www.dafx.de/paper-archive/2000/pdf/Bernardini.pdf - Provides code
        float scaling_factor = std::powf(2.0f, ((float)semitones / 12.0f));
        Uint64 target_stretch = (float)this->n_frames * scaling_factor;

        size_t buffer_size = target_stretch * spec.channels * sample_size;
        float* analysis_buffer_f32 = (float*)malloc(buffer_size);
        for (size_t i = 0; i < buffer_size / sizeof(float); i++) {
            analysis_buffer_f32[i] = 0.0f;
        }

        int nfft = 1024;
        int analysis_frame_size = nfft;
        int analysis_hop_size = nfft / 4;
        int synthesis_frame_size = nfft;
        int synthesis_hop_size = std::round(analysis_hop_size * scaling_factor);

        kiss_fft_cfg fft_config = kiss_fft_alloc(nfft, 0, 0, 0);

        kiss_fft_cpx *fft_in_l = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft);
        kiss_fft_cpx *fft_in_r = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft);
        kiss_fft_cpx *fft_out_l = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft);
        kiss_fft_cpx *fft_out_r = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft);

        float hann_window[analysis_frame_size];
        for (int w = 0; w < analysis_frame_size; w++) {
            // hann_window[w] = 0.5f * (1.0f - std::cosf(2.0f * std::numbers::pi_v<float> * (w + 0.5f) / analysis_frame_size));
            hann_window[w] = 0.5f * (1.0f - std::cosf(2.0f * std::numbers::pi_v<float> * w / (analysis_frame_size - 1)));
        }

        // int pad = nfft * 1/4;
        int pad = 0;
        Uint64 n_fft_frame = (Uint64)std::round(((float)this->n_frames / (float)analysis_hop_size));
        kiss_fft_cpx** analysis_fft_out_l = (kiss_fft_cpx**)malloc(n_fft_frame * sizeof(kiss_fft_cpx*));
        kiss_fft_cpx** analysis_fft_out_r = (kiss_fft_cpx**)malloc(n_fft_frame * sizeof(kiss_fft_cpx*));

        for (int i = 0; i < n_fft_frame; i++) {
            analysis_fft_out_l[i] = (kiss_fft_cpx*)calloc(nfft, sizeof(kiss_fft_cpx));
            analysis_fft_out_r[i] = (kiss_fft_cpx*)calloc(nfft, sizeof(kiss_fft_cpx));
        }

        // analysis windowing
        for (int i = 0; i < n_fft_frame; i++) {
            for (int w = 0; w < nfft; w++) {
                fft_in_l[w].r = 0.0f;
                fft_in_l[w].i = 0.0f;
                fft_in_r[w].r = 0.0f;
                fft_in_r[w].i = 0.0f;
            }

            for (int n = 0; n < analysis_frame_size; n++) {
                int index = (i * analysis_hop_size + n) * 2;

                if (index < this->samples_f32_count) {
                    fft_in_l[n + pad].r = this->samples_f32[index] * hann_window[n];
                    fft_in_r[n + pad].r = this->samples_f32[index + 1] * hann_window[n];
                }
            }

            kiss_fft(fft_config, fft_in_l, fft_out_l);
            kiss_fft(fft_config, fft_in_r, fft_out_r);

            for (int n = 0; n < nfft; n++) {
                analysis_fft_out_l[i][n] = fft_out_l[n];
                analysis_fft_out_r[i][n] = fft_out_r[n];
            }
        }

        // Processing
        kiss_fft_cpx** new_complex_fft_l = (kiss_fft_cpx**)malloc(n_fft_frame * sizeof(kiss_fft_cpx*));
        kiss_fft_cpx** new_complex_fft_r = (kiss_fft_cpx**)malloc(n_fft_frame * sizeof(kiss_fft_cpx*));
        float** prev_new_phases_l = (float**) malloc(n_fft_frame * sizeof(float*));
        float** prev_new_phases_r = (float**) malloc(n_fft_frame * sizeof(float*));

        for (int i = 0; i < n_fft_frame; i++) {
            new_complex_fft_l[i] = (kiss_fft_cpx*)calloc(nfft, sizeof(kiss_fft_cpx));
            new_complex_fft_r[i] = (kiss_fft_cpx*)calloc(nfft, sizeof(kiss_fft_cpx));
            prev_new_phases_l[i] = (float*) calloc(nfft, sizeof(float));
            prev_new_phases_r[i] = (float*) calloc(nfft, sizeof(float));
        }

        for (int w = 0; w < n_fft_frame; w++) {
            for (int k = 0; k < nfft; k++) {
                float prev_phase_l = 0.0f;
                float prev_new_phase_l = 0.0f;

                float prev_phase_r = 0.0f;
                float prev_new_phase_r = 0.0f;
                if (w > 0)  {
                    prev_new_phase_l = prev_new_phases_l[w - 1][k];
                    prev_phase_l = std::atan2f(analysis_fft_out_l[w - 1][k].i, analysis_fft_out_l[w - 1][k].r);

                    prev_new_phase_r = prev_new_phases_r[w - 1][k];
                    prev_phase_r = std::atan2f(analysis_fft_out_r[w - 1][k].i, analysis_fft_out_r[w - 1][k].r);
                }

                float real_l = analysis_fft_out_l[w][k].r;
                float imag_l = analysis_fft_out_l[w][k].i;

                float real_r = analysis_fft_out_r[w][k].r;
                float imag_r = analysis_fft_out_r[w][k].i;

                float magnitude_l = std::sqrtf(real_l * real_l + imag_l * imag_l);
                float phase_l = std::atan2f(imag_l, real_l);

                float magnitude_r = std::sqrtf(real_r * real_r + imag_r * imag_r);
                float phase_r = std::atan2f(imag_r, real_r);

                float center_angular = 2 * std::numbers::pi_v<float> * k / nfft;
                float delta_phase_l = (phase_l - prev_phase_l);
                float delta_phase_r = (phase_r - prev_phase_r);

                float delta_omega_k_l = delta_phase_l - analysis_hop_size * center_angular;
                float delta_omega_k_r = delta_phase_r - analysis_hop_size * center_angular;

                // float delta_omega_wrapped_l = std::fmod(delta_omega_k_l + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float> + std::numbers::pi_v<float>) - std::numbers::pi_v<float>;
                // float delta_omega_wrapped_r = std::fmod(delta_omega_k_r + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float> + std::numbers::pi_v<float>) - std::numbers::pi_v<float>;

                float delta_omega_wrapped_l = std::remainderf(delta_omega_k_l, 2.0f * std::numbers::pi_v<float>);
                float delta_omega_wrapped_r = std::remainderf(delta_omega_k_r, 2.0f * std::numbers::pi_v<float>);

                float omega_true_k_l = center_angular + delta_omega_wrapped_l / analysis_hop_size;
                float omega_true_k_r = center_angular + delta_omega_wrapped_r / analysis_hop_size;

                float new_phase_l = prev_new_phase_l + synthesis_hop_size * omega_true_k_l;
                float new_phase_r = prev_new_phase_r + synthesis_hop_size * omega_true_k_r;

                new_complex_fft_l[w][k].r = magnitude_l * std::cosf(new_phase_l);
                new_complex_fft_l[w][k].i = magnitude_l * std::sinf(new_phase_l);

                new_complex_fft_r[w][k].r = magnitude_r * std::cosf(new_phase_r);
                new_complex_fft_r[w][k].i = magnitude_r * std::sinf(new_phase_r);

                prev_new_phases_l[w][k] = new_phase_l;
                prev_new_phases_r[w][k] = new_phase_r;
            }
        }

        // Synthesis
        kiss_fft_cfg ifft_config = kiss_fft_alloc(nfft, 1, 0, 0);
        kiss_fft_cpx *ifft_out_l = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft);
        kiss_fft_cpx *ifft_out_r = (kiss_fft_cpx*)malloc(sizeof(kiss_fft_cpx) * nfft);
        float inv_n = 1.0f / nfft;
        for (int i = 0; i < n_fft_frame; i++) {
            kiss_fft(ifft_config, new_complex_fft_l[i], ifft_out_l);
            kiss_fft(ifft_config, new_complex_fft_r[i], ifft_out_r);

            for (int n = 0; n < synthesis_frame_size; n++) {
                int index = (i * synthesis_hop_size + n) * 2;
                if (index < target_stretch * 2) {
                    analysis_buffer_f32[index] += ifft_out_l[pad + n].r *  inv_n * hann_window[n];
                    analysis_buffer_f32[index + 1] += ifft_out_r[pad + n].r *  inv_n * hann_window[n];
                }
            }
        }

        // Resampling
        for (size_t i = 0; i < this->n_frames; i++) {
            float src_pos = i * scaling_factor;
            size_t idx1 = static_cast<size_t>(src_pos);
            size_t idx2 = idx1 + 1;

            float frac = src_pos - std::floor(idx1);

            float l1 = (idx1 - 1 > 0 && idx1 - 1 < target_stretch) ? analysis_buffer_f32[(idx1 - 1) * 2] : 0.0f;
            float l2 = (idx1     < target_stretch) ? analysis_buffer_f32[idx1     * 2] : 0.0f;
            float l3 = (idx1 + 1 < target_stretch) ? analysis_buffer_f32[(idx1 + 1) * 2] : 0.0f;
            float l4 = (idx1 + 2 < target_stretch) ? analysis_buffer_f32[(idx1 + 2) * 2] : 0.0f;

            float r1 = (idx1 - 1 > 0 && idx1 - 1 < target_stretch) ? analysis_buffer_f32[(idx1 - 1) * 2 + 1] : 0.0f;
            float r2 = (idx1     < target_stretch) ? analysis_buffer_f32[idx1     * 2 + 1] : 0.0f;
            float r3 = (idx1 + 1 < target_stretch) ? analysis_buffer_f32[(idx1 + 1) * 2 + 1] : 0.0f;
            float r4 = (idx1 + 2 < target_stretch) ? analysis_buffer_f32[(idx1 + 2) * 2 + 1] : 0.0f;

            this->samples_f32[i * 2] = HermiteInterpolation(l1, l2, l3, l4, frac);
            this->samples_f32[i * 2 + 1] = HermiteInterpolation(r1, r2, r3, r4, frac);

            // this->samples_f32[i * 2] = LinearInterpolation(l2, l3, frac);
            // this->samples_f32[i * 2 + 1] = LinearInterpolation(r2, r3, frac);
        }

        free(analysis_buffer_f32);
        free(fft_in_l);
        free(fft_in_r);
        free(fft_out_l);
        free(fft_out_r);
        for (int i = 0; i < n_fft_frame; i++) {
            free(analysis_fft_out_l[i]);
            free(analysis_fft_out_r[i]);
            free(new_complex_fft_l[i]);
            free(new_complex_fft_r[i]);
            free(prev_new_phases_l[i]);
            free(prev_new_phases_r[i]);
        }
        free(analysis_fft_out_l);
        free(analysis_fft_out_r);
        free(new_complex_fft_l);
        free(new_complex_fft_r);
        free(prev_new_phases_l);
        free(prev_new_phases_r);
        free(ifft_out_l);
        free(ifft_out_r);

        kiss_fft_free(fft_config);
        kiss_fft_free(ifft_config);
    }

    int Waveform::GenerateSinewave(Uint32 sample_rate, Uint32 frequency) {
        return 0;
    }

    Waveform::~Waveform() {
        free(this->samples);
    }

}
