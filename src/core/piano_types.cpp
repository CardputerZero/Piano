#include "core/piano_types.hpp"

#include <array>

namespace piano {
namespace {

constexpr std::array<PianoKey, PianoState::kChordCount> kChordKeys = {
    PianoKey::C, PianoKey::D, PianoKey::E, PianoKey::F, PianoKey::G, PianoKey::A, PianoKey::B, PianoKey::HighC,
};

constexpr std::array<const char*, PianoState::kChordCount> kMajorSuffixes = {"", "m", "m", "", "", "m", "dim", ""};
constexpr std::array<const char*, PianoState::kChordCount> kMinorSuffixes = {"m", "dim", "", "m", "", "", "dim", "m"};
constexpr std::array<const char*, PianoState::kChordCount> kMajorDegrees  = {"I", "ii", "iii", "IV",
                                                                             "V", "vi", "vii", "I"};
constexpr std::array<const char*, PianoState::kChordCount> kMinorDegrees  = {"i", "ii", "III", "iv",
                                                                             "V", "VI", "vii", "i"};

using ScaleNames = std::array<const char*, 7>;

constexpr std::array<ScaleNames, 12> kMajorScaleNames = {
    ScaleNames{"C", "D", "E", "F", "G", "A", "B"},       ScaleNames{"Db", "Eb", "F", "Gb", "Ab", "Bb", "C"},
    ScaleNames{"D", "E", "F#", "G", "A", "B", "C#"},     ScaleNames{"Eb", "F", "G", "Ab", "Bb", "C", "D"},
    ScaleNames{"E", "F#", "G#", "A", "B", "C#", "D#"},   ScaleNames{"F", "G", "A", "Bb", "C", "D", "E"},
    ScaleNames{"F#", "G#", "A#", "B", "C#", "D#", "E#"}, ScaleNames{"G", "A", "B", "C", "D", "E", "F#"},
    ScaleNames{"Ab", "Bb", "C", "Db", "Eb", "F", "G"},   ScaleNames{"A", "B", "C#", "D", "E", "F#", "G#"},
    ScaleNames{"Bb", "C", "D", "Eb", "F", "G", "A"},     ScaleNames{"B", "C#", "D#", "E", "F#", "G#", "A#"},
};

constexpr std::array<ScaleNames, 12> kMinorScaleNames = {
    ScaleNames{"C", "D", "Eb", "F", "G", "Ab", "B"},    ScaleNames{"C#", "D#", "E", "F#", "G#", "A", "B#"},
    ScaleNames{"D", "E", "F", "G", "A", "Bb", "C#"},    ScaleNames{"Eb", "F", "Gb", "Ab", "Bb", "Cb", "D"},
    ScaleNames{"E", "F#", "G", "A", "B", "C", "D#"},    ScaleNames{"F", "G", "Ab", "Bb", "C", "Db", "E"},
    ScaleNames{"F#", "G#", "A", "B", "C#", "D", "E#"},  ScaleNames{"G", "A", "Bb", "C", "D", "Eb", "F#"},
    ScaleNames{"G#", "A#", "B", "C#", "D#", "E", "Fx"}, ScaleNames{"A", "B", "C", "D", "E", "F", "G#"},
    ScaleNames{"Bb", "C", "Db", "Eb", "F", "Gb", "A"},  ScaleNames{"B", "C#", "D", "E", "F#", "G", "A#"},
};

int normalizedPitchClass(int pitch_class)
{
    const int remainder = pitch_class % 12;
    return remainder < 0 ? remainder + 12 : remainder;
}

}  // namespace

int noteIndex(PianoKey key)
{
    const int index = static_cast<int>(key) - static_cast<int>(PianoKey::C);
    return index >= 0 && index < static_cast<int>(PianoState::kNoteCount) ? index : -1;
}

int chordIndex(PianoKey key)
{
    for (std::size_t index = 0; index < kChordKeys.size(); ++index) {
        if (kChordKeys[index] == key) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

std::string keyName(int tonic, ChordTonality tonality)
{
    const auto& scales = tonality == ChordTonality::Major ? kMajorScaleNames : kMinorScaleNames;
    return scales[static_cast<std::size_t>(normalizedPitchClass(tonic))][0];
}

std::string chordLabel(int tonic, ChordTonality tonality, std::size_t chord_index)
{
    if (chord_index >= PianoState::kChordCount) {
        return {};
    }

    const auto& scales   = tonality == ChordTonality::Major ? kMajorScaleNames : kMinorScaleNames;
    const auto& suffixes = tonality == ChordTonality::Major ? kMajorSuffixes : kMinorSuffixes;
    const auto& scale    = scales[static_cast<std::size_t>(normalizedPitchClass(tonic))];
    return std::string(scale[chord_index % scale.size()]) + suffixes[chord_index];
}

std::string chordDegreeLabel(ChordTonality tonality, std::size_t chord_index)
{
    if (chord_index >= PianoState::kChordCount) {
        return {};
    }
    const auto& degrees = tonality == ChordTonality::Major ? kMajorDegrees : kMinorDegrees;
    return degrees[chord_index];
}

std::array<std::string, PianoState::kChordCount> chordLabels(int tonic, ChordTonality tonality)
{
    std::array<std::string, PianoState::kChordCount> labels;
    for (std::size_t index = 0; index < labels.size(); ++index) {
        labels[index] = chordLabel(tonic, tonality, index);
    }
    return labels;
}

}  // namespace piano
