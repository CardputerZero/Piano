#include "models/piano_model.hpp"

#include <algorithm>
#include <utility>

namespace piano {
namespace {

constexpr float kDefaultVelocity  = 0.78f;
constexpr float kChordVelocity    = 0.64f;
constexpr float kDemoVelocity     = 0.70f;
constexpr float kAssistedVelocity = 0.76f;

constexpr VoiceHandle kDemoVoiceFirst     = 128;
constexpr VoiceHandle kAssistedVoiceFirst = 132;

constexpr std::array<int, PianoState::kChordCount> kMajorRootOffsets              = {0, 2, 4, 5, 7, 9, 11, 12};
constexpr std::array<int, PianoState::kChordCount> kMinorRootOffsets              = {0, 2, 3, 5, 7, 8, 11, 12};
constexpr std::array<std::array<int, 3>, PianoState::kChordCount> kMajorIntervals = {
    std::array<int, 3>{0, 4, 7}, std::array<int, 3>{0, 3, 7}, std::array<int, 3>{0, 3, 7}, std::array<int, 3>{0, 4, 7},
    std::array<int, 3>{0, 4, 7}, std::array<int, 3>{0, 3, 7}, std::array<int, 3>{0, 3, 6}, std::array<int, 3>{0, 4, 7},
};
constexpr std::array<std::array<int, 3>, PianoState::kChordCount> kMinorIntervals = {
    std::array<int, 3>{0, 3, 7}, std::array<int, 3>{0, 3, 6}, std::array<int, 3>{0, 4, 7}, std::array<int, 3>{0, 3, 7},
    std::array<int, 3>{0, 4, 7}, std::array<int, 3>{0, 4, 7}, std::array<int, 3>{0, 3, 6}, std::array<int, 3>{0, 3, 7},
};

struct PlayalongPattern {
    const char* name;
    struct Step {
        PianoKey key;
        uint8_t duration_eighths;
    };
    std::array<Step, PianoState::kMaximumPlayalongLength> steps;
    std::size_t length;
};

constexpr PlayalongPattern::Step score(PianoKey key, uint8_t duration_eighths)
{
    return {key, duration_eighths};
}

constexpr PianoKey C = PianoKey::C;
constexpr PianoKey D = PianoKey::D;
constexpr PianoKey E = PianoKey::E;
constexpr PianoKey F = PianoKey::F;
constexpr PianoKey G = PianoKey::G;
constexpr PianoKey A = PianoKey::A;

template <typename... Steps>
constexpr PlayalongPattern makePattern(const char* name, Steps... steps)
{
    static_assert(sizeof...(steps) > 0);
    static_assert(sizeof...(steps) <= PianoState::kMaximumPlayalongLength);
    return {name, {steps...}, sizeof...(steps)};
}

// Complete, recognizable public-domain phrases. Durations are in eighth-note units.
constexpr std::array<PlayalongPattern, 8> kPianoMelodies = {
    makePattern("Ode to Joy", score(E, 2), score(E, 2), score(F, 2), score(G, 2), score(G, 2), score(F, 2), score(E, 2),
                score(D, 2), score(C, 2), score(C, 2), score(D, 2), score(E, 2), score(E, 3), score(D, 1), score(D, 4),
                score(E, 2), score(E, 2), score(F, 2), score(G, 2), score(G, 2), score(F, 2), score(E, 2), score(D, 2),
                score(C, 2), score(C, 2), score(D, 2), score(E, 2), score(D, 3), score(C, 1), score(C, 4)),
    makePattern("Twinkle Twinkle Little Star", score(C, 2), score(C, 2), score(G, 2), score(G, 2), score(A, 2),
                score(A, 2), score(G, 4), score(F, 2), score(F, 2), score(E, 2), score(E, 2), score(D, 2), score(D, 2),
                score(C, 4), score(G, 2), score(G, 2), score(F, 2), score(F, 2), score(E, 2), score(E, 2), score(D, 4),
                score(G, 2), score(G, 2), score(F, 2), score(F, 2), score(E, 2), score(E, 2), score(D, 4), score(C, 2),
                score(C, 2), score(G, 2), score(G, 2), score(A, 2), score(A, 2), score(G, 4), score(F, 2), score(F, 2),
                score(E, 2), score(E, 2), score(D, 2), score(D, 2), score(C, 4)),
    makePattern("Mary Had a Little Lamb", score(E, 2), score(D, 2), score(C, 2), score(D, 2), score(E, 2), score(E, 2),
                score(E, 4), score(D, 2), score(D, 2), score(D, 4), score(E, 2), score(G, 2), score(G, 4), score(E, 2),
                score(D, 2), score(C, 2), score(D, 2), score(E, 2), score(E, 2), score(E, 2), score(E, 2), score(D, 2),
                score(D, 2), score(E, 2), score(D, 2), score(C, 4)),
    makePattern("Frere Jacques", score(C, 2), score(D, 2), score(E, 2), score(C, 2), score(C, 2), score(D, 2),
                score(E, 2), score(C, 2), score(E, 2), score(F, 2), score(G, 4), score(E, 2), score(F, 2), score(G, 4),
                score(G, 1), score(A, 1), score(G, 1), score(F, 1), score(E, 2), score(C, 2), score(G, 1), score(A, 1),
                score(G, 1), score(F, 1), score(E, 2), score(C, 2), score(C, 2), score(G, 2), score(C, 4), score(C, 2),
                score(G, 2), score(C, 4)),
    makePattern("Row Row Row Your Boat", score(C, 3), score(C, 3), score(C, 2), score(D, 1), score(E, 3), score(E, 2),
                score(D, 1), score(E, 2), score(F, 1), score(G, 6), score(PianoKey::HighC, 1),
                score(PianoKey::HighC, 1), score(PianoKey::HighC, 1), score(G, 1), score(G, 1), score(G, 1),
                score(E, 1), score(E, 1), score(E, 1), score(C, 1), score(C, 1), score(C, 1), score(G, 2), score(F, 1),
                score(E, 2), score(D, 1), score(C, 6)),
    makePattern("London Bridge", score(G, 2), score(A, 2), score(G, 2), score(F, 2), score(E, 2), score(F, 2),
                score(G, 4), score(D, 2), score(E, 2), score(F, 4), score(E, 2), score(F, 2), score(G, 4), score(G, 2),
                score(A, 2), score(G, 2), score(F, 2), score(E, 2), score(F, 2), score(G, 4), score(D, 4), score(G, 2),
                score(E, 2), score(C, 4)),
    makePattern("Lightly Row", score(G, 2), score(E, 2), score(E, 4), score(F, 2), score(D, 2), score(D, 4),
                score(C, 2), score(D, 2), score(E, 2), score(F, 2), score(G, 2), score(G, 2), score(G, 4), score(G, 2),
                score(E, 2), score(E, 4), score(F, 2), score(D, 2), score(D, 4), score(C, 2), score(E, 2), score(G, 2),
                score(G, 2), score(C, 2), score(C, 2), score(C, 4)),
    makePattern("Au Clair de la Lune", score(C, 2), score(C, 2), score(C, 2), score(D, 2), score(E, 4), score(D, 4),
                score(C, 2), score(E, 2), score(D, 2), score(D, 2), score(C, 4), score(C, 2), score(C, 2), score(C, 2),
                score(D, 2), score(E, 4), score(D, 4), score(C, 2), score(E, 2), score(D, 2), score(D, 2), score(C, 4)),
};

constexpr std::array<PlayalongPattern, 8> kMajorProgressions = {
    makePattern("Pop loop", score(C, 4), score(G, 4), score(A, 4), score(F, 4), score(C, 4), score(G, 4), score(F, 4),
                score(G, 4), score(C, 8)),
    makePattern("50s progression", score(C, 4), score(A, 4), score(F, 4), score(G, 4), score(C, 4), score(A, 4),
                score(D, 4), score(G, 4), score(C, 8)),
    makePattern("Anthem loop", score(A, 4), score(F, 4), score(C, 4), score(G, 4), score(A, 2), score(F, 2),
                score(C, 4), score(G, 8), score(C, 8)),
    makePattern("Canon", score(C, 4), score(G, 4), score(A, 4), score(E, 4), score(F, 4), score(C, 4), score(F, 4),
                score(G, 4), score(C, 8)),
    makePattern("Folk cadence", score(C, 8), score(F, 4), score(C, 4), score(G, 4), score(A, 4), score(F, 4),
                score(D, 4), score(G, 4), score(C, 8)),
    makePattern("Mini blues", score(C, 4), score(C, 4), score(C, 4), score(C, 4), score(F, 4), score(F, 4), score(C, 4),
                score(C, 4), score(G, 4), score(F, 4), score(C, 4), score(G, 4), score(C, 8)),
    makePattern("Mixed cadence", score(D, 4), score(G, 4), score(C, 8), score(A, 4), score(D, 2), score(G, 2),
                score(C, 4), score(G, 4), score(C, 8)),
    makePattern("Rock turnaround", score(C, 4), score(F, 2), score(G, 2), score(C, 4), score(A, 4), score(F, 4),
                score(G, 8), score(C, 4), score(F, 2), score(G, 2), score(C, 8)),
};

constexpr std::array<PlayalongPattern, 7> kMinorProgressions = {
    makePattern("Minor pop", score(C, 4), score(A, 4), score(E, 4), score(G, 4), score(C, 4), score(A, 4), score(F, 4),
                score(G, 4), score(C, 8)),
    makePattern("Minor cadence", score(C, 4), score(F, 4), score(G, 4), score(C, 4), score(A, 4), score(F, 4),
                score(G, 4), score(C, 4), score(G, 4), score(C, 8)),
    makePattern("Minor anthem", score(C, 4), score(A, 4), score(F, 4), score(G, 4), score(C, 2), score(A, 2),
                score(F, 4), score(G, 8), score(C, 8)),
    makePattern("Minor pulse", score(C, 2), score(G, 2), score(A, 4), score(F, 4), score(C, 4), score(E, 4),
                score(F, 4), score(G, 8), score(C, 8)),
    makePattern("Minor blues", score(C, 4), score(C, 4), score(C, 4), score(C, 4), score(F, 4), score(F, 4),
                score(C, 4), score(C, 4), score(G, 4), score(F, 4), score(C, 4), score(G, 4), score(C, 8)),
    makePattern("Minor mixed cadence", score(F, 4), score(C, 4), score(A, 8), score(G, 4), score(E, 2), score(F, 2),
                score(G, 4), score(C, 4), score(G, 4), score(C, 8)),
    makePattern("Minor canon", score(C, 4), score(G, 4), score(A, 4), score(E, 4), score(F, 4), score(C, 4),
                score(F, 4), score(G, 4), score(C, 8)),
};

constexpr int whiteKeyIndex(PianoKey key)
{
    switch (key) {
        case PianoKey::C:
            return 0;
        case PianoKey::D:
            return 1;
        case PianoKey::E:
            return 2;
        case PianoKey::F:
            return 3;
        case PianoKey::G:
            return 4;
        case PianoKey::A:
            return 5;
        case PianoKey::B:
            return 6;
        case PianoKey::HighC:
            return 7;
        default:
            return -1;
    }
}

template <std::size_t PatternCount>
constexpr bool patternsStayCompact(const std::array<PlayalongPattern, PatternCount>& patterns, int maximum_jump,
                                   int maximum_span, int maximum_key, std::size_t minimum_length)
{
    for (const auto& pattern : patterns) {
        if (pattern.length < minimum_length || pattern.length > PianoState::kMaximumPlayalongLength) {
            return false;
        }
        int minimum  = 7;
        int maximum  = 0;
        int previous = -1;
        for (std::size_t index = 0; index < pattern.length; ++index) {
            const auto& step  = pattern.steps[index];
            const int current = whiteKeyIndex(step.key);
            if (current < 0 || current > maximum_key || step.duration_eighths == 0) {
                return false;
            }
            if (previous >= 0) {
                const int jump = current > previous ? current - previous : previous - current;
                if (jump > maximum_jump) {
                    return false;
                }
            }
            minimum  = current < minimum ? current : minimum;
            maximum  = current > maximum ? current : maximum;
            previous = current;
        }
        if (maximum - minimum > maximum_span) {
            return false;
        }
    }
    return true;
}

template <std::size_t PatternCount>
constexpr bool melodiesHaveRhythm(const std::array<PlayalongPattern, PatternCount>& patterns)
{
    for (const auto& pattern : patterns) {
        bool varied = false;
        for (std::size_t index = 1; index < pattern.length; ++index) {
            if (pattern.steps[index].duration_eighths != pattern.steps.front().duration_eighths) {
                varied = true;
            }
            if (pattern.steps[index].duration_eighths > 8) {
                return false;
            }
        }
        if (!varied) {
            return false;
        }
    }
    return true;
}

constexpr uint32_t patternDurationEighths(const PlayalongPattern& pattern)
{
    uint32_t duration = 0;
    for (std::size_t index = 0; index < pattern.length; ++index) {
        duration += pattern.steps[index].duration_eighths;
    }
    return duration;
}

template <std::size_t PatternCount>
constexpr bool progressionsHaveFullLength(const std::array<PlayalongPattern, PatternCount>& patterns)
{
    for (const auto& pattern : patterns) {
        const uint32_t duration = patternDurationEighths(pattern);
        if (pattern.length < 9 || duration < 40 || duration > 64 || pattern.steps[pattern.length - 1].key != C) {
            return false;
        }
    }
    return true;
}

template <std::size_t PatternCount>
constexpr bool poolUsesDuration(const std::array<PlayalongPattern, PatternCount>& patterns, uint8_t duration_eighths)
{
    for (const auto& pattern : patterns) {
        for (std::size_t index = 0; index < pattern.length; ++index) {
            if (pattern.steps[index].duration_eighths == duration_eighths) {
                return true;
            }
        }
    }
    return false;
}

static_assert(patternsStayCompact(kPianoMelodies, 4, 7, 7, 20));
static_assert(patternsStayCompact(kMajorProgressions, 5, 5, 5, 8));
static_assert(patternsStayCompact(kMinorProgressions, 5, 5, 5, 8));
static_assert(melodiesHaveRhythm(kPianoMelodies));
static_assert(progressionsHaveFullLength(kMajorProgressions));
static_assert(progressionsHaveFullLength(kMinorProgressions));
static_assert(poolUsesDuration(kMajorProgressions, 2) && poolUsesDuration(kMajorProgressions, 4) &&
              poolUsesDuration(kMajorProgressions, 8));
static_assert(poolUsesDuration(kMinorProgressions, 2) && poolUsesDuration(kMinorProgressions, 4) &&
              poolUsesDuration(kMinorProgressions, 8));
static_assert(kPianoMelodies[1].length == 42);

int midiNote(int octave, int note_index)
{
    return 12 * (octave + 1) + note_index;
}

VoiceHandle voiceHandle(std::size_t note_index)
{
    return static_cast<VoiceHandle>(note_index + 1);
}

VoiceHandle chordVoiceHandle(std::size_t chord_index, std::size_t voice_index)
{
    return static_cast<VoiceHandle>(PianoState::kNoteCount + chord_index * 3 + voice_index + 1);
}

int shiftedTonic(int tonic, int semitones)
{
    const int shifted = (tonic + semitones) % 12;
    return shifted < 0 ? shifted + 12 : shifted;
}

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms)
{
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

uint32_t stepDurationMs(uint8_t duration_eighths)
{
    return static_cast<uint32_t>(duration_eighths) * PianoModel::kPlayalongEighthNoteMs;
}

uint32_t gateDurationMs(PianoMode mode, uint8_t duration_eighths)
{
    const uint32_t duration = stepDurationMs(duration_eighths);
    const uint32_t gap =
        mode == PianoMode::Piano ? std::clamp(duration / 8U, 45U, 110U) : std::clamp(duration / 16U, 30U, 80U);
    return duration - gap;
}

}  // namespace

PianoModel::PianoModel(SynthOutput& synth) : PianoModel(synth, std::random_device{}())
{
}

PianoModel::PianoModel(SynthOutput& synth, uint32_t random_seed) : _synth(synth), _random(random_seed)
{
}

smooth_ui_toolkit::SingleObservable<PianoState>& PianoModel::state()
{
    return _state;
}

void PianoModel::handleInput(const PianoInputEvent& event)
{
    if (event.type == PianoInputEventType::AllReleased) {
        releaseAll(true);
        return;
    }
    if (event.repeated) {
        return;
    }
    _now_ms = event.timestamp_ms;

    PianoState next = _state.get();
    const int note  = noteIndex(event.key);
    if (note >= 0) {
        if (next.mode == PianoMode::Chord) {
            handleChord(next, event.key, event.pressed, event.timestamp_ms);
        } else {
            handlePianoNote(next, note, event.pressed, event.timestamp_ms);
        }
        return;
    }

    switch (event.key) {
        case PianoKey::OctaveDown:
            if (event.pressed && !next.octave_down_pressed) {
                if (next.playalong_active) {
                    stopPlayalong(next);
                }
                if (next.mode == PianoMode::Chord) {
                    silenceNotes(next, true);
                    next.tonic = shiftedTonic(next.tonic, -1);
                } else {
                    next.octave = std::max(kMinimumOctave, next.octave - 1);
                }
            }
            next.octave_down_pressed = event.pressed;
            break;
        case PianoKey::OctaveUp:
            if (event.pressed && !next.octave_up_pressed) {
                if (next.playalong_active) {
                    stopPlayalong(next);
                }
                if (next.mode == PianoMode::Chord) {
                    silenceNotes(next, true);
                    next.tonic = shiftedTonic(next.tonic, 1);
                } else {
                    next.octave = std::min(kMaximumOctave, next.octave + 1);
                }
            }
            next.octave_up_pressed = event.pressed;
            break;
        case PianoKey::ToggleKeymap:
            if (!event.pressed) {
                return;
            }
            next.keymap_visible = !next.keymap_visible;
            break;
        case PianoKey::ToggleMode:
            if (event.pressed && !next.mode_toggle_pressed) {
                if (next.playalong_active) {
                    stopPlayalong(next);
                }
                silenceNotes(next, true);
                next.mode = next.mode == PianoMode::Piano ? PianoMode::Chord : PianoMode::Piano;
                if (next.mode == PianoMode::Piano) {
                    next.tonality_toggle_pressed = false;
                }
            }
            next.mode_toggle_pressed = event.pressed;
            break;
        case PianoKey::ToggleTonality:
            if (next.mode != PianoMode::Chord) {
                if (!event.pressed && next.tonality_toggle_pressed) {
                    next.tonality_toggle_pressed = false;
                    _state.set(std::move(next));
                }
                return;
            }
            if (event.pressed && !next.tonality_toggle_pressed) {
                if (next.playalong_active) {
                    stopPlayalong(next);
                }
                silenceNotes(next, true);
                next.tonality = next.tonality == ChordTonality::Major ? ChordTonality::Minor : ChordTonality::Major;
            }
            next.tonality_toggle_pressed = event.pressed;
            break;
        case PianoKey::TogglePlayalong:
            if (event.pressed && !next.playalong_toggle_pressed) {
                if (next.playalong_active) {
                    stopPlayalong(next);
                } else {
                    startPlayalong(next, event.timestamp_ms);
                }
            }
            next.playalong_toggle_pressed = event.pressed;
            break;
        default:
            return;
    }
    _state.set(std::move(next));
}

void PianoModel::tick(uint32_t now_ms)
{
    _now_ms         = now_ms;
    PianoState next = _state.get();
    if (!next.playalong_active) {
        return;
    }

    if (next.playalong_phase == PlayalongPhase::Demo) {
        const bool demo_voice_active =
            std::any_of(_demo_voices.begin(), _demo_voices.end(), [](const ActiveNote& voice) { return voice.active; });
        if (demo_voice_active && deadlineReached(now_ms, _playalong_note_off_at)) {
            stopTimedVoices(_demo_voices);
            next.playalong_demo_target = -1;
            _state.set(std::move(next));
            return;
        }
        if (!deadlineReached(now_ms, _playalong_step_end_at)) {
            return;
        }

        ++next.playalong_step;
        if (next.playalong_step < next.playalong_length) {
            startDemoStep(next, now_ms);
        } else {
            next.playalong_phase            = PlayalongPhase::Interlude;
            next.playalong_demo_target      = -1;
            next.playalong_target           = -1;
            next.playalong_duration_eighths = 0;
            _playalong_step_end_at          = now_ms + kPlayalongInterludeMs;
        }
        _state.set(std::move(next));
        return;
    }

    if (next.playalong_phase == PlayalongPhase::Interlude) {
        if (deadlineReached(now_ms, _playalong_step_end_at)) {
            enterGuide(next);
            _state.set(std::move(next));
        }
        return;
    }

    if (next.playalong_phase != PlayalongPhase::Guide || next.playalong_target >= 0) {
        return;
    }

    const bool assisted_voice_active = std::any_of(_assisted_voices.begin(), _assisted_voices.end(),
                                                   [](const ActiveNote& voice) { return voice.active; });
    if (assisted_voice_active && deadlineReached(now_ms, _playalong_note_off_at)) {
        stopTimedVoices(_assisted_voices);
        const bool manual_chord_active = std::any_of(_active_chord_voices.begin(), _active_chord_voices.end(),
                                                     [](const ActiveNote& voice) { return voice.active; });
        if (!manual_chord_active) {
            next.active_chord_index = -1;
        }
    }
    if (!deadlineReached(now_ms, _playalong_step_end_at)) {
        return;
    }

    stopTimedVoices(_assisted_voices);
    ++next.playalong_step;
    if (next.playalong_step >= next.playalong_length) {
        stopPlayalong(next, true);
    } else {
        const PlayalongStep& step       = _playalong_steps[next.playalong_step];
        next.playalong_target           = chordIndex(step.key);
        next.playalong_duration_eighths = step.duration_eighths;
        const bool manual_chord_active  = std::any_of(_active_chord_voices.begin(), _active_chord_voices.end(),
                                                      [](const ActiveNote& voice) { return voice.active; });
        if (!manual_chord_active) {
            next.active_chord_index = -1;
        }
    }
    _state.set(std::move(next));
}

void PianoModel::reset()
{
    _synth.allNotesOff(true);
    _active_notes.fill({});
    _active_chord_voices.fill({});
    clearTimedVoices();
    _playalong_steps.fill({});
    _playalong_note_off_at = 0;
    _playalong_step_end_at = 0;
    _state.set(PianoState{});
}

void PianoModel::releaseAll(bool immediate)
{
    PianoState next = _state.get();
    if (next.playalong_active) {
        stopPlayalong(next);
    }
    _synth.allNotesOff(immediate);
    _active_notes.fill({});
    _active_chord_voices.fill({});
    clearTimedVoices();

    next.pressed_notes            = {};
    next.octave_down_pressed      = false;
    next.octave_up_pressed        = false;
    next.active_chord_index       = -1;
    next.mode_toggle_pressed      = false;
    next.tonality_toggle_pressed  = false;
    next.playalong_toggle_pressed = false;
    _state.set(std::move(next));
}

void PianoModel::handlePianoNote(PianoState& state, int note, bool pressed, uint32_t timestamp_ms)
{
    const auto index = static_cast<std::size_t>(note);
    if (state.pressed_notes[index] == pressed) {
        return;
    }

    state.pressed_notes[index] = pressed;
    if (pressed) {
        const bool assisted = advancePlayalong(state, static_cast<PianoKey>(note), timestamp_ms);
        if (!assisted) {
            const VoiceHandle handle = voiceHandle(index);
            _active_notes[index]     = {handle, true};
            _synth.noteOn(handle, midiNote(state.octave, note), kDefaultVelocity);
        }
    } else if (_active_notes[index].active) {
        _synth.noteOff(_active_notes[index].handle);
        _active_notes[index] = {};
    }
    _state.set(std::move(state));
}

void PianoModel::handleChord(PianoState& state, PianoKey key, bool pressed, uint32_t timestamp_ms)
{
    const int chord = chordIndex(key);
    if (chord < 0) {
        return;
    }

    const auto note = static_cast<std::size_t>(noteIndex(key));
    if (state.pressed_notes[note] == pressed) {
        return;
    }

    state.pressed_notes[note] = pressed;
    if (pressed) {
        const bool assisted = advancePlayalong(state, key, timestamp_ms);
        stopActiveChord(state);
        if (assisted) {
            state.active_chord_index = chord;
        } else {
            startChord(state, static_cast<std::size_t>(chord));
        }
    } else if (state.active_chord_index == chord) {
        stopActiveChord(state);
    }
    _state.set(std::move(state));
}

void PianoModel::startChord(PianoState& state, std::size_t chord_index)
{
    const auto& root_offsets = state.tonality == ChordTonality::Major ? kMajorRootOffsets : kMinorRootOffsets;
    const auto& intervals    = state.tonality == ChordTonality::Major ? kMajorIntervals : kMinorIntervals;
    const int root_midi      = midiNote(state.octave, state.tonic + root_offsets[chord_index]);

    for (std::size_t voice = 0; voice < _active_chord_voices.size(); ++voice) {
        const VoiceHandle handle    = chordVoiceHandle(chord_index, voice);
        _active_chord_voices[voice] = {handle, true};
        _synth.noteOn(handle, root_midi + intervals[chord_index][voice], kChordVelocity);
    }
    state.active_chord_index = static_cast<int>(chord_index);
}

void PianoModel::stopActiveChord(PianoState& state)
{
    for (ActiveNote& voice : _active_chord_voices) {
        if (voice.active) {
            _synth.noteOff(voice.handle);
            voice = {};
        }
    }
    state.active_chord_index = -1;
}

void PianoModel::silenceNotes(PianoState& state, bool immediate)
{
    _synth.allNotesOff(immediate);
    _active_notes.fill({});
    _active_chord_voices.fill({});
    clearTimedVoices();
    state.pressed_notes      = {};
    state.active_chord_index = -1;
}

void PianoModel::startPlayalong(PianoState& state, uint32_t now_ms)
{
    silenceNotes(state, true);
    _playalong_steps.fill({});

    const PlayalongPattern* selected = nullptr;
    if (state.mode == PianoMode::Piano) {
        const std::size_t pattern = choosePlayalongPattern(state.mode, state.tonality, kPianoMelodies.size());
        selected                  = &kPianoMelodies[pattern];
    } else if (state.tonality == ChordTonality::Major) {
        const std::size_t pattern = choosePlayalongPattern(state.mode, state.tonality, kMajorProgressions.size());
        selected                  = &kMajorProgressions[pattern];
    } else {
        const std::size_t pattern = choosePlayalongPattern(state.mode, state.tonality, kMinorProgressions.size());
        selected                  = &kMinorProgressions[pattern];
    }
    for (std::size_t index = 0; index < selected->length; ++index) {
        _playalong_steps[index] = {selected->steps[index].key, selected->steps[index].duration_eighths};
    }
    state.playalong_length = selected->length;

    state.playalong_active         = true;
    state.playalong_phase          = PlayalongPhase::Demo;
    state.playalong_step           = 0;
    state.playalong_demo_target    = -1;
    state.playalong_target         = -1;
    state.playalong_success_target = -1;
    startDemoStep(state, now_ms);
}

void PianoModel::stopPlayalong(PianoState& state, bool preserve_success)
{
    stopTimedVoices(_demo_voices);
    stopTimedVoices(_assisted_voices);
    stopActiveChord(state);
    _playalong_steps.fill({});
    _playalong_note_off_at      = 0;
    _playalong_step_end_at      = 0;
    state.playalong_active      = false;
    state.playalong_phase       = PlayalongPhase::Inactive;
    state.playalong_demo_target = -1;
    state.playalong_target      = -1;
    if (!preserve_success) {
        state.playalong_success_target = -1;
    }
    state.playalong_step             = 0;
    state.playalong_length           = 0;
    state.playalong_duration_eighths = 0;
    state.active_chord_index         = -1;
}

bool PianoModel::advancePlayalong(PianoState& state, PianoKey key, uint32_t now_ms)
{
    const int key_index = chordIndex(key);
    if (state.playalong_phase != PlayalongPhase::Guide || key_index != state.playalong_target) {
        return false;
    }

    const PlayalongStep& step = _playalong_steps[state.playalong_step];
    startTimedVoices(_assisted_voices, kAssistedVoiceFirst, state, step.key, kAssistedVelocity);
    _playalong_note_off_at = now_ms + gateDurationMs(state.mode, step.duration_eighths);
    _playalong_step_end_at = now_ms + stepDurationMs(step.duration_eighths);

    state.playalong_success_target = key_index;
    ++state.playalong_success_revision;
    state.playalong_target = -1;
    return true;
}

void PianoModel::startDemoStep(PianoState& state, uint32_t start_ms)
{
    const PlayalongStep& step = _playalong_steps[state.playalong_step];
    startTimedVoices(_demo_voices, kDemoVoiceFirst, state, step.key, kDemoVelocity);
    state.playalong_demo_target      = chordIndex(step.key);
    state.playalong_target           = -1;
    state.playalong_duration_eighths = step.duration_eighths;
    _playalong_note_off_at           = start_ms + gateDurationMs(state.mode, step.duration_eighths);
    _playalong_step_end_at           = start_ms + stepDurationMs(step.duration_eighths);
}

void PianoModel::enterGuide(PianoState& state)
{
    stopTimedVoices(_demo_voices);
    state.playalong_phase            = PlayalongPhase::Guide;
    state.playalong_step             = 0;
    state.playalong_demo_target      = -1;
    state.playalong_target           = chordIndex(_playalong_steps.front().key);
    state.playalong_success_target   = -1;
    state.playalong_duration_eighths = _playalong_steps.front().duration_eighths;
}

void PianoModel::startTimedVoices(std::array<ActiveNote, kChordVoiceCount>& voices, VoiceHandle first_handle,
                                  const PianoState& state, PianoKey key, float velocity)
{
    stopTimedVoices(voices);
    if (state.mode == PianoMode::Piano) {
        const int note = noteIndex(key);
        if (note < 0) {
            return;
        }
        voices.front() = {first_handle, true};
        _synth.noteOn(first_handle, midiNote(state.octave, note), velocity);
        return;
    }

    const int chord = chordIndex(key);
    if (chord < 0) {
        return;
    }
    const auto chord_index   = static_cast<std::size_t>(chord);
    const auto& root_offsets = state.tonality == ChordTonality::Major ? kMajorRootOffsets : kMinorRootOffsets;
    const auto& intervals    = state.tonality == ChordTonality::Major ? kMajorIntervals : kMinorIntervals;
    const int root_midi      = midiNote(state.octave, state.tonic + root_offsets[chord_index]);
    for (std::size_t voice = 0; voice < voices.size(); ++voice) {
        const VoiceHandle handle = first_handle + static_cast<VoiceHandle>(voice);
        voices[voice]            = {handle, true};
        _synth.noteOn(handle, root_midi + intervals[chord_index][voice], velocity);
    }
}

void PianoModel::stopTimedVoices(std::array<ActiveNote, kChordVoiceCount>& voices)
{
    for (ActiveNote& voice : voices) {
        if (voice.active) {
            _synth.noteOff(voice.handle);
            voice = {};
        }
    }
}

void PianoModel::clearTimedVoices()
{
    _demo_voices.fill({});
    _assisted_voices.fill({});
}

std::size_t PianoModel::choosePlayalongPattern(PianoMode mode, ChordTonality tonality, std::size_t pattern_count)
{
    const std::size_t history_slot = mode == PianoMode::Piano ? 0 : (tonality == ChordTonality::Major ? 1 : 2);
    int& previous                  = _last_playalong_patterns[history_slot];

    std::size_t selected = 0;
    if (pattern_count > 1 && previous >= 0) {
        std::uniform_int_distribution<std::size_t> offset_distribution(1, pattern_count - 1);
        selected = (static_cast<std::size_t>(previous) + offset_distribution(_random)) % pattern_count;
    } else {
        std::uniform_int_distribution<std::size_t> pattern_distribution(0, pattern_count - 1);
        selected = pattern_distribution(_random);
    }
    previous = static_cast<int>(selected);
    return selected;
}

}  // namespace piano
