# Building WDForceStereo

WDForceStereo v1.0 is a 64-bit Windows XAudio2 2.7 stereo downmix fix for Watch Dogs (2014).

The project builds three independent loader binaries around one shared audio core:

```text
dinput8.dll       standalone DInput8 proxy
winmm.dll         WinMM proxy, including NexusTools-compatible use
WDForceStereo.asi dedicated ASI plugin
```

All three compile `src/wd_core.c`; only the loader-specific translation unit changes. The tested toolchain is Clang targeting the MSVC x64 ABI plus `lld-link`, without the CRT.

## Requirements

- `clang` with the `x86_64-pc-windows-msvc` target
- `lld-link`

No Visual Studio project or legacy DirectX SDK is required.

## 1. Optional build identity

`src/wd_build.h` contains a fallback build ID of `unknown`. GitHub Actions replaces this header in the runner workspace with the short source commit before compilation so `WDForceStereo.log` identifies the exact CI build.

For a local build you can leave the fallback unchanged, or replace `WD_BUILD_ID` with your own identifier before compiling.

## 2. Build the minimal kernel32 import library

The repository contains `kernel32.def` with the Win32 imports required by the core and loader layers.

```bat
lld-link /lib /machine:x64 /def:kernel32.def /out:kernel32.lib
```

## 3. Compile the shared core

```bat
clang --target=x86_64-pc-windows-msvc -c src\wd_core.c ^
  -o wd_core.obj -O2 -ffreestanding -fno-builtin ^
  -fno-stack-protector -Wno-incompatible-library-redeclaration
```

## 4. Build the standalone DInput8 proxy

Compile the loader:

```bat
clang --target=x86_64-pc-windows-msvc -c src\dinput8_proxy.c ^
  -o dinput8_proxy.obj -O2 -ffreestanding -fno-builtin ^
  -fno-stack-protector -Wno-incompatible-library-redeclaration
```

Link it with the shared core:

```bat
lld-link /dll /machine:x64 /entry:DllMain /nodefaultlib /opt:ref ^
  wd_core.obj dinput8_proxy.obj kernel32.lib ^
  /def:src\dinput8_exports.def /out:dinput8.dll
```

The DInput8 proxy forwards these exports to the real System32 `dinput8.dll`:

```text
DirectInput8Create
DllCanUnloadNow
DllGetClassObject
DllRegisterServer
DllUnregisterServer
```

## 5. Build the WinMM proxy

Compile the loader:

```bat
clang --target=x86_64-pc-windows-msvc -c src\winmm_proxy.c ^
  -o winmm_proxy.obj -O2 -ffreestanding -fno-builtin ^
  -fno-stack-protector -Wno-incompatible-library-redeclaration
```

Link it with the shared core and WinMM export definition:

```bat
lld-link /dll /machine:x64 /entry:DllMain /nodefaultlib /opt:ref ^
  wd_core.obj winmm_proxy.obj kernel32.lib ^
  /def:src\winmm_exports.def /out:winmm.dll
```

`winmm_exports.inc` is the common forwarding table used to generate the proxy stubs. `winmm_exports.def` exposes the public WinMM names and maps them to those stubs.

## 6. Build the dedicated ASI plugin

Compile the ASI entry point:

```bat
clang --target=x86_64-pc-windows-msvc -c src\asi_entry.c ^
  -o asi_entry.obj -O2 -ffreestanding -fno-builtin ^
  -fno-stack-protector -Wno-incompatible-library-redeclaration
```

Link it with the shared core:

```bat
lld-link /dll /machine:x64 /entry:DllMain /nodefaultlib /opt:ref ^
  wd_core.obj asi_entry.obj kernel32.lib ^
  /out:WDForceStereo.asi
```

The ASI is a dedicated binary. It is **not** a renamed copy of `dinput8.dll` and does not expose the DInput8 proxy API.

## Installation tests

Test only one WDForceStereo loader at a time.

### Standalone DInput8

Next to `watch_dogs.exe`:

```text
dinput8.dll
WDForceStereo.ini
```

### NexusTools with WinMM

Keep NexusTools' own `dinput8.dll` and add:

```text
winmm.dll
WDForceStereo.ini
```

### NexusTools with ASI

Keep NexusTools' own `dinput8.dll` and add:

```text
WDForceStereo.asi
WDForceStereo.ini
```

The ASI is loaded by NexusTools' ASI Injection Helper and does not need to appear in NexusTools' Installed Mods list.

The DInput8, WinMM and ASI routes have been tested in-game. The original affected cinematic has also been A/B checked against the unmodified game after the shared-core refactor.

## Runtime verification

Set `Log=1` in `WDForceStereo.ini`. Each launch creates a fresh `WDForceStereo.log` next to the game executable.

A successful affected path should identify the loader and build, then show the relevant stages, including:

```text
XAudio2 2.7 hook installed.
CreateMasteringVoice: requested=6 passed=2 ... [FORCED STEREO]
CONFIGURED DOWNMIX 6->2: OK ...
```

The log also records the original and final matrices. This is the preferred first diagnostic when a user reports that the fix did not load or did not affect a scene.

## Architecture notes

`wd_core.c` owns the configuration, support logging, XAudio2 2.7 hook and matrix repair. `dinput8_proxy.c`, `winmm_proxy.c` and `asi_entry.c` only provide loader-specific entry/forwarding behavior and call `WDCoreProcessAttach()` with their loader identity.

The core resolves COM/XAudio2 functions dynamically. The small subset of the XAudio2 2.7 ABI required by the hook is declared locally, so legacy DirectX SDK headers are not needed.

For reproducible CI commands, see `.github/workflows/build.yml` and `.github/workflows/release.yml`.
