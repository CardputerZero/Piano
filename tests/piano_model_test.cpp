#include "models/piano_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

enum class CommandType {
    NoteOn,
    NoteOff,
    AllNotesOff,
};

struct Command {
    CommandType type;
    piano::VoiceHandle handle;
    int midi_note;
    float velocity;
    bool immediate;
    uint32_t timestamp_ms;
};

class FakeSynth final : public piano::SynthOutput {
public:
    void noteOn(piano::VoiceHandle handle, int midi_note, float velocity) override
    {
        commands.push_back({CommandType::NoteOn, handle, midi_note, velocity, false, now_ms});
        active_handles.push_back(handle);
    }

    void noteOff(piano::VoiceHandle handle) override
    {
        commands.push_back({CommandType::NoteOff, handle, 0, 0.0f, false, now_ms});
        active_handles.erase(std::remove(active_handles.begin(), active_handles.end(), handle), active_handles.end());
    }

    void allNotesOff(bool immediate) override
    {
        commands.push_back({CommandType::AllNotesOff, 0, 0, 0.0f, immediate, now_ms});
        active_handles.clear();
    }

    uint32_t now_ms = 0;
    std::vector<Command> commands;
    std::vector<piano::VoiceHandle> active_handles;
};

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

int pitchClassForChordLabel(const std::string& label)
{
    expect(!label.empty(), "chord label must not be empty");
    int pitch_class = 0;
    switch (label.front()) {
        case 'C':
            pitch_class = 0;
            break;
        case 'D':
            pitch_class = 2;
            break;
        case 'E':
            pitch_class = 4;
            break;
        case 'F':
            pitch_class = 5;
            break;
        case 'G':
            pitch_class = 7;
            break;
        case 'A':
            pitch_class = 9;
            break;
        case 'B':
            pitch_class = 11;
            break;
        default:
            fail("chord label must start with a note name");
    }

    if (label.size() > 1) {
        if (label[1] == '#') {
            ++pitch_class;
        } else if (label[1] == 'b') {
            --pitch_class;
        } else if (label[1] == 'x') {
            pitch_class += 2;
        }
    }
    pitch_class %= 12;
    return pitch_class < 0 ? pitch_class + 12 : pitch_class;
}

piano::PianoInputEvent keyEvent(piano::PianoKey key, bool pressed, bool repeated = false)
{
    return {piano::PianoInputEventType::Key, key, pressed, repeated, 0};
}

void sendKey(piano::PianoModel& model, FakeSynth& synth, piano::PianoKey key, bool pressed, uint32_t now_ms)
{
    synth.now_ms = now_ms;
    model.handleInput({piano::PianoInputEventType::Key, key, pressed, false, now_ms});
}

void tickAt(piano::PianoModel& model, FakeSynth& synth, uint32_t now_ms)
{
    synth.now_ms = now_ms;
    model.tick(now_ms);
}

piano::PianoKey whiteKey(std::size_t index)
{
    constexpr std::array<piano::PianoKey, piano::PianoState::kChordCount> kWhiteKeys = {
        piano::PianoKey::C, piano::PianoKey::D, piano::PianoKey::E, piano::PianoKey::F,
        piano::PianoKey::G, piano::PianoKey::A, piano::PianoKey::B, piano::PianoKey::HighC,
    };
    expect(index < kWhiteKeys.size(), "playalong target must name a white key");
    return kWhiteKeys[index];
}

bool hasTimedVoice(const FakeSynth& synth)
{
    return std::any_of(synth.active_handles.begin(), synth.active_handles.end(),
                       [](piano::VoiceHandle handle) { return handle >= 128; });
}

struct DemoRun {
    uint32_t guide_started_at     = 0;
    uint32_t interlude_started_at = 0;
    std::vector<int> targets;
    std::vector<uint8_t> durations;
};

