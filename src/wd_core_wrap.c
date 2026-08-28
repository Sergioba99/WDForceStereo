// Mechanical core wrapper used during the loader refactor.
//
// The proven audio implementation remains in wd_force_stereo.c unchanged for
// this first regression test. We compile it here with its DInput8 exports
// renamed and hidden, and expose its original DllMain body as WDCoreAttachEntry.
// This lets loader-specific DLLs live in separate translation units without
// changing the XAudio2/downmix behavior yet.

#define dllexport
#define DirectInput8Create WDCore_Unused_DirectInput8Create
#define DllCanUnloadNow WDCore_Unused_DllCanUnloadNow
#define DllGetClassObject WDCore_Unused_DllGetClassObject
#define DllRegisterServer WDCore_Unused_DllRegisterServer
#define DllUnregisterServer WDCore_Unused_DllUnregisterServer
#define DllMain WDCoreAttachEntry

#include "wd_force_stereo.c"
