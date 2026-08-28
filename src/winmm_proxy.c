// Experimental WinMM loader/proxy layer for WDForceStereo.
// Shared audio/XAudio2 logic lives in wd_core.c.
//
// Watch Dogs' Disrupt_b64.dll imports timeGetTime from WINMM.dll. This test
// proxy forwards that function to the real System32\winmm.dll and starts the
// shared WDForceStereo core from DllMain.

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int UINT;
typedef unsigned long DWORD;
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

typedef DWORD(WINAPI *PFN_timeGetTime)(void);

static HMODULE g_realWinmm = NULL;

static HMODULE EnsureRealWinmm(void) {
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

__declspec(dllexport) DWORD WINAPI timeGetTime(void) {
  HMODULE m = EnsureRealWinmm();
  PFN_timeGetTime f;

  if (!m)
    return 0;

  f = (PFN_timeGetTime)GetProcAddress(m, "timeGetTime");
  return f ? f() : 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x) {
  (void)x;
  if (r == DLL_PROCESS_ATTACH)
    WDCoreProcessAttach(h);
  return TRUE;
}

#ifdef __cplusplus
}
#endif