DemoRun runDemoToGuide(piano::PianoModel& model, FakeSynth& synth, uint32_t start_ms)
{
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, start_ms);
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, start_ms + 1);

    DemoRun run;
    uint32_t now_ms           = start_ms + 1;
    std::size_t captured_step = static_cast<std::size_t>(-1);
    bool saw_interlude        = false;
    for (std::size_t guard = 0; guard < 200000; ++guard) {
        const piano::PianoState state = model.state().get();
        if (state.playalong_phase == piano::PlayalongPhase::Demo && state.playalong_demo_target >= 0 &&
            state.playalong_step != captured_step) {
            captured_step = state.playalong_step;
            run.targets.push_back(state.playalong_demo_target);
            run.durations.push_back(state.playalong_duration_eighths);
            expect(state.playalong_target == -1, "Demo must expose only its blue target");
        }
        if (state.playalong_phase == piano::PlayalongPhase::Interlude && !saw_interlude) {
            saw_interlude            = true;
            run.interlude_started_at = now_ms;
            expect(state.playalong_demo_target == -1 && state.playalong_target == -1,
                   "the interlude must hide both targets");
        }
        if (state.playalong_phase == piano::PlayalongPhase::Guide) {
            run.guide_started_at = now_ms;
            return run;
        }
        tickAt(model, synth, ++now_ms);
    }
    fail("Demo did not reach Guide under the virtual clock");
}

uint32_t stepDurationMs(uint8_t duration_eighths)
{
    return static_cast<uint32_t>(duration_eighths) * piano::PianoModel::kPlayalongEighthNoteMs;
}

uint32_t pianoGateDurationMs(uint8_t duration_eighths)
{
    const uint32_t duration = stepDurationMs(duration_eighths);
    return duration - std::clamp(duration / 8U, 45U, 110U);
}

std::size_t countNoteOns(const FakeSynth& synth, piano::VoiceHandle first_handle, piano::VoiceHandle last_handle)
{
    return static_cast<std::size_t>(std::count_if(
        synth.commands.begin(), synth.commands.end(), [first_handle, last_handle](const Command& command) {
            return command.type == CommandType::NoteOn && command.handle >= first_handle &&
                   command.handle <= last_handle;
        }));
}

void expectNoteOns(const std::vector<Command>& commands, const std::array<int, 3>& expected_notes,
                   const std::string& context)
{
    expect(commands.size() == expected_notes.size(), context + " must start exactly three voices");
    for (std::size_t index = 0; index < expected_notes.size(); ++index) {
        expect(commands[index].type == CommandType::NoteOn, context + " must only emit note-on commands");
        expect(commands[index].midi_note == expected_notes[index], context + " emitted the wrong MIDI note");
    }
}

void enterChordMode(piano::PianoModel& model, FakeSynth& synth)
{
    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, true));
    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, false));
    expect(model.state().get().mode == piano::PianoMode::Chord, "Tab must enter chord mode");
    synth.commands.clear();
}

void testPolyphonicEdges()
{
    FakeSynth synth;
    piano::PianoModel model(synth);

    model.handleInput(keyEvent(piano::PianoKey::C, true));
    model.handleInput(keyEvent(piano::PianoKey::C, true));
    model.handleInput(keyEvent(piano::PianoKey::E, true));
    expect(synth.commands.size() == 2, "duplicate key-down must not retrigger a voice");
    expect(synth.commands[0].type == CommandType::NoteOn && synth.commands[0].midi_note == 60,
           "C4 must map to MIDI note 60");
    expect(synth.commands[1].type == CommandType::NoteOn && synth.commands[1].midi_note == 64,
           "E4 must map to MIDI note 64");

    model.handleInput(keyEvent(piano::PianoKey::C, false));
    model.handleInput(keyEvent(piano::PianoKey::C, false));
    expect(synth.commands.size() == 3 && synth.commands.back().type == CommandType::NoteOff,
           "duplicate key-up must not emit another note-off");
    expect(model.state().get().pressed_notes[4], "releasing C must not release E");
}

