# Contributing to UNX DJ Engine

First off, thank you for considering contributing to UNX DJ Engine! It's people like you that make this project a great tool for the community.

## How Can I Contribute?

### Reporting Bugs
*   Check the [Issues](https://github.com/rayocta303/UNX-DJ-ENGINE/issues) to see if the bug has already been reported.
*   If not, open a new issue with a clear title and description.
*   Include steps to reproduce the bug and details about your environment (OS, Hardware, etc.).

### Suggesting Enhancements
*   Open an issue to discuss your idea before implementing it.
*   Provide a clear explanation of the feature and its benefits.

### Pull Requests
1.  Fork the repository and create your branch from `master`.
2.  If you've added code that should be tested, add tests.
3.  Ensure your code adheres to the project's coding style.
4.  Issue a pull request with a detailed description of your changes.

## Development Setup

### Toolchain
This project heavily relies on the **Zig toolchain** (0.11.0+) for cross-compilation. Ensure `zig` is in your PATH.

### Building
*   **Windows**: Use `./build.bat`
*   **Linux**: Use `./build.sh` or `make`

### Coding Standards
*   **C/C++**: We follow a clean, low-allocation style suitable for embedded systems.
*   **Memory**: Avoid frequent heap allocations in the audio thread.
*   **Portability**: Use platform-specific defines (`PLATFORM_DRM`, `PLATFORM_IOS`, etc.) only when necessary.

## Community
Join our community on [GitHub Discussions](https://github.com/rayocta303/UNX-DJ-ENGINE/discussions) to stay updated and discuss developments!

---
By contributing, you agree that your contributions will be licensed under its [MIT License](LICENSE).
