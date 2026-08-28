// WDForceStereo core - Watch Dogs (2014), configurable XAudio2 2.7
// stereo downmix x64, no CRT.
//
// Loader-specific code lives in separate translation units. This file contains
// only the shared XAudio2 hook, configuration and downmix implementation.

#include "wd_core.h"
#include "wd_build.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned long ULONG;
typedef long HRESULT;
typedef int BOOL;
typedef unsigned long long ULONG_PTR;
typedef unsigned long long SIZE_T;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *FARPROC;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef unsigned short WCHAR;
typedef const WCHAR *LPCWSTR;
typedef char CHAR;
typedef const CHAR *LPCSTR;
typedef DWORD *LPDWORD;
typedef void *LPUNKNOWN;

int _fltused = 0;

#define WINAPI __stdcall
#define NULL 0
#define MAX_PATH 260
#define LOG_LINE_CAP 1024
#define PAGE_EXECUTE_READWRITE 0x40
#define FILE_APPEND_DATA 0x00000004
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define OPEN_ALWAYS 4
#define CREATE_ALWAYS 2
#define GENERIC_WRITE 0x40000000
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define CLSCTX_INPROC_SERVER 0x1
#define S_OK ((HRESULT)0L)
#define E_FAIL ((HRESULT)0x80004005L)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define INVALID_HANDLE_VALUE ((HANDLE)(~(ULONG_PTR)0))

__declspec(dllimport) BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
__declspec(dllimport) HANDLE WINAPI CreateThread(LPVOID, SIZE_T,
                                                 DWORD(WINAPI *)(LPVOID),
                                                 LPVOID, DWORD, LPDWORD);
__declspec(dllimport) HMODULE WINAPI GetModuleHandleW(LPCWSTR);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE, LPCSTR);
__declspec(dllimport) DWORD WINAPI GetModuleFileNameW(HMODULE, WCHAR *, DWORD);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD,
                                                 LPDWORD);
__declspec(dllimport) HANDLE WINAPI CreateFileW(LPCWSTR, DWORD, DWORD, LPVOID,
                                                DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD,
                                            LPVOID);
__declspec(dllimport) DWORD WINAPI GetPrivateProfileStringW(
    LPCWSTR, LPCWSTR, LPCWSTR, WCHAR *, DWORD, LPCWSTR);
__declspec(dllimport) DWORD WINAPI GetFileAttributesW(LPCWSTR);

void *memset(void *dst, int c, SIZE_T n) {
  BYTE *d = (BYTE *)dst;
  SIZE_T i;
  for (i = 0; i < n; ++i)
    d[i] = (BYTE)c;
  return dst;
}

typedef struct _GUID {
  DWORD Data1;
  WORD Data2;
  WORD Data3;
  BYTE Data4[8];
} GUID;

typedef struct _WAVEFORMATEX_MIN {
  WORD wFormatTag;
  WORD nChannels;
} WAVEFORMATEX_MIN;

typedef struct _XAUDIO2_VOICE_DETAILS_MIN {
  UINT CreationFlags;
  UINT InputChannels;
  UINT InputSampleRate;
} XAUDIO2_VOICE_DETAILS_MIN;

static const GUID kCLSID_XAudio2 = {
    0x5a508685,
    0xa254,
    0x4fba,
    {0x9b, 0x82, 0x9a, 0x24, 0xb0, 0x03, 0x06, 0xaf}};
static const GUID kIID_IXAudio2 = {
    0x8bcf1f58,
    0x9fe7,
    0x4583,
    {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb}};

typedef HRESULT(WINAPI *PFN_CoCreateInstance)(const GUID *, LPUNKNOWN, DWORD,
                                              const GUID *, LPVOID *);
typedef HRESULT(WINAPI *PFN_CoInitializeEx)(LPVOID, DWORD);
typedef void(WINAPI *PFN_CoUninitialize)(void);
typedef ULONG(WINAPI *PFN_IUnknownRelease)(void *);
typedef HRESULT(WINAPI *PFN_CreateSourceVoice27)(void *, void **,
                                                 const WAVEFORMATEX_MIN *, UINT,
                                                 float, void *, const void *,
                                                 const void *);