void testOctaveIsLatchedPerKeyPress()
{
    FakeSynth synth;
    piano::PianoModel model(synth);

    model.handleInput(keyEvent(piano::PianoKey::C, true));
    model.handleInput(keyEvent(piano::PianoKey::OctaveUp, true));
    model.handleInput(keyEvent(piano::PianoKey::OctaveUp, true));
    model.handleInput(keyEvent(piano::PianoKey::OctaveUp, false));
    expect(model.state().get().octave == 5, "duplicate octave key-down must only advance once");

    model.handleInput(keyEvent(piano::PianoKey::C, false));
    expect(synth.commands.back().type == CommandType::NoteOff && synth.commands.back().handle == 1,
           "releasing after an octave change must stop the original voice handle");

    model.handleInput(keyEvent(piano::PianoKey::C, true));
    expect(synth.commands.back().type == CommandType::NoteOn && synth.commands.back().midi_note == 72,
           "a new press must use the new octave");
}

void testKeymapStartsVisibleAndToggles()
{
    FakeSynth synth;
    piano::PianoModel model(synth);

    expect(model.state().get().keymap_visible, "the key map must be visible on startup");
    model.handleInput(keyEvent(piano::PianoKey::ToggleKeymap, true));
    expect(!model.state().get().keymap_visible, "Space must hide the key map");
    model.handleInput(keyEvent(piano::PianoKey::ToggleKeymap, false));
    expect(!model.state().get().keymap_visible, "releasing Space must not toggle the key map again");
    model.handleInput(keyEvent(piano::PianoKey::ToggleKeymap, true));
    expect(model.state().get().keymap_visible, "a second Space press must show the key map");
}

void testPanicReleasesVisualAndAudioState()
{
    FakeSynth synth;
    piano::PianoModel model(synth);

    model.handleInput(keyEvent(piano::PianoKey::D, true));
    model.handleInput(keyEvent(piano::PianoKey::OctaveDown, true));
    model.handleInput({piano::PianoInputEventType::AllReleased, piano::PianoKey::Unknown, false, false, 0});

    const piano::PianoState state = model.state().get();
    expect(synth.commands.back().type == CommandType::AllNotesOff && synth.commands.back().immediate,
           "input reset must request an immediate all-notes-off");
    for (bool pressed : state.pressed_notes) {
        expect(!pressed, "input reset must clear every visual piano key");
    }
    expect(!state.octave_down_pressed && !state.octave_up_pressed, "input reset must clear octave button feedback");
}

void testChordLabelsAndDegrees()
{
    constexpr std::array<int, piano::PianoState::kChordCount> kMajorOffsets = {0, 2, 4, 5, 7, 9, 11, 12};
    constexpr std::array<int, piano::PianoState::kChordCount> kMinorOffsets = {0, 2, 3, 5, 7, 8, 11, 12};
    const auto major = piano::chordLabels(0, piano::ChordTonality::Major);
    const auto minor = piano::chordLabels(0, piano::ChordTonality::Minor);
    const std::array<std::string, piano::PianoState::kChordCount> expected_major = {
        "C", "Dm", "Em", "F", "G", "Am", "Bdim", "C",
    };
    const std::array<std::string, piano::PianoState::kChordCount> expected_minor = {
        "Cm", "Ddim", "Eb", "Fm", "G", "Ab", "Bdim", "Cm",
    };

    expect(major == expected_major, "C major chord labels must follow I ii iii IV V vi vii-dim I");
    expect(minor == expected_minor, "C minor chord labels must use the harmonic-minor dominant and leading tone");
    expect(piano::chordDegreeLabel(piano::ChordTonality::Major, 6) == "vii",
           "major leading-tone degree must use the seventh Roman numeral");
    expect(piano::chordDegreeLabel(piano::ChordTonality::Minor, 1) == "ii",
           "minor supertonic degree must use the second Roman numeral");
    expect(piano::keyName(1, piano::ChordTonality::Major) == "Db",
           "major key names must prefer conventional enharmonic spelling");
    expect(piano::keyName(1, piano::ChordTonality::Minor) == "C#",
           "minor key names must prefer conventional enharmonic spelling");
    expect(piano::chordLabel(1, piano::ChordTonality::Major, 0) == "Db",
           "Db major tonic chord must not be mislabeled C#");
    expect(piano::chordLabel(8, piano::ChordTonality::Minor, 6) == "Fxdim",
           "double sharps must use the compact ASCII x notation");
    expect(piano::chordLabel(0, piano::ChordTonality::Major, piano::PianoState::kChordCount).empty(),
           "out-of-range chord labels must be empty");

    for (int tonic = 0; tonic < 12; ++tonic) {
        for (const auto tonality : {piano::ChordTonality::Major, piano::ChordTonality::Minor}) {
            const auto labels   = piano::chordLabels(tonic, tonality);
            const auto& offsets = tonality == piano::ChordTonality::Major ? kMajorOffsets : kMinorOffsets;
            for (std::size_t chord = 0; chord < labels.size(); ++chord) {
                expect(labels[chord].size() <= 5, "every chord label must fit the compact white key");
                expect(pitchClassForChordLabel(labels[chord]) == (tonic + offsets[chord]) % 12,
                       "chord label root must match the sounding scale degree");
            }
        }
    }
}

