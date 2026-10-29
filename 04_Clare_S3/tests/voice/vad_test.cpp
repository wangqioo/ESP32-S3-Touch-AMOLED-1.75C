#include "clare_vad.h"
#include <cassert>
#include <cstdio>
static int16_t pcm[320];
static ClareVad::Event frame(ClareVad &vad, int amplitude) {
    for (int i = 0; i < 320; ++i) pcm[i] = (i & 1) ? amplitude : -amplitude;
    return vad.process(pcm, 320);
}
int main() {
    ClareVad vad;
    for (int i = 0; i < 500; ++i) assert(frame(vad, 70) == ClareVad::Event::None);
    for (int i = 0; i < 4; ++i) assert(frame(vad, 3000) == ClareVad::Event::None);
    for (int i = 0; i < 100; ++i) assert(frame(vad, 60) == ClareVad::Event::None);
    for (int i = 0; i < 7; ++i) assert(frame(vad, 1200) == ClareVad::Event::None);
    assert(frame(vad, 1200) == ClareVad::Event::SpeechStarted);
    // Natural sub-second pauses inside a question do not send it.
    for (int i = 0; i < 50; ++i) assert(frame(vad, 60) == ClareVad::Event::None);
    assert(frame(vad, 1200) == ClareVad::Event::None);
    for (int i = 0; i < 59; ++i) assert(frame(vad, 60) == ClareVad::Event::None);
    assert(frame(vad, 60) == ClareVad::Event::EndOfSpeech);
    for (int i = 0; i < 200; ++i) assert(frame(vad, 1200) == ClareVad::Event::None);
    vad.reset();
    for (int i = 0; i < 7; ++i) assert(frame(vad, 1500) == ClareVad::Event::None);
    assert(frame(vad, 1500) == ClareVad::Event::SpeechStarted);
    int ends = 0;
    for (int i = 0; i < 1600; ++i) ends += frame(vad, 1500) == ClareVad::Event::EndOfSpeech;
    assert(ends == 1); // Bounded long monologue, only one send per turn.
    vad.reset();
    for (auto &sample : pcm) sample = 8000; // DC offset is not speech.
    for (int i = 0; i < 100; ++i) assert(vad.process(pcm, 320) == ClareVad::Event::None);
    puts("VAD silence, short noise, pauses, endpoint, next turn, long speech and DC: PASS");
}
