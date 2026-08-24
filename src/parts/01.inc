// WDForceStereo v1.2 - Watch Dogs (2014), configurable XAudio2 2.7 stereo downmix
// x64, no CRT. Drop as dinput8.dll next to watch_dogs.exe.
//
// Findings from previous probes on the target machine:
//   - Physical device reports 2ch stereo / 48 kHz / mask 0x3.
//   - Watch Dogs nevertheless creates a 6ch mastering voice.
//   - Main game audio is a 6ch IEEE-float SourceVoice and the game explicitly
//     sets a 6->6 matrix to the 6ch mastering voice.
//   - Forcing only the mastering voice to 2ch breaks that 6->6 matrix and loses dialogue.
//   - v0.7 confirms dialogue-like bursts live in the source Front Center channel.
//
// v1.2 targets the actual 6->2 matrix observed after forcing the mastering voice to stereo:
//   1) requests for a 6ch mastering voice are changed to 2ch;
//   2) the game's 6->2 matrix is adjusted using gains from WDForceStereo.ini;
//   3) FL/FR, FC, LFE and rear channels can be tuned independently, plus MasterGain.
//
// Defaults intentionally reproduce v1.1 working behavior:
//   FrontGain=1.0, CenterGain=1.0, SurroundGain=0.0, LFEGain=0.0, MasterGain=1.0.
//
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned long ULONG;
typedef long LONG;
typedef long HRESULT;
typedef int BOOL;
typedef unsigned long long ULONG_PTR;
typedef unsigned long long SIZE_T;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HINSTANCE;
typedef void* FARPROC;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef unsigned short WCHAR;
typedef const WCHAR* LPCWSTR;
typedef char CHAR;
typedef const CHAR* LPCSTR;
typedef DWORD* LPDWORD;
typedef void* LPUNKNOWN;

int _fltused = 0;

#define WINAPI __stdcall
#define TRUE 1
#define FALSE 0
#define NULL 0
#define DLL_PROCESS_ATTACH 1
#define MAX_PATH 260
#define PAGE_EXECUTE_READWRITE 0x40
#define FILE_APPEND_DATA 0x00000004
#define FILE_SHARE_READ  0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_END 2
#define CLSCTX_INPROC_SERVER 0x1
#define S_OK ((HRESULT)0L)
#define E_FAIL ((HRESULT)0x80004005L)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define INVALID_HANDLE_VALUE ((HANDLE)(~(ULONG_PTR)0))
#define WAVE_FORMAT_PCM 0x0001
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#define WAVE_FORMAT_EXTENSIBLE 0xFFFE

__declspec(dllimport) BOOL WINAPI DisableThreadLibraryCalls(HMODULE);
__declspec(dllimport) HANDLE WINAPI CreateThread(LPVOID, SIZE_T, DWORD (WINAPI*)(LPVOID), LPVOID, DWORD, LPDWORD);
__declspec(dllimport) HMODULE WINAPI GetModuleHandleW(LPCWSTR);
__declspec(dllimport) HMODULE WINAPI LoadLibraryW(LPCWSTR);
__declspec(dllimport) FARPROC WINAPI GetProcAddress(HMODULE, LPCSTR);
__declspec(dllimport) DWORD WINAPI GetModuleFileNameW(HMODULE, WCHAR*, DWORD);
__declspec(dllimport) UINT WINAPI GetSystemDirectoryW(WCHAR*, UINT);
__declspec(dllimport) BOOL WINAPI CloseHandle(HANDLE);
__declspec(dllimport) BOOL WINAPI VirtualProtect(LPVOID, SIZE_T, DWORD, LPDWORD);
__declspec(dllimport) HANDLE WINAPI CreateFileW(LPCWSTR, DWORD, DWORD, LPVOID, DWORD, DWORD, HANDLE);
__declspec(dllimport) BOOL WINAPI WriteFile(HANDLE, LPCVOID, DWORD, LPDWORD, LPVOID);
__declspec(dllimport) DWORD WINAPI SetFilePointer(HANDLE, LONG, LONG*, DWORD);