void testChordVoicingAndReplacement()
{
    FakeSynth synth;
    piano::PianoModel model(synth);
    enterChordMode(model, synth);

    model.handleInput(keyEvent(piano::PianoKey::C, true));
    expectNoteOns(synth.commands, {60, 64, 67}, "C major chord");
    expect(model.state().get().active_chord_index == 0, "A key must activate the tonic chord");

    synth.commands.clear();
    model.handleInput(keyEvent(piano::PianoKey::D, true));
    expect(synth.commands.size() == 6, "a replacement chord must release three voices and start three voices");
    for (std::size_t index = 0; index < 3; ++index) {
        expect(synth.commands[index].type == CommandType::NoteOff,
               "a replacement chord must release every old voice first");
    }
    for (std::size_t index = 0; index < 3; ++index) {
        const Command& command            = synth.commands[index + 3];
        const std::array<int, 3> expected = {62, 65, 69};
        expect(command.type == CommandType::NoteOn && command.midi_note == expected[index],
               "the second major-scale key must play a minor triad");
    }
    expect(model.state().get().active_chord_index == 1, "new chord must replace the previous active chord");

    synth.commands.clear();
    model.handleInput(keyEvent(piano::PianoKey::C, false));
    expect(synth.commands.empty(), "releasing an older held key must not stop the replacement chord");
    expect(model.state().get().active_chord_index == 1, "replacement chord must remain active");

    model.handleInput(keyEvent(piano::PianoKey::D, false));
    expect(synth.commands.size() == 3, "releasing the active chord key must release all three voices");
    expect(model.state().get().active_chord_index == -1, "released chord must clear active chord state");
}

void testChordModeIgnoresBlackKeys()
{
    FakeSynth synth;
    piano::PianoModel model(synth);
    enterChordMode(model, synth);

    model.handleInput(keyEvent(piano::PianoKey::CSharp, true));
    model.handleInput(keyEvent(piano::PianoKey::CSharp, false));
    expect(synth.commands.empty(), "black keys must not make sound in chord mode");
    expect(!model.state().get().pressed_notes[1], "black keys must not enter visual pressed state in chord mode");
}

void testTonalityAndTonicChangesReleaseChord()
{
    FakeSynth synth;
    piano::PianoModel model(synth);
    enterChordMode(model, synth);

    model.handleInput(keyEvent(piano::PianoKey::C, true));
    synth.commands.clear();
    model.handleInput(keyEvent(piano::PianoKey::ToggleTonality, true));
    expect(synth.commands.size() == 1 && synth.commands.back().type == CommandType::AllNotesOff &&
               synth.commands.back().immediate,
           "changing major/minor must immediately release active voices");
    expect(model.state().get().tonality == piano::ChordTonality::Minor, "M must change major chord mode to minor");
    expect(model.state().get().active_chord_index == -1, "tonality change must clear active chord state");

    model.handleInput(keyEvent(piano::PianoKey::ToggleTonality, false));
    synth.commands.clear();
    model.handleInput(keyEvent(piano::PianoKey::C, true));
    expectNoteOns(synth.commands, {60, 63, 67}, "C minor chord");

    synth.commands.clear();
    model.handleInput(keyEvent(piano::PianoKey::OctaveUp, true));
    expect(model.state().get().tonic == 1, "Right/C must transpose the chord tonic up one semitone");
    expect(model.state().get().octave == 4, "chord transposition must not change the piano octave");
    expect(synth.commands.size() == 1 && synth.commands.back().type == CommandType::AllNotesOff,
           "tonic transposition must release active voices");
    expect(model.state().get().active_chord_index == -1, "tonic transposition must clear active chord state");

    model.handleInput(keyEvent(piano::PianoKey::OctaveUp, false));
    model.handleInput(keyEvent(piano::PianoKey::OctaveDown, true));
    expect(model.state().get().tonic == 0, "Left/Z must transpose the chord tonic down one semitone");
    model.handleInput(keyEvent(piano::PianoKey::OctaveDown, false));
    model.handleInput(keyEvent(piano::PianoKey::OctaveDown, true));
    expect(model.state().get().tonic == 11, "tonic transposition must wrap below C to B");
}

