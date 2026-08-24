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
