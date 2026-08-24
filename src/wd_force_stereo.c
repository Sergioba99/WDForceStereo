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
static volatile LONG g_hooksInstalled=0;
static WCHAR g_gameDir[MAX_PATH],g_logPath[MAX_PATH],g_iniPath[MAX_PATH];

#define MAX_ENGINES 8
#define MAX_VOICES 64
#define MAX_VTBLS 8

typedef struct _ENGINE_INFO { void* engine; void* mastering; UINT masteringChannels; UINT masteringRate; } ENGINE_INFO;
typedef struct _VOICE_INFO { void* voice; void* engine; UINT channels; UINT rate; WORD tag; WORD bits; WORD blockAlign; WORD cbSize; DWORD extMask; DWORD subFormatData1; UINT explicitSendCount; void* explicitDest; } VOICE_INFO;
typedef struct _VTBL_HOOK { void** vtbl; PFN_SetOutputVoices27 setVoices; PFN_SetOutputMatrix27 setMatrix; } VTBL_HOOK;
static ENGINE_INFO g_engines[MAX_ENGINES]; static UINT g_engineCount=0;
static VOICE_INFO g_voices[MAX_VOICES]; static UINT g_voiceCount=0;
static VTBL_HOOK g_vtbls[MAX_VTBLS]; static UINT g_vtblCount=0;