void testModeChangeReleasesChordAndRestoresPianoControls()
{
    FakeSynth synth;
    piano::PianoModel model(synth);
    enterChordMode(model, synth);
    model.handleInput(keyEvent(piano::PianoKey::C, true));

    synth.commands.clear();
    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, true));
    expect(model.state().get().mode == piano::PianoMode::Piano, "Tab must return to piano mode");
    expect(synth.commands.size() == 1 && synth.commands.back().type == CommandType::AllNotesOff,
           "mode changes must release active chord voices");
    expect(model.state().get().active_chord_index == -1, "mode changes must clear active chord state");

    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, false));
    model.handleInput(keyEvent(piano::PianoKey::OctaveUp, true));
    expect(model.state().get().octave == 5, "Right/C must change octave again in piano mode");
    expect(model.state().get().tonic == 0, "piano octave control must not transpose chord tonic");
}

void testModeChangeClearsHeldTonalityToggle()
{
    FakeSynth synth;
    piano::PianoModel model(synth);
    enterChordMode(model, synth);

    model.handleInput(keyEvent(piano::PianoKey::ToggleTonality, true));
    expect(model.state().get().tonality == piano::ChordTonality::Minor, "M must enter minor tonality");
    expect(model.state().get().tonality_toggle_pressed, "held M must be reflected in model state");

    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, true));
    expect(model.state().get().mode == piano::PianoMode::Piano, "Tab must leave chord mode while M is held");
    expect(!model.state().get().tonality_toggle_pressed, "leaving chord mode must clear the held M latch");
    model.handleInput(keyEvent(piano::PianoKey::ToggleTonality, false));
    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, false));

    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, true));
    model.handleInput(keyEvent(piano::PianoKey::ToggleMode, false));
    model.handleInput(keyEvent(piano::PianoKey::ToggleTonality, true));
    expect(model.state().get().tonality == piano::ChordTonality::Major,
           "the first M press after returning to chord mode must work");
}

void testPianoPlayalongTiming()
{
    FakeSynth synth;
    piano::PianoModel model(synth, 0x12345678U);

    expect(model.state().get().keymap_visible, "the key map must be visible when Play Along starts");

    sendKey(model, synth, piano::PianoKey::C, true, 90);
    expect(!synth.active_handles.empty(), "the setup note must be sounding before Play Along starts");
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, 100);

    piano::PianoState state = model.state().get();
    expect(state.playalong_active && state.playalong_phase == piano::PlayalongPhase::Demo,
           "P must begin with the automatic Demo phase");
    expect(state.playalong_step == 0 && state.playalong_length >= 20 &&
               state.playalong_length <= piano::PianoState::kMaximumPlayalongLength,
           "piano Play Along must select a complete melody");
    expect(state.playalong_demo_target >= 0 && state.playalong_demo_target < 8 && state.playalong_target == -1,
           "Demo must expose only the blue key target");
    expect(std::none_of(state.pressed_notes.begin(), state.pressed_notes.end(), [](bool pressed) { return pressed; }),
           "starting Play Along must clear held-key visuals");
    expect(synth.active_handles.size() == 1 && synth.active_handles.front() == 128,
           "starting Play Along must replace ordinary held notes with its independent Demo voice");

    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, 101);
    expect(model.state().get().playalong_active, "duplicate P key-down must not cancel Play Along");
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, 102);

    const uint8_t first_duration = state.playalong_duration_eighths;
    const uint32_t gate_end      = 100 + pianoGateDurationMs(first_duration);
    const uint32_t step_end      = 100 + stepDurationMs(first_duration);
    tickAt(model, synth, gate_end - 1);
    expect(model.state().get().playalong_demo_target >= 0 && hasTimedVoice(synth),
           "the Demo note must remain visible and sounding until its gate ends");
    tickAt(model, synth, gate_end);
    expect(model.state().get().playalong_demo_target == -1 && !hasTimedVoice(synth),
           "the Demo gate gap must clear its target so repeated notes can retrigger");
    tickAt(model, synth, step_end - 1);
    expect(model.state().get().playalong_step == 0,
           "the next Demo target must not appear before the written duration ends");
    tickAt(model, synth, step_end);
    expect(model.state().get().playalong_step == 1 && model.state().get().playalong_demo_target >= 0,
           "the next Demo target must appear at the written duration boundary");
}

