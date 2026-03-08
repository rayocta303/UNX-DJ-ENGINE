# UNX DJ ENGINE

UNX DJ ENGINE is an experimental DJ Media Player firmware subset, ported to C and Raylib for high-performance cross-platform development, simulation, and embedded deployment. It aims to provide a professional-grade mixing experience inspired by industry-standard hardware.

## Features

### Audio Engine
- High-fidelity 32-bit float internal mixing pipeline.
- Multi-format decoding support: MP3 (minimp3), WAV, and AIFF (dr_wav).
- Advanced Master Tempo using SOLA-lite time-stretching algorithm.
- Professional 3-Band ISO EQs and Filters (Biquad).
- Integrated Sound Color FX and BPM-synced Beat FX.
- Reliable mono-to-stereo automatic upmixing.

### User Interface
- Ultra-responsive UI rendered with Raylib and OpenGL.
- Multi-style Waveform displays (Blue, RGB, 3-Band Spectrum).
- Dynamic layout scaling for multiple resolutions and high-DPI displays.
- Hardware-accurate Top Bar, Deck Strips, and FX Panels.

### Library and Browser
- Deep integration with Rekordbox (PDB/ANLZ) databases.
- Serato metadata and waveform parsing support.
- Advanced Browser with Playlist Bank (Drag-and-Drop shortcuts).
- Multi-device storage scanning (USB/SD/Internal).

### Platform Support
- Windows (x64): Native development and simulation.
- Linux (ARM64): Optimized for embedded targets using DRM-KMS and GLES2.

## Upcoming Features (TODO)
- ColorFX Tab (UI)
- Mixer View Section (UI)
- Integrated Audio Recording (WAV/FLAC).
- Comprehensive Hardware Mapping (MIDI/HID over SPI).
- Pro DJ Link / Networking for multi-player synchronization.
- Enhanced Library Management (Tagging and advanced search).
- More Sound Color FX and Beat FX.
- Serato Library integration into browser.
- Load tracks directly from Serato database.

## Build Instructions

The project uses Zig as a C/C++ compiler for seamless cross-compilation.

### Windows
Run the provided build script:
```powershell
./build.bat
```

### Linux / Embedded
Use the included Makefile:
```bash
make PLATFORM=LINUX_ARM64
```

## Credits

Developed as part of the UNX DJ ENGINE project ecosystem.

Special thanks to:
- [Mixxx](https://mixxx.org/) development team for architectural insights and DSP logic.
- [Raylib](https://www.raylib.com/), [minimp3](https://github.com/lieff/minimp3), and [dr_libs](https://github.com/mackron/dr_libs) contributors.

## Social Media and Links

Follow the project progress:
Follow the project progress:
- GitHub: [github.com/unxchr](https://github.com/rayocta303)
- YouTube: [youtube.com/@unxchr](https://youtube.com/@unxchr)
- Instagram: [instagram.com/unxchr](https://instagram.com/unxchr)

## Donation

If you find this project useful and would like to support its development:
- PayPal: [paypal.me/unxchr](https://paypal.me/unxchr)
- Trakteer: [saweria.co/patradev](https://saweria.co/patradev)

---
Disclaimer: This project is for educational and experimental purposes only.