typedef HRESULT(WINAPI *PFN_CreateMasteringVoice27)(void *, void **, UINT, UINT,
                                                    UINT, UINT, const void *);
typedef HRESULT(WINAPI *PFN_SetOutputMatrix27)(void *, void *, UINT, UINT,
                                               const float *, UINT);
typedef void(WINAPI *PFN_GetVoiceDetails27)(void *,
                                            XAUDIO2_VOICE_DETAILS_MIN *);

static float g_frontGain = 1.0f;
static float g_centerGain = 1.0f;
static float g_surroundGain = 0.0f;
static float g_lfeGain = 0.0f;
static float g_masterGain = 1.0f;
static UINT g_logEnabled = 1;
static UINT g_configRecreated = 0;
static UINT g_configCreateFailed = 0;
static const CHAR *g_loaderName = "Unknown";
static PFN_CreateSourceVoice27 g_realCreateSourceVoice = NULL;
static PFN_CreateMasteringVoice27 g_realCreateMasteringVoice = NULL;
static UINT g_hooksInstalled = 0;
static WCHAR g_logPath[MAX_PATH];
static WCHAR g_iniPath[MAX_PATH];

#define MAX_ENGINES 8
#define MAX_VOICES 64
#define MAX_VTBLS 8

typedef struct _ENGINE_INFO {
  void *engine;
  void *mastering;
  UINT masteringChannels;
} ENGINE_INFO;

typedef struct _VOICE_INFO {
  void *voice;
  void *engine;
} VOICE_INFO;

typedef struct _VTBL_HOOK {
  void **vtbl;
  PFN_SetOutputMatrix27 setMatrix;
} VTBL_HOOK;

static ENGINE_INFO g_engines[MAX_ENGINES];
static UINT g_engineCount = 0;
static VOICE_INFO g_voices[MAX_VOICES];
static UINT g_voiceCount = 0;
static VTBL_HOOK g_vtbls[MAX_VTBLS];
static UINT g_vtblCount = 0;