static void BuildPaths(void){
    WCHAR exe[MAX_PATH];DWORD n=GetModuleFileNameW(NULL,exe,MAX_PATH),i,cut=0;
    if(!n||n>=MAX_PATH)return;
    for(i=0;i<n;++i){g_gameDir[i]=exe[i];if(exe[i]==L'\\'||exe[i]==L'/')cut=i+1;}
    g_gameDir[cut]=0;
    for(i=0;i<cut&&i<MAX_PATH-1;++i){g_logPath[i]=g_gameDir[i];g_iniPath[i]=g_gameDir[i];}
    {
        const WCHAR logName[]=L"WDForceStereo.log";UINT j=0;
        while(logName[j]&&cut+j<MAX_PATH-1){g_logPath[cut+j]=logName[j];++j;}g_logPath[cut+j]=0;
    }
    {
        const WCHAR iniName[]=L"WDForceStereo.ini";UINT j=0;
        while(iniName[j]&&cut+j<MAX_PATH-1){g_iniPath[cut+j]=iniName[j];++j;}g_iniPath[cut+j]=0;
    }
}
static UINT AS(CHAR*o,UINT p,UINT c,const CHAR*s){UINT i=0;while(s&&s[i]&&p+1<c)o[p++]=s[i++];o[p]=0;return p;}
static UINT AC(CHAR*o,UINT p,UINT c,CHAR x){if(p+1<c)o[p++]=x;o[p]=0;return p;}
static UINT AU(CHAR*o,UINT p,UINT c,UINT v){CHAR t[16];UINT n=0,i;if(!v){if(p+1<c)o[p++]='0';o[p]=0;return p;}while(v&&n<15){t[n++]=(CHAR)('0'+v%10);v/=10;}for(i=0;i<n&&p+1<c;++i)o[p++]=t[n-1-i];o[p]=0;return p;}
static UINT AH(CHAR*o,UINT p,UINT c,DWORD v){static const CHAR h[]="0123456789ABCDEF";int i;p=AS(o,p,c,"0x");for(i=7;i>=0&&p+1<c;--i)o[p++]=h[(v>>(i*4))&15];o[p]=0;return p;}
static UINT AP(CHAR*o,UINT p,UINT c,const void*x){static const CHAR h[]="0123456789ABCDEF";ULONG_PTR v=(ULONG_PTR)x;int i;p=AS(o,p,c,"0x");for(i=15;i>=0&&p+1<c;--i)o[p++]=h[(UINT)((v>>(i*4))&15ULL)];o[p]=0;return p;}
static UINT AF3(CHAR*o,UINT p,UINT c,float f){UINT w,r;if(f<0){p=AC(o,p,c,'-');f=-f;}if(f>999)return AS(o,p,c,">999");w=(UINT)f;r=(UINT)((f-(float)w)*1000.0f+0.5f);if(r>=1000){++w;r-=1000;}p=AU(o,p,c,w);p=AC(o,p,c,'.');p=AC(o,p,c,(CHAR)('0'+(r/100)%10));p=AC(o,p,c,(CHAR)('0'+(r/10)%10));p=AC(o,p,c,(CHAR)('0'+r%10));return p;}
static void LogRaw(const CHAR*s){HANDLE f;DWORD w=0,l=0;if(!g_logEnabled)return;if(!g_logPath[0])BuildPaths();while(s[l])++l;f=CreateFileW(g_logPath,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);if(f==INVALID_HANDLE_VALUE)return;SetFilePointer(f,0,NULL,FILE_END);WriteFile(f,s,l,&w,NULL);CloseHandle(f);}
static void LogLine(const CHAR*s){LogRaw(s);LogRaw("\r\n");}
static int ParseFloatW(const WCHAR*s,float*out){
    UINT i=0,digits=0;float v=0.0f,frac=0.1f;int neg=0;
    if(!s||!out)return 0;
    while(s[i]==L' '||s[i]==L'\t')++i;
    if(s[i]==L'-'){neg=1;++i;}else if(s[i]==L'+')++i;
    while(s[i]>=L'0'&&s[i]<=L'9'){v=v*10.0f+(float)(s[i]-L'0');++i;++digits;}
    if(s[i]==L'.'||s[i]==L','){
        ++i;
        while(s[i]>=L'0'&&s[i]<=L'9'){
            v+=(float)(s[i]-L'0')*frac;frac*=0.1f;++i;++digits;
        }
    }
    while(s[i]==L' '||s[i]==L'\t')++i;
    if(!digits||s[i]!=0)return 0;
    if(neg)v=-v;
    *out=v;return 1;
}
typedef DWORD (WINAPI *PFN_GetPrivateProfileStringW)(LPCWSTR,LPCWSTR,LPCWSTR,WCHAR*,DWORD,LPCWSTR);
static float ReadIniGain(PFN_GetPrivateProfileStringW gp,LPCWSTR key,LPCWSTR def,float fallback){
    WCHAR b[64];float v=fallback;
    b[0]=0;gp(L"Audio",key,def,b,64,g_iniPath);
    if(ParseFloatW(b,&v)){if(v<0.0f)v=0.0f;if(v>4.0f)v=4.0f;return v;}
    return fallback;
}
static void LoadConfig(void){
    HMODULE k;PFN_GetPrivateProfileStringW gp;WCHAR b[64];
    if(!g_iniPath[0])BuildPaths();
    k=GetModuleHandleW(L"kernel32.dll");
    gp=k?(PFN_GetPrivateProfileStringW)GetProcAddress(k,"GetPrivateProfileStringW"):NULL;
    if(!gp)return;
    g_frontGain=ReadIniGain(gp,L"FrontGain",L"1.0",1.0f);
    g_centerGain=ReadIniGain(gp,L"CenterGain",L"1.0",1.0f);
    g_surroundGain=ReadIniGain(gp,L"SurroundGain",L"0.0",0.0f);
    g_lfeGain=ReadIniGain(gp,L"LFEGain",L"0.0",0.0f);
    g_masterGain=ReadIniGain(gp,L"MasterGain",L"1.0",1.0f);
    b[0]=0;gp(L"Debug",L"Log",L"1",b,64,g_iniPath);
    g_logEnabled=(b[0]==L'0'&&b[1]==0)?0:1;
}
static HMODULE EnsureRealDinput8(void){WCHAR p[MAX_PATH];UINT n,i;if(g_realDinput8)return g_realDinput8;n=GetSystemDirectoryW(p,MAX_PATH);if(!n||n>MAX_PATH-14)return NULL;i=n;if(i&&p[i-1]!=L'\\')p[i++]=L'\\';{const WCHAR d[]=L"dinput8.dll";UINT j=0;while(d[j]&&i<MAX_PATH-1)p[i++]=d[j++];p[i]=0;}g_realDinput8=LoadLibraryW(p);return g_realDinput8;}

