// WinMM loader/proxy layer for WDForceStereo (x64).
// Shared audio/XAudio2 logic lives in wd_core.c.
//
// All WinMM exports are forwarded to the real System32\winmm.dll. The proxy
// stubs deliberately have no typed parameters: on Win64 the calling convention
// is uniform and the optimized stub becomes a direct tail jump, preserving the
// caller's integer, floating-point and stack arguments.

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned long long ULONG_PTR;
typedef int BOOL;
typedef unsigned short WCHAR;
typedef const WCHAR *LPCWSTR;
typedef const char *LPCSTR;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *FARPROC;
typedef void *LPVOID;

#define WINAPI __stdcall
#define TRUE 1
#define NULL 0
#define DLL_PROCESS_ATTACH 1
#define MAX_PATH 260

__declspec(dllimport) UINT WINAPI GetSystemDirectoryW(WCHAR *, UINT);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE, LPCSTR);

void WDCoreProcessAttach(HINSTANCE module);

typedef ULONG_PTR(WINAPI *PFN_GENERIC_WINMM)(void);

static HMODULE g_realWinmm = NULL;

#define X(name) static FARPROC g_##name = NULL;
#include "winmm_exports.inc"
#undef X

static ULONG_PTR WINAPI MissingWinmmExport(void) {
  return 0;
}

static HMODULE LoadRealWinmm(void) {
  WCHAR path[MAX_PATH];
  UINT n, i;
  static const WCHAR name[] = L"winmm.dll";
  UINT j = 0;

  if (g_realWinmm)
    return g_realWinmm;

  n = GetSystemDirectoryW(path, MAX_PATH);
  if (!n || n >= MAX_PATH - 11)
    return NULL;

  i = n;
  if (i && path[i - 1] != L'\\')
    path[i++] = L'\\';

  while (name[j] && i < MAX_PATH - 1)
    path[i++] = name[j++];
  path[i] = 0;

  g_realWinmm = LoadLibraryW(path);
  return g_realWinmm;
}

static void InitWinmmForwarders(void) {
  HMODULE m = LoadRealWinmm();

#define X(name)                                                               \
  do {                                                                        \
    FARPROC p = m ? GetProcAddress(m, #name) : NULL;                          \
    g_##name = p ? p : (FARPROC)&MissingWinmmExport;                          \
  } while (0);
#include "winmm_exports.inc"
#undef X
}

#define X(name)                                                               \
  __declspec(noinline) ULONG_PTR WINAPI Proxy_##name(void) {                  \
    return ((PFN_GENERIC_WINMM)g_##name)();                                   \
  }
#include "winmm_exports.inc"
#undef X

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x) {
  (void)x;
  if (r == DLL_PROCESS_ATTACH) {
    InitWinmmForwarders();
    WDCoreProcessAttach(h);
  }
  return TRUE;
}

#ifdef __cplusplus
}
#endif