static void BuildPaths(void) {
  static const WCHAR logName[] = L"WDForceStereo.log";
  static const WCHAR iniName[] = L"WDForceStereo.ini";
  WCHAR exe[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
  UINT i, cut = 0, j;

  if (!n || n >= MAX_PATH)
    return;

  for (i = 0; i < n; ++i)
    if (exe[i] == L'\\' || exe[i] == L'/')
      cut = i + 1;

  for (i = 0; i < cut && i < MAX_PATH - 1; ++i) {
    g_logPath[i] = exe[i];
    g_iniPath[i] = exe[i];
  }

  j = 0;
  while (logName[j] && cut + j < MAX_PATH - 1) {
    g_logPath[cut + j] = logName[j];
    ++j;
  }
  g_logPath[cut + j] = 0;

  j = 0;
  while (iniName[j] && cut + j < MAX_PATH - 1) {
    g_iniPath[cut + j] = iniName[j];
    ++j;
  }
  g_iniPath[cut + j] = 0;
}

static UINT AppendString(CHAR *o, UINT p, UINT c, const CHAR *s) {
  UINT i = 0;
  while (s && s[i] && p + 1 < c)
    o[p++] = s[i++];
  o[p] = 0;
  return p;
}

static UINT AppendChar(CHAR *o, UINT p, UINT c, CHAR x) {
  if (p + 1 < c)
    o[p++] = x;
  o[p] = 0;
  return p;
}

static UINT AppendUInt(CHAR *o, UINT p, UINT c, UINT v) {
  CHAR t[16];
  UINT n = 0, i;
  if (!v) {
    if (p + 1 < c)
      o[p++] = '0';
    o[p] = 0;
    return p;
  }
  while (v && n < 15) {
    t[n++] = (CHAR)('0' + v % 10);
    v /= 10;
  }
  for (i = 0; i < n && p + 1 < c; ++i)
    o[p++] = t[n - 1 - i];
  o[p] = 0;
  return p;
}

static UINT AppendHex32(CHAR *o, UINT p, UINT c, DWORD v) {
  static const CHAR h[] = "0123456789ABCDEF";
  int i;
  p = AppendString(o, p, c, "0x");
  for (i = 7; i >= 0 && p + 1 < c; --i)
    o[p++] = h[(v >> (i * 4)) & 15];
  o[p] = 0;
  return p;
}

static UINT AppendPointer(CHAR *o, UINT p, UINT c, const void *x) {
  static const CHAR h[] = "0123456789ABCDEF";
  ULONG_PTR v = (ULONG_PTR)x;
  int i;
  p = AppendString(o, p, c, "0x");
  for (i = 15; i >= 0 && p + 1 < c; --i)
    o[p++] = h[(UINT)((v >> (i * 4)) & 15ULL)];
  o[p] = 0;
  return p;
}

static UINT AppendFloat3(CHAR *o, UINT p, UINT c, float f) {
  UINT w, r;
  if (f < 0) {
    p = AppendChar(o, p, c, '-');
    f = -f;
  }
  if (f > 999)
    return AppendString(o, p, c, ">999");
  w = (UINT)f;
  r = (UINT)((f - (float)w) * 1000.0f + 0.5f);
  if (r >= 1000) {
    ++w;
    r -= 1000;
  }
  p = AppendUInt(o, p, c, w);
  p = AppendChar(o, p, c, '.');
  p = AppendChar(o, p, c, (CHAR)('0' + (r / 100) % 10));
  p = AppendChar(o, p, c, (CHAR)('0' + (r / 10) % 10));
  p = AppendChar(o, p, c, (CHAR)('0' + r % 10));
  return p;
}

static void LogLine(const CHAR *s) {
  CHAR line[LOG_LINE_CAP];
  HANDLE f;
  DWORD written = 0;
  UINT n = 0;

  if (!g_logEnabled)
    return;
  if (!g_logPath[0])
    BuildPaths();

  while (s && s[n] && n < LOG_LINE_CAP - 3) {
    line[n] = s[n];
    ++n;
  }
  line[n++] = '\r';
  line[n++] = '\n';

  f = CreateFileW(g_logPath, FILE_APPEND_DATA,
                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return;
  WriteFile(f, line, n, &written, NULL);
  CloseHandle(f);
}

static void ResetLogForSession(void) {
  HANDLE f;
  if (!g_logEnabled)
    return;
  if (!g_logPath[0])
    BuildPaths();
  f = CreateFileW(g_logPath, GENERIC_WRITE,
                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL, NULL);
  if (f != INVALID_HANDLE_VALUE)
    CloseHandle(f);
}

static int ParseFloatW(const WCHAR *s, float *out) {
  UINT i = 0, digits = 0;
  float v = 0.0f, frac = 0.1f;
  int neg = 0;

  if (!s || !out)
    return 0;
  while (s[i] == L' ' || s[i] == L'\t')
    ++i;
  if (s[i] == L'-') {
    neg = 1;
    ++i;
  } else if (s[i] == L'+') {
    ++i;
  }
  while (s[i] >= L'0' && s[i] <= L'9') {
    v = v * 10.0f + (float)(s[i] - L'0');
    ++i;
    ++digits;
  }
  if (s[i] == L'.' || s[i] == L',') {
    ++i;
    while (s[i] >= L'0' && s[i] <= L'9') {
      v += (float)(s[i] - L'0') * frac;
      frac *= 0.1f;
      ++i;
      ++digits;
    }
  }
  while (s[i] == L' ' || s[i] == L'\t')
    ++i;
  if (!digits || s[i] != 0)
    return 0;
  if (neg)
    v = -v;
  *out = v;
  return 1;
}

static void WriteDefaultConfigIfMissing(void) {
  static const CHAR defaultIni[] =
      "; WDForceStereo configuration\r\n"
      "; This file is recreated automatically if it is missing.\r\n"
      "; Edit values, save, and restart Watch Dogs.\r\n"
      "\r\n"
      "[Audio]\r\n"
      "; Front left/right gain\r\n"
      "FrontGain=1.000\r\n"
      "; Dialogue/front-center gain\r\n"
      "CenterGain=1.000\r\n"
      "; Rear/surround fold-down gain\r\n"
      "SurroundGain=0.000\r\n"
      "; Subwoofer/LFE fold-down gain\r\n"
      "LFEGain=0.000\r\n"
      "; Final global multiplier\r\n"
      "MasterGain=1.000\r\n"
      "\r\n"
      "[Debug]\r\n"
      "; 1 = create WDForceStereo.log, 0 = disable logging\r\n"
      "Log=1\r\n";
  HANDLE file;
  DWORD written = 0;

  if (!g_iniPath[0])
    BuildPaths();
  if (GetFileAttributesW(g_iniPath) != INVALID_FILE_ATTRIBUTES)
    return;

  file = CreateFileW(g_iniPath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                     CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (file == INVALID_HANDLE_VALUE) {
    g_configCreateFailed = 1;
    return;
  }

  WriteFile(file, defaultIni, (DWORD)(sizeof(defaultIni) - 1), &written, NULL);
  CloseHandle(file);
  g_configRecreated = 1;
}

static float ReadIniGain(LPCWSTR key, LPCWSTR def, float fallback) {
  WCHAR b[64];
  float v = fallback;
  b[0] = 0;
  GetPrivateProfileStringW(L"Audio", key, def, b, 64, g_iniPath);
  if (!ParseFloatW(b, &v))
    return fallback;
  if (v < 0.0f)
    v = 0.0f;
  if (v > 4.0f)
    v = 4.0f;
  return v;
}

static void LoadConfig(void) {
  WCHAR b[64];
  if (!g_iniPath[0])
    BuildPaths();
  WriteDefaultConfigIfMissing();

  g_frontGain = ReadIniGain(L"FrontGain", L"1.0", 1.0f);
  g_centerGain = ReadIniGain(L"CenterGain", L"1.0", 1.0f);
  g_surroundGain = ReadIniGain(L"SurroundGain", L"0.0", 0.0f);
  g_lfeGain = ReadIniGain(L"LFEGain", L"0.0", 0.0f);
  g_masterGain = ReadIniGain(L"MasterGain", L"1.0", 1.0f);

  b[0] = 0;
  GetPrivateProfileStringW(L"Debug", L"Log", L"1", b, 64, g_iniPath);
  g_logEnabled = (b[0] == L'0' && b[1] == 0) ? 0 : 1;
}

static ENGINE_INFO *FindEngineInfo(void *engine, int create) {
  UINT i;
  for (i = 0; i < g_engineCount; ++i)
    if (g_engines[i].engine == engine)
      return &g_engines[i];

  if (create && g_engineCount < MAX_ENGINES) {
    ENGINE_INFO *info = &g_engines[g_engineCount++];
    memset(info, 0, sizeof(*info));
    info->engine = engine;
    return info;
  }
  return NULL;
}

static VOICE_INFO *FindVoiceInfo(void *voice, int create) {
  UINT i;
  for (i = 0; i < g_voiceCount; ++i)
    if (g_voices[i].voice == voice)
      return &g_voices[i];

  if (create && g_voiceCount < MAX_VOICES) {
    VOICE_INFO *info = &g_voices[g_voiceCount++];
    memset(info, 0, sizeof(*info));
    info->voice = voice;
    return info;
  }
  return NULL;
}

static VTBL_HOOK *FindVtableHook(void *voice) {
  void **vt = voice ? *(void ***)voice : NULL;
  UINT i;
  if (!vt)
    return NULL;
  for (i = 0; i < g_vtblCount; ++i)
    if (g_vtbls[i].vtbl == vt)
      return &g_vtbls[i];
  return NULL;
}

static void LogMatrixRow(const CHAR *prefix, const float *row) {
  CHAR l[420];
  UINT i, p = 0;
  if (!g_logEnabled || !row)
    return;
  p = AppendString(l, p, sizeof(l), prefix);
  for (i = 0; i < 6; ++i) {
    if (i)
      p = AppendChar(l, p, sizeof(l), ' ');
    p = AppendFloat3(l, p, sizeof(l), row[i]);
  }
  LogLine(l);
}

static void ApplyConfigured6x2(const float *game, float *out) {
  UINT i;
  for (i = 0; i < 12; ++i)
    out[i] = game[i];

  out[0] *= g_frontGain;
  out[1] *= g_frontGain;
  out[6] *= g_frontGain;
  out[7] *= g_frontGain;

  out[2] = g_centerGain;
  out[8] = g_centerGain;
  out[3] = g_lfeGain;
  out[9] = g_lfeGain;
  out[4] = g_surroundGain;
  out[10] = 0.0f;
  out[5] = 0.0f;
  out[11] = g_surroundGain;

  for (i = 0; i < 12; ++i)
    out[i] *= g_masterGain;
}

static void BuildStereoKernel(float *k) {
  UINT i;
  for (i = 0; i < 12; ++i)
    k[i] = 0.0f;

  k[0] = g_frontGain;
  k[2] = g_centerGain;
  k[3] = g_lfeGain;
  k[4] = g_surroundGain;
  k[7] = g_frontGain;
  k[8] = g_centerGain;
  k[9] = g_lfeGain;
  k[11] = g_surroundGain;
}

static void Compose6x6To6x2(const float *game6, float *out12) {
  float k[12];
  UINT source, dest, intermediate;
  BuildStereoKernel(k);

  for (dest = 0; dest < 2; ++dest) {
    for (source = 0; source < 6; ++source) {
      float sum = 0.0f;
      for (intermediate = 0; intermediate < 6; ++intermediate)
        sum += k[intermediate + 6 * dest] *
               game6[source + 6 * intermediate];
      out12[source + 6 * dest] = sum * g_masterGain;
    }
  }
}

static HRESULT WINAPI Hook_SetOutputMatrix(void *This, void *dest, UINT sc,
                                           UINT dc, const float *m, UINT op) {
  VTBL_HOOK *hook = FindVtableHook(This);
  VOICE_INFO *voice = FindVoiceInfo(This, 0);
  HRESULT hr;

  if (!hook || !hook->setMatrix)
    return E_FAIL;

  if (voice) {
    if (g_logEnabled) {
      CHAR l[280];
      UINT p = 0;
      p = AppendString(l, p, sizeof(l),
                       "Game SetOutputMatrix on 6ch source: ");
      p = AppendUInt(l, p, sizeof(l), sc);
      p = AppendString(l, p, sizeof(l), "->");
      p = AppendUInt(l, p, sizeof(l), dc);
      p = AppendString(l, p, sizeof(l), " dest=");
      p = AppendPointer(l, p, sizeof(l), dest);
      LogLine(l);
    }

    if (m && sc == 6 && dc == 2) {
      LogMatrixRow("  Original L [FL FR FC LFE BL BR] = ", m);
      LogMatrixRow("  Original R [FL FR FC LFE BL BR] = ", m + 6);

#ifndef WD_DIAGNOSTIC_6TO2
      {
        ENGINE_INFO *engine = FindEngineInfo(voice->engine, 0);
        if (engine && engine->mastering == dest &&
            engine->masteringChannels == 2) {
          float configured[12];
          ApplyConfigured6x2(m, configured);
          hr = hook->setMatrix(This, dest, 6, 2, configured, op);

          if (g_logEnabled) {
            CHAR l[220];
            UINT p = 0;
            p = AppendString(l, p, sizeof(l),
                             "CONFIGURED DOWNMIX 6->2: ");
            p = AppendString(l, p, sizeof(l), FAILED(hr) ? "FAILED hr=" : "OK hr=");
            p = AppendHex32(l, p, sizeof(l), (DWORD)hr);
            LogLine(l);
          }
          LogMatrixRow("  Final L    [FL FR FC LFE BL BR] = ", configured);
          LogMatrixRow("  Final R    [FL FR FC LFE BL BR] = ", configured + 6);
          return hr;
        }
        LogLine("6->2 matrix target does not match the tracked forced-stereo "
                "mastering voice; pass-through.");
      }
#else
      LogLine("DIAGNOSTIC: actual 6->2 matrix logged; not modified.");
#endif
    }

#ifndef WD_DIAGNOSTIC_6TO2
    if (m && sc == 6 && dc == 6) {
      ENGINE_INFO *engine = FindEngineInfo(voice->engine, 0);
      if (engine && engine->mastering == dest &&
          engine->masteringChannels == 2) {
        float configured[12];
        Compose6x6To6x2(m, configured);
        hr = hook->setMatrix(This, dest, 6, 2, configured, op);
        if (g_logEnabled) {
          CHAR l[220];
          UINT p = 0;
          p = AppendString(l, p, sizeof(l),
                           "FALLBACK 6->6 => 6->2: hr=");
          p = AppendHex32(l, p, sizeof(l), (DWORD)hr);
          LogLine(l);
        }
        return hr;
      }
    }
#endif
  }

  return hook->setMatrix(This, dest, sc, dc, m, op);
}

static void HookSourceVtable(void *voice) {
  void **vt;
  DWORD old;
  UINT i;

  if (!voice)
    return;
  vt = *(void ***)voice;
  if (!vt)
    return;

  for (i = 0; i < g_vtblCount; ++i)
    if (g_vtbls[i].vtbl == vt)
      return;

  if (g_vtblCount >= MAX_VTBLS) {
    LogLine("WARNING: source vtable hook table full.");
    return;
  }

  if (!VirtualProtect(&vt[16], sizeof(void *), PAGE_EXECUTE_READWRITE, &old)) {
    LogLine("ERROR: cannot hook SetOutputMatrix.");
    return;
  }

  g_vtbls[g_vtblCount].vtbl = vt;
  g_vtbls[g_vtblCount].setMatrix = (PFN_SetOutputMatrix27)vt[16];
  vt[16] = (void *)&Hook_SetOutputMatrix;

  {
    DWORD ignored;
    VirtualProtect(&vt[16], sizeof(void *), old, &ignored);
  }

  ++g_vtblCount;
  LogLine("Source hook installed: SetOutputMatrix.");
}

static HRESULT WINAPI Hook_CreateSourceVoice27(void *This, void **pp,
                                               const WAVEFORMATEX_MIN *format,
                                               UINT flags, float ratio,
                                               void *callback,
                                               const void *sendList,
                                               const void *effects) {
  HRESULT hr = g_realCreateSourceVoice(This, pp, format, flags, ratio, callback,
                                       sendList, effects);

  if (!FAILED(hr) && pp && *pp) {
    HookSourceVtable(*pp);

    if (format && format->nChannels == 6) {
      VOICE_INFO *voice = FindVoiceInfo(*pp, 1);
      if (voice) {
        voice->voice = *pp;
        voice->engine = This;
      }

      if (g_logEnabled) {
        CHAR l[220];
        UINT p = 0;
        p = AppendString(l, p, sizeof(l), "Tracked 6ch SourceVoice: ptr=");
        p = AppendPointer(l, p, sizeof(l), *pp);
        p = AppendString(l, p, sizeof(l), " engine=");
        p = AppendPointer(l, p, sizeof(l), This);
        LogLine(l);
      }
    }
  }
  return hr;
}

static HRESULT WINAPI Hook_CreateMasteringVoice27(void *This, void **pp,
                                                  UINT ch, UINT rate,
                                                  UINT flags, UINT dev,
                                                  const void *effects) {
  UINT passedCh = (ch == 6) ? 2 : ch;
  HRESULT hr = g_realCreateMasteringVoice(This, pp, passedCh, rate, flags, dev,
                                          effects);
  XAUDIO2_VOICE_DETAILS_MIN details;

  memset(&details, 0, sizeof(details));
  if (!FAILED(hr) && pp && *pp) {
    ENGINE_INFO *engine = FindEngineInfo(This, 1);
    void **vt = *(void ***)*pp;

    if (vt && vt[0])
      ((PFN_GetVoiceDetails27)vt[0])(*pp, &details);

    if (engine) {
      engine->mastering = *pp;
      engine->masteringChannels =
          details.InputChannels ? details.InputChannels : passedCh;
    }
  }

  if (g_logEnabled) {
    CHAR l[420];
    UINT p = 0;
    p = AppendString(l, p, sizeof(l), "CreateMasteringVoice: requested=");
    p = AppendUInt(l, p, sizeof(l), ch);
    p = AppendString(l, p, sizeof(l), " passed=");
    p = AppendUInt(l, p, sizeof(l), passedCh);
    p = AppendString(l, p, sizeof(l), " rate=");
    p = AppendUInt(l, p, sizeof(l), rate);
    p = AppendString(l, p, sizeof(l), " dev=");
    p = AppendUInt(l, p, sizeof(l), dev);
    p = AppendString(l, p, sizeof(l), " hr=");
    p = AppendHex32(l, p, sizeof(l), (DWORD)hr);

    if (!FAILED(hr) && pp && *pp) {
      p = AppendString(l, p, sizeof(l), " ptr=");
      p = AppendPointer(l, p, sizeof(l), *pp);
      p = AppendString(l, p, sizeof(l), " actualCh=");
      p = AppendUInt(l, p, sizeof(l), details.InputChannels);
      p = AppendString(l, p, sizeof(l), " actualRate=");
      p = AppendUInt(l, p, sizeof(l), details.InputSampleRate);
      if (ch == 6)
        p = AppendString(l, p, sizeof(l), " [FORCED STEREO]");
    }
    LogLine(l);
  }

  return hr;
}

static void InstallEngineHooks(void *xa) {
  void **vt;
  DWORD old;

  if (!xa || g_hooksInstalled)
    return;
  vt = *(void ***)xa;
  if (!vt)
    return;

  if (!VirtualProtect(&vt[8], 3 * sizeof(void *), PAGE_EXECUTE_READWRITE,
                      &old)) {
    LogLine("ERROR: VirtualProtect engine vtable failed.");
    return;
  }

  g_realCreateSourceVoice = (PFN_CreateSourceVoice27)vt[8];
  g_realCreateMasteringVoice = (PFN_CreateMasteringVoice27)vt[10];
  vt[8] = (void *)&Hook_CreateSourceVoice27;
  vt[10] = (void *)&Hook_CreateMasteringVoice27;

  {
    DWORD ignored;
    VirtualProtect(&vt[8], 3 * sizeof(void *), old, &ignored);
  }

  g_hooksInstalled = 1;
  LogLine("XAudio2 2.7 hook installed.");
}

static int Probe(void) {
  HMODULE ole32;
  PFN_CoInitializeEx coInitialize;
  PFN_CoUninitialize coUninitialize;
  PFN_CoCreateInstance coCreate;
  HRESULT initHr, hr;
  void *xa = NULL;
  int shouldUninitialize = 0;

  if (!LoadLibraryW(L"XAudio2_7.dll")) {
    LogLine("ERROR: XAudio2_7.dll load failed.");
    return 0;
  }

  ole32 = GetModuleHandleW(L"ole32.dll");
  if (!ole32)
    ole32 = LoadLibraryW(L"ole32.dll");
  if (!ole32) {
    LogLine("ERROR: ole32.dll load failed.");
    return 0;
  }

  coInitialize = (PFN_CoInitializeEx)GetProcAddress(ole32, "CoInitializeEx");
  coUninitialize =
      (PFN_CoUninitialize)GetProcAddress(ole32, "CoUninitialize");
  coCreate = (PFN_CoCreateInstance)GetProcAddress(ole32, "CoCreateInstance");
  if (!coCreate) {
    LogLine("ERROR: CoCreateInstance unavailable.");
    return 0;
  }

  initHr = coInitialize ? coInitialize(NULL, 0) : S_OK;
  if (initHr == S_OK || initHr == 1)
    shouldUninitialize = 1;

  hr = coCreate(&kCLSID_XAudio2, NULL, CLSCTX_INPROC_SERVER, &kIID_IXAudio2,
                &xa);

  if (g_logEnabled) {
    CHAR l[150];
    UINT p = 0;
    p = AppendString(l, p, sizeof(l), "XAudio2 probe CoCreateInstance: hr=");
    p = AppendHex32(l, p, sizeof(l), (DWORD)hr);
    LogLine(l);
  }

  if (!FAILED(hr) && xa) {
    void **vt = *(void ***)xa;
    InstallEngineHooks(xa);
    if (vt && vt[2])
      ((PFN_IUnknownRelease)vt[2])(xa);
  }

  if (shouldUninitialize && coUninitialize)
    coUninitialize();

  return g_hooksInstalled ? 1 : 0;
}

static void LogStartupHeader(void) {
  CHAR l[360];
  UINT p = 0;

  LogLine("============================================================");
  p = AppendString(l, p, sizeof(l), "WDForceStereo v");
  p = AppendString(l, p, sizeof(l), WD_VERSION);
  LogLine(l);

  p = 0;
  p = AppendString(l, p, sizeof(l), "Build: ");
  p = AppendString(l, p, sizeof(l), WD_BUILD_ID);
  LogLine(l);

  p = 0;
  p = AppendString(l, p, sizeof(l), "Loader: ");
  p = AppendString(l, p, sizeof(l), g_loaderName ? g_loaderName : "Unknown");
  LogLine(l);
  LogLine("============================================================");

  if (g_configRecreated)
    LogLine("Config: WDForceStereo.ini was missing and was recreated.");
  else if (g_configCreateFailed)
    LogLine("WARNING: WDForceStereo.ini was missing and could not be recreated; using compiled defaults.");
}

static DWORD WINAPI HookThread(LPVOID unused) {
  int probeOk;
  (void)unused;

  BuildPaths();
  LoadConfig();
  ResetLogForSession();
  LogStartupHeader();

#ifdef WD_DIAGNOSTIC_6TO2
  LogLine("Mode: DIAGNOSTIC 6->2 matrix (matrix is not modified).");
#else
  LogLine("Mode: configurable 6->2 stereo downmix.");
#endif

  if (g_logEnabled) {
    CHAR l[360];
    UINT p = 0;
    p = AppendString(l, p, sizeof(l), "Config: FrontGain=");
    p = AppendFloat3(l, p, sizeof(l), g_frontGain);
    p = AppendString(l, p, sizeof(l), " CenterGain=");
    p = AppendFloat3(l, p, sizeof(l), g_centerGain);
    p = AppendString(l, p, sizeof(l), " SurroundGain=");
    p = AppendFloat3(l, p, sizeof(l), g_surroundGain);
    p = AppendString(l, p, sizeof(l), " LFEGain=");
    p = AppendFloat3(l, p, sizeof(l), g_lfeGain);
    p = AppendString(l, p, sizeof(l), " MasterGain=");
    p = AppendFloat3(l, p, sizeof(l), g_masterGain);
    LogLine(l);
  }

  probeOk = Probe();
  if (probeOk)
    LogLine("Initialization: config=OK | XAudio2 hook=OK");
  else
    LogLine("Initialization: config=OK | XAudio2 hook=FAILED");

  return 0;
}

void WDCoreProcessAttach(void *module, const char *loaderName) {
  HANDLE thread;

  g_loaderName = loaderName ? loaderName : "Unknown";
  DisableThreadLibraryCalls((HMODULE)module);

  thread = CreateThread(NULL, 0, HookThread, NULL, 0, NULL);
  if (thread)
    CloseHandle(thread);
}

#ifdef __cplusplus
}
#endif
