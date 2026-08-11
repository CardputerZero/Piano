# CardputerZero Piano

A compact sampled piano and guided music playground for M5Stack CardputerZero.

<img width="320" height="170" alt="11111" src="https://github.com/user-attachments/assets/533ca646-bd44-4252-97af-6e57329575bf" />

## Features

- Play a polyphonic sampled piano across selectable octaves
- Switch to a diatonic chord layout with major and minor keys
- Follow familiar melodies and chord progressions with Play Along
- Show or hide the on-screen keyboard map

## Controls

- White keys: `A S D F G H J K`
- Black keys: `W E T Y U`
- Keymap: `Space` toggles the on-screen keyboard map
- Mode: `Tab` switches between Piano and Chord
- Octave / key: Left / Right or `Z` / `C`
- Chord tonality: `M` switches between Major and Minor
- Play Along: `P` plays a demonstration, then guides each note or chord
- Exit: Hold `Esc`

## Build

Fetch the pinned dependencies once after cloning:

```sh
./bootstrap.sh
```

Build and run the SDL desktop version:

```sh
cmake -S . -B build/sdl -DPIANO_USE_SDL=ON -DPIANO_BUILD_TESTS=ON
cmake --build build/sdl -j"$(nproc)"
ctest --test-dir build/sdl --output-on-failure
./dist/M5CardputerZero-Piano
```

Set `PIANO_SOUNDFONT_PATH=/path/to/piano.sf2` to test another SF2 bank. Preset 0 is used.

## Package

Build the CardputerZero ARM64 APPLaunch package:

```sh
./packaging/deb/package_deb.sh
```

The script builds natively on ARM64 or cross-compiles from x86 Linux and writes these files to `dist/`:

```text
dist/M5CardputerZero-Piano
dist/piano.sf2
dist/m5cardputerzero-piano_<version>_m5stack1_arm64.deb
```

The Debian package installs the SoundFont automatically. For standalone device testing, keep `piano.sf2` beside the
executable. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency and SoundFont attribution.