static ENGINE_INFO* EngineInfo(void* e,int create){UINT i;for(i=0;i<g_engineCount;++i)if(g_engines[i].engine==e)return &g_engines[i];if(create&&g_engineCount<MAX_ENGINES){ENGINE_INFO*x=&g_engines[g_engineCount++];memset(x,0,sizeof(*x));x->engine=e;return x;}return NULL;}
static VOICE_INFO* VoiceInfo(void*v,int create){UINT i;for(i=0;i<g_voiceCount;++i)if(g_voices[i].voice==v)return &g_voices[i];if(create&&g_voiceCount<MAX_VOICES){VOICE_INFO*x=&g_voices[g_voiceCount++];memset(x,0,sizeof(*x));x->voice=v;return x;}return NULL;}
static VTBL_HOOK* VtblInfo(void*voice){void**vt=voice?*(void***)voice:NULL;UINT i;if(!vt)return NULL;for(i=0;i<g_vtblCount;++i)if(g_vtbls[i].vtbl==vt)return &g_vtbls[i];return NULL;}
static UINT ReadDword(const BYTE*p){UINT v;memcpy(&v,p,4);return v;}

static void ParseFormat(VOICE_INFO*vi,const WAVEFORMATEX_MIN*f){const BYTE*b=(const BYTE*)f;if(!vi||!f)return;vi->channels=f->nChannels;vi->rate=f->nSamplesPerSec;vi->tag=f->wFormatTag;vi->bits=f->wBitsPerSample;vi->blockAlign=f->nBlockAlign;vi->cbSize=f->cbSize;vi->extMask=0;vi->subFormatData1=0;if(f->wFormatTag==WAVE_FORMAT_EXTENSIBLE&&f->cbSize>=22){vi->extMask=ReadDword(b+20);vi->subFormatData1=ReadDword(b+24);}}
static void LogSource(const VOICE_INFO*vi,const XAUDIO2_VOICE_SENDS_MIN*sends,const void*effects){CHAR l[720];UINT p=0;p=AS(l,p,sizeof(l),"SourceVoice created: ptr=");p=AP(l,p,sizeof(l),vi->voice);p=AS(l,p,sizeof(l)," engine=");p=AP(l,p,sizeof(l),vi->engine);p=AS(l,p,sizeof(l)," ch=");p=AU(l,p,sizeof(l),vi->channels);p=AS(l,p,sizeof(l)," rate=");p=AU(l,p,sizeof(l),vi->rate);p=AS(l,p,sizeof(l)," tag=");p=AH(l,p,sizeof(l),(DWORD)vi->tag);p=AS(l,p,sizeof(l)," bits=");p=AU(l,p,sizeof(l),vi->bits);p=AS(l,p,sizeof(l)," align=");p=AU(l,p,sizeof(l),vi->blockAlign);p=AS(l,p,sizeof(l)," cbSize=");p=AU(l,p,sizeof(l),vi->cbSize);if(vi->tag==WAVE_FORMAT_EXTENSIBLE){p=AS(l,p,sizeof(l)," extMask=");p=AH(l,p,sizeof(l),vi->extMask);p=AS(l,p,sizeof(l)," subFmtData1=");p=AH(l,p,sizeof(l),vi->subFormatData1);}p=AS(l,p,sizeof(l)," effects=");p=AP(l,p,sizeof(l),effects);if(!sends)p=AS(l,p,sizeof(l)," sends=DEFAULT_MASTERING");else{p=AS(l,p,sizeof(l)," sends=");p=AU(l,p,sizeof(l),sends->SendCount);if(sends->SendCount&&sends->pSends){p=AS(l,p,sizeof(l)," firstDest=");p=AP(l,p,sizeof(l),sends->pSends[0].pOutputVoice);}}LogLine(l);}

static UINT DestChannelsFor(VOICE_INFO*vi,const XAUDIO2_VOICE_SENDS_MIN*sends,void**destOut){XAUDIO2_VOICE_DETAILS_MIN d;ENGINE_INFO*ei;if(destOut)*destOut=NULL;if(sends){if(sends->SendCount!=1||!sends->pSends)return 0;if(destOut)*destOut=sends->pSends[0].pOutputVoice;if(!sends->pSends[0].pOutputVoice)return 0;memset(&d,0,sizeof(d));{void**vt=*(void***)sends->pSends[0].pOutputVoice;if(!vt||!vt[0])return 0;((PFN_GetVoiceDetails27)vt[0])(sends->pSends[0].pOutputVoice,&d);}return d.InputChannels;}ei=EngineInfo(vi->engine,0);if(ei){if(destOut)*destOut=ei->mastering;return ei->masteringChannels;}return 0;}

