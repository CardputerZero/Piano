#include "audio/synth_engine.hpp"

#include "audio/soundfont_renderer.hpp"

#include <miniaudio.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#ifndef PIANO_DEFAULT_SOUNDFONT_PATH
#define PIANO_DEFAULT_SOUNDFONT_PATH "assets/soundfonts/piano.sf2"
#endif

namespace piano {
namespace {

constexpr ma_uint32 kChannels          = 2;
constexpr ma_uint32 kSampleRate        = 48000;
constexpr ma_uint32 kPanicDrainFrames  = 256;
constexpr std::size_t kCommandCapacity = 128;
constexpr int kPanicDrainPeriodCount   = 4;

std::string soundFontPath()
{
    const char* override_path = std::getenv("PIANO_SOUNDFONT_PATH");
    if (override_path && *override_path) {
        return override_path;
    }

    const std::filesystem::path configured_path = PIANO_DEFAULT_SOUNDFONT_PATH;
    std::error_code error;
    if (std::filesystem::is_regular_file(configured_path, error)) {
        return configured_path.string();
    }

    error.clear();
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        const std::filesystem::path adjacent_path = executable.parent_path() / "piano.sf2";
        if (std::filesystem::is_regular_file(adjacent_path, error)) {
            return adjacent_path.string();
        }
    }
    return configured_path.string();
}

enum class CommandType : std::uint8_t {
    NoteOn,
    NoteOff,
    AllNotesOff,
};

struct SynthCommand {
    CommandType type   = CommandType::AllNotesOff;
    VoiceHandle handle = 0;
    int midi_note      = 0;
    float velocity     = 0.0f;
    bool immediate     = false;
};

template <std::size_t Capacity>
class CommandQueue {
public:
    static_assert(Capacity > 1, "Command queue needs at least two slots");

    bool push(const SynthCommand& command)
    {
        const std::size_t write = _write.load(std::memory_order_relaxed);
        const std::size_t next  = increment(write);
        if (next == _read.load(std::memory_order_acquire)) {
            return false;
        }
        _commands[write] = command;
        _write.store(next, std::memory_order_release);
        return true;
    }

    bool pop(SynthCommand& command)
    {
        const std::size_t read = _read.load(std::memory_order_relaxed);
        if (read == _write.load(std::memory_order_acquire)) {
            return false;
        }
        command = _commands[read];
        _read.store(increment(read), std::memory_order_release);
        return true;
    }

    void discard()
    {
        _read.store(_write.load(std::memory_order_acquire), std::memory_order_release);
    }

private:
    static constexpr std::size_t increment(std::size_t index)
    {
        return (index + 1) % Capacity;
    }

    std::array<SynthCommand, Capacity> _commands{};
    std::atomic<std::size_t> _read{0};
    std::atomic<std::size_t> _write{0};
};

}  // namespace

struct SynthEngine::Impl {
    ma_context context{};
    ma_device device{};
    SoundFontRenderer voices;
    CommandQueue<kCommandCapacity> commands;
    std::atomic<bool> panic_requested{false};
    std::atomic<bool> overflow_reported{false};
    bool context_initialized = false;
    bool device_initialized  = false;
    bool device_running      = false;

    ~Impl()
    {
        stop();
    }

    bool start()
    {
        if (device_running) {
            return true;
        }

        const std::string soundfont_path = soundFontPath();
        if (!voices.ready() && !voices.load(soundfont_path, static_cast<int>(kSampleRate))) {
            spdlog::error("Piano synth: failed to load SoundFont: {}", soundfont_path);
            return false;
        }
        spdlog::info("Piano synth: SoundFont loaded path={} preset={}", soundfont_path, voices.presetName());

#if PIANO_USE_PULSEAUDIO
        ma_backend backends[]          = {ma_backend_pulseaudio};
        const ma_result context_result = ma_context_init(backends, 1, nullptr, &context);
#else
        const ma_result context_result = ma_context_init(nullptr, 0, nullptr, &context);
#endif
        if (context_result != MA_SUCCESS) {
            spdlog::error("Piano synth: audio context initialization failed: {}",
                          ma_result_description(context_result));
            return false;
        }
        context_initialized = true;

        ma_device_config config       = ma_device_config_init(ma_device_type_playback);
        config.playback.format        = ma_format_f32;
        config.playback.channels      = kChannels;
        config.sampleRate             = kSampleRate;
        config.dataCallback           = dataCallback;
        config.pUserData              = this;
        const ma_result device_result = ma_device_init(&context, &config, &device);
        if (device_result != MA_SUCCESS) {
            spdlog::error("Piano synth: playback device initialization failed: {}",
                          ma_result_description(device_result));
            cleanupContext();
            return false;
        }
        device_initialized = true;

        const ma_result start_result = ma_device_start(&device);
        if (start_result != MA_SUCCESS) {
            spdlog::error("Piano synth: playback device start failed: {}", ma_result_description(start_result));
            cleanupDevice();
            cleanupContext();
            return false;
        }
        device_running = true;
        spdlog::info("Piano synth: started backend={} output={}/{}ch/{}Hz internal={}/{}ch/{}Hz period={}x{}",
                     ma_get_backend_name(context.backend), ma_get_format_name(device.playback.format),
                     device.playback.channels, device.sampleRate, ma_get_format_name(device.playback.internalFormat),
                     device.playback.internalChannels, device.playback.internalSampleRate,
                     device.playback.internalPeriodSizeInFrames, device.playback.internalPeriods);
        return true;
    }

