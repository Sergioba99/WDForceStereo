// DInput8 loader/proxy layer for WDForceStereo.
// Shared audio/XAudio2 logic lives in wd_core.c.

#include "wd_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef long HRESULT;
typedef int BOOL;
typedef void *HMODULE;
typedef void *FARPROC;
typedef void *LPVOID;
typedef const unsigned short *LPCWSTR;
typedef const char *LPCSTR;
typedef void *LPUNKNOWN;

#define WINAPI __stdcall
#define TRUE 1
#define NULL 0
#define DLL_PROCESS_ATTACH 1
#define MAX_PATH 260
#define E_FAIL ((HRESULT)0x80004005L)

typedef struct _GUID {
  DWORD Data1;
  WORD Data2;
  WORD Data3;
  BYTE Data4[8];
} GUID;

typedef HRESULT(WINAPI *PFN_DirectInput8Create)(HINSTANCE, DWORD, const GUID *,
                                                LPVOID *, LPUNKNOWN);
typedef HRESULT(WINAPI *PFN_DllNoArgs)(void);
typedef HRESULT(WINAPI *PFN_DllGetClassObject)(const GUID *, const GUID *,
                                               LPVOID *);

__declspec(dllimport) UINT WINAPI GetSystemDirectoryW(unsigned short *, UINT);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE, LPCSTR);

static HMODULE g_realDinput8 = NULL;

static HMODULE EnsureRealDinput8(void) {
  unsigned short p[MAX_PATH];
  UINT n, i;
  static const unsigned short d[] = L"dinput8.dll";
  UINT j = 0;

  if (g_realDinput8)
    return g_realDinput8;

  n = GetSystemDirectoryW(p, MAX_PATH);
  if (!n || n > MAX_PATH - 14)
    return NULL;

  i = n;
  if (i && p[i - 1] != L'\\')
    p[i++] = L'\\';

  while (d[j] && i < MAX_PATH - 1)
    p[i++] = d[j++];
  p[i] = 0;

  g_realDinput8 = LoadLibraryW(p);
  return g_realDinput8;
}

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE h, DWORD v,
                                                        const GUID *r,
                                                        LPVOID *out,
                                                        LPUNKNOWN outer) {
  PFN_DirectInput8Create f;
  HMODULE m = EnsureRealDinput8();
  if (!m)
    return E_FAIL;
  f = (PFN_DirectInput8Create)GetProcAddress(m, "DirectInput8Create");
  return f ? f(h, v, r, out, outer) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void) {
  PFN_DllNoArgs f;
  HMODULE m = EnsureRealDinput8();
  if (!m)
    return E_FAIL;
  f = (PFN_DllNoArgs)GetProcAddress(m, "DllCanUnloadNow");
  return f ? f() : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllGetClassObject(const GUID *a,
                                                       const GUID *b,
                                                       LPVOID *c) {
  PFN_DllGetClassObject f;
  HMODULE m = EnsureRealDinput8();
  if (!m)
    return E_FAIL;
  f = (PFN_DllGetClassObject)GetProcAddress(m, "DllGetClassObject");
  return f ? f(a, b, c) : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllRegisterServer(void) {
  PFN_DllNoArgs f;
  HMODULE m = EnsureRealDinput8();
  if (!m)
    return E_FAIL;
  f = (PFN_DllNoArgs)GetProcAddress(m, "DllRegisterServer");
  return f ? f() : E_FAIL;
}

__declspec(dllexport) HRESULT WINAPI DllUnregisterServer(void) {
  PFN_DllNoArgs f;
  HMODULE m = EnsureRealDinput8();
  if (!m)
    return E_FAIL;
  f = (PFN_DllNoArgs)GetProcAddress(m, "DllUnregisterServer");
  return f ? f() : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x) {
  (void)x;
  if (r == DLL_PROCESS_ATTACH)
    WDCoreProcessAttach(h, "DInput8");
  return TRUE;
}

#ifdef __cplusplus
}
#endif