static void BuildStereoKernel(float*k){
    UINT i;for(i=0;i<12;++i)k[i]=0.0f;
    /* XAudio2 matrix layout: source S -> destination D is k[S + 6*D].
       Stereo L row: FL, FR, FC, LFE, BL, BR
       Stereo R row: FL, FR, FC, LFE, BL, BR */
    k[0]=g_frontGain;          /* FL -> L */
    k[2]=g_centerGain;         /* FC -> L */
    k[3]=g_lfeGain;            /* LFE -> L */
    k[4]=g_surroundGain;       /* BL -> L */
    k[6+1]=g_frontGain;        /* FR -> R */
    k[6+2]=g_centerGain;       /* FC -> R */
    k[6+3]=g_lfeGain;          /* LFE -> R */
    k[6+5]=g_surroundGain;     /* BR -> R */
}

/* Fallback for a game 6->6 matrix: compose that matrix with the configured
   5.1->stereo kernel so the call remains valid against the forced 2ch master. */
static void Compose6x6To6x2(const float*game6,float*out12){
    float k[12];UINT s,d,v;BuildStereoKernel(k);
    for(d=0;d<2;++d){
        for(s=0;s<6;++s){
            float sum=0.0f;
            for(v=0;v<6;++v)sum += k[v + 6*d] * game6[s + 6*v];
            out12[s + 6*d]=sum*g_masterGain;
        }
    }
}
static HRESULT WINAPI Hook_SetOutputVoices(void*This,const XAUDIO2_VOICE_SENDS_MIN*sends){
    VTBL_HOOK*h=VtblInfo(This);VOICE_INFO*vi=VoiceInfo(This,0);HRESULT hr;
    if(!h||!h->setVoices)return E_FAIL;
    hr=h->setVoices(This,sends);
    if(vi&&vi->channels==6){
        CHAR l[280];UINT p=0;void*dest=NULL;UINT dc=DestChannelsFor(vi,sends,&dest);
        p=AS(l,p,sizeof(l),"SetOutputVoices on 6ch source: hr=");p=AH(l,p,sizeof(l),(DWORD)hr);
        p=AS(l,p,sizeof(l)," resolvedDestCh=");p=AU(l,p,sizeof(l),dc);
        p=AS(l,p,sizeof(l)," dest=");p=AP(l,p,sizeof(l),dest);LogLine(l);
    }
    return hr;
}