    void stop()
    {
        if (device_running) {
            ma_device_stop(&device);
            device_running = false;
        }
        voices.allNotesOff(true);
        std::array<float, kPanicDrainFrames * kChannels> drain_buffer{};
        for (int period = 0; period < kPanicDrainPeriodCount && voices.activeVoiceCount() > 0; ++period) {
            voices.render(drain_buffer.data(), kPanicDrainFrames, kChannels);
        }
        commands.discard();
        panic_requested.store(false, std::memory_order_release);
        cleanupDevice();
        cleanupContext();
    }

    bool running() const
    {
        return device_running;
    }

    void enqueue(const SynthCommand& command)
    {
        if (!device_running) {
            return;
        }
        if (commands.push(command)) {
            return;
        }

        panic_requested.store(true, std::memory_order_release);
        if (!overflow_reported.exchange(true, std::memory_order_acq_rel)) {
            spdlog::error("Piano synth: command queue overflow; requesting all-notes-off");
        }
    }

private:
    void cleanupDevice()
    {
        if (device_initialized) {
            ma_device_uninit(&device);
            device_initialized = false;
        }
    }

    void cleanupContext()
    {
        if (context_initialized) {
            ma_context_uninit(&context);
            context_initialized = false;
        }
    }

    void processCommands()
    {
        if (panic_requested.exchange(false, std::memory_order_acq_rel)) {
            voices.allNotesOff(true);
            commands.discard();
            overflow_reported.store(false, std::memory_order_release);
            return;
        }

        SynthCommand command;
        while (commands.pop(command)) {
            switch (command.type) {
                case CommandType::NoteOn:
                    voices.noteOn(command.handle, command.midi_note, command.velocity);
                    break;
                case CommandType::NoteOff:
                    voices.noteOff(command.handle);
                    break;
                case CommandType::AllNotesOff:
                    voices.allNotesOff(command.immediate);
                    break;
            }
        }
    }

    static void dataCallback(ma_device* callback_device, void* output, const void* input, ma_uint32 frame_count)
    {
        (void)input;
        auto* self = static_cast<Impl*>(callback_device->pUserData);
        auto* out  = static_cast<float*>(output);
        if (!out) {
            return;
        }
        if (!self) {
            std::memset(out, 0, static_cast<std::size_t>(frame_count) * kChannels * sizeof(float));
            return;
        }

        self->processCommands();
        self->voices.render(out, frame_count, kChannels);
    }
};

SynthEngine::SynthEngine() : _impl(std::make_unique<Impl>())
{
}

SynthEngine::~SynthEngine() = default;

bool SynthEngine::start()
{
    return _impl->start();
}

void SynthEngine::stop()
{
    _impl->stop();
}

bool SynthEngine::running() const
{
    return _impl->running();
}

void SynthEngine::noteOn(VoiceHandle handle, int midi_note, float velocity)
{
    _impl->enqueue(
        {CommandType::NoteOn, handle, std::clamp(midi_note, 0, 127), std::clamp(velocity, 0.0f, 1.0f), false});
}

void SynthEngine::noteOff(VoiceHandle handle)
{
    _impl->enqueue({CommandType::NoteOff, handle, 0, 0.0f, false});
}

void SynthEngine::allNotesOff(bool immediate)
{
    _impl->enqueue({CommandType::AllNotesOff, 0, 0, 0.0f, immediate});
}

}  // namespace piano
