#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>

// Local endpoint detection on 20ms, 16kHz PCM frames, before digital gain.
// Never sends silence alone; the cloud remains responsible for recognition.
class ClareVad {
public:
    enum class Event { None, SpeechStarted, EndOfSpeech };
    void reset() { noise_ = 80; voiced_ms_ = quiet_ms_ = elapsed_ms_ = 0; speech_ = sent_ = false; }
    Event process(const int16_t *pcm, size_t count) {
        if (!count || sent_) return Event::None;
        double mean = 0, power = 0;
        for (size_t i = 0; i < count; ++i) mean += pcm[i];
        mean /= count;
        for (size_t i = 0; i < count; ++i) {
            const double sample = pcm[i] - mean;
            power += sample * sample;
        }
        const float rms = std::sqrt(power / count);
        const unsigned ms = static_cast<unsigned>(count * 1000 / 16000);
        const float threshold = noise_ * 3.0f > 240.0f ? noise_ * 3.0f : 240.0f;
        const bool voice = rms > threshold;
        if (!speech_) {
            if (!voice) {
                noise_ += (rms - noise_) * 0.025f;
                voiced_ms_ = 0;
            } else {
                voiced_ms_ += ms;
                if (voiced_ms_ >= 160) {
                    speech_ = true;
                    elapsed_ms_ = voiced_ms_;
                    return Event::SpeechStarted;
                }
            }
            return Event::None;
        }
        elapsed_ms_ += ms;
        quiet_ms_ = voice ? 0 : quiet_ms_ + ms;
        if (quiet_ms_ >= 1200 || elapsed_ms_ >= 30000) {
            sent_ = true;
            return Event::EndOfSpeech;
        }
        return Event::None;
    }
private:
    float noise_ = 80;
    unsigned voiced_ms_ = 0, quiet_ms_ = 0, elapsed_ms_ = 0;
    bool speech_ = false, sent_ = false;
};