static HRESULT WINAPI Hook_SetOutputMatrix(void*This,void*dest,UINT sc,UINT dc,const float*m,UINT op){
    VTBL_HOOK*h=VtblInfo(This);VOICE_INFO*vi=VoiceInfo(This,0);HRESULT hr;
    CHAR l[900];UINT p=0;
    if(!h||!h->setMatrix)return E_FAIL;

    if(vi&&vi->channels==6){
        p=AS(l,p,sizeof(l),"Game SetOutputMatrix on 6ch source: ");
        p=AU(l,p,sizeof(l),sc);p=AS(l,p,sizeof(l),"->");p=AU(l,p,sizeof(l),dc);
        p=AS(l,p,sizeof(l)," dest=");p=AP(l,p,sizeof(l),dest);LogLine(l);

        if(m&&sc==6&&dc==2){
            /* XAudio2 layout is m[SourceChannels * D + S]. */
            p=0;p=AS(l,p,sizeof(l),"  L row [FL FR FC LFE BL BR] = ");
            p=AF3(l,p,sizeof(l),m[0]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[1]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[2]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[3]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[4]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[5]);LogLine(l);
            p=0;p=AS(l,p,sizeof(l),"  R row [FL FR FC LFE BL BR] = ");
            p=AF3(l,p,sizeof(l),m[6]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[7]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[8]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[9]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[10]);p=AS(l,p,sizeof(l)," ");
            p=AF3(l,p,sizeof(l),m[11]);LogLine(l);

#ifndef WD_DIAGNOSTIC_6TO2
            {
                ENGINE_INFO*ei=EngineInfo(vi->engine,0);
                if(ei&&ei->mastering==dest&&ei->masteringChannels==2){
                    float t[12];UINT i;
                    for(i=0;i<12;++i)t[i]=m[i];
                    /* FrontGain scales the game's existing FL/FR routing. */
                    t[0]*=g_frontGain; t[6]*=g_frontGain;
                    t[1]*=g_frontGain; t[7]*=g_frontGain;
                    /* The tested game matrix zeros these sources entirely, so expose
                       explicit stereo fold-down gains for them. */
                    t[2]=g_centerGain;      t[8]=g_centerGain;
                    t[3]=g_lfeGain;         t[9]=g_lfeGain;
                    t[4]=g_surroundGain;    t[10]=0.0f;
                    t[5]=0.0f;              t[11]=g_surroundGain;
                    for(i=0;i<12;++i)t[i]*=g_masterGain;
                    hr=h->setMatrix(This,dest,6,2,t,op);
                    p=0;p=AS(l,p,sizeof(l),"  -> CONFIGURED DOWNMIX 6->2 hr=");
                    p=AH(l,p,sizeof(l),(DWORD)hr);LogLine(l);
                    p=0;p=AS(l,p,sizeof(l),"     L final [FL FR FC LFE BL BR] = ");
                    p=AF3(l,p,sizeof(l),t[0]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[1]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[2]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[3]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[4]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[5]);LogLine(l);
                    p=0;p=AS(l,p,sizeof(l),"     R final [FL FR FC LFE BL BR] = ");
                    p=AF3(l,p,sizeof(l),t[6]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[7]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[8]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[9]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[10]);p=AS(l,p,sizeof(l)," ");p=AF3(l,p,sizeof(l),t[11]);LogLine(l);
                    return hr;
                }
                LogLine("  -> 6->2 matrix is not targeting the tracked forced-stereo mastering voice; pass-through.");
            }
#else
            LogLine("  -> DIAGNOSTIC: actual 6->2 matrix logged; not modified.");
#endif
        }

        /* Fallback retained from v0.8 in case a 6->6 call is seen despite the
           mastering voice having been forced to stereo. */
#ifndef WD_DIAGNOSTIC_6TO2
        if(m&&sc==6&&dc==6){
            ENGINE_INFO*ei=EngineInfo(vi->engine,0);
            if(ei&&ei->mastering==dest&&ei->masteringChannels==2){
                float t[12];
                Compose6x6To6x2(m,t);
                hr=h->setMatrix(This,dest,6,2,t,op);
                p=0;p=AS(l,p,sizeof(l),"  -> FALLBACK rewrite 6->6 => 6->2 hr=");
                p=AH(l,p,sizeof(l),(DWORD)hr);LogLine(l);
                return hr;
            }
        }
#endif
    }
    return h->setMatrix(This,dest,sc,dc,m,op);
}

static void HookSourceVtable(void*voice){void**vt;DWORD old;UINT i;if(!voice)return;vt=*(void***)voice;if(!vt)return;for(i=0;i<g_vtblCount;++i)if(g_vtbls[i].vtbl==vt)return;if(g_vtblCount>=MAX_VTBLS){LogLine("WARNING: source vtable hook table full.");return;}if(!VirtualProtect(&vt[1],sizeof(void*),PAGE_EXECUTE_READWRITE,&old)){LogLine("ERROR: cannot hook SetOutputVoices.");return;}g_vtbls[g_vtblCount].vtbl=vt;g_vtbls[g_vtblCount].setVoices=(PFN_SetOutputVoices27)vt[1];vt[1]=(void*)&Hook_SetOutputVoices;{DWORD x;VirtualProtect(&vt[1],sizeof(void*),old,&x);}if(!VirtualProtect(&vt[16],sizeof(void*),PAGE_EXECUTE_READWRITE,&old)){LogLine("ERROR: cannot hook SetOutputMatrix.");return;}g_vtbls[g_vtblCount].setMatrix=(PFN_SetOutputMatrix27)vt[16];vt[16]=(void*)&Hook_SetOutputMatrix;{DWORD x;VirtualProtect(&vt[16],sizeof(void*),old,&x);}++g_vtblCount;LogLine("Installed source vtable hooks: SetOutputVoices + SetOutputMatrix.");}