void testPianoPlayalongGuide()
{
    FakeSynth synth;
    piano::PianoModel model(synth, 0x89ABCDEFU);
    const DemoRun melody = runDemoToGuide(model, synth, 1000);

    piano::PianoState state = model.state().get();
    expect(melody.targets.size() == state.playalong_length && melody.targets.size() >= 20,
           "Demo must play every note in the selected melody");
    expect(melody.durations.size() == melody.targets.size(), "every melody note must carry a duration");
    expect(melody.guide_started_at - melody.interlude_started_at == piano::PianoModel::kPlayalongInterludeMs,
           "Guide must begin after the 500 ms Demo interlude");
    expect(state.playalong_phase == piano::PlayalongPhase::Guide && state.playalong_demo_target == -1 &&
               state.playalong_target == melody.targets.front(),
           "Guide must replace the blue Demo target with its yellow user target");
    expect(std::adjacent_find(melody.durations.begin(), melody.durations.end(), std::not_equal_to<>()) !=
               melody.durations.end(),
           "the selected melody must preserve varied written durations");

    int minimum_target = melody.targets.front();
    int maximum_target = melody.targets.front();
    for (std::size_t index = 0; index < melody.targets.size(); ++index) {
        expect(melody.targets[index] >= 0 && melody.targets[index] < 8,
               "piano melodies must stay on visible white keys");
        if (index > 0) {
            expect(std::abs(melody.targets[index] - melody.targets[index - 1]) <= 4,
                   "piano melodies must avoid broad adjacent jumps");
        }
        minimum_target = std::min(minimum_target, melody.targets[index]);
        maximum_target = std::max(maximum_target, melody.targets[index]);
    }
    expect(maximum_target - minimum_target <= 7, "piano melodies must stay within the visible keyboard");
    expect(countNoteOns(synth, 128, 128) == melody.targets.size(),
           "Demo must sound each melody note exactly once on its independent voice");

    uint32_t now_ms                 = melody.guide_started_at + 10;
    const int first_target          = state.playalong_target;
    const int wrong_target          = (first_target + 1) % static_cast<int>(piano::PianoState::kChordCount);
    const std::size_t command_count = synth.commands.size();
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(wrong_target)), true, now_ms);
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(wrong_target)), false, now_ms + 1);
    state = model.state().get();
    expect(state.playalong_step == 0 && state.playalong_target == first_target && state.playalong_success_revision == 0,
           "a wrong key must sound normally without advancing Guide");
    expect(synth.commands.size() == command_count + 2 && synth.commands[command_count].handle < 128,
           "a wrong Guide key must use the normal playable voice");

    now_ms += 10;
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(first_target)), true, now_ms);
    state = model.state().get();
    expect(state.playalong_target == -1 && state.playalong_step == 0 && state.playalong_success_revision == 1,
           "a correct key must celebrate immediately and hide the current target");
    expect(std::find(synth.active_handles.begin(), synth.active_handles.end(), 132) != synth.active_handles.end(),
           "a correct piano key must start the independent assisted voice");
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(first_target)), false, now_ms + 1);
    expect(std::find(synth.active_handles.begin(), synth.active_handles.end(), 132) != synth.active_handles.end(),
           "releasing the physical key must not truncate assisted playback");

    uint32_t duration = stepDurationMs(melody.durations.front());
    tickAt(model, synth, now_ms + duration - 1);
    expect(model.state().get().playalong_step == 0 && model.state().get().playalong_target == -1,
           "Guide must wait for the full written duration before showing its next target");
    tickAt(model, synth, now_ms + duration);
    expect(model.state().get().playalong_step == 1 && model.state().get().playalong_target == melody.targets[1],
           "Guide must reveal the next target exactly after the current duration");

    now_ms += duration;
    for (std::size_t index = 1; index < melody.targets.size(); ++index) {
        state = model.state().get();
        expect(state.playalong_step == index && state.playalong_target == melody.targets[index] &&
                   state.playalong_duration_eighths == melody.durations[index],
               "Guide targets and durations must match the demonstrated melody");
        sendKey(model, synth, whiteKey(static_cast<std::size_t>(melody.targets[index])), true, now_ms + 1);
        sendKey(model, synth, whiteKey(static_cast<std::size_t>(melody.targets[index])), false, now_ms + 2);
        duration = stepDurationMs(melody.durations[index]);
        tickAt(model, synth, now_ms + duration);
        expect(model.state().get().playalong_step == index,
               "a completed target must still occupy its full written duration");
        tickAt(model, synth, now_ms + duration + 1);
        now_ms += duration + 1;
    }

    state = model.state().get();
    expect(!state.playalong_active && state.playalong_phase == piano::PlayalongPhase::Inactive &&
               state.playalong_target == -1 && state.playalong_length == 0,
           "Play Along must clear only after the final note's full duration");
    expect(state.playalong_success_target == melody.targets.back(),
           "natural completion must preserve the final target until its success effect finishes");
    expect(state.playalong_success_revision == melody.targets.size(),
           "every correct melody note must publish one success revision");
    expect(!hasTimedVoice(synth), "finishing Play Along must release all timed voices");
}

