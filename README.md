# WDForceStereo

XAudio2 2.7 stereo downmix fix for **Watch Dogs (2014)** on Windows.

## What it fixes

On affected stereo setups, Watch Dogs can create a 6-channel XAudio2 mastering path and then generate a 6→2 output matrix that keeps only Front Left and Front Right. The Front Center channel — where dialogue is carried — is effectively discarded, making voices extremely quiet or inaudible.

WDForceStereo hooks the relevant XAudio2 2.7 calls and repairs that stereo downmix. The mix is configurable through `WDForceStereo.ini`.

## Installation

### Standalone

Copy `dinput8.dll` and `WDForceStereo.ini` next to `watch_dogs.exe`, then start the game normally.

### NexusTools / ASI loader

NexusTools already uses its own `dinput8.dll`, so do **not** replace it with WDForceStereo's proxy DLL.

Instead, use `WDForceStereo.asi` together with `WDForceStereo.ini`. Put `WDForceStereo.asi` next to `watch_dogs.exe` so NexusTools' ASI injection helper can load it.

`WDForceStereo.asi` contains the same audio-fix code as the standalone DLL. The XAudio2 hook is initialized from `DllMain` when the module is loaded, so the DirectInput proxy entry points are not required when NexusTools is acting as the loader.

Use **either** `dinput8.dll` (standalone) **or** `WDForceStereo.asi` (with NexusTools), not both.

NexusTools compatibility was first reported by a user who successfully loaded the mod after renaming the DLL to `.asi`. The packaged `.asi` is provided to make that setup explicit and avoid users having to rename files manually.

The repository includes GitHub Actions workflows that build the x64 DLL, create the equivalent `WDForceStereo.asi`, and package both variants. You can also build locally using the steps in [BUILD.md](BUILD.md).

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
- `CenterGain` — Front Center → L/R gain; primarily controls dialogue.
- `SurroundGain` — rear/surround → stereo gain.
- `LFEGain` — LFE → stereo gain.
- `MasterGain` — final multiplier for the repaired matrix.
- `Log` — `1` enables `WDForceStereo.log`; `0` disables it.

The defaults reproduce the known-working dialogue fix. Restart Watch Dogs after editing the INI.

If `WDForceStereo.ini` is missing, the mod automatically recreates it next to `watch_dogs.exe` using the safe default values above. This means the mod still works if somebody forgets to copy the INI or deletes it accidentally. If the game directory is not writable, the mod falls back to its compiled defaults and writes a warning to the log when possible.

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

The 6-channel game mix was observed as 32-bit float at 48 kHz with speaker mask `0x3F`. Dialogue-like bursts were present in the Front Center channel.

Once the game's 6-channel mastering request was forced to stereo, Watch Dogs generated this effective 6→2 matrix:

```text
        FL   FR   FC   LFE  BL   BR
L       1    0    0    0    0    0
R       0    1    0    0    0    0
```

So the stereo path discarded Front Center entirely. WDForceStereo intercepts this matrix and restores configurable routing for the missing channels.

With the default v1.2 configuration the repaired matrix is effectively:

```text
        FL   FR   FC   LFE  BL   BR
L       1    0    1    0    0    0
R       0    1    1    0    0    0
```

`SurroundGain` and `LFEGain` can optionally fold those channels into stereo as well.

## Source layout

```text
src/
├── wd_force_stereo.c      # complete implementation
└── exports.def            # dinput8 proxy exports
```

The source is intentionally kept as one small freestanding C translation unit. It is formatted with `clang-format`, and helper functions use descriptive names so the hook, configuration, logging and matrix-repair logic are easier to follow.

## Building

See [BUILD.md](BUILD.md). The tested implementation is x64, freestanding C and does not link the CRT.

## Tested build

Current version: **1.2**.

The dialogue/center-channel fix has been tested successfully. Surround and LFE fold-down are optional tuning controls. Hashes for the development-tested package are recorded in [CHECKSUMS.md](CHECKSUMS.md).

## Logging

With `Log=1`, the mod writes `WDForceStereo.log` next to the game executable. It records the loaded gains, mastering-voice conversion, original game matrix and final configured matrix. Set `Log=0` once you are happy with the setup.

## Uninstall

Standalone: delete `dinput8.dll`, `WDForceStereo.ini`, and optionally `WDForceStereo.log` from the Watch Dogs executable directory.

NexusTools: delete `WDForceStereo.asi`, `WDForceStereo.ini`, and optionally `WDForceStereo.log`. Do not remove NexusTools' own `dinput8.dll`.

## Disclaimer

Unofficial community fix. Not affiliated with Ubisoft, Microsoft, or the Watch Dogs developers/publishers. Provided as-is; back up files before modifying a game installation.