static HRESULT WINAPI Hook_CreateSourceVoice27(void*This,void**pp,const WAVEFORMATEX_MIN*f,UINT flags,float ratio,void*cb,const void*sendList,const void*fx){HRESULT hr=g_realCreateSourceVoice(This,pp,f,flags,ratio,cb,sendList,fx);if(!FAILED(hr)&&pp&&*pp){VOICE_INFO*vi=VoiceInfo(*pp,1);if(vi){memset(vi,0,sizeof(*vi));vi->voice=*pp;vi->engine=This;ParseFormat(vi,f);if(sendList){const XAUDIO2_VOICE_SENDS_MIN*s=(const XAUDIO2_VOICE_SENDS_MIN*)sendList;vi->explicitSendCount=s->SendCount;if(s->SendCount&&s->pSends)vi->explicitDest=s->pSends[0].pOutputVoice;}LogSource(vi,(const XAUDIO2_VOICE_SENDS_MIN*)sendList,fx);HookSourceVtable(*pp);
}}return hr;}
static HRESULT WINAPI Hook_CreateMasteringVoice27(void*This,void**pp,UINT ch,UINT rate,UINT flags,UINT dev,const void*fx){
    UINT passedCh=(ch==6)?2:ch;
    HRESULT hr=g_realCreateMasteringVoice(This,pp,passedCh,rate,flags,dev,fx);
    CHAR l[420];UINT p=0;
    p=AS(l,p,sizeof(l),"CreateMasteringVoice: requested=");p=AU(l,p,sizeof(l),ch);
    p=AS(l,p,sizeof(l)," passed=");p=AU(l,p,sizeof(l),passedCh);
    p=AS(l,p,sizeof(l)," rate=");p=AU(l,p,sizeof(l),rate);
    p=AS(l,p,sizeof(l)," dev=");p=AU(l,p,sizeof(l),dev);
    p=AS(l,p,sizeof(l)," hr=");p=AH(l,p,sizeof(l),(DWORD)hr);
    if(!FAILED(hr)&&pp&&*pp){
        XAUDIO2_VOICE_DETAILS_MIN d;ENGINE_INFO*ei=EngineInfo(This,1);
        memset(&d,0,sizeof(d));
        {void**vt=*(void***)*pp;if(vt&&vt[0])((PFN_GetVoiceDetails27)vt[0])(*pp,&d);}
        if(ei){
            ei->mastering=*pp;
            ei->masteringChannels=d.InputChannels?d.InputChannels:passedCh;
            ei->masteringRate=d.InputSampleRate?d.InputSampleRate:rate;
        }
        p=AS(l,p,sizeof(l)," ptr=");p=AP(l,p,sizeof(l),*pp);
        p=AS(l,p,sizeof(l)," actualCh=");p=AU(l,p,sizeof(l),d.InputChannels);
        p=AS(l,p,sizeof(l)," actualRate=");p=AU(l,p,sizeof(l),d.InputSampleRate);
        if(ch==6)p=AS(l,p,sizeof(l)," [FORCED STEREO]");
    }
    LogLine(l);return hr;
}

