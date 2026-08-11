#include "audio/soundfont_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#ifndef PIANO_TEST_SOUNDFONT_PATH
#error "PIANO_TEST_SOUNDFONT_PATH must point to a test SoundFont"
#endif

namespace {

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << message << '\n';
    std::exit(1);
}

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        fail(message);
    }
}

void renderFrames(piano::SoundFontRenderer& synth, std::size_t frame_count, std::vector<float>& output)
{
    output.resize(frame_count * 2);
    synth.render(output.data(), frame_count, 2);
}

void renderUntilSilent(piano::SoundFontRenderer& synth, std::vector<float>& output, int maximum_blocks)
{
    for (int block = 0; block < maximum_blocks && synth.activeVoiceCount() > 0; ++block) {
        renderFrames(synth, 512, output);
    }
}

void testLoading()
{
    piano::SoundFontRenderer synth;
    expect(!synth.load("/path/that/does/not/exist.sf2", 48000), "a missing SoundFont must fail to load");
    expect(!synth.ready(), "a failed load must leave the renderer unavailable");
    expect(synth.load(PIANO_TEST_SOUNDFONT_PATH, 48000), "the packaged SoundFont must load");
    expect(synth.ready(), "a successful load must make the renderer ready");
    expect(!synth.presetName().empty(), "the piano preset must have a name");
}

void testSilenceAndSingleNote()
{
    piano::SoundFontRenderer synth;
    expect(synth.load(PIANO_TEST_SOUNDFONT_PATH, 48000), "the packaged SoundFont must load");

    std::vector<float> output;
    renderFrames(synth, 512, output);
    expect(std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.0f; }),
           "an idle renderer must produce silence");

    synth.noteOn(1, 60, 0.8f);
    renderFrames(synth, 2048, output);
    float peak = 0.0f;
    for (float sample : output) {
        expect(std::isfinite(sample), "rendered samples must remain finite");
        expect(std::abs(sample) <= 1.0f, "rendered samples must remain in the output range");
        peak = std::max(peak, std::abs(sample));
    }
    expect(peak > 0.001f, "a note-on must produce audible output");
}

void testIndependentSamePitchHandles()
{
    piano::SoundFontRenderer synth;
    expect(synth.load(PIANO_TEST_SOUNDFONT_PATH, 48000), "the packaged SoundFont must load");

    std::vector<float> output;
    synth.noteOn(1, 60, 0.8f);
    synth.noteOn(2, 60, 0.8f);
    renderFrames(synth, 512, output);
    const std::size_t layered_voice_count = synth.activeVoiceCount();
    expect(layered_voice_count >= 2, "separate handles at the same pitch must start separate voices");

    synth.noteOff(1);
    for (int block = 0; block < 375; ++block) {
        renderFrames(synth, 256, output);
    }
    expect(synth.activeVoiceCount() > 0, "releasing one handle must leave the other handle sounding");

    synth.noteOff(2);
    renderUntilSilent(synth, output, 2000);
    expect(synth.activeVoiceCount() == 0, "both released handles must eventually become silent");
}

void testFullKeyboardChordDoesNotClip()
{
    piano::SoundFontRenderer synth;
    expect(synth.load(PIANO_TEST_SOUNDFONT_PATH, 48000), "the packaged SoundFont must load");

    for (int note = 60; note <= 72; ++note) {
        synth.noteOn(static_cast<piano::VoiceHandle>(note), note, 0.78f);
    }

    std::vector<float> output;
    renderFrames(synth, 48000, output);
    const float peak = std::accumulate(output.begin(), output.end(), 0.0f,
                                       [](float current, float sample) { return std::max(current, std::abs(sample)); });
    expect(peak < 1.0f, "the full keyboard chord must retain headroom instead of hitting the hard clamp (peak=" +
                            std::to_string(peak) + ")");
}

void testReleaseAndPanic()
{
    piano::SoundFontRenderer synth;
    expect(synth.load(PIANO_TEST_SOUNDFONT_PATH, 48000), "the packaged SoundFont must load");
    std::vector<float> output;

    synth.noteOn(7, 69, 0.8f);
    renderFrames(synth, 512, output);
    synth.noteOff(7);
    renderUntilSilent(synth, output, 2000);
    expect(synth.activeVoiceCount() == 0, "a released note must eventually become silent");

    synth.noteOn(8, 64, 0.8f);
    synth.noteOn(9, 67, 0.8f);
    synth.allNotesOff(true);
    renderUntilSilent(synth, output, 8);
    expect(synth.activeVoiceCount() == 0, "panic must silence every voice within the fast release window");
}

}  // namespace

int main()
{
    testLoading();
    testSilenceAndSingleNote();
    testIndependentSamePitchHandles();
    testFullKeyboardChordDoesNotClip();
    testReleaseAndPanic();
    return 0;
}
