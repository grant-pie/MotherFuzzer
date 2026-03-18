# MotherFuzzer

**MotherFuzzer** is a VCV Rack plugin module that applies a rich, musical fuzz/distortion effect to audio signals. It provides intuitive controls for drive, tone, and output level, plus CV inputs for modulation.

## Features

- **Drive**: 1×–100× soft clipping drive
- **Tone**: simple low-pass filter with mix between filtered/unfiltered signal
- **Volume**: output level compensation
- **CV inputs** for Drive, Tone, and Volume
- **Clip indicator** LED

## Module Layout

- **Drive** knob + CV input
- **Tone** knob + CV input
- **Volume** knob + CV input
- **Audio In** / **Audio Out**
- **Clip LED** shows when the signal is heavily driven

## Building

### Prerequisites

- VCV Rack SDK (matching your Rack version)
- A C++ compiler (e.g., GCC/Clang on Linux/macOS or MSVC on Windows)
- `make`

### Build steps

1. Clone or download the plugin source.
2. Ensure `RACK_DIR` points to your Rack SDK root (default is two levels up from this repo).
3. Run:

```sh
make
```

This will build `bin/` and create the plugin bundle for Rack.

### Packaging

To create a distributable ZIP package:

```sh
make dist
```

The resulting zip will include the compiled plugin, `plugin.json`, and any additional resources.

## Usage

- Insert **MotherFuzzer** into a rack.
- Patch audio into the input and take the output to your mixer or effects chain.
- Use the **Drive** control to dial in distortion and the **Tone** control to shape brightness.
- Use CV inputs to modulate drive, tone, or volume in real time.

## License

This plugin is marked as **proprietary** in `plugin.json`.

---

Happy patching! 🎛️