void* memcpy(void* dst, const void* src, SIZE_T n) { BYTE* d=(BYTE*)dst; const BYTE* s=(const BYTE*)src; SIZE_T i; for(i=0;i<n;++i)d[i]=s[i]; return dst; }
void* memset(void* dst, int c, SIZE_T n) { BYTE* d=(BYTE*)dst; SIZE_T i; for(i=0;i<n;++i)d[i]=(BYTE)c; return dst; }

typedef struct _GUID { DWORD Data1; WORD Data2; WORD Data3; BYTE Data4[8]; } GUID;
#pragma pack(push,1)
typedef struct _WAVEFORMATEX_MIN { WORD wFormatTag; WORD nChannels; DWORD nSamplesPerSec; DWORD nAvgBytesPerSec; WORD nBlockAlign; WORD wBitsPerSample; WORD cbSize; } WAVEFORMATEX_MIN;
#pragma pack(pop)
typedef struct _XAUDIO2_VOICE_DETAILS_MIN { UINT CreationFlags; UINT InputChannels; UINT InputSampleRate; } XAUDIO2_VOICE_DETAILS_MIN;
typedef struct _XAUDIO2_SEND_DESCRIPTOR_MIN { UINT Flags; void* pOutputVoice; } XAUDIO2_SEND_DESCRIPTOR_MIN;
typedef struct _XAUDIO2_VOICE_SENDS_MIN { UINT SendCount; XAUDIO2_SEND_DESCRIPTOR_MIN* pSends; } XAUDIO2_VOICE_SENDS_MIN;

static const GUID kCLSID_XAudio2={0x5a508685,0xa254,0x4fba,{0x9b,0x82,0x9a,0x24,0xb0,0x03,0x06,0xaf}};
static const GUID kIID_IXAudio2={0x8bcf1f58,0x9fe7,0x4583,{0x8a,0xc6,0xe2,0xad,0xc4,0x65,0xc8,0xbb}};

typedef HRESULT (WINAPI *PFN_CoCreateInstance)(const GUID*,LPUNKNOWN,DWORD,const GUID*,LPVOID*);
typedef HRESULT (WINAPI *PFN_CoInitializeEx)(LPVOID,DWORD);
typedef void (WINAPI *PFN_CoUninitialize)(void);
typedef ULONG (WINAPI *PFN_IUnknownRelease)(void*);
typedef HRESULT (WINAPI *PFN_CreateSourceVoice27)(void*,void**,const WAVEFORMATEX_MIN*,UINT,float,void*,const void*,const void*);
typedef HRESULT (WINAPI *PFN_CreateMasteringVoice27)(void*,void**,UINT,UINT,UINT,UINT,const void*);
typedef HRESULT (WINAPI *PFN_SetOutputVoices27)(void*,const XAUDIO2_VOICE_SENDS_MIN*);
typedef HRESULT (WINAPI *PFN_SetOutputMatrix27)(void*,void*,UINT,UINT,const float*,UINT);
typedef void (WINAPI *PFN_GetVoiceDetails27)(void*,XAUDIO2_VOICE_DETAILS_MIN*);
typedef HRESULT (WINAPI *PFN_DirectInput8Create)(HINSTANCE,DWORD,const GUID*,LPVOID*,LPUNKNOWN);
typedef HRESULT (WINAPI *PFN_DllNoArgs)(void);
typedef HRESULT (WINAPI *PFN_DllGetClassObject)(const GUID*,const GUID*,LPVOID*);

static HMODULE g_self=NULL,g_realDinput8=NULL;
static float g_frontGain=1.0f;
static float g_centerGain=1.0f;
static float g_surroundGain=0.0f;
static float g_lfeGain=0.0f;
static float g_masterGain=1.0f;
static UINT g_logEnabled=1;
static PFN_CoCreateInstance g_realCoCreateInstance=NULL;
static PFN_CreateSourceVoice27 g_realCreateSourceVoice=NULL;
static PFN_CreateMasteringVoice27 g_realCreateMasteringVoice=NULL;
