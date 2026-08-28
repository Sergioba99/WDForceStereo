// Experimental WinMM proxy for WDForceStereo.
//
// This file intentionally contains only the WinMM-facing forwarding layer.
// The existing WDForceStereo core/DllMain remains in wd_force_stereo.c.
//
// Watch Dogs' Disrupt_b64.dll imports timeGetTime from WINMM.dll, so this
// minimal test proxy forwards that call to the real System32\winmm.dll while
// allowing WDForceStereo's existing DllMain to be loaded early.

typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned short WCHAR;
typedef const WCHAR *LPCWSTR;
typedef const char *LPCSTR;
typedef void *HMODULE;
typedef void *FARPROC;

#define WINAPI __stdcall
#define NULL 0
#define MAX_PATH 260

__declspec(dllimport) UINT WINAPI GetSystemDirectoryW(WCHAR *, UINT);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE, LPCSTR);

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
