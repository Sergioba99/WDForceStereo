# WDForceStereo

XAudio2 2.7 stereo downmix fix for **Watch Dogs (2014)** on Windows.

## What it fixes

On affected stereo setups, Watch Dogs can create a 6-channel XAudio2 mastering path and then generate a 6→2 output matrix that keeps only Front Left and Front Right. The Front Center channel — where dialogue is carried — is effectively discarded, making voices extremely quiet or inaudible.

WDForceStereo is a `dinput8.dll` proxy that hooks the relevant XAudio2 2.7 calls and repairs that stereo downmix. The mix is configurable through `WDForceStereo.ini`.

## Installation

Use a compiled `dinput8.dll` together with `release/WDForceStereo.ini`, and copy both files next to `watch_dogs.exe`.

The repository includes a GitHub Actions workflow that builds the x64 DLL and uploads a ready-to-copy `WDForceStereo-v1.2` artifact. You can also build it locally using the steps in [BUILD.md](BUILD.md).

Then start the game normally.

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
├── wd_force_stereo.c      # source entry point
├── exports.def            # dinput8 proxy exports
└── parts/
    ├── 01.inc
    ├── 02.inc
    ├── 03.inc
    └── 04.inc
```

The implementation is split into line-preserving include chunks because of how the source was transferred into the repository; compiling `src/wd_force_stereo.c` produces the complete implementation.

## Building

See [BUILD.md](BUILD.md). The tested implementation is x64, freestanding C and does not link the CRT.

## Tested build

Current version: **1.2**.

The dialogue/center-channel fix has been tested successfully. Surround and LFE fold-down are optional tuning controls. Hashes for the development-tested package are recorded in [CHECKSUMS.md](CHECKSUMS.md).

## Logging

With `Log=1`, the mod writes `WDForceStereo.log` next to the game executable. It records the loaded gains, mastering-voice conversion, original game matrix and final configured matrix. Set `Log=0` once you are happy with the setup.

## Uninstall

Delete `dinput8.dll`, `WDForceStereo.ini`, and optionally `WDForceStereo.log` from the Watch Dogs executable directory.

## Disclaimer

Unofficial community fix. Not affiliated with Ubisoft, Microsoft, or the Watch Dogs developers/publishers. Provided as-is; back up files before modifying a game installation.
