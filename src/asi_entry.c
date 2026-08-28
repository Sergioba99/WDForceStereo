// Native ASI loader entry for WDForceStereo.
//
// This facade contains no proxy exports. An ASI loader only needs to load the
// module; DLL_PROCESS_ATTACH starts the shared WDForceStereo core.

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long DWORD;
typedef int BOOL;
typedef void *HINSTANCE;
typedef void *LPVOID;

#define WINAPI __stdcall
#define TRUE 1
#define DLL_PROCESS_ATTACH 1

void WDCoreProcessAttach(HINSTANCE module);

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH)
    WDCoreProcessAttach(h);
  return TRUE;
}

#ifdef __cplusplus
}
#endif
