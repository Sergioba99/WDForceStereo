# WDForceStereo

XAudio2 2.7 stereo downmix fix for **Watch Dogs (2014)** on Windows.

## What it fixes

On affected stereo setups, Watch Dogs can create a 6-channel XAudio2 mastering path and then generate a 6→2 output matrix that keeps only Front Left and Front Right. The Front Center channel — where important audio such as dialogue can be carried — is effectively discarded, making some voices extremely quiet or inaudible.

WDForceStereo hooks the relevant XAudio2 2.7 calls, forces the affected 6-channel mastering request to stereo, and repairs the game's resulting stereo matrix. The mix is configurable through `WDForceStereo.ini`.

The original failure and the repaired result have been A/B tested in an affected early-game cinematic.

## Installation

WDForceStereo provides three loader variants. **Install only one.** All three use the same shared audio-fix core.

### Standalone — DInput8 proxy

Recommended when NexusTools or another `dinput8.dll` loader is not installed.

Copy these files next to `watch_dogs.exe`:

```text
dinput8.dll
WDForceStereo.ini
```

Then start the game normally.

### NexusTools — WinMM proxy

Recommended NexusTools-compatible proxy option. Keep NexusTools' own `dinput8.dll` in place.

Copy these files next to `watch_dogs.exe`:

```text
winmm.dll
WDForceStereo.ini
```

Then start the game normally.

### NexusTools — ASI plugin

Alternatively, keep NexusTools' own `dinput8.dll` and copy:

```text
WDForceStereo.asi
WDForceStereo.ini
```

next to `watch_dogs.exe`. NexusTools' ASI Injection Helper loads the plugin.

`WDForceStereo.asi` may not appear in NexusTools' Installed Mods list. That is expected because it is an ASI plugin rather than a NexusTools `modconfig` mod.

### Important

Do **not** install more than one WDForceStereo loader at the same time. In particular, do not replace NexusTools' own `dinput8.dll` with WDForceStereo's DInput8 proxy.

## Configuration

```ini
[Audio]
FrontGain=1.000
CenterGain=1.000
SurroundGain=0.000
LFEGain=0.000
MasterGain=1.000

[Debug]
Log=1
```

- `FrontGain` — Front Left / Front Right gain.
- `CenterGain` — Front Center → L/R gain; primarily controls affected dialogue.
- `SurroundGain` — rear/surround → stereo gain.
- `LFEGain` — LFE → stereo gain.
- `MasterGain` — final multiplier for the repaired matrix.
- `Log` — `1` enables `WDForceStereo.log`; `0` disables it.

Gain values are clamped to `0.0`–`4.0`. Both `.` and `,` are accepted as decimal separators. Configuration is read at startup, so restart Watch Dogs after editing the INI.

The defaults reproduce the known-working dialogue fix. If `WDForceStereo.ini` is missing, the mod recreates it next to `watch_dogs.exe` using the safe defaults when possible. If the directory is not writable, the compiled defaults remain active.

Useful linear-gain references:

```text
0.500  ≈ -6 dB
0.707  ≈ -3 dB
1.000  =   0 dB
1.414  ≈ +3 dB
2.000  ≈ +6 dB
```

## Technical summary

During investigation, XAudio2 correctly reported the physical output device as stereo (2 channels, speaker mask `0x3`). Watch Dogs nevertheless created a 6-channel mastering path.

The affected 6-channel game mix was observed as 32-bit float at 48 kHz with speaker mask `0x3F`. Dialogue-like audio was present in the Front Center channel.

Once the game's 6-channel mastering request was forced to stereo, Watch Dogs generated this effective 6→2 matrix:

```text
        FL   FR   FC   LFE  BL   BR
L       1    0    0    0    0    0
R       0    1    0    0    0    0
```

The stereo path therefore discarded Front Center entirely. With the default WDForceStereo configuration, the repaired matrix is effectively:

```text
        FL   FR   FC   LFE  BL   BR
L       1    0    1    0    0    0
R       0    1    1    0    0    0
```

`SurroundGain` and `LFEGain` can optionally fold those channels into stereo as well.

## Source layout

```text
src/
├── wd_core.c             # shared XAudio2 hook, config, logging and downmix
├── wd_core.h             # shared loader/core interface
├── wd_build.h            # build identity fallback; CI stamps the commit ID
├── dinput8_proxy.c       # standalone DInput8 proxy loader
├── dinput8_exports.def   # DInput8 export surface
├── winmm_proxy.c         # WinMM proxy loader
├── winmm_exports.def     # public WinMM exports mapped to proxy stubs
├── winmm_exports.inc     # shared WinMM forwarding table
└── asi_entry.c           # dedicated ASI loader entry
```

The three loaders are intentionally separate binaries but call the same `WDCoreProcessAttach()` implementation. The core is freestanding x64 C and does not link the CRT.

## Building

See [BUILD.md](BUILD.md). GitHub Actions uses the same Clang/LLD commands documented there.

## Current version

Current public version: **v1.1**.

v1.1 introduces the shared-core / multi-loader architecture, adds dedicated WinMM and ASI variants, improves support logging and build identification, and removes the obsolete monolithic implementation used during development.

The DInput8, WinMM and ASI loader routes have been tested in-game. The original low-dialogue case was also A/B tested against the unmodified game after the shared-core refactor.

## Logging and support

With `Log=1`, WDForceStereo creates a fresh `WDForceStereo.log` for each game launch. The log is intended to make support reports self-contained and records:

- WDForceStereo version and build commit
- loader route (`DInput8`, `WinMM` or `ASI`)
- loaded gain configuration
- XAudio2 hook initialization result
- forced 6→2 mastering conversion
- original and repaired 6→2 matrices
- final downmix result

If reporting a problem, attach `WDForceStereo.log` and state which installation method you used. Set `Log=0` if logging is not wanted.

## Reproducible source and binaries

The release binaries are built from this repository by GitHub Actions. The exact build commands are public in `.github/workflows/build.yml` and `.github/workflows/release.yml`, and [BUILD.md](BUILD.md) documents how to compile the same sources locally.

Release workflows print SHA-256 hashes for the generated binaries and package. The runtime log includes the source commit embedded by CI, making it possible to identify which source revision produced a reported build.

## Uninstall

Remove the WDForceStereo loader you installed (`dinput8.dll`, `winmm.dll`, or `WDForceStereo.asi`), `WDForceStereo.ini`, and optionally `WDForceStereo.log`.

If using NexusTools, do **not** remove NexusTools' own `dinput8.dll`.

## Disclaimer

Unofficial community fix. Not affiliated with Ubisoft, Microsoft, or the Watch Dogs developers/publishers. Provided as-is; back up files before modifying a game installation.