void testChordPlayalongAndRandomSelection()
{
    FakeSynth synth;
    piano::PianoModel model(synth, 0xABCDEF01U);
    enterChordMode(model, synth);

    const DemoRun first     = runDemoToGuide(model, synth, 2000);
    piano::PianoState state = model.state().get();
    expect(first.targets.size() == state.playalong_length && first.targets.size() >= 9,
           "chord Play Along must contain a complete progression and resolving cadence");
    uint32_t total_eighths = 0;
    for (std::size_t index = 0; index < first.targets.size(); ++index) {
        expect(first.targets[index] >= 0 && first.targets[index] <= 5,
               "chord progressions must stay on the six common scale degrees");
        total_eighths += first.durations[index];
    }
    expect(total_eighths >= 40 && total_eighths <= 64, "chord progressions must last between 20 and 32 beats");
    expect(first.targets.back() == 0, "chord progressions must resolve to the tonic");
    expect(countNoteOns(synth, 128, 130) == first.targets.size() * 3,
           "each demonstrated chord must use three independent voices");

    const uint32_t revision = state.playalong_success_revision;
    sendKey(model, synth, piano::PianoKey::CSharp, true, first.guide_started_at + 1);
    sendKey(model, synth, piano::PianoKey::CSharp, false, first.guide_started_at + 2);
    expect(model.state().get().playalong_success_revision == revision, "black keys must not advance chord Play Along");

    const int target = model.state().get().playalong_target;
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(target)), true, first.guide_started_at + 3);
    expect(countNoteOns(synth, 132, 134) == 3, "a correct guided chord must start three assisted voices");
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(target)), false, first.guide_started_at + 4);
    expect(std::count_if(synth.active_handles.begin(), synth.active_handles.end(),
                         [](piano::VoiceHandle handle) { return handle >= 132 && handle <= 134; }) == 3,
           "releasing a chord key must not truncate its assisted voicing");

    const int manual_target = (target + 1) % 6;
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(manual_target)), true, first.guide_started_at + 5);
    expect(std::count_if(synth.active_handles.begin(), synth.active_handles.end(),
                         [](piano::VoiceHandle handle) { return handle < 128; }) == 3,
           "a non-target chord must remain normally playable while the assisted chord rings");
    const uint32_t step_end = first.guide_started_at + 3 + stepDurationMs(first.durations.front());
    tickAt(model, synth, step_end);
    expect(model.state().get().active_chord_index == manual_target,
           "Guide timing must not discard ownership of a held manual chord");
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(manual_target)), false, step_end + 1);
    expect(std::none_of(synth.active_handles.begin(), synth.active_handles.end(),
                        [](piano::VoiceHandle handle) { return handle < 128; }),
           "a held manual chord must still release after Guide advances");

    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, step_end + 2);
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, step_end + 3);
    expect(!model.state().get().playalong_active && model.state().get().playalong_success_target == -1 &&
               !hasTimedVoice(synth),
           "P must cancel Guide and release its assisted voices");

    const DemoRun second = runDemoToGuide(model, synth, step_end + 10);
    expect(second.targets != first.targets || second.durations != first.durations,
           "consecutive Play Along runs must select different progressions");

    const int second_target = model.state().get().playalong_target;
    const int wrong_chord   = (second_target + 1) % 6;
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(wrong_chord)), true, second.guide_started_at + 1);
    expect(std::count_if(synth.active_handles.begin(), synth.active_handles.end(),
                         [](piano::VoiceHandle handle) { return handle < 128; }) == 3,
           "wrong Guide chords must use normal playable voices");
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, second.guide_started_at + 2);
    expect(!model.state().get().playalong_active && synth.active_handles.empty(),
           "cancelling Play Along must release a held manual chord without stranding voices");
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, second.guide_started_at + 3);
    sendKey(model, synth, whiteKey(static_cast<std::size_t>(wrong_chord)), false, second.guide_started_at + 4);
}

