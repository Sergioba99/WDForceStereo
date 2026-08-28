# Building WDForceStereo

WDForceStereo v1.0 is a 64-bit Windows XAudio2 stereo downmix fix for Watch Dogs (2014). The standalone build is a `dinput8.dll` proxy. The NexusTools build uses the same binary renamed to `WDForceStereo.asi`, because NexusTools loads it through its ASI Injection Helper.

The tested build uses Clang targeting the MSVC ABI and `lld-link`, without the CRT.

## Requirements

- `clang` with the `x86_64-pc-windows-msvc` target
- `lld-link`

No Visual Studio project or legacy DirectX SDK is required.

## 1. Build the minimal kernel32 import library

The repository already contains `kernel32.def` with the imports required by WDForceStereo:

```def
LIBRARY KERNEL32.dll
EXPORTS
DisableThreadLibraryCalls
CreateThread
GetModuleHandleW
LoadLibraryW
GetProcAddress
GetModuleFileNameW
GetSystemDirectoryW
CloseHandle
VirtualProtect
CreateFileW
WriteFile
SetFilePointer
GetPrivateProfileStringW
GetFileAttributesW
```

Generate the import library from the repository root:

```bat
lld-link /lib /machine:x64 /def:kernel32.def /out:kernel32.lib
```

## 2. Compile

```bat
clang --target=x86_64-pc-windows-msvc -c src\wd_force_stereo.c ^
  -o wd_force_stereo.obj -O2 -ffreestanding -fno-builtin ^
  -fno-stack-protector -Wno-incompatible-library-redeclaration
```

`src/wd_force_stereo.c` is the complete implementation.

## 3. Link the standalone DLL

```bat
lld-link /dll /machine:x64 /entry:DllMain /nodefaultlib ^
  wd_force_stereo.obj kernel32.lib ^
  /def:src\exports.def ^
  /out:dinput8.dll
```

## 4. Create the NexusTools ASI build

No separate compilation is required. `WDForceStereo.asi` is byte-for-byte the same PE DLL as `dinput8.dll`; only the filename/extension is different so NexusTools' ASI Injection Helper can load it.

Using Command Prompt:

```bat
copy /Y dinput8.dll WDForceStereo.asi
```

Using PowerShell:

```powershell
Copy-Item .\dinput8.dll .\WDForceStereo.asi -Force
```

The GitHub Actions workflows use this same method.

## Expected exports

The resulting binary exports:

```text
DirectInput8Create
DllCanUnloadNow
DllGetClassObject
DllRegisterServer
DllUnregisterServer
```

These exports are required when the binary is used as the standalone `dinput8.dll` proxy. They are harmless when the same binary is loaded as `WDForceStereo.asi` by NexusTools.

## Installation tests

### Standalone

Copy these files next to `watch_dogs.exe`:

```text
dinput8.dll
WDForceStereo.ini
```

Start the game normally.

### NexusTools

Keep NexusTools' own `dinput8.dll` in place and copy these files next to `watch_dogs.exe`:

```text
WDForceStereo.asi
WDForceStereo.ini
```

Do not install WDForceStereo's `dinput8.dll` at the same time when using NexusTools.

WDForceStereo is loaded by NexusTools' ASI Injection Helper and therefore does not need to appear in NexusTools' Installed Mods list.

Both standalone and NexusTools/ASI loading have been tested in-game.

## Verification

With logging enabled (`Log=1` in `WDForceStereo.ini`), `WDForceStereo.log` should be created next to the game executable. It should contain entries showing the requested 6-channel XAudio2 mastering voice being passed as 2 channels and the configured 6-to-2 matrix.

## Notes

WDForceStereo resolves COM/XAudio2 functions dynamically and, in standalone mode, forwards DInput8 calls to the Windows system DLL. It does not require the legacy DirectX SDK headers because the small subset of the XAudio2 2.7 ABI used by the hook is declared locally in the source.

For a reproducible reference build, see `.github/workflows/build.yml` and `.github/workflows/release.yml`, which contain the exact commands used by GitHub Actions to build the release binaries.
