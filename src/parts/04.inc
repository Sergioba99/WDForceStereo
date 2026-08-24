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
