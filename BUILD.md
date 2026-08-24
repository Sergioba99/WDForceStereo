# Building WDForceStereo

WDForceStereo v1.2 is a 64-bit Windows DLL proxy for `dinput8.dll`. The tested build was produced with Clang targeting MSVC ABI and `lld-link`, without the CRT.

## Requirements

- `clang` with the `x86_64-pc-windows-msvc` target
- `lld-link`

No Visual Studio project is required.

## 1. Create a minimal kernel32 import library

Create `kernel32.def`:

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
```

Generate the import library:

```bat
lld-link /lib /machine:x64 /def:kernel32.def /out:kernel32.lib
```

## 2. Compile

From the repository root:

```bat
clang --target=x86_64-pc-windows-msvc -c src\wd_force_stereo.c ^
  -o wd_force_stereo.obj -O2 -ffreestanding -fno-builtin ^
  -fno-stack-protector -Wno-incompatible-library-redeclaration
```

`src/wd_force_stereo.c` includes the line-preserving implementation chunks from `src/parts/`.

## 3. Link

```bat
lld-link /dll /machine:x64 /entry:DllMain /nodefaultlib ^
  wd_force_stereo.obj kernel32.lib ^
  /def:src\exports.def ^
  /out:dinput8.dll
```

## Expected exports

The resulting proxy must export:

```text
DirectInput8Create
DllCanUnloadNow
DllGetClassObject
DllRegisterServer
DllUnregisterServer
```

## Installation test

Copy the generated `dinput8.dll` and `release/WDForceStereo.ini` next to `watch_dogs.exe`.

With logging enabled, `WDForceStereo.log` should contain entries showing a requested 6-channel XAudio2 mastering voice being passed as 2 channels and a configured 6→2 matrix.

## Notes

This project intentionally resolves COM/XAudio2 functions dynamically and forwards DInput8 to the Windows system DLL. It does not require the legacy DirectX SDK headers to compile because the small subset of XAudio2 2.7 ABI structures and interfaces used by the hook is declared locally.
