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