void testPlayalongCancellationAndContextChanges()
{
    FakeSynth synth;
    piano::PianoModel model(synth, 42U);

    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, 10);
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, 11);
    expect(hasTimedVoice(synth), "Demo must have an active timed voice before cancellation");
    sendKey(model, synth, piano::PianoKey::OctaveUp, true, 12);
    expect(!model.state().get().playalong_active && !hasTimedVoice(synth),
           "changing octave must cancel Play Along and release Demo audio");
    sendKey(model, synth, piano::PianoKey::OctaveUp, false, 13);

    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, 20);
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, 21);
    sendKey(model, synth, piano::PianoKey::ToggleMode, true, 22);
    expect(model.state().get().mode == piano::PianoMode::Chord && !model.state().get().playalong_active &&
               !hasTimedVoice(synth),
           "changing mode must cancel Play Along and release Demo audio");
    sendKey(model, synth, piano::PianoKey::ToggleMode, false, 23);

    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, 30);
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, 31);
    sendKey(model, synth, piano::PianoKey::ToggleTonality, true, 32);
    expect(!model.state().get().playalong_active && !hasTimedVoice(synth),
           "changing tonality must cancel chord Play Along and release Demo audio");
    sendKey(model, synth, piano::PianoKey::ToggleTonality, false, 33);

    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, 40);
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, 41);
    model.releaseAll(true);
    expect(!model.state().get().playalong_active && !hasTimedVoice(synth),
           "releaseAll must stop Play Along and all timed voices");

    sendKey(model, synth, piano::PianoKey::TogglePlayalong, true, 50);
    sendKey(model, synth, piano::PianoKey::TogglePlayalong, false, 51);
    model.reset();
    expect(!model.state().get().playalong_active && !hasTimedVoice(synth),
           "reset must return Play Along and its voices to the initial state");
}

}  // namespace

int main()
{
    testPolyphonicEdges();
    testOctaveIsLatchedPerKeyPress();
    testKeymapStartsVisibleAndToggles();
    testPanicReleasesVisualAndAudioState();
    testChordLabelsAndDegrees();
    testChordVoicingAndReplacement();
    testChordModeIgnoresBlackKeys();
    testTonalityAndTonicChangesReleaseChord();
    testModeChangeReleasesChordAndRestoresPianoControls();
    testModeChangeClearsHeldTonalityToggle();
    testPianoPlayalongTiming();
    testPianoPlayalongGuide();
    testChordPlayalongAndRandomSelection();
    testPlayalongCancellationAndContextChanges();
    return 0;
}
