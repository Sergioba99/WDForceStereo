# WDForceStereo

XAudio2.7 stereo downmix fix for **Watch Dogs (2014)** on Windows.

## What it fixes

On affected stereo setups, Watch Dogs can create a 6-channel XAudio2 mastering path and then generate a 6→2 output matrix that keeps only Front Left and Front Right. The Front Center channel — where dialogue is carried — is effectively discarded, making voices extremely quiet or inaudible.

WDForceStereo is a `dinput8.dll` proxy that hooks the relevant XAudio2 2.7 calls and repairs the stereo downmix matrix. The mix is configurable through `WDForceStereo.ini`.

## Installation

Copy `release/dinput8.dll` and `release/WDForceStereo.ini` next to `watch_dogs.exe`, then start the game normally.

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

The defaults reproduce the known-working fix. Restart Watch Dogs after editing the INI.

## Technical summary

During investigation, XAudio2 correctly reported the physical output device as stereo (2 channels, stereo speaker mask). Watch Dogs nevertheless created a 6-channel mastering path. Once the mastering output was forced to stereo, the game generated a 6→2 matrix that retained FL→L and FR→R while assigning zero to FC→L and FC→R. Dialogue was observed in the Front Center channel, so it disappeared from the stereo output.

The fix intercepts that downmix and rebuilds it using the gains from the INI.

## Building

The source is in `src/wd_force_stereo.c` with proxy exports in `src/exports.def`. Build as a 64-bit Windows DLL named `dinput8.dll`.

## Status

Current version: **1.2**. The dialogue/center-channel fix has been tested successfully. Surround and LFE fold-down are optional tuning controls.

## Disclaimer

Unofficial community fix. Not affiliated with Ubisoft, Microsoft, or the Watch Dogs developers/publishers.