static void InstallEngineHooks(void*xa){void**vt;DWORD old;if(!xa||g_hooksInstalled)return;vt=*(void***)xa;if(!vt)return;if(!VirtualProtect(&vt[8],3*sizeof(void*),PAGE_EXECUTE_READWRITE,&old)){LogLine("ERROR: VirtualProtect engine vtable failed.");return;}g_realCreateSourceVoice=(PFN_CreateSourceVoice27)vt[8];vt[8]=(void*)&Hook_CreateSourceVoice27;g_realCreateMasteringVoice=(PFN_CreateMasteringVoice27)vt[10];vt[10]=(void*)&Hook_CreateMasteringVoice27;{DWORD x;VirtualProtect(&vt[8],3*sizeof(void*),old,&x);}g_hooksInstalled=1;LogLine("XAudio2 2.7 hooks installed: 6ch mastering -> 2ch + configurable 6->2 downmix.");}
static void InitCoCreate(void){HMODULE o;if(g_realCoCreateInstance)return;o=GetModuleHandleW(L"ole32.dll");if(!o)o=LoadLibraryW(L"ole32.dll");if(o)g_realCoCreateInstance=(PFN_CoCreateInstance)GetProcAddress(o,"CoCreateInstance");}
static int Probe(void){HMODULE o,x;PFN_CoInitializeEx ci;PFN_CoUninitialize cu;HRESULT ih,hr;void*xa=NULL;int un=0;x=LoadLibraryW(L"XAudio2_7.dll");if(!x){LogLine("Probe: XAudio2_7.dll load failed.");return 0;}o=GetModuleHandleW(L"ole32.dll");if(!o)o=LoadLibraryW(L"ole32.dll");if(!o)return 0;ci=(PFN_CoInitializeEx)GetProcAddress(o,"CoInitializeEx");cu=(PFN_CoUninitialize)GetProcAddress(o,"CoUninitialize");InitCoCreate();if(!g_realCoCreateInstance)return 0;ih=ci?ci(NULL,0):S_OK;if(ih==S_OK||ih==1)un=1;hr=g_realCoCreateInstance(&kCLSID_XAudio2,NULL,CLSCTX_INPROC_SERVER,&kIID_IXAudio2,&xa);{CHAR l[150];UINT p=0;p=AS(l,p,sizeof(l),"Probe CoCreateInstance hr=");p=AH(l,p,sizeof(l),(DWORD)hr);LogLine(l);}if(!FAILED(hr)&&xa){void**vt=*(void***)xa;InstallEngineHooks(xa);if(vt&&vt[2])((PFN_IUnknownRelease)vt[2])(xa);}if(un&&cu)cu();return g_hooksInstalled?1:0;}
static DWORD WINAPI HookThread(LPVOID u){(void)u;BuildPaths();LoadConfig();
#ifdef WD_DIAGNOSTIC_6TO2
LogLine("=== WDForceStereo 1.0 DIAGNOSTIC 6->2 MATRIX ===");
LogLine("Mode: force game's 6ch mastering to 2ch, log the actual 6->2 matrix, modify matrix NOTHING.");
#else
LogLine("=== WDForceStereo 1.2 CONFIGURABLE DOWNMIX ===");
LogLine("Mode: force game's 6ch mastering to 2ch and tune the 6->2 matrix from WDForceStereo.ini.");
#endif
{CHAR l[360];UINT p=0;p=AS(l,p,sizeof(l),"Config: FrontGain=");p=AF3(l,p,sizeof(l),g_frontGain);p=AS(l,p,sizeof(l)," CenterGain=");p=AF3(l,p,sizeof(l),g_centerGain);p=AS(l,p,sizeof(l)," SurroundGain=");p=AF3(l,p,sizeof(l),g_surroundGain);p=AS(l,p,sizeof(l)," LFEGain=");p=AF3(l,p,sizeof(l),g_lfeGain);p=AS(l,p,sizeof(l)," MasterGain=");p=AF3(l,p,sizeof(l),g_masterGain);LogLine(l);}
LogLine("Matrix order: destination rows L/R; source columns FL FR FC LFE BL BR.");
if(Probe())LogLine("Engine hook active.");else LogLine("ERROR: XAudio2 probe failed.");return 0;}

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE h,DWORD v,const GUID*r,LPVOID*out,LPUNKNOWN outer){PFN_DirectInput8Create f;HMODULE m=EnsureRealDinput8();if(!m)return E_FAIL;f=(PFN_DirectInput8Create)GetProcAddress(m,"DirectInput8Create");return f?f(h,v,r,out,outer):E_FAIL;}
__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void){PFN_DllNoArgs f;HMODULE m=EnsureRealDinput8();if(!m)return E_FAIL;f=(PFN_DllNoArgs)GetProcAddress(m,"DllCanUnloadNow");return f?f():E_FAIL;}
__declspec(dllexport) HRESULT WINAPI DllGetClassObject(const GUID*a,const GUID*b,LPVOID*c){PFN_DllGetClassObject f;HMODULE m=EnsureRealDinput8();if(!m)return E_FAIL;f=(PFN_DllGetClassObject)GetProcAddress(m,"DllGetClassObject");return f?f(a,b,c):E_FAIL;}
__declspec(dllexport) HRESULT WINAPI DllRegisterServer(void){PFN_DllNoArgs f;HMODULE m=EnsureRealDinput8();if(!m)return E_FAIL;f=(PFN_DllNoArgs)GetProcAddress(m,"DllRegisterServer");return f?f():E_FAIL;}
__declspec(dllexport) HRESULT WINAPI DllUnregisterServer(void){PFN_DllNoArgs f;HMODULE m=EnsureRealDinput8();if(!m)return E_FAIL;f=(PFN_DllNoArgs)GetProcAddress(m,"DllUnregisterServer");return f?f():E_FAIL;}
BOOL WINAPI DllMain(HINSTANCE h,DWORD r,LPVOID x){(void)x;if(r==DLL_PROCESS_ATTACH){HANDLE t;g_self=(HMODULE)h;DisableThreadLibraryCalls(g_self);BuildPaths();t=CreateThread(NULL,0,HookThread,NULL,0,NULL);if(t)CloseHandle(t);}return TRUE;}

#ifdef __cplusplus
}
#endif
