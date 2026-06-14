#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <cstring>

namespace gf
{

// Phase-vocoder pitch shifter with optional formant (spectral-envelope)
// preservation. At a given ratio it moves the harmonics while a moving-average
// magnitude envelope holds the formants in place, avoiding the "chipmunk"
// effect of the plain time-domain shifter. Mono per instance, ~17 ms latency at
// 44.1 kHz, allocation-free in process(). Based on the classic SMB/Bernsee
// phase vocoder, reworked onto JUCE's complex FFT with explicit gain calibration.
class FormantShifter
{
public:
    static constexpr int kOrder   = 10;
    static constexpr int kFftSize = 1 << kOrder;      // 1024
    static constexpr int kOsamp   = 4;
    static constexpr int kStep    = kFftSize / kOsamp; // 256
    static constexpr int kLatency = kFftSize - kStep;  // 768
    static constexpr int kBins    = kFftSize / 2 + 1;

    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        for (int k = 0; k < kFftSize; ++k)
            window[(size_t) k] = 0.5f * (1.0f - std::cos (2.0f * pi * (float) k / (float) kFftSize));

        // Overlap-add normalization: sum of synthesis-window^2 across the hops.
        olaNorm = 0.0f;
        for (int j = 0; j < kOsamp; ++j)
        {
            const float w = window[(size_t) ((j * kStep) % kFftSize)];
            olaNorm += w * w;
        }
        if (olaNorm < 1.0e-6f) olaNorm = 1.0f;

        // Calibrate JUCE's FFT round-trip gain (forward then inverse of an
        // impulse) so reconstruction is unity regardless of its normalization.
        std::array<juce::dsp::Complex<float>, kFftSize> a {}, b {};
        a[0] = { 1.0f, 0.0f };
        fft.perform (a.data(), b.data(), false);
        fft.perform (b.data(), a.data(), true);
        const float rt = a[0].real();
        invScale = std::abs (rt) > 1.0e-9f ? 1.0f / rt : 1.0f;

        reset();
    }

    void reset()
    {
        inFIFO.fill (0.0f); outFIFO.fill (0.0f); outputAccum.fill (0.0f);
        lastPhase.fill (0.0f); sumPhase.fill (0.0f);
        rover = kLatency;
    }

    void setRatio (float r)   { pitchShift = juce::jlimit (0.25f, 4.0f, r); }
    void setFormant (bool on) { formantOn = on; }

