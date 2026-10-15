# DeskTicker

DeskTicker is a small desktop clock widget built with Qt Widgets and C++17. Its application UI is designed to run on Windows, macOS, and Linux.

## Requirements

- CMake 3.21 or newer
- Qt 6.2 or newer, including the Widgets module
- A C++17 compiler

## Build

Configure and build with CMake:

```sh
cmake -S . -B build
cmake --build build --config Release
```

On single-configuration generators, select the build type during configuration if desired, for example `-DCMAKE_BUILD_TYPE=Release`.

On Windows, CMake uses the Qt resource compiler for the application icon and runs `windeployqt` after building to bundle the Qt runtime. The optional Windows installer can be generated with `installer/build-installer.ps1` and the Qt Installer Framework.

## Installers

- Windows: versioned Qt Installer Framework `.exe` installer.
- Linux: versioned `.deb` package for Debian/Ubuntu systems.
- macOS: versioned `.dmg` containing the app bundle and deployed Qt runtime; drag the app to Applications.

Installer artifact names use the version declared in `CMakeLists.txt`. Linux and macOS packages are unsigned.

## Platform-specific behavior

The widget and its interface use Qt APIs. Starting DeskTicker when you sign in uses native integration for Windows, macOS, and Linux desktop environments; other platforms report that this setting is unsupported. The Windows resource file, Qt deployment step, and installer are Windows packaging details and are conditional or isolated from the cross-platform application build.

## GitHub Actions

The CI workflow builds the application on Windows, macOS, and Ubuntu. To build and attach versioned Linux and macOS installers to an existing GitHub release, manually run the **Build and publish installers** workflow and provide the matching release tag.
