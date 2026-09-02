#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace piano {

enum class PianoKey {
    C,
    CSharp,
    D,
    DSharp,
    E,
    F,
    FSharp,
    G,
    GSharp,
    A,
    ASharp,
    B,
    HighC,
    OctaveDown,
    OctaveUp,
    ToggleKeymap,
    ToggleMode,
    ToggleTonality,
    TogglePlayalong,
    Help,
    Escape,
    Unknown,
};

enum class PianoMode {
    Piano,
    Chord,
};

enum class ChordTonality {
    Major,
    Minor,
};

enum class PlayalongPhase {
    Inactive,
    Demo,
    Interlude,
    Guide,
};

enum class PianoInputEventType {
    Key,
    AllReleased,
};

struct PianoInputEvent {
    PianoInputEventType type = PianoInputEventType::Key;
    PianoKey key             = PianoKey::Unknown;
    bool pressed             = false;
    bool repeated            = false;
    uint32_t timestamp_ms    = 0;
};

struct PianoState {
    static constexpr std::size_t kNoteCount              = 13;
    static constexpr std::size_t kChordCount             = 8;
    static constexpr std::size_t kMaximumPlayalongLength = 48;

    std::array<bool, kNoteCount> pressed_notes{};
    int octave                          = 4;
    bool keymap_visible                 = true;
    bool octave_down_pressed            = false;
    bool octave_up_pressed              = false;
    PianoMode mode                      = PianoMode::Piano;
    ChordTonality tonality              = ChordTonality::Major;
    int tonic                           = 0;
    int active_chord_index              = -1;
    bool mode_toggle_pressed            = false;
    bool tonality_toggle_pressed        = false;
    bool playalong_toggle_pressed       = false;
    bool playalong_active               = false;
    PlayalongPhase playalong_phase      = PlayalongPhase::Inactive;
    int playalong_demo_target           = -1;
    int playalong_target                = -1;
    int playalong_success_target        = -1;
    std::size_t playalong_step          = 0;
    std::size_t playalong_length        = 0;
    uint8_t playalong_duration_eighths  = 0;
    uint32_t playalong_success_revision = 0;
};

int noteIndex(PianoKey key);
int chordIndex(PianoKey key);
std::string keyName(int tonic, ChordTonality tonality);
std::string chordLabel(int tonic, ChordTonality tonality, std::size_t chord_index);
std::string chordDegreeLabel(ChordTonality tonality, std::size_t chord_index);
std::array<std::string, PianoState::kChordCount> chordLabels(int tonic, ChordTonality tonality);

}  // namespace piano