    void process (float* data, int numSamples)
    {
        const float freqPerBin = (float) sampleRate / (float) kFftSize;
        const float expct = 2.0f * pi * (float) kStep / (float) kFftSize;
        for (int s = 0; s < numSamples; ++s)
        {
            inFIFO[(size_t) rover] = data[s];
            data[s] = outFIFO[(size_t) (rover - kLatency)];
            if (++rover >= kFftSize)
            {
                rover = kLatency;
                processFrame (freqPerBin, expct);
            }
        }
    }

private:
    void processFrame (float freqPerBin, float expct)
    {
        // Analysis: window + forward FFT, then magnitude + true frequency per bin.
        for (int k = 0; k < kFftSize; ++k)
            fftIn[(size_t) k] = { inFIFO[(size_t) k] * window[(size_t) k], 0.0f };
        fft.perform (fftIn.data(), fftOut.data(), false);

        for (int k = 0; k < kBins; ++k)
        {
            const float re = fftOut[(size_t) k].real();
            const float im = fftOut[(size_t) k].imag();
            anaMagn[(size_t) k] = std::sqrt (re * re + im * im);
            const float phase = std::atan2 (im, re);
            float tmp = phase - lastPhase[(size_t) k];
            lastPhase[(size_t) k] = phase;
            tmp -= (float) k * expct;
            const int qpd = (int) std::round (tmp / pi);
            tmp -= pi * (float) qpd;
            tmp = (float) kOsamp * tmp / (2.0f * pi);
            anaFreq[(size_t) k] = (float) k * freqPerBin + tmp * freqPerBin;
        }

        if (formantOn) computeEnvelope();

        // Processing: move each bin to bin*ratio. With formants on, whiten by the
        // envelope first and re-apply the original envelope afterwards so the
        // formant peaks stay at their source frequencies.
        synMagn.fill (0.0f);
        synFreq.fill (0.0f);
        for (int k = 0; k < kBins; ++k)
        {
            const int index = (int) std::lround ((float) k * pitchShift);
            if (index >= 0 && index < kBins)
            {
                float m = anaMagn[(size_t) k];
                if (formantOn) m /= env[(size_t) k];
                synMagn[(size_t) index] += m;
                synFreq[(size_t) index] = anaFreq[(size_t) k] * pitchShift;
            }
        }
        if (formantOn)
            for (int k = 0; k < kBins; ++k)
                synMagn[(size_t) k] *= env[(size_t) k];

        // Synthesis: accumulate phase, rebuild the conjugate-symmetric spectrum.
        for (int k = 0; k < kBins; ++k)
        {
            const float magn = synMagn[(size_t) k];
            float tmp = synFreq[(size_t) k] - (float) k * freqPerBin;
            tmp /= freqPerBin;
            tmp = 2.0f * pi * tmp / (float) kOsamp;
            tmp += (float) k * expct;
            sumPhase[(size_t) k] += tmp;
            const float phase = sumPhase[(size_t) k];
            fftIn[(size_t) k] = { magn * std::cos (phase), magn * std::sin (phase) };
        }
        for (int k = kBins; k < kFftSize; ++k)
            fftIn[(size_t) k] = std::conj (fftIn[(size_t) (kFftSize - k)]);

        fft.perform (fftIn.data(), fftOut.data(), true);

        // Windowed overlap-add (invScale makes the FFT round-trip unity).
        for (int k = 0; k < kFftSize; ++k)
            outputAccum[(size_t) k] += window[(size_t) k] * fftOut[(size_t) k].real() * invScale;

        for (int k = 0; k < kStep; ++k)
            outFIFO[(size_t) k] = outputAccum[(size_t) k] / olaNorm;

        std::memmove (outputAccum.data(), outputAccum.data() + kStep, (size_t) (kFftSize - kStep) * sizeof (float));
        for (int k = kFftSize - kStep; k < kFftSize; ++k) outputAccum[(size_t) k] = 0.0f;
        std::memmove (inFIFO.data(), inFIFO.data() + kStep, (size_t) (kFftSize - kStep) * sizeof (float));
    }

    void computeEnvelope()
    {
        // Moving-average magnitude envelope (cheap formant estimate) via prefix sums.
        prefix[0] = 0.0f;
        for (int k = 0; k < kBins; ++k)
            prefix[(size_t) (k + 1)] = prefix[(size_t) k] + anaMagn[(size_t) k];
        constexpr int W = 12; // half-width in bins
        for (int k = 0; k < kBins; ++k)
        {
            const int lo = juce::jmax (0, k - W);
            const int hi = juce::jmin (kBins, k + W + 1);
            env[(size_t) k] = (prefix[(size_t) hi] - prefix[(size_t) lo]) / (float) (hi - lo) + 1.0e-6f;
        }
    }

    static constexpr float pi = juce::MathConstants<float>::pi;

    juce::dsp::FFT fft { kOrder };
    double sampleRate = 44100.0;
    float pitchShift = 1.0f;
    bool  formantOn = false;
    int   rover = kLatency;
    float olaNorm = 1.0f, invScale = 1.0f;

    std::array<float, kFftSize> window {}, inFIFO {}, outFIFO {}, outputAccum {};
    std::array<float, kBins> lastPhase {}, sumPhase {}, anaMagn {}, anaFreq {}, synMagn {}, synFreq {}, env {};
    std::array<float, kBins + 1> prefix {};
    std::array<juce::dsp::Complex<float>, kFftSize> fftIn {}, fftOut {};
};

} // namespace gf
