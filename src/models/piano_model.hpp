#pragma once

#include "audio/synth_engine.hpp"
#include "core/piano_types.hpp"

#include <tools/observable/single_observable.hpp>

#include <array>
#include <cstdint>
#include <random>

namespace piano {

class PianoModel {
public:
    static constexpr int kMinimumOctave              = 1;
    static constexpr int kMaximumOctave              = 7;
    static constexpr uint32_t kPlayalongEighthNoteMs = 240;
    static constexpr uint32_t kPlayalongInterludeMs  = 500;

    explicit PianoModel(SynthOutput& synth);
    PianoModel(SynthOutput& synth, uint32_t random_seed);

    smooth_ui_toolkit::SingleObservable<PianoState>& state();
    void handleInput(const PianoInputEvent& event);
    void tick(uint32_t now_ms);
    void reset();
    void releaseAll(bool immediate);

private:
    struct ActiveNote {
        VoiceHandle handle = 0;
        bool active        = false;
    };

    static constexpr std::size_t kChordVoiceCount = 3;

    struct PlayalongStep {
        PianoKey key             = PianoKey::Unknown;
        uint8_t duration_eighths = 0;
    };

    void handlePianoNote(PianoState& state, int note, bool pressed, uint32_t timestamp_ms);
    void handleChord(PianoState& state, PianoKey key, bool pressed, uint32_t timestamp_ms);
    void startChord(PianoState& state, std::size_t chord_index);
    void stopActiveChord(PianoState& state);
    void silenceNotes(PianoState& state, bool immediate);
    void startPlayalong(PianoState& state, uint32_t now_ms);
    void stopPlayalong(PianoState& state, bool preserve_success = false);
    bool advancePlayalong(PianoState& state, PianoKey key, uint32_t now_ms);
    void startDemoStep(PianoState& state, uint32_t start_ms);
    void enterGuide(PianoState& state);
    void startTimedVoices(std::array<ActiveNote, kChordVoiceCount>& voices, VoiceHandle first_handle,
                          const PianoState& state, PianoKey key, float velocity);
    void stopTimedVoices(std::array<ActiveNote, kChordVoiceCount>& voices);
    void clearTimedVoices();
    std::size_t choosePlayalongPattern(PianoMode mode, ChordTonality tonality, std::size_t pattern_count);

    SynthOutput& _synth;
    smooth_ui_toolkit::SingleObservable<PianoState> _state{PianoState{}};
    std::array<ActiveNote, PianoState::kNoteCount> _active_notes{};
    std::array<ActiveNote, kChordVoiceCount> _active_chord_voices{};
    std::array<PlayalongStep, PianoState::kMaximumPlayalongLength> _playalong_steps{};
    std::array<ActiveNote, kChordVoiceCount> _demo_voices{};
    std::array<ActiveNote, kChordVoiceCount> _assisted_voices{};
    uint32_t _playalong_note_off_at = 0;
    uint32_t _playalong_step_end_at = 0;
    uint32_t _now_ms                = 0;
    std::mt19937 _random;
    std::array<int, 3> _last_playalong_patterns{-1, -1, -1};
};

}  // namespace piano
